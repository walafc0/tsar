/**
 * \file   reset_inval.h
 * \date   December 14, 2014
 * \author Cesar Fuguet
 */

#ifndef RESET_INVAL_H
#define RESET_INVAL_H

#include <inttypes.h>

void reset_L1_inval (void* const buffer, size_t size);

void reset_L2_inval (void* const buffer, size_t size);

void reset_L2_sync (void* const buffer, size_t size);

#endif

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
