/* Overlay 153 */
OUTPUT_ARCH(arm)
MEMORY {

        rom     : ORIGIN = 0x021E5900, LENGTH = 0x60000
        ewram   : ORIGIN = 0x021E5900, LENGTH = 0x60000
}

/* romhack randomizer options screen: loaded only while it is open (New Game, Randomizer Settings item)
   in the apps overlay region. Its app template must come first (RH_MENU_APP_TEMPLATE). */

SECTIONS {
        .text : {

                FILL (0xABCD)

                __text_start = . ;
                *(.rh_header)
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
