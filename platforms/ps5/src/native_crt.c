/*
 * Native PS5 title startup for NetWalk.
 *
 * The payload SDK's crt1.o is payload-specific: it calls main directly and
 * supplies klog. A native title instead receives PS5 process parameters and a
 * loader teardown callback. This CRT initializes libc, runs the executable's
 * constructors, enters the shared game path, and lets the platform layer close
 * the title through the shell.
 *
 * SPDX-License-Identifier: MIT
 */
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "platform.h"

/* libSceLibcInternal exports write(2).  klog is the kernel log descriptor the
 * payload SDK's crt1.o uses; writing it here makes native-title output visible
 * through klogsrv as well as on stderr. */
extern long write(int descriptor, const void *buffer, size_t size);

typedef void (*NwInitFunction)(void);

extern void _init_env(void *process_parameters);
extern int atexit(NwInitFunction callback);
extern void exit(int status) __attribute__((noreturn));
extern void NwLinuxSetArgs(int argc, char **argv);
extern int InitializeAndRunWinMain(void);

extern NwInitFunction __preinit_array_start[] __attribute__((weak));
extern NwInitFunction __preinit_array_end[] __attribute__((weak));
extern NwInitFunction __init_array_start[] __attribute__((weak));
extern NwInitFunction __init_array_end[] __attribute__((weak));
extern NwInitFunction __fini_array_start[] __attribute__((weak));
extern NwInitFunction __fini_array_end[] __attribute__((weak));

/*
 * platform.c supplies the real handoff, which asks the shell to close the
 * title and never returns. The weak fallback keeps this object linkable in
 * isolation and lets a future target use ordinary process exit.
 */
__attribute__((weak)) void
catchReturnFromMain(int status)
{
    (void)status;
}

/*
 * The payload SDK's crt1.o is the only other definition of klog_printf. The
 * native title does not link crt1.o, so provide the platform layer's log sink
 * here. stderr is backed by libSceLibcInternal once _init_env has run; fd 7 is
 * the kernel log that klogsrv streams.  The payload klog syscall (0x259) is
 * illegal in a native title (SYSTEM_ILLEGAL_FUNCTION_CALL), so keep the write.
 */
int
klog_printf(const char *format, ...)
{
    static int nonblocking;
    char line[1024];
    va_list args;
    int result;
    size_t length;

    va_start(args, format);
    result = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (result <= 0) {
        return result;
    }

    if (!nonblocking) {
        /* A missing klogsrv reader must never stall the game on a full
         * kernel-log buffer.  Diagnostic lines are lossy by design. */
        int flags = fcntl(7, F_GETFL, 0);
        if (flags >= 0)
            (void)fcntl(7, F_SETFL, flags | O_NONBLOCK);
        nonblocking = 1;
    }

    length = (size_t)result;
    if (length >= sizeof(line)) {
        length = sizeof(line) - 1;
        line[length] = '\0';
    }
    (void)write(7, line, length);
    (void)fwrite(line, 1, length, stderr);
    fflush(stderr);
    return result;
}

static void
run_forward_marked(NwInitFunction *first, NwInitFunction *last,
                   const char *prefix)
{
    unsigned int index = 0;

    if (first == NULL || last == NULL)
        return;
    while (first != last) {
        if (*first != NULL) {
            char stage[32];
            snprintf(stage, sizeof(stage), "%s_%u", prefix, index);
            platform_boot_mark(stage);
            (*first)();
        }
        ++first;
        ++index;
    }
}

static void
run_reverse(NwInitFunction *first, NwInitFunction *last)
{
    if (first == NULL || last == NULL)
        return;
    while (last != first) {
        --last;
        if (*last != NULL)
            (*last)();
    }
}

void
_init(void)
{
    run_forward_marked(__preinit_array_start, __preinit_array_end, "native_preinit");
    run_forward_marked(__init_array_start, __init_array_end, "native_ctor");
}

void
_fini(void)
{
    run_reverse(__fini_array_start, __fini_array_end);
}

__attribute__((noreturn)) void
_start(void *process_parameters, NwInitFunction loader_teardown)
{
    const int argc = *(const int *)process_parameters;
    char **argv = (char **)((uint8_t *)process_parameters + sizeof(uint64_t));
    int status;

    _init_env(process_parameters);
    platform_boot_mark("native_start");
    if (loader_teardown != NULL)
        (void)atexit(loader_teardown);
    (void)atexit(_fini);
    _init();
    platform_boot_mark("native_init_done");
    NwLinuxSetArgs(argc, argv);
    platform_boot_mark("native_args_done");
    platform_boot_mark("native_main_enter");
    status = InitializeAndRunWinMain();
    platform_boot_mark("native_main_returned");
    catchReturnFromMain(status);
    exit(status);
}
