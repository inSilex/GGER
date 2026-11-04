#!/usr/bin/env python3
"""Windows-loader section-VA layout validator + gap-filler planner.

WHY THIS EXISTS  (HANDOFF.md "0h")
----------------------------------
The real Windows memory manager builds the SEC_IMAGE prototype PTEs from the
PE section table and requires the sections to be **contiguous at page
granularity** in the virtual address space:

    VirtualAddress[i] == ALIGN_UP_PAGE(VirtualAddress[i-1] + extent[i-1])

extent = max(VirtualSize, SizeOfRawData), i.e. how many bytes of the image the
section spans.  If a *whole page* between two consecutive sections belongs to
no section, the loader fails the image with STATUS_INVALID_IMAGE_FORMAT and the
shell reports

    "This app can't run on your PC"

Wine's loader does not implement that check, so a file with such a hole runs
under Wine and is refused on Windows -- which is exactly the split we kept
seeing.  Evidence for the rule (all measured, not guessed):

    original NETWALK.EXE   gaps: 0x200/0xA00/0xC00  (all < 1 page)   RUNS
    stock mingw GUI exe    all gaps < 1 page                          RUNS
    our link flags         all gaps < 1 page                          RUNS
    our flags + big overlay all gaps < 1 page                         RUNS
    our flags + our layout big hole (0x15600)                         REFUSED
    our build/netwalk.exe  two 1-page holes (0x17000, 0x3B000)        REFUSED

Sub-page holes are fine (e.g. a section ending at 0x60B0 with the next one at
0x7000): the page is still covered, so the loader sees one segment.

Usage
-----
    pe_layout_check.py check <exe>
        Report the section table with every hole; exit 1 if any hole exists
        or a structural invariant (ordering/overlap/SizeOfImage) is broken.

    pe_layout_check.py plan <pass1.exe> <outdir>
        Emit <outdir>/nwfill.s and <outdir>/nwfill.args: one zero-filled
        section per hole, pinned so the hole is covered.  Linking those in
        makes the image comply; the VAs of the real sections do not move.
"""
import struct
import sys
import os

PAGE = 0x1000


def align_up(v, a=PAGE):
    return (v + a - 1) & ~(a - 1)


def parse(path):
    d = open(path, "rb").read()
    if d[:2] != b"MZ":
        raise SystemExit("%s: not an MZ image" % path)
    pe = struct.unpack_from("<I", d, 0x3c)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("%s: no PE signature" % path)
    nsec, soh = struct.unpack_from("<HH", d, pe + 6)[0], struct.unpack_from("<H", d, pe + 20)[0]
    opt = pe + 24
    sohdrs, size_of_image = struct.unpack_from("<II", d, opt + 60)[0], struct.unpack_from("<I", d, opt + 56)[0]
    image_base = struct.unpack_from("<I", d, opt + 28)[0]
    st = opt + soh
    secs = []
    for i in range(nsec):
        nm, vs, va, rs, rp = struct.unpack_from("<8sIIII", d, st + i * 40)
        secs.append({
            "name": nm.rstrip(b"\0").decode(errors="replace"),
            "va": va, "vs": vs, "rs": rs, "raw": rp,
            "extent": max(vs, rs),
        })
    secs.sort(key=lambda s: s["va"])
    return {"size_of_image": size_of_image, "size_of_headers": sohdrs,
            "image_base": image_base, "file": len(d), "sections": secs}


def holes(img):
    """Return [(start_page, end_page)] page ranges covered by no section."""
    out = []
    boundary = align_up(img["size_of_headers"])
    prev = None
    for s in img["sections"]:
        if prev is not None and s["va"] < prev["va"] + prev["extent"]:
            out.append(("overlap", s, prev))
        if s["va"] > boundary:
            out.append(("hole", boundary, s["va"]))
        boundary = max(boundary, align_up(s["va"] + s["extent"]))
        prev = s
    return out, boundary


def check(path, verbose=True):
    img = parse(path)
    problems, boundary = holes(img)
    hard = [p for p in problems if p[0] == "overlap"]
    if verbose:
        print("== %s ==" % path)
        print("   SizeOfHeaders=%#x SizeOfImage=%#x file=%#x" %
              (img["size_of_headers"], img["size_of_image"], img["file"]))
        prev = None
        for s in img["sections"]:
            note = ""
            if prev is not None:
                gap = s["va"] - align_up(prev["va"] + prev["extent"])
                if gap > 0:
                    note = "   <-- HOLE %#x (page %#x)" % (gap, align_up(prev["va"] + prev["extent"]))
            print("   %-10s VA=%#08x VS=%#08x RS=%#08x extent=%#08x%s"
                  % (s["name"], s["va"], s["vs"], s["rs"], s["extent"], note))
            prev = s
    if boundary != img["size_of_image"]:
        hard.append(("sizeofimage", boundary, img["size_of_image"]))
        if verbose:
            print("   ** SizeOfImage=%#x does not cover the sections (needs %#x)"
                  % (img["size_of_image"], boundary))
    if problems or hard:
        if not verbose:
            for p in problems:
                if p[0] == "hole":
                    print("   HOLE %#x bytes at page %#x (covered by no section)"
                          % (p[2] - p[1], p[1]))
                else:
                    print("   OVERLAP: %s starts inside %s" % (p[1]["name"], p[2]["name"]))
            if boundary != img["size_of_image"]:
                print("   SizeOfImage=%#x, sections need %#x"
                      % (img["size_of_image"], boundary))
        print("   FAIL: %d hole(s), %d structural problem(s)" % (len(problems), len(hard)))
        return 1
    if verbose:
        print("   OK: sections contiguous at page granularity; "
              "SizeOfImage covers the image")
    return 0


def plan(path, outdir):
    img = parse(path)
    problems, boundary = holes(img)
    if any(p[0] == "overlap" for p in problems):
        raise SystemExit("plan: sections overlap -- fix the layout first")
    lines = ["/* GENERATED by tools/pe_layout_check.py -- one zero-filled",
             " * section per empty page between the real sections.  Pinned with",
             " * -Wl,--section-start so the real sections do not move. */"]
    args = []
    n = 0
    base = img["image_base"]
    for p in problems:
        if p[0] != "hole":
            continue
        start, end = p[1], p[2]
        size = end - start
        name = ".nwfill%d" % n
        lines.append('.section %s,"x"\n  .space %d, 0' % (name, size))
        # --section-start takes an absolute VA, not an RVA.
        args.append("-Wl,--section-start=%s=0x%x" % (name, base + start))
        print("   filler %-9s RVA %#08x..%#08x (VA %#x, %#x bytes)"
              % (name, start, end, base + start, size))
        n += 1
    os.makedirs(outdir, exist_ok=True)
    open(os.path.join(outdir, "nwfill.s"), "w").write("\n".join(lines) + "\n")
    open(os.path.join(outdir, "nwfill.args"), "w").write(" ".join(args) + "\n")
    if boundary != img["size_of_image"]:
        print("   WARNING: SizeOfImage=%#x, sections need %#x"
              % (img["size_of_image"], boundary))
    print("   %d filler section(s) written to %s/nwfill.s" % (n, outdir))
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    cmd = sys.argv[1]
    if cmd == "check":
        sys.exit(check(sys.argv[2], verbose="quiet" not in sys.argv[3:]))
    if cmd == "plan":
        sys.exit(plan(sys.argv[2], sys.argv[3]))
    raise SystemExit("unknown command %r" % cmd)
