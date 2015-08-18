#ifndef _SEGMENTATION_H
#define _SEGMENTATION_H

#define MEMC_BASE   0x00000000
#define MEMC_SIZE   0x10000000 // 256Mb

#define BOOT_BASE   0xbfc00000
#define BOOT_SIZE   0x00040000

#define XICU_BASE   0x10000000
#define XICU_SIZE   0x00001000 // 1 page

#define MTTY_BASE   0x14000000
#define MTTY_SIZE   0x0000000c // 3 mapped-registers

#define BD_BASE     0x1c000000
#define BD_SIZE     0x00000040 // 9 mapped-registers

#define FB_XSIZE    640
#define FB_YSIZE    480
#define FB_BASE     0x20000000
#define FB_SIZE     (FB_XSIZE * FB_YSIZE * 2)

#endif
