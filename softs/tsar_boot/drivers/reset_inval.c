/**
 * \file   reset_inval.c
 * \date   December 14, 2014
 * \author Cesar Fuguet / Alain Greiner
 */

#include <reset_inval.h>
#include <io.h>
#include <defs.h>

#ifndef SEG_MMC_BASE
#   error "SEG_MMC_BASE constant must be defined in the hard_config.h file"
#endif

static int* const mmc_address = (int* const)SEG_MMC_BASE;

enum memc_registers
{
    MCC_ADDR_LO   = 0,
    MCC_ADDR_HI   = 1,
    MCC_LENGTH    = 2,
    MCC_CMD       = 3
};

enum memc_operations
{
    MCC_CMD_NOP   = 0,
    MCC_CMD_INVAL = 1,
    MCC_CMD_SYNC  = 2
};

/**
 * \brief Invalidate all L1 cache lines corresponding to a memory buffer
 *        (identified by an address and a size).
 */
void reset_L1_inval( void* const buffer, size_t size )
{
    unsigned int i;

    // iterate on L1 cache lines containing target buffer
    for (i = 0; i <= size; i += CACHE_LINE_SIZE)
    {
        asm volatile(
            " cache %0, %1"
            : /* no outputs */
            : "i" (0x11), "R" (*((char*)buffer + i))
            : "memory" );
    }
}

/**
 * \brief Invalidate all L2 cache lines corresponding to a memory buffer
 *        (identified by an address and a size).
 */
void reset_L2_inval( void* const buffer, size_t size )
{
    iowrite32( &mmc_address[MCC_ADDR_LO], (unsigned int)buffer );
    iowrite32( &mmc_address[MCC_ADDR_HI], 0 );
    iowrite32( &mmc_address[MCC_LENGTH] , size );
    iowrite32( &mmc_address[MCC_CMD]    , MCC_CMD_INVAL);
}

/**
 * \brief Update external RAM for all L2 cache lines corresponding to 
 *        a memory buffer (identified by an address and a size).
 */
void reset_L2_sync ( void* const buffer, size_t size )
{
    iowrite32( &mmc_address[MCC_ADDR_LO], (unsigned int)buffer );
    iowrite32( &mmc_address[MCC_ADDR_HI], 0 );
    iowrite32( &mmc_address[MCC_LENGTH] , size );
    iowrite32( &mmc_address[MCC_CMD]    , MCC_CMD_SYNC );
}

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
