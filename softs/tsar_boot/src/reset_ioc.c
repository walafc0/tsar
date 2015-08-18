/**
 * \file   reset_ioc.c
 * \date   December 2013
 * \author Cesar Fuguet
 *
 * \brief  API for accessing the disk controller
 *
 * \note   These functions call the specific disk controller driver depending on
 *          USE_IOC_BDV / USE_IOC_SDC / USE_IOC_RDK / USE_IOC_SDC / USE_IOC_SPI
 */

#include <reset_ioc.h>
#include <defs.h>

#if (USE_IOC_BDV + USE_IOC_SDC + USE_IOC_RDK + USE_IOC_HBA + USE_IOC_SPI) != 1
#   error "in reset_ioc.c : undefined disk controller in hard_config.h"
#endif

#if USE_IOC_SDC
#include <reset_ioc_sdc.h> 
#endif

#if USE_IOC_BDV
#include <reset_ioc_bdv.h>
#endif

#if USE_IOC_RDK
#include <reset_ioc_rdk.h>
#endif

#if USE_IOC_HBA
#include <reset_ioc_hba.h>
#endif

#if USE_IOC_SPI
#include <reset_ioc_spi.h>
#endif


/**
 * \brief Initialize the disk controller
 */
int reset_ioc_init()
{
#if USE_IOC_BDV
    return reset_bdv_init();
#elif USE_IOC_SDC
    return reset_sdc_init();
#elif USE_IOC_RDK
    return reset_rdk_init();
#elif USE_IOC_HBA
    return reset_hba_init();
#elif USE_IOC_SPI
    return reset_spi_init();
#else
#   error "in reset_ioc_init.c : undefined disk controller in hard_config.h"
#endif
}

/**
 * \param lba   : first block index on the disk
 * \param buffer: base address of the memory buffer
 * \param count : number of blocks to be transfered
 *
 * \brief Transfer data from disk to a memory buffer
 *
 * \note  This is a blocking function. The function returns once the transfer
 *        is completed.
 */
int reset_ioc_read( unsigned int lba, void* buffer, unsigned int count )
{
#if USE_IOC_BDV
    return reset_bdv_read(lba, buffer, count);
#elif USE_IOC_SDC
    return reset_sdc_read(lba, buffer, count);
#elif USE_IOC_RDK
    return reset_rdk_read(lba, buffer, count);
#elif USE_IOC_HBA
    return reset_hba_read(lba, buffer, count);
#elif USE_IOC_SPI
    return reset_spi_read(lba, buffer, count);
#else
#   error "in reset_ioc_read.c : undefined disk controller in hard_config.h"
#endif
}

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
