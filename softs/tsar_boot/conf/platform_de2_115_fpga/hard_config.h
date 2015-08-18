#ifndef HARD_CONFIG_H
#define HARD_CONFIG_H

/* General platform parameters */

#define X_SIZE                 1
#define Y_SIZE                 1
#define X_WIDTH                4
#define Y_WIDTH                4
#define P_WIDTH                1
#define X_IO                   0
#define Y_IO                   0
#define NB_PROCS_MAX           2
#define IRQ_PER_PROCESSOR      4
#define RESET_ADDRESS          0xff000000
#define NB_TOTAL_PROCS         16

/* Peripherals */

#define NB_TTY_CHANNELS        1
#define NB_IOC_CHANNELS        1
#define NB_NIC_CHANNELS        0
#define NB_CMA_CHANNELS        0
#define NB_TIM_CHANNELS        0
#define NB_DMA_CHANNELS        0

#define USE_XCU                1
#define USE_IOB                0
#define USE_PIC                0
#define USE_FBF                1

#define USE_IOC_BDV            0
#define USE_IOC_SDC            1
#define USE_IOC_HBA            0
#define USE_IOC_RDK            0

#define FBUF_X_SIZE            640
#define FBUF_Y_SIZE            480

#define XCU_NB_INPUTS          16

/* base addresses and sizes for physical segments */

#define SEG_RAM_BASE           0x0
#define SEG_RAM_SIZE           0x08000000 /* 128 MB */

#define SEG_CMA_BASE           0xffffffff
#define SEG_CMA_SIZE           0x0

#define SEG_DMA_BASE           0xffffffff
#define SEG_DMA_SIZE           0x0

#define SEG_FBF_BASE           0xf3000000
#define SEG_FBF_SIZE           0x100000

#define SEG_ICU_BASE           0xffffffff
#define SEG_ICU_SIZE           0x0

#define SEG_IOB_BASE           0xffffffff
#define SEG_IOB_SIZE           0x0

#define SEG_IOC_BASE           0xf2000000
#define SEG_IOC_SIZE           0x1000

#define SEG_MMC_BASE           0xe0000000
#define SEG_MMC_SIZE           0x1000

#define SEG_MWR_BASE           0xffffffff
#define SEG_MWR_SIZE           0x0

#define SEG_ROM_BASE           0xff000000
#define SEG_ROM_SIZE           0x10000

#define SEG_SIM_BASE           0xffffffff
#define SEG_SIM_SIZE           0x0

#define SEG_NIC_BASE           0xffffffff
#define SEG_NIC_SIZE           0x0

#define SEG_PIC_BASE           0xffffffff
#define SEG_PIC_SIZE           0x0

#define SEG_TIM_BASE           0xffffffff
#define SEG_TIM_SIZE           0x0

#define SEG_TTY_BASE           0xf4000000
#define SEG_TTY_SIZE           0x4000

#define SEG_XCU_BASE           0xf0000000
#define SEG_XCU_SIZE           0x1000

#define SEG_RDK_BASE           0xffffffff
#define SEG_RDK_SIZE           0x0

#endif
