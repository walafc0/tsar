/* Generated by genmap for tsar_iob_2_2_4 */

#ifndef HARD_CONFIG_H
#define HARD_CONFIG_H

/* General platform parameters */

#define X_SIZE                 2
#define Y_SIZE                 2
#define X_WIDTH                4
#define Y_WIDTH                4
#define P_WIDTH                2
#define X_IO                   0
#define Y_IO                   0
#define NB_PROCS_MAX           4
#define IRQ_PER_PROCESSOR      4
#define RESET_ADDRESS          0xbfc00000
#define NB_TOTAL_PROCS         16

/* Peripherals */

#define NB_TTY_CHANNELS        1
#define NB_IOC_CHANNELS        1
#define NB_NIC_CHANNELS        2
#define NB_CMA_CHANNELS        4
#define NB_TIM_CHANNELS        0
#define NB_DMA_CHANNELS        4

#define USE_XCU                1
#define USE_IOB                1
#define USE_PIC                1
#define USE_FBF                1

#define USE_IOC_BDV            1
#define USE_IOC_SPI            0
#define USE_IOC_HBA            0
#define USE_IOC_RDK            0

#define FBUF_X_SIZE            128
#define FBUF_Y_SIZE            128

#define XCU_NB_INPUTS          16

/* base addresses and sizes for physical segments */

#define SEG_RAM_BASE           0x0
#define SEG_RAM_SIZE           0x40000

#define SEG_CMA_BASE           0xb6000000
#define SEG_CMA_SIZE           0x4000

#define SEG_DMA_BASE           0xb1000000
#define SEG_DMA_SIZE           0x4000

#define SEG_FBF_BASE           0xb7000000
#define SEG_FBF_SIZE           0x4000

#define SEG_ICU_BASE           0xffffffff
#define SEG_ICU_SIZE           0x0

#define SEG_IOB_BASE           0xbe000000
#define SEG_IOB_SIZE           0x1000

#define SEG_IOC_BASE           0xb3000000
#define SEG_IOC_SIZE           0x1000

#define SEG_MMC_BASE           0xb2000000
#define SEG_MMC_SIZE           0x1000

#define SEG_MWR_BASE           0xffffffff
#define SEG_MWR_SIZE           0x0

#define SEG_ROM_BASE           0xbfc00000
#define SEG_ROM_SIZE           0x8000

#define SEG_SIM_BASE           0xb9000000
#define SEG_SIM_SIZE           0x1000

#define SEG_NIC_BASE           0xb5000000
#define SEG_NIC_SIZE           0x80000

#define SEG_PIC_BASE           0xb8000000
#define SEG_PIC_SIZE           0x1000

#define SEG_TIM_BASE           0xffffffff
#define SEG_TIM_SIZE           0x0

#define SEG_TTY_BASE           0xb4000000
#define SEG_TTY_SIZE           0x4000

#define SEG_XCU_BASE           0xb0000000
#define SEG_XCU_SIZE           0x1000

#define SEG_RDK_BASE           0xffffffff
#define SEG_RDK_SIZE           0x0

#define PERI_CLUSTER_INCREMENT 0x10000

/* physical base addresses for identity mapped vsegs */
/* used by the GietVM OS                             */

#define SEG_BOOT_MAPPING_BASE  0x0
#define SEG_BOOT_MAPPING_SIZE  0x80000

#define SEG_BOOT_CODE_BASE     0x80000
#define SEG_BOOT_CODE_SIZE     0x40000

#define SEG_BOOT_DATA_BASE     0xc0000
#define SEG_BOOT_DATA_SIZE     0x80000

#define SEG_BOOT_STACK_BASE    0x140000
#define SEG_BOOT_STACK_SIZE    0x50000
#endif
