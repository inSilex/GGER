/*
 * Diagnostics -- opt-in VM and low-level tag tracing.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_DIAG_WIN32_DIAGNOSTICS_C_H
#define NW_GROUP_DIAG_WIN32_DIAGNOSTICS_C_H

#include <stdint.h>
#include <stddef.h>
#include <windows.h>

#ifndef NW_GROUP_COMPAT_TYPES
#define NW_GROUP_COMPAT_TYPES
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned int uint;
typedef unsigned char byte;
typedef uint32_t dword;
typedef void *HANDLE_T;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Units in this group: */
/*   NwVmTrace                                                  diag/win32/NwVmTrace.c */
/*   nwtrace                                                    diag/win32/nwtrace.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */

extern void NwVmTrace (uint32_t opcode, uint32_t slot, uint32_t value);
extern void nwtrace_c (const char *tag);
extern void nwsafe_trace (const char *tag);
extern void nwtrace_asm (const char *tag);
extern void nwtrace_hex (const char *tag, long unsigned int v);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_DIAG_WIN32_DIAGNOSTICS_C_H */
