/* romhack: the AMOLED theme's loadable part (data/rh_theme.bin, see include/rh/rh_theme_blob.h). Linked at
   RHTB_BASE; the loader relocates it to where the system heap gave it room (tools/rh/mk_theme_blob.py lists the
   words to fix). */
OUTPUT_ARCH(arm)
MEMORY {
        blob    : ORIGIN = 0x0A000000, LENGTH = 0x10000
}

SECTIONS {
        .text : {
                __text_start = . ;
                *(.rhtb_header)
                *(.text*)
                *(.rodata*)
                *(.data*)
                . = ALIGN(4);
                __text_end  = . ;
        } >blob
        .bss (NOLOAD) : {
                __bss_start__ = . ;
                *(.bss*)
                *(COMMON)
                . = ALIGN(4);
                __bss_end__ = . ;
        } >blob
        /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) }
}
