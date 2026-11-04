/*
 * Intermediate PIE layout for the native PS5 title converter.
 *
 * The converter needs code, immutable data, RELRO, and writable data in
 * separate page-aligned ranges so it can insert the PS5 process parameters and
 * rebuild the loader-visible program headers.  Keep this file in sync with
 * platforms/ps5/tools/validate-native.py.
 *
 * SPDX-License-Identifier: MIT
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

PHDRS {
    text    PT_LOAD FLAGS (0x5);
    ro      PT_LOAD FLAGS (0x4);
    relro   PT_LOAD FLAGS (0x6);
    data    PT_LOAD FLAGS (0x6);
    dynamic PT_DYNAMIC FLAGS (0x6);
}

SECTIONS {
    /* The converter requires .text at address zero and outside the ELF
     * headers.  The two reserved words match the validated native layout. */
    .text 0 : ALIGN(0x4000) {
        QUAD(0xcccccccccccccccc)
        QUAD(0xcccccccccccccccc)
        *(.text .text.*)
        *(.plt .plt.*)
    } : text

    .eh_frame_hdr : ALIGN(0x4000) {
        KEEP(*(.eh_frame_hdr))
    } : ro
    .eh_frame : {
        KEEP(*(.eh_frame))
    } : ro
    .gcc_except_table : {
        *(.gcc_except_table .gcc_except_table.*)
    } : ro
    .rodata : {
        *(.rodata .rodata.*)
    } : ro

    . = ALIGN(0x4000);
    /* The native converter uses .got's file offset as the base of the RELRO
     * load segment.  .got must therefore be the first section in that
     * segment; otherwise every RELRO virtual address is shifted by the size
     * of the sections before it, and DT_INIT_ARRAY points into relocation
     * data.  Keep this order in sync with sce_module_writer.cpp. */
    .got : {
        *(.got)
    } : relro
    .got.plt : {
        *(.got.plt)
    } : relro
    .data.rel.ro : {
        *(.data.rel.ro .data.rel.ro.*)
    } : relro
    .preinit_array : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } : relro
    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)
               SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE_HIDDEN(__init_array_end = .);
    } : relro
    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)
               SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } : relro

    /* LLVM emits these; the converter reads them and writes its own dynamic
     * table and symbol hashes into the final executable. */
    .dynsym : {
        *(.dynsym)
    } : relro
    .gnu.hash : {
        *(.gnu.hash)
    } : relro
    .dynstr : {
        *(.dynstr)
    } : relro
    .rela.dyn : {
        *(.rela.dyn)
    } : relro
    .rela.plt : {
        *(.rela.plt)
    } : relro

    . = ALIGN(0x4000);
    .dynamic : {
        *(.dynamic)
    } : data : dynamic
    .data : {
        *(.data .data.*)
    } : data
    .tdata : {
        *(.tdata .tdata.*)
    } : data
    .tbss : {
        *(.tbss .tbss.*)
    } : data
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } : data
}
