/*
 * \file    : reset_utils.h
 * \date    : August 2012
 * \author  : Cesar Fuguet
 */

#ifndef RESET_UTILS_H
#define RESET_UTILS_H

#include <elf-types.h>
#include <inttypes.h>
#include <defs.h>

/********************************************************************
 * Other types definition
 ********************************************************************/

#define __cache_aligned__ __attribute__((aligned(CACHE_LINE_SIZE)))

/*
 * cache line aligned disk block (sector) buffer
 */
struct aligned_blk
{
    char b[BLOCK_SIZE];
} __cache_aligned__;

/********************************************************************
 * Utility functions definition
 ********************************************************************/

/**
 * \brief processor waits for n cycles
 */
static inline void reset_sleep(int cycles)
{
    volatile int i;
    for (i = 0; i < cycles; i++);
}

/**
 * \brief returns processor count
 */
static inline unsigned int proctime()
{
    register unsigned int ret asm ("v0");
    asm volatile ("mfc0   %0,        $9":"=r" (ret));
    return ret;
}

int pread(size_t file_offset, void *buf, size_t nbyte, size_t offset);

void* memcpy(void *_dst, const void *_src, size_t n);
void* memset(void *_dst, int c, size_t len);

void check_elf_header(Elf32_Ehdr *ehdr);
void reset_print_elf_phdr(Elf32_Phdr * elf_phdr_ptr);
void reset_display_block( char* buffer );

#endif /* RESET_UTILS_H */

// vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab

