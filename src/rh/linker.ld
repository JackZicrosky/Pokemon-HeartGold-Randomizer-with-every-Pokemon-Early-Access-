/* Overlay 152 */
OUTPUT_ARCH(arm)
MEMORY {

        rom     : ORIGIN = 0x01FF8700, LENGTH = 0x7900
        ewram   : ORIGIN = 0x01FF8700, LENGTH = 0x7900
}

/* romhack randomizer core: lives in the free part of ITCM (the SDK's own ITCM code ends at 0x01FF8620)
   and stays loaded for the whole game. */

SECTIONS {
        .text : {

                FILL (0xABCD)

                __text_start = . ;
                *(.init)
                *(.text)
                *(.ctors)
                *(.dtors)
                *(.rodata)
                *(.data)
                *(.fini)
                *(COMMON)
                __text_end  = . ;

                __bss_start__ = . ;
                *(.bss)
                __bss_end__ = . ;
        _end = __bss_end__ ;
        __end__ = __bss_end__ ;
        } >rom = 0xff
}
