/*
 * Nw datablock -- fixed data-window image and fallback definition.
 *
 * Module: data
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#data-nw-datablock-c
 */

#include <stdint.h>

/* The explicit asm name keeps the C spelling and the assembler spelling in
 * sync: GCC prefixes C symbols with an underscore on this target. */
extern unsigned char nw_data_window[] __asm__("nw_data_window");

#ifndef NW_RELOC_WINDOW
__asm__(".section .data\n"
        "  .globl nw_data_window\n"
        "  .align 16\n"
        "nw_data_window:\n"
        "  .incbin \"src/data/nw_datablock.bin\"\n");
#else
/* NW_RELOC_WINDOW is the normal build. The portableizer generates
 * build/nw_datablock_reloc.s so the window's absolute pointers become linker
 * relocations, and the window remains pinned at 0x418030. This branch only
 * keeps the declarations available for type checking. */
#endif

/* COFF namespacing, and the reason both spellings exist here.
 *
 * A C reference to `nw_data_window` is emitted by GCC as `_nw_data_window`
 * (one leading underscore). A GAS operand naming `nw_data_window` is emitted
 * directly. If only the C spelling is defined, every C caller links and every
 * GAS caller does not; if only the GAS spelling is defined, it is the reverse.
 * Defining both, with one an alias of the other, keeps zero bytes duplicated:
 * a `.set` adds a symbol-table entry, not code or data.
 */
