#include <hard_config.h>

#define RESET_VERSION         0x00010003
#define RESET_STACK_SIZE      0x2000
#define RESET_LOADER_LBA      2
#define RESET_PHDR_ARRAY_SIZE 16

#ifndef RESET_DEBUG
#   define RESET_DEBUG        0
#endif

#ifndef RESET_HARD_CC
#   define RESET_HARD_CC      0
#endif

/**
 * Supported OS
 */
#define OS_LINUX              0

/**
 * Default clock frequency (by default is 25 MHz for FPGA dev card DE2-115)
 */
#ifndef RESET_SYSTEM_CLK
#   define RESET_SYSTEM_CLK   25000 /* KHz   */
#endif

/*
 * TSAR platform independent hardware parameters
 */

#define BLOCK_SIZE            512   /* bytes */
#define CACHE_LINE_SIZE       64    /* bytes */

/*
 * Verify that all used constants have been defined in the hard_config.h file
 */

#ifndef P_WIDTH
#   error "P_WIDTH constant must be defined in the hard_config.h file"
#endif

#ifndef IRQ_PER_PROCESSOR
#   error "IRQ_PER_PROCESSOR constant must be defined in hard_config.h file"
#endif

#ifndef SEG_XCU_BASE
#   error "SEG_XCU_BASE constant must be defined in the hard_config.h file"
#endif

#ifndef RESET_ADDRESS
#   error "RESET_ADDRESS constant must be defined in the hard_config.h file"
#endif

/*
 * IO cluster constants
 */

#ifndef USE_IOB
#   error "USE_IOB constant must be defined in the hard_config.h file"
#endif

#if !defined(X_IO) || !defined(Y_IO)
#   error "X_IO and Y_IO constants must be defined in the hard_config.h file"
#endif

#if !defined(X_WIDTH) || !defined(Y_WIDTH)
#   error "X_WIDTH and Y_WIDTH constants must be defined in the hard_config.h "
          "file"
#endif

#define CLUSTER_IO  ((X_IO << Y_WIDTH) | Y_IO)

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
