/*
 * OpcodeExecution -- VM opcode fetch, decode, and execution.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_VM_OPCODEEXECUTION_C_H
#define NW_GROUP_VM_OPCODEEXECUTION_C_H

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
/*   DispatchScriptOpcode                                       vm/DispatchScriptOpcode.c */
/*   ExecuteOpcodeAndStoreOperand                               vm/ExecuteOpcodeAndStoreOperand.c */
/*   ExecuteOpcodeStream                                        vm/ExecuteOpcodeStream.c */
/*   ExecuteOpcodeStreamPreservingGlobals                       vm/ExecuteOpcodeStreamPreservingGlobals.c */
/*   ExecuteScriptInstruction                                   vm/ExecuteScriptInstruction.c */
/*   FetchNextStreamByteIntoSlotDescriptor                      vm/FetchNextStreamByteIntoSlotDescriptor.c */
/*   InterpretOpcodeStreamUntilEnd                              vm/InterpretOpcodeStreamUntilEnd.c */
/*   ResetStreamCursorState                                     vm/ResetStreamCursorState.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint32_t __stdcall DispatchScriptOpcode(uint8_t *pCursor);
extern uint8_t ExecuteOpcodeAndStoreOperand (uint32_t arg1_operand, uint32_t arg2_addend, uint32_t arg3_index, uint32_t arg4_index, uint32_t arg5_slot, uint32_t arg6_opcode);
extern int ExecuteOpcodeStream (uint32_t dwMode, uint32_t dwStream);
extern uint32_t __stdcall ExecuteOpcodeStreamPreservingGlobals(const uint32_t *pOpcodeStream);
extern void ExecuteScriptInstruction (byte bFlag, byte bOperandA, byte bOperandB, uint32_t dwCtxA, uint32_t dwCtxB, uint32_t dwCtxC, uint32_t dwCtxD, byte *pCursor);
extern int FetchNextStreamByteIntoSlotDescriptor (void);
extern uint8_t InterpretOpcodeStreamUntilEnd (void);
extern void __stdcall ResetStreamCursorState(unsigned char *pStreamSource);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_VM_OPCODEEXECUTION_C_H */
