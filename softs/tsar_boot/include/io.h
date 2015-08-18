/**********************************************************************
 * \file    io.h
 * \date    5 September 2012
 * \author  Cesar Fuguet / Alain Greiner
 *
 * Utility functions to write or read memory mapped hardware registers
 *********************************************************************/

#ifndef IO_H
#define IO_H

#include <defs.h>

/**********************************************************************
 * Read an 32 bits memory mapped hardware register
 * with physical address extension to access cluster_io
 *********************************************************************/
static inline unsigned int ioread32(void * addr)
{
    unsigned int value;
    unsigned int ext = CLUSTER_IO;

    asm volatile(
        "mtc2  %2,    $24            \n"  /* PADDR_EXT <= cluster_io */
        "lw    %0,    0(%1)          \n"  /* value <= *(ext\addr)    */
        "mtc2  $0,    $24            \n"  /* PADDR_EXT <= 0          */
        : "=r" (value)
        : "r" (addr), "r" (ext)
        : "memory" );

    return (volatile unsigned int)value;
}

/**********************************************************************
 * Read an 16 bits memory mapped hardware register
 *********************************************************************/
static inline unsigned short ioread16(void * addr)
{
    unsigned short value;
    unsigned int ext = CLUSTER_IO;

    asm volatile(
        "mtc2  %2,    $24            \n"  /* PADDR_EXT <= cluster_io */
        "lhu   %0,    0(%1)          \n"  /* value <= *(ext\addr)    */
        "mtc2  $0,    $24            \n"  /* PADDR_EXT <= 0          */
        : "=r" (value)
        : "r" (addr), "r" (ext)
        : "memory" );

    return (volatile unsigned short)value;
}

/**********************************************************************
 * Read an 8 bits memory mapped hardware register
 *********************************************************************/
static inline unsigned char ioread8(void * addr)
{
    unsigned char value;
    unsigned int ext = CLUSTER_IO;

    asm volatile(
        "mtc2  %2,    $24            \n"  /* PADDR_EXT <= cluster_io */
        "lbu   %0,    0(%1)          \n"  /* value <= *(ext\addr)    */
        "mtc2  $0,    $24            \n"  /* PADDR_EXT <= 0          */
        : "=r" (value)
        : "r" (addr), "r" (ext)
        : "memory" );

    return (volatile unsigned char)value;
}

/**********************************************************************
 * Write an 32 bits memory mapped hardware register
 * with physical address extension to access cluster_io
 *********************************************************************/
static inline void iowrite32(void * addr, unsigned int value)
{
    unsigned int ext = CLUSTER_IO;

    asm volatile(
        "mtc2  %2,    $24            \n"  /* PADDR_EXT <= cluster_io */
        "sw    %0,    0(%1)          \n"  /* *(ext\addr) <= value    */
        "mtc2  $0,    $24            \n"  /* PADDR_EXT <= 0          */
        "sync                        \n"  /* sync barrier            */
        :
        : "r" (value), "r" (addr), "r" (ext)
        : "memory" );
}

/**********************************************************************
 * Write an 16 bits memory mapped hardware register
 *********************************************************************/
static inline void iowrite16(void * addr, unsigned short value)
{
    unsigned int ext = CLUSTER_IO;

    asm volatile(
        "mtc2  %2,    $24            \n"  /* PADDR_EXT <= cluster_io */
        "sh    %0,    0(%1)          \n"  /* *(ext\addr) <= value    */
        "mtc2  $0,    $24            \n"  /* PADDR_EXT <= 0          */
        "sync                        \n"  /* sync barrier            */
        :
        : "r" (value), "r" (addr), "r" (ext)
        : "memory" );
}

/**********************************************************************
 * Write an 8 bits memory mapped hardware register
 *********************************************************************/
static inline void iowrite8(void * addr, unsigned char value)
{
    unsigned int ext = CLUSTER_IO;

    asm volatile(
        "mtc2  %2,    $24            \n"  /* PADDR_EXT <= cluster_io */
        "sb    %0,    0(%1)          \n"  /* *(ext\addr) <= value    */
        "mtc2  $0,    $24            \n"  /* PADDR_EXT <= 0          */
        "sync                        \n"  /* sync barrier            */
        :
        : "r" (value), "r" (addr), "r" (ext)
        : "memory" );
}

#endif

// vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
