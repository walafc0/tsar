/**
 * \file   reset_ioc_bdv.c
 * \date   December 14, 2014
 * \author Cesar Fuguet
 */

#include <reset_ioc_bdv.h>
#include <reset_tty.h>
#include <reset_inval.h>
#include <io.h>
#include <defs.h>

#ifndef SEG_IOC_BASE
#    error "SEG_IOC_BASE constant must be defined in the hard_config.h file"
#endif

static int* const ioc_address = (int* const)SEG_IOC_BASE;

enum block_device_registers {
    BLOCK_DEVICE_BUFFER,
    BLOCK_DEVICE_LBA,
    BLOCK_DEVICE_COUNT,
    BLOCK_DEVICE_OP,
    BLOCK_DEVICE_STATUS,
    BLOCK_DEVICE_IRQ_ENABLE,
    BLOCK_DEVICE_SIZE,
    BLOCK_DEVICE_BLOCK_SIZE,
};

enum block_device_operations {
    BLOCK_DEVICE_NOOP,
    BLOCK_DEVICE_READ,
    BLOCK_DEVICE_WRITE,
};

enum block_device_status {
    BLOCK_DEVICE_IDLE,
    BLOCK_DEVICE_BUSY,
    BLOCK_DEVICE_READ_SUCCESS,
    BLOCK_DEVICE_WRITE_SUCCESS,
    BLOCK_DEVICE_READ_ERROR,
    BLOCK_DEVICE_WRITE_ERROR,
    BLOCK_DEVICE_ERROR,
};

////////////////////
int reset_bdv_init()
{
    return 0;
}

////////////////////////////////////
int reset_bdv_read( unsigned int lba, 
                    void* buffer, 
                    unsigned int count )
{
    // block_device configuration
    iowrite32( &ioc_address[BLOCK_DEVICE_BUFFER], (unsigned int) buffer );
    iowrite32( &ioc_address[BLOCK_DEVICE_COUNT], count );
    iowrite32( &ioc_address[BLOCK_DEVICE_LBA], lba );
    iowrite32( &ioc_address[BLOCK_DEVICE_IRQ_ENABLE], 0 );

    //  trigger transfer
    iowrite32( &ioc_address[BLOCK_DEVICE_OP], ( unsigned int )
               BLOCK_DEVICE_READ );

#if (RESET_HARD_CC == 0) || USE_IOB
    // inval buffer in L1 cache
    reset_L1_inval( buffer , count * 512 );
#endif

#if USE_IOB
    // inval buffer in L2 cache
    reset_L2_inval( buffer , count * 512 );
#endif

    unsigned int status = 0;
    while ( 1 )
    {
        status = ioread32(&ioc_address[BLOCK_DEVICE_STATUS]);
        if ( status == BLOCK_DEVICE_READ_SUCCESS )
        {
            break;
        }
        if ( status == BLOCK_DEVICE_READ_ERROR   ) 
        {
            reset_puts("ERROR during read on the BLK device\n");
            return 1;
        }
    }

    return 0;
}

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
