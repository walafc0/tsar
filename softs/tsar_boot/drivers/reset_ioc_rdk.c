/**
 * \file   reset_ioc_rdk.c
 * \date   December 14, 2014
 * \author Cesar Fuguet
 */

#include <reset_ioc_rdk.h>
#include <reset_utils.h>
#include <defs.h>

#ifndef SEG_RDK_BASE
#    error "SEG_RDK_BASE constant must be defined in the hard_config.h file"
#endif

static int* const rdk_address = (int* const)SEG_RDK_BASE;

int reset_rdk_init()
{
    return 0;
}

int reset_rdk_read( unsigned int lba, void* buffer, unsigned int count )
{
    char* const src = (char* const)rdk_address + (lba * BLOCK_SIZE);
    memcpy(buffer, (void*)src, count * BLOCK_SIZE);
    return 0;
}

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
