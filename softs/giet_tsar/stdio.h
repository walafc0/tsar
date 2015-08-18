////////////////////////////////////////////////////////////////////////////////////////
// File : stdio.h
// Written by Alain Greiner
// Date : 17/01/2014
//
// This file define varions functions that can be used by applications to access
// peripherals, or other ressources such as processor registers, spin_locks
// or synchronisation barriers.
// It is dedicated for the TSAR multi-processors multi_clusters architecture.
// There is NO separation between application code and system code.
// This basic GIET does not use the virtual memory, and does nort support multi-tasking.
//
//The supported peripherals are:
//- the SoClib multi_tty
//- The SoCLib frame_buffer
//- The SoCLib block_device
//
//The following parameters must be defined in the hard_config.h file.
//- X_SIZE          : number of clusters in a row
//- Y_SIZE          : number of clusters in a column
//- X_WIDTH         : number of bits for X field in proc_id
//- Y_WIDTH         : number of bits for Y field in proc_id
//- NB_PROCS_MAX    : max number of processor per cluster
//- NB_TTY_CHANNELS : max number of TTY channels
//
//The follobing base addresses must be defined in the ldscript
//- seg_tty_base
//- seg_fbf_base
//- seg_ioc_base
////////////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_STDIO_H_
#define _GIET_STDIO_H_

#include "tty.h"
#include "block_device.h"
#include "hard_config.h"
#include <stdarg.h>

typedef unsigned int    size_t;

// constants

#define CLUSTER_IO (((X_IO) << (Y_WIDTH)) | (Y_IO))

// functions defined in stdio.c

void*           _memcpy( void* dst, const void* src, size_t size );

void            _extended_memcpy( unsigned int dst_cluster,
                                  unsigned int dst_address,
                                  unsigned int src_cluster,
                                  unsigned int src_address,
                                  unsigned int length );
inline unsigned int _procid();
inline unsigned int _proctime();
inline unsigned int _procnumber();

inline unsigned int _io_cluster();

unsigned int    _rand();

void            _it_mask();
void            _it_enable();

int             _sr_read();
void            _sr_write(int sr);

void            _dcache_buf_invalidate( const void* buffer, size_t size );

void            _exit();

void            _itoa_dec( unsigned int val, char* buf );
void            _itoa_hex( unsigned int val, char* buf );

int             _tty_write( char* buffer, size_t length, size_t channel );
int             _tty_read( char* buffer, size_t channel );
void            _tty_puts( char* string );
void            _tty_putd( unsigned int val );
void            _tty_putx( unsigned int val );
void            _tty_get_lock( size_t channel );
void            _tty_release_lock( size_t channel );
void            _tty_getc( char* buffer );
void            _tty_getw( unsigned int* buffer );
void            _tty_printf( char* format, ... );
void            _tty_isr();

void            _ioc_get_lock();
void            _ioc_write( size_t lba, void* buffer, size_t count, size_t ext );
void            _ioc_read (size_t lba, void* buffer, size_t count, size_t ext );
unsigned int    _ioc_get_blocksize();
void            _ioc_completed();
void            _ioc_isr();

void            _mmc_isr();

void            _fb_sync_write( unsigned int offset,
                                unsigned int buffer,
                                unsigned int length,
                                unsigned int ext );
void            _fb_sync_read(  unsigned int offset,
                                unsigned int buffer,
                                unsigned int length,
                                unsigned int ext );

void            _release_lock( size_t lock_index );
void            _get_lock( size_t lock_index );

void            _barrier_init(size_t index, size_t count);
void            _barrier_wait(size_t index);

volatile unsigned char _byte_extended_read( unsigned int cluster,
                                            unsigned int address );
volatile unsigned int  _word_extended_read( unsigned int cluster,
                                            unsigned int address );

void            _word_extended_write( unsigned int   cluster,
                                      unsigned int   address,
                                      unsigned int   word );
void            _byte_extended_write( unsigned int   cluster,
                                      unsigned int   address,
                                      unsigned char  byte );
#endif

// Local Variables:
// tab-width: 4;
// c-basic-offset: 4;
// c-file-offsets:((innamespace . 0)(inline-open . 0));
// indent-tabs-mode: nil;
// End:
//
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

