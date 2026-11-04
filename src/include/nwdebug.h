/*
 * Optional developer tracing.
 *
 * Debug-only formatting and environment probes are compiled out unless the
 * build enables NW_DEBUG_TOOLS.  The debug presets enable it; release builds do
 * not.  Routine NWDEBUG output goes to stderr.  The low-level trace symbols are
 * retained for compatibility, but their environment gate is a null lookup in
 * release.  Explicit capture variables (for example NW_DEBUG_TRACE) may create
 * a named file only when they are set.
 */

#ifndef NWDEBUG_H
#define NWDEBUG_H

#if defined(NW_DEBUG_TOOLS)

#include <stdio.h>

#define NWDEBUG(...)                                                        \
    do {                                                                    \
        fputs("nwdebug: ", stderr);                                         \
        fprintf(stderr, __VA_ARGS__);                                       \
        fputc('\n', stderr);                                                \
    } while (0)
#define NWDEBUG_CLOSE() do { } while (0)

#include <stdlib.h>
#define NW_DEBUG_GETENV(name) getenv(name)
#define NW_DEBUG_GETENV_W(name, buf, size) GetEnvironmentVariableA(name, buf, size)

#else   /* !NW_DEBUG_TOOLS -- the default: expands to nothing */

#define NWDEBUG(...)          do { } while (0)
#define NWDEBUG_CLOSE()       do { } while (0)
#define NW_DEBUG_GETENV(name) ((const char *)0)
#define NW_DEBUG_GETENV_W(name, buf, size) 0

#endif

#endif /* NWDEBUG_H */

/* nwtrace_hex -- log a 32-bit value as a tag line. Used when a branch decision
 * depends on a return value and "did the trace fire" is not enough to explain
 * which way it went. */
void nwtrace_hex(const char *tag, unsigned long v);
