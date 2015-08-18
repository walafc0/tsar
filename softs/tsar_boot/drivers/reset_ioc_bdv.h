/**
 * \file   reset_ioc_bdv.h
 * \date   December 14, 2014
 * \author Cesar Fuguet
 */

#ifndef RESET_IOC_BDV_H
#define RESET_IOC_BDV_H

int reset_bdv_init();

int reset_bdv_read( unsigned int lba, void* buffer, unsigned int count );

#endif

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
