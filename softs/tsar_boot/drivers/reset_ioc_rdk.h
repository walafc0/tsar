/**
 * \file   reset_ioc_rdk.h
 * \date   December 14, 2014
 * \author Cesar Fuguet
 */

#ifndef RESET_IOC_RDK_H
#define RESET_IOC_RDK_H

int reset_rdk_init();

int reset_rdk_read( unsigned int lba, void* buffer, unsigned int count );

#endif

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
