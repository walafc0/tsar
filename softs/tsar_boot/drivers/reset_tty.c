/********************************************************************
 * \file    reset_tty.c
 * \date    5 mars 2014
 * \author  Cesar Fuguet
 *
 * Minimal driver for TTY controler
 *******************************************************************/

#include <reset_tty.h>
#include <io.h>
#include <defs.h>

#ifndef SEG_TTY_BASE
#   error "SEG_TTY_BASE constant must be defined in the hard_config.h file"
#endif

static int* const tty_address = (int* const)SEG_TTY_BASE;

enum tty_registers {
    TTY_WRITE   = 0,
    TTY_STATUS  = 1,
    TTY_READ    = 2,
    TTY_CONFIG  = 3,

    TTY_SPAN    = 4,
};

///////////////////////
int reset_getc(char *c)
{
    if (ioread32( &tty_address[TTY_STATUS] ) == 0) return 0;
    *c = ioread32( &tty_address[TTY_READ] );
    return 1;
}

/////////////////////////////
void reset_putc(const char c)
{
    iowrite32( &tty_address[TTY_WRITE], (unsigned int)c );
    if (c == '\n') reset_putc( '\r' );
}

///////////////////////////////////
void reset_puts(const char *buffer)
{
    unsigned int n;

    for ( n=0; n<100; n++)
    {
        if (buffer[n] == 0) break;
        reset_putc(buffer[n]);
    }
}

/////////////////////////////////
void reset_putx(unsigned int val)
{
    static const char HexaTab[] = "0123456789ABCDEF";
    char              buf[11];
    unsigned int      c;

    buf[0]  = '0';
    buf[1]  = 'x';
    buf[10] = 0;

    for ( c = 0 ; c < 8 ; c++ )
    {
        buf[9-c] = HexaTab[val&0xF];
        val = val >> 4;
    }
    reset_puts(buf);
}

/////////////////////////////////
void reset_putd(unsigned int val)
{
    static const char DecTab[] = "0123456789";
    char              buf[11];
    unsigned int      i;
    unsigned int      first = 0;

    buf[10] = 0;

    for ( i = 0 ; i < 10 ; i++ )
    {
        if ((val != 0) || (i == 0))
        {
            buf[9-i] = DecTab[val % 10];
            first    = 9-i;
        }
        else
        {
            break;
        }
        val /= 10;
    }
    reset_puts( &buf[first] );
}

/////////////////
void reset_exit()
{
    register int pid;
    asm volatile( "mfc0 %0, $15, 1": "=r"(pid) );

    reset_puts("\n!!! Exit Processor ");
    reset_putx(pid & 0x3FF);
    reset_puts(" !!!\n");

    while(1) asm volatile("nop");   // infinite loop...
}



