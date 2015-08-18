/**
 * \file   reset_ioc.h
 * \date   December 14, 2013
 * \author Cesar Fuguet
 *
 * \brief  API for accessing the disk controller
 *
 * \note   These functions call the specific disk controller driver depending
 *         on the USE_IOC_BDV, USE_IOC_SDC or USE_IOC_HBA USE_IOC_RDK flags
 */

#ifndef RESET_IOC_H
#define RESET_IOC_H

int reset_ioc_init();

int reset_ioc_read( unsigned int lba, void* buffer, unsigned int count );

#endif /* RESET_IOC_H */

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
