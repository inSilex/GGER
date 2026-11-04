#!/usr/bin/env python3
"""Repeatable frame-pacing benchmark for NetWalk debug builds.

The profiler is compiled only when NW_DEBUG_TOOLS is enabled.  Set
NW_DEBUG_PROFILE and the game writes a CSV:

  F,frame,t_us,gap_us,dur_us,evt_us,step_us,step_pres_us,final_pres_us,
    present_us,conv_us,update_us,audio_us,events,presents,click,key,res
  I,t_us,kind,a,b

This tool launches the executable with the profiler enabled, waits for the
main loop to produce frame rows, measures for a fixed interval, then summarizes
frame pacing.  It does not inject input, so the menu remains on its idle path.
If the run only reaches the startup animation, the report contains compose
rows instead of presented-frame rows.

Examples:
  python3 tools/nwbench.py --exe build/linux-x86_64-debug/NETWALK.EXE --native
  python3 tools/nwbench.py --exe build/windows-x64-mingw-debug/netwalk.exe --dummy
  python3 tools/nwbench.py --exe ... --scale 4 --seconds 10 --audio

Release executables ignore NW_DEBUG_PROFILE; use a *-debug preset.
"""
import argparse
import glob
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
NW = os.path.dirname(HERE)
DEFAULT_BANK = os.path.join(NW, "third_party", "windowsgs", "gm.sf2")


def log(message):
    print(message, flush=True)


def percentile(values, q):
    if not values:
        return None
    values = sorted(values)
    return values[int(q * (len(values) - 1))]


def summary(values):
    if not values:
        return {"n": 0}
    return {
        "n": len(values),
        "mean": round(sum(values) / len(values), 1),
        "p50": percentile(values, 0.50),
        "p95": percentile(values, 0.95),
        "p99": percentile(values, 0.99),
        "max": max(values),
    }


def parse_profile(path):
    frames = []
    events = []
    if not os.path.isfile(path):
        return frames, events
    with open(path, "r", errors="replace") as handle:
        for line in handle:
            fields = line.rstrip("\n").split(",")
            if not fields:
                continue
            if fields[0] == "F" and len(fields) >= 18:
                try:
                    frames.append({
                        "frame": int(fields[1]),
                        "t": int(fields[2]),
                        "gap": int(fields[3]),
                        "dur": int(fields[4]),
                        "evt": int(fields[5]),
                        "step": int(fields[6]),
                        "step_pres": int(fields[7]),
                        "final_pres": int(fields[8]),
                        "present": int(fields[9]),
                        "conv": int(fields[10]),
                        "update": int(fields[11]),
                        "audio": int(fields[12]),
                        "events": int(fields[13]),
                        "presents": int(fields[14]),
                        "click": int(fields[15]) & 0xFF,
                        "key": int(fields[16]) & 0xFF,
                        "res": int(fields[17]),
                    })
                except ValueError:
                    pass
            elif fields[0] == "I" and len(fields) >= 5:
                try:
                    events.append({
                        "t": int(fields[1]),
                        "kind": int(fields[2]),
                        "a": int(fields[3]),
                        "b": int(fields[4]),
                    })
                except ValueError:
                    pass
    return frames, events


def analyse(frames, events):
    report = {}
    compose = [event for event in events if event["kind"] == 5]
    if not frames:
        if not compose:
            report["error"] = "no frame or compose rows"
            return report
        times = [event["t"] for event in compose]
        gaps = [(times[i] - times[i - 1]) / 1000.0
                for i in range(1, len(times))]
        report["mode"] = "startup-compose"
        report["frames"] = 0
        report["fps"] = 0
        report["compose"] = {
            "frames": len(compose),
            "span_s": round((times[-1] - times[0]) / 1e6, 3),
            "interval_ms": summary(gaps),
        }
        return report

    report["mode"] = "frames"
    span_us = frames[-1]["t"] - frames[0]["t"]
    report["frames"] = len(frames)
    report["span_s"] = round(span_us / 1e6, 3)
    report["fps"] = round((len(frames) - 1) * 1e6 / span_us, 2) if span_us else 0
    for key in ("gap", "dur", "evt", "step", "present", "conv", "update",
                "audio"):
        report[key + "_us"] = summary([frame[key] for frame in frames])
    report["presents_per_frame"] = round(
        sum(frame["presents"] for frame in frames) / len(frames), 3)

    if compose:
        times = [event["t"] for event in compose]
        gaps = [(times[i] - times[i - 1]) / 1000.0
                for i in range(1, len(times))]
        report["compose"] = {
            "frames": len(compose),
            "span_s": round((times[-1] - times[0]) / 1e6, 3),
            "interval_ms": summary(gaps),
        }

    stalled = [frame for frame in frames if frame["gap"] > 40000]
    report["stalls_ms"] = [
        {"frame": frame["frame"], "gap_ms": round(frame["gap"] / 1000.0, 1),
         "step_ms": round(frame["step"] / 1000.0, 1)}
        for frame in stalled[:20]
    ]
    last = frames[-1]
    report["last_frame"] = {
        "frame": last["frame"],
        "click": hex(last["click"]),
        "key": hex(last["key"]),
        "step_ms": round(last["step"] / 1000.0, 2),
    }
    return report


def profile_ready(path):
    """Return 'frames', 'compose', or None for the profile's current state."""
    compose = False
    try:
        with open(path, "r", errors="replace") as handle:
            for line in handle:
                fields = line.split(",")
                if (line.startswith("F,") and len(fields) > 1
                        and fields[1].isdigit()):
                    return "frames"
                if (line.startswith("I,") and len(fields) > 2
                        and fields[2] == "5"):
                    compose = True
    except OSError:
        pass
    return "compose" if compose else None


def is_pe(path):
    try:
        with open(path, "rb") as handle:
            return handle.read(2) == b"MZ"
    except OSError:
        return path.lower().endswith(".exe")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True,
                        help="debug-build executable")
    parser.add_argument("--seconds", type=float, default=10.0,
                        help="measurement time after the first frame")
    parser.add_argument("--warmup", type=float, default=1.0)
    parser.add_argument("--run", default=os.path.join(tempfile.gettempdir(),
                                                       "nwbench_run"))
    parser.add_argument("--scale", type=int, default=1)
    parser.add_argument("--audio", action="store_true",
                        help="allow audio and copy gm.sf2")
    parser.add_argument("--dummy", action="store_true",
                        help="SDL_VIDEODRIVER=dummy (logic only, no window)")
    parser.add_argument("--native", action="store_true",
                        help="run the executable directly instead of through Wine")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="time allowed for the first frame")
    parser.add_argument("--baseline", default=None,
                        help="previous bench_report.json for a p95 comparison")
    args = parser.parse_args()

    exe_abs = os.path.abspath(args.exe)
    use_wine = (not args.native) and os.name != "nt" and is_pe(exe_abs)
    run_dir = os.path.abspath(args.run)
    if os.path.isdir(run_dir):
        shutil.rmtree(run_dir)
    os.makedirs(run_dir, exist_ok=True)

    executable = os.path.join(run_dir, os.path.basename(exe_abs))
    shutil.copy2(exe_abs, executable)
    source_dir = os.path.dirname(exe_abs)
    for name in ("overlay.nw", "overlay.bin", "overlay.trailer"):
        source = os.path.join(source_dir, name)
        if os.path.isfile(source):
            shutil.copy2(source, os.path.join(run_dir, name))
    if args.audio and os.path.isfile(DEFAULT_BANK):
        shutil.copy2(DEFAULT_BANK, os.path.join(run_dir, "gm.sf2"))

    env = dict(os.environ)
    env["NW_DEBUG_PROFILE"] = "1"
    if args.audio:
        env.pop("NW_NO_AUDIO", None)
    else:
        env["NW_NO_AUDIO"] = "1"
    if args.scale >= 1:
        env["NW_RENDER_SCALE"] = str(args.scale)
    else:
        env.pop("NW_RENDER_SCALE", None)
    if args.dummy:
        env["SDL_VIDEODRIVER"] = "dummy"

    if use_wine:
        wine = shutil.which("wine64") or shutil.which("wine")
        if wine is None:
            log("error: neither wine64 nor wine is installed")
            return 2
        command = [wine, "./" + os.path.basename(executable)]
    elif os.name != "nt":
        command = ["./" + os.path.basename(executable)]
    else:
        command = [executable]

    # Keep DISPLAY/XAUTHORITY for a real window; dummy mode ignores X11.
    if os.name != "nt" and not args.dummy:
        env.setdefault("DISPLAY", ":0")
        if not env.get("XAUTHORITY"):
            auths = glob.glob("/run/user/%d/.mutter-Xwaylandauth.*" % os.getuid())
            if auths:
                env["XAUTHORITY"] = auths[0]

    stdout = open(os.path.join(run_dir, "stdout.txt"), "wb")
    stderr = open(os.path.join(run_dir, "stderr.txt"), "wb")
    log("run: %s" % run_dir)
    proc = subprocess.Popen(command, cwd=run_dir, env=env,
                            stdout=stdout, stderr=stderr,
                            start_new_session=True)
    profile = os.path.join(run_dir, "nwprof.csv")
    deadline = time.time() + args.timeout
    status = "timeout"
    while time.time() < deadline:
        if proc.poll() is not None:
            status = "exited"
            break
        ready = profile_ready(profile)
        if ready == "frames":
            status = "ready"
            break
        if ready == "compose":
            status = "compose"
            break
        time.sleep(0.2)
    log("profile: %s (%s)" % (profile, status))
    if status == "ready":
        time.sleep(max(0.0, args.warmup))
        time.sleep(max(0.0, args.seconds))
    elif status == "compose":
        # The startup animation is short; collect a little more of it without
        # waiting for a menu frame that the dummy/idle run will never present.
        time.sleep(min(0.5, max(0.0, args.seconds)))

    if proc.poll() is None:
        try:
            if os.name == "nt":
                proc.terminate()
            else:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except (OSError, ProcessLookupError):
            pass
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            try:
                if os.name == "nt":
                    proc.kill()
                else:
                    os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except (OSError, ProcessLookupError):
                pass
            proc.wait(timeout=5)
    stdout.close()
    stderr.close()

    frames, events = parse_profile(profile)
    report = analyse(frames, events)
    report["profile"] = profile
    report["exe"] = exe_abs
    if report.get("mode") == "startup-compose":
        report["hint"] = (
            "No presented frame rows were produced, so this run measures the "
            "startup composition path only. A menu click or gameplay input is "
            "needed for frame pacing rows."
        )
        log("hint: " + report["hint"])
    elif not frames:
        report["hint"] = (
            "No profile data was produced. NW_DEBUG_PROFILE is available only "
            "in a *-debug build; release builds ignore it."
        )
        log("hint: " + report["hint"])
    out = os.path.join(run_dir, "bench_report.json")
    with open(out, "w") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
    print(json.dumps(report, indent=2, sort_keys=True))
    log("report: %s" % out)

    if args.baseline:
        try:
            with open(args.baseline) as handle:
                baseline = json.load(handle)
        except (OSError, ValueError) as error:
            log("baseline: %s" % error)
        else:
            regressions = []
            for key in ("step_us", "present_us", "gap_us"):
                old = baseline.get(key, {}).get("p95")
                new = report.get(key, {}).get("p95")
                if old and new and new > old * 1.25:
                    regressions.append("%s p95 %.0f -> %.0f" % (key, old, new))
            if regressions:
                log("REGRESSION: " + "; ".join(regressions))
            else:
                log("baseline: within 25%")
    return 0 if (frames or report.get("mode") == "startup-compose") else 2


if __name__ == "__main__":
    sys.exit(main())
