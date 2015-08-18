////////////////////////////////////////////////////////////////////////////////////////
// File : stdio.c
// Written by Alain Greiner
// Date : janvier 2014
//
// This file defines various functions that can be used by applications to access
// peripherals, for the TSAR multi-processors multi_clusters architecture.
// There is NO separation between application code and system code, as the
// application are running in kernel mode without system calls.
// This basic GIET does not support virtual memory, and does not support multi-tasking.
//
// The supported peripherals are:
// - the SoClib multi_tty
// - The SoCLib frame_buffer
// - The SoCLib block_device
//
// The following parameters must be defined in the hard_config.h file.
// - X_SIZE          : number of clusters in a row
// - Y_SIZE          : number of clusters in a column
// - X_WIDTH         : number of bits for X field in proc_id
// - Y_WIDTH         : number of bits for Y field in proc_id
// - NB_PROCS_MAX    : max number of processor per cluster
// - NB_TTY_CHANNELS : max number of TTY channels
//
// The follobing base addresses must be defined in the ldscript
// - seg_tty_base
// - seg_fbf_base
// - seg_ioc_base
////////////////////////////////////////////////////////////////////////////////////////

#include "stdio.h"

#if !defined(NB_PROCS_MAX)
#error: you must define NB_PROCS_MAX in the hard_config.h file
#endif

#if !defined(X_IO) || !defined(Y_IO)
#error: you must define X_IO and Y_IO in the hard_config.h file
#endif

#if !defined(X_SIZE)
#error: you must define X_SIZE in the hard_config.h file
#endif

#if !defined(Y_SIZE)
#error: you must define Y_SIZE in the hard_config.h file
#endif

#if !defined(X_WIDTH)
#error: you must define X_WIDTH in the hard_config.h file
#endif

#if (X_WIDTH != 4)
#error: The X_WIDTH parameter must be equal to 4
#endif

#if !defined(Y_WIDTH)
#error: you must define X_WIDTH in the hard_config.h file
#endif

#if (X_WIDTH != 4)
#error: The Y_WIDTH parameter must be equal to 4
#endif

#if !defined(NB_TTY_CHANNELS)
#error: you must define NB_TTY_CHANNELS in the hard_config.h file
#endif

#define NB_LOCKS      256
#define NB_BARRIERS   16

#define in_drivers __attribute__((section (".drivers")))
#define in_unckdata __attribute__((section (".unckdata")))
#define cacheline_aligned __attribute__((aligned(64)))

//////////////////////////////////////////////////////////////
// various informations that must be defined in ldscript
//////////////////////////////////////////////////////////////

struct plouf;
extern volatile struct plouf seg_tty_base;
extern volatile struct plouf seg_fbf_base;
extern volatile struct plouf seg_ioc_base;
extern volatile struct plouf seg_mmc_base;
extern volatile struct plouf seg_ramdisk_base;

////////////////////////////////////////////////////////////////////////////////////////
//  Global uncachable variables for synchronization between drivers and ISRs
////////////////////////////////////////////////////////////////////////////////////////

static in_unckdata int volatile  _ioc_lock    = 0;
static in_unckdata int volatile  _ioc_done    = 0;
static in_unckdata int volatile  _ioc_status;

static volatile in_unckdata struct cacheline_aligned {
    int  lock;
    int  get_full;
    char get_buf;
    char __padding[55];
} _tty_channel[NB_TTY_CHANNELS];

////////////////////////////////////////////////////////////////////////////////////////
//  Global uncachable variables for inter-task barriers
////////////////////////////////////////////////////////////////////////////////////////

static in_unckdata int volatile  _barrier_value[NB_BARRIERS] = { [0 ... NB_BARRIERS-1] = 0 };
static in_unckdata int volatile  _barrier_count[NB_BARRIERS] = { [0 ... NB_BARRIERS-1] = 0 };
static in_unckdata int volatile  _barrier_lock[NB_BARRIERS]  = { [0 ... NB_BARRIERS-1] = 0 };

////////////////////////////////////////////////////////////////////////////////////////
//  Global uncachable variables for spin_locks using LL/C instructions
////////////////////////////////////////////////////////////////////////////////////////

static in_unckdata int volatile  _spin_lock[NB_LOCKS] = { [0 ... NB_LOCKS-1] = 0 };

////////////////////////////////////////////////////////////////////////////////////////
// Memcopy taken from MutekH.
////////////////////////////////////////////////////////////////////////////////////////
in_drivers void* _memcpy( void*        _dst,
                          const void*  _src,
                          unsigned int size )
{
    unsigned int *dst = _dst;
    const unsigned int *src = _src;
    if ( ! ((unsigned int)dst & 3) && ! ((unsigned int)src & 3) )
    {
        while (size > 3)
        {
            *dst++ = *src++;
            size -= 4;
        }
    }

    unsigned char *cdst = (unsigned char*)dst;
    unsigned char *csrc = (unsigned char*)src;

    while (size--)
    {
        *cdst++ = *csrc++;
    }
    return _dst;
}
////////////////////////////////////////////////////////////////////////////////////////
// Memcopy using extended addresses
////////////////////////////////////////////////////////////////////////////////////////
in_drivers void  _extended_memcpy( unsigned int dst_cluster,
                                   unsigned int dst_address,
                                   unsigned int src_cluster,
                                   unsigned int src_address,
                                   unsigned int length )
{
    if ( (dst_address & 0x3) || (src_address & 0x3) || (length & 0x3) )
    {
        _tty_get_lock( 0 );
        _tty_puts( "ERROR in _extended_memcpy()" );
        _tty_release_lock( 0 );
        _exit();
    }

    unsigned int i;
    unsigned int word;

    for ( i = 0 ; i < length ; i = i+4 )
    {
        word = _word_extended_read( src_cluster, src_address + i );
        _word_extended_write( dst_cluster, dst_address + i, word );
    }
}
////////////////////////////////////////////////////////////////////////////////////////
// Access CP0 and returns processor ident
// No more than 1024 processors...
////////////////////////////////////////////////////////////////////////////////////////
in_drivers unsigned int _procid()
{
    unsigned int ret;
    asm volatile( "mfc0 %0, $15, 1": "=r"(ret) );
    return (ret & 0x3FF);
}
////////////////////////////////////////////////////////////////////////////////////////
// Access CP0 and returns processor time
////////////////////////////////////////////////////////////////////////////////////////
in_drivers unsigned int _proctime()
{
    unsigned int ret;
    asm volatile( "mfc0 %0, $9": "=r"(ret) );
    return ret;
}
////////////////////////////////////////////////////////////////////////////////////////
// Returns the number of processsors controled by the GIET
////////////////////////////////////////////////////////////////////////////////////////
in_drivers inline unsigned int _procnumber()
{
    return (unsigned int)(NB_PROCS_MAX * X_SIZE * Y_SIZE);
}
////////////////////////////////////////////////////////////////////////////////////////
// Returns pseudo-random number
////////////////////////////////////////////////////////////////////////////////////////
in_drivers unsigned int _rand()
{
    unsigned int x = _proctime();
    if((x & 0xF) > 7)
        return (x*x & 0xFFFF);
    else
        return (x*x*x & 0xFFFF);
}
////////////////////////////////////////////////////////////////////////////////////////
// Access CP0 and enable IRQs
////////////////////////////////////////////////////////////////////////////////////////
in_drivers inline void _it_enable()
{
    asm volatile(
            "mfc0   $8,     $12         \n"
            "ori    $8,     $8,     1   \n"
            "mtc0   $8,     $12         \n"
            ::: "$8");
}
////////////////////////////////////////////////////////////////////////////////////////
// Access CP0 and mask IRQs
////////////////////////////////////////////////////////////////////////////////////////
in_drivers inline void _it_disable()
{
    asm volatile(
            "li     $9,     0xFFFFFFFE  \n"
            "mfc0   $8,     $12         \n"
            "and    $8,     $8,     $9  \n"
            "mtc0   $8,     $12         \n"
            ::: "$8","$9");
}

////////////////////////////////////////////////////////////////////////////////////////
// Access CP0 and mask IRQs
////////////////////////////////////////////////////////////////////////////////////////
in_drivers inline void _sr_write(int sr)
{
    asm volatile("mtc0  %0, $12        \n" : /* no outputs */ : "r" (sr));
}

in_drivers inline int _sr_read()
{
    int ret;
    asm volatile("mfc0  %0, $12        \n" : "=r" (ret));
    return ret;
}

//////////////////////////////////////////////////////////////////////
// Invalidate all cache lines corresponding to a memory buffer.
// This is used by the block_device driver.
/////////////////////////////////////////////////////////////////////////
in_drivers void _dcache_buf_invalidate(const void * buffer, size_t size)
{
    size_t i;
    size_t dcache_line_size;

    // retrieve dcache line size from config register (bits 12:10)
    asm volatile("mfc0 %0, $16, 1" : "=r" (dcache_line_size));

    dcache_line_size = 2 << ((dcache_line_size>>10) & 0x7);

    // iterate on lines to invalidate each one of them
    for ( i=0; i<size; i+=dcache_line_size )
        asm volatile(" cache %0, %1"
                :
                :"i" (0x11), "R" (*((char*)buffer+i)));
}

////////////////////////////////////////////////////////////////////////////
// This function makes a physical read access to a 32 bits word in memory,
// after a temporary paddr extension.
////////////////////////////////////////////////////////////////////////////
in_drivers volatile unsigned int _word_extended_read( unsigned int cluster,
                                                      unsigned int address )
{
    int sr = _sr_read();
    volatile unsigned int value;
    asm volatile(
            "li      $3,        0xFFFFFFFE    \n"
            "and     $3,        %3,     $3    \n"
            "mtc0    $3,        $12           \n"     /* IRQ disabled     */

            "mtc2    %2,        $24           \n"     /* PADDR_EXT <= msb */
            "lw      %0,        0(%1)         \n"     /* value <= *paddr  */
            "mtc2    $0,        $24           \n"     /* PADDR_EXT <= 0   */

            : "=r" (value)
            : "r" (address), "r" (cluster), "r" (sr)
            : "$2", "$3", "memory" );

    _sr_write(sr);
    return value;
}
////////////////////////////////////////////////////////////////////////////
// This function makes a physical read access to a single byte in memory,
// after a temporary paddr extension.
////////////////////////////////////////////////////////////////////////////
in_drivers volatile unsigned char _byte_extended_read( unsigned int  cluster,
                                                       unsigned int  address )
{
    int sr = _sr_read();
    volatile unsigned char value;
    asm volatile(
            "li      $3,        0xFFFFFFFE    \n"
            "and     $3,        %3,     $3    \n"
            "mtc0    $3,        $12           \n"     /* IRQ disabled     */

            "mtc2    %2,        $24           \n"     /* PADDR_EXT <= msb */
            "lb      %0,        0(%1)         \n"     /* value <= *paddr  */
            "mtc2    $0,        $24           \n"     /* PADDR_EXT <= 0   */

            : "=r" (value)
            : "r" (address), "r" (cluster), "r" (sr)
            : "$2", "$3", "memory" );

    _sr_write(sr);
    return value;
}
////////////////////////////////////////////////////////////////////////////
// This function makes a physical write access to a 32 bits word in memory,
// after a temporary DTLB address extension.
////////////////////////////////////////////////////////////////////////////
in_drivers void _word_extended_write( unsigned int  cluster,
                                      unsigned int  address,
                                      unsigned int  word )
{
    int sr = _sr_read();
    asm volatile(
            "li      $3,        0xFFFFFFFE    \n"
            "and     $3,        %3,     $3    \n"
            "mtc0    $3,        $12           \n"     /* IRQ disabled     */

            "mtc2    %2,        $24           \n"     /* PADDR_EXT <= msb */
            "sw      %0,        0(%1)         \n"     /* *paddr <= value  */
            "mtc2    $0,        $24           \n"     /* PADDR_EXT <= 0   */

            "sync                             \n"
            :
            : "r" (word), "r" (address), "r" (cluster), "r" (sr)
            : "$2", "$3", "memory");

    _sr_write(sr);
}
////////////////////////////////////////////////////////////////////////////
// This function makes a physical write access to single byte in memory,
// after a temporary DTLB de-activation and address extension.
////////////////////////////////////////////////////////////////////////////
in_drivers void _byte_extended_write( unsigned int  cluster,
                                      unsigned int  address,
                                      unsigned char byte )
{
    int sr = _sr_read();
    asm volatile(
            "li      $3,        0xFFFFFFFE    \n"
            "and     $3,        %3,     $3    \n"
            "mtc0    $3,        $12           \n"     /* IRQ disabled     */

            "mtc2    %2,        $24           \n"     /* PADDR_EXT <= msb */
            "sb      %0,        0(%1)         \n"     /* *paddr <= value  */
            "mtc2    $0,        $24           \n"     /* PADDR_EXT <= 0   */

            "sync                             \n"
            :
            : "r" (byte), "r" (address), "r" (cluster), "r" (sr)
            : "$2", "$3", "memory");

    _sr_write(sr);
}

///////////////////////////////////////////////////////////////////////////////////////
// Exit (suicide) after printing message on TTY0
///////////////////////////////////////////////////////////////////////////////////////
in_drivers void _exit()
{
    unsigned int proc_id = _procid();
    unsigned int l       = proc_id % NB_PROCS_MAX;
    unsigned int x       = (proc_id / NB_PROCS_MAX) >> Y_WIDTH;
    unsigned int y       = (proc_id / NB_PROCS_MAX) & ((1<<Y_WIDTH) - 1);

    _tty_get_lock( 0 );
    _tty_puts("\n !!! exit proc[");
    _tty_putd( x );
    _tty_puts(",");
    _tty_putd( y );
    _tty_puts(",");
    _tty_putd( l );
    _tty_puts("]  !!!\n");
    _tty_release_lock( 0 );

    while(1) asm volatile("nop");   // infinite loop...
}

/////////////////////////////////////////////////////////////////////////
// convert a 32 bits unsigned int to a string of 10 decimal characters.
/////////////////////////////////////////////////////////////////////////
in_drivers void _itoa_dec(unsigned val, char* buf)
{
    const char  DecTab[] = "0123456789";
    unsigned int i;
    for( i=0 ; i<10 ; i++ )
    {
        if( (val!=0) || (i==0) ) buf[9-i] = DecTab[val % 10];
        else                     buf[9-i] = 0x20;
        val /= 10;
    }
}
//////////////////////////////////////////////////////////////////////////
// convert a 32 bits unsigned int to a string of 8 hexadecimal characters.
///////////////////////////////////////////////////////////////////////////
in_drivers void _itoa_hex(unsigned int val, char* buf)
{
    const char  HexaTab[] = "0123456789ABCD";
    unsigned int i;
    for( i=0 ; i<8 ; i++ )
    {
        buf[7-i] = HexaTab[val % 16];
        val /= 16;
    }
}


///////////////////////////////////////////////////////////////////////////////////////
// VCI MULTI_TTY
///////////////////////////////////////////////////////////////////////////////////////
//  The total number of TTY terminals is defined by NB_TTY_CHANNELS.
//  - If there is only one terminal, it is supposed to be shared, and used by
//    all processors: a lock must be taken before display.
//  - If there is several terminals, and the number of processors is smaller
//    than the number of terminals, there is one terminal per processor, but
//    the TTY index is not equal to the proc_id, due to cluster indexing policy:
//    proc_id = cluster_xy * NB_PROCS_MAX + local_id (with cluster_xy = x << Y_WIDTH + y)
//    tty_id  = cluster_id * NB_PROCS_MAX + local_id (with cluster_id = x * Y_SIZE + y)
//  - If the computed tty_id is larger than NB_TTY_CHANNELS, an error is returned.
///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
// Write one or several characters directly from a fixed length user buffer
// to the TTY_WRITE register of the TTY controler.
// The channel index must be checked by the calling function.
// This is a non blocking call : it test the TTY_STATUS register.
// If the TTY_STATUS_WRITE bit is set, the transfer stops and the function
// returns  the number of characters that have been actually written.
///////////////////////////////////////////////////////////////////////////////////////
in_drivers int _tty_write( char*           buffer,
                           unsigned int    length,
                           unsigned int    channel )
{
    unsigned int    base     = (unsigned int)&seg_tty_base + channel*TTY_SPAN*4;
    unsigned int    nwritten = 0;
    unsigned int    status;
    unsigned int    i;

    for ( i=0 ; i < length ; i++ )
    {
        status = _word_extended_read( CLUSTER_IO, base + TTY_STATUS*4 );
        if ( (status & 0x2) == 0x2 ) break;
        else
        {
            _byte_extended_write( CLUSTER_IO, base + TTY_WRITE*4 , buffer[i] );
            nwritten++;
        }
    }

    return nwritten;
}

///////////////////////////////////////////////////////////////////////////////////////
// Fetch one character directly from the TTY_READ register of the TTY controler,
// and writes this character to the user buffer.
// The channel index must be checked by the calling function.
// This is a non blocking call : it returns 0 if the register is empty,
// and returns 1 if the register is full.
///////////////////////////////////////////////////////////////////////////////////////
in_drivers int _tty_read( char*          buffer,
                          unsigned int   channel )
{
    unsigned int base    = (unsigned int)&seg_tty_base + channel*TTY_SPAN*4;
    unsigned int status;

    status = _word_extended_read( CLUSTER_IO, base + TTY_STATUS*4 );
    if ( (status & 0x1) == 0x1 )
    {
        buffer[0] = (char)_word_extended_read( CLUSTER_IO, base + TTY_READ*4 );
        return 1;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// This function displays a string on TTY0.
// The string must be terminated by a NUL character.
//////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_puts( char* string )
{
    int length = 0;
    while (string[length] != 0) length++;
    _tty_write( string, length, 0 );
}

///////////////////////////////////////////////////////////////////////////////
// This function displays a 32 bits unsigned int as an hexa string on TTY0.
///////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_putx(unsigned int val)
{
    static const char HexaTab[] = "0123456789ABCDEF";
    char buf[11];
    unsigned int c;

    buf[0] = '0';
    buf[1] = 'x';
    buf[10] = 0;

    for (c = 0; c < 8; c++)
    {
        buf[9 - c] = HexaTab[val & 0xF];
        val = val >> 4;
    }
    _tty_puts( buf );
}

///////////////////////////////////////////////////////////////////////////////
// This function displays a 32 bits unsigned int as a decimal string on TTY0.
///////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_putd( unsigned int val )
{
    static const char DecTab[] = "0123456789";
    char buf[11];
    unsigned int i;
    unsigned int first = 0;

    buf[10] = 0;

    for (i = 0; i < 10; i++)
    {
        if ((val != 0) || (i == 0))
        {
            buf[9 - i] = DecTab[val % 10];
            first = 9 - i;
        }
        else
        {
            break;
        }
        val /= 10;
    }
    _tty_puts( &buf[first] );
}

//////////////////////////////////////////////////////////////////////////////
// This function try to take the hardwired lock protecting exclusive access
// to TTY terminal identified by the channel argument.
// It returns only when the lock has been successfully taken.
//////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_get_lock( unsigned int channel )
{
    register unsigned int* plock = (unsigned int*)&_tty_channel[channel].lock;

    asm volatile (
            "1:                         \n"
            "ll     $2,     0(%0)       \n" // $2 <= _tty_lock
            "bnez   $2,     1b          \n" // retry  if busy
            "li     $3,     1           \n" // prepare argument for sc
            "sc     $3,     0(%0)       \n" // try to set _tty_busy
            "beqz   $3,     1b          \n" // retry if not atomic
            ::"r"(plock) :"$2","$3");
}

//////////////////////////////////////////////////////////////////////////////
// This function releases the hardwired lock protecting exclusive access
// to TTY terminal identified by the channel argument.
//////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_release_lock( unsigned int channel )
{
    _tty_channel[channel].lock = 0;
}

//////////////////////////////////////////////////////////////////////////////
// This function fetch a single ascii character from a terminal
// implicitely defined by the processor ID.
// It is a blocking function.
//////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_getc( char* buf )
{
    unsigned int proc_id = _procid();
    unsigned int channel;
    unsigned int l;
    unsigned int x;
    unsigned int y;

    // check TTY channel
    l           = (proc_id % NB_PROCS_MAX);
    x           = (proc_id / NB_PROCS_MAX) >> Y_WIDTH;
    y           = (proc_id / NB_PROCS_MAX) & ((1<<Y_WIDTH) - 1);
    channel = (x * Y_SIZE + y) * NB_PROCS_MAX + l;
    if (channel >= NB_TTY_CHANNELS )
    {
        _tty_get_lock( 0 );
        _tty_puts( "ERROR in _tty_getc(): TTY index too large\n" );
        _tty_release_lock( 0 );
        _exit();
    }

    while( _tty_read( buf, channel ) == 0 ) asm volatile("nop");
}

//////////////////////////////////////////////////////////////////////////////
//  Fetch a string of decimal characters (most significant digit first)
//  to build a 32 bits unsigned int.
//  The terminal index is implicitely defined by the processor ID.
//  This is a blocking function.
//  The decimal characters are written in a 32 characters buffer
//  until a <LF> or <CR> character is read.
//  The <DEL> character is interpreted, and previous characters can be
//  cancelled. All others characters are ignored.
//  When the <LF> or <CR> character is received, the string is converted
//  to an unsigned int value. If the number of decimal digit is too large
//  for the 32 bits range, the zero value is returned.
//////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_getw( unsigned int* word_buffer )
{
    char          buf[32];
    char          byte;
    char          cancel_string[3] = { 0x08, 0x20, 0x08 };
    char          zero             = 0x30;
    unsigned int  save = 0;
    unsigned int  val = 0;
    unsigned int  done = 0;
    unsigned int  overflow = 0;
    unsigned int  max = 0;
    unsigned int  proc_id = _procid();
    unsigned int  i;
    unsigned int  channel;
    unsigned int  x;
    unsigned int  y;
    unsigned int  l;

    // check TTY channel
    l           = (proc_id % NB_PROCS_MAX);
    x           = (proc_id / NB_PROCS_MAX) >> Y_WIDTH;
    y           = (proc_id / NB_PROCS_MAX) & ((1<<Y_WIDTH) - 1);
    channel = (x * Y_SIZE + y) * NB_PROCS_MAX + l;
    if (channel >= NB_TTY_CHANNELS )
    {
        _tty_get_lock( 0 );
        _tty_puts( "ERROR in _tty_getw(): TTY index too large\n" );
        _tty_release_lock( 0 );
        _exit();
    }

    while( done == 0 )
    {
        _tty_read( &byte, channel );

        if (( byte > 0x2F) && (byte < 0x3A))  // decimal character
        {
            buf[max] = byte;
            max++;
            _tty_write( &byte, 1, channel );
        }
        else if ( (byte == 0x0A) || (byte == 0x0D) ) // LF or CR character
        {
            done = 1;
        }
        else if ( byte == 0x7F )        // DEL character
        {
            if (max > 0)
            {
                max--;          // cancel the character
                _tty_write( cancel_string, 3, channel );
            }
        }
    } // end while

    // string conversion
    for( i=0 ; i<max ; i++ )
    {
        val = val*10 + (buf[i] - 0x30);
        if (val < save) overflow = 1;
        save = val;
    }
    if (overflow == 0)
    {
        *word_buffer = val;     // return decimal value
    }
    else
    {
        for( i=0 ; i<max ; i++)     // cancel the string
        {
            _tty_write( cancel_string, 3, channel );
        }
        _tty_write( &zero, 1, channel );
        *word_buffer = 0;       // return 0 value
    }
}

//////////////////////////////////////////////////////////////////////////////
//  This function is a simplified version of the mutek_printf() function.
//  It takes the TTY lock on the selected channel for exclusive access.
//  Only a limited number of formats are supported:
//  - %d : signed decimal
//  - %u : unsigned decimal
//  - %x : hexadecimal
//  - %c : char
//  - %s : string
//////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_printf( char *format, ...)
{
    va_list ap;
    va_start( ap, format );

    unsigned int channel;
    unsigned int x;
    unsigned int y;
    unsigned int proc_id = _procid();

    // compute TTY channel :
    // if the number of TTY channels is smaller
    // than the number of clusters, use TTY_0_0
    // else, TTY channel <= cluster index
    if ( NB_TTY_CHANNELS < (X_SIZE * Y_SIZE) )
    {
        channel = 0;
    }
    else
    {
        x           = (proc_id / NB_PROCS_MAX) >> Y_WIDTH;
        y           = (proc_id / NB_PROCS_MAX) & ((1<<Y_WIDTH) - 1);
        channel     = (x * Y_SIZE + y);
    }

    // take the TTY lock
    _tty_get_lock( channel );

printf_text:

    while (*format)
    {
        unsigned int i;
        for (i = 0; format[i] && format[i] != '%'; i++)
            ;
        if (i)
        {
            _tty_write( format, i, channel );
            format += i;
        }
        if (*format == '%')
        {
            format++;
            goto printf_arguments;
        }
    } // end while

    va_end( ap );

    // release lock
    _tty_release_lock( 0 );

    return;

printf_arguments:

    {
        int                 val = va_arg(ap, long);
        char                buf[20];
        char*               pbuf;
        unsigned int        len = 0;
        static const char   HexaTab[] = "0123456789ABCDEF";
        unsigned int        i;

        switch (*format++) {
            case ('c'):             // char conversion
                len = 1;
                buf[0] = val;
                pbuf = buf;
                break;
            case ('d'):             // decimal signed integer
                if (val < 0)
                {
                    val = -val;
                    _tty_write( "_" , 1, channel );
                }
            case ('u'):             // decimal unsigned integer
                for( i=0 ; i<10 ; i++)
                {
                    buf[9-i] = HexaTab[val % 10];
                    if (!(val /= 10)) break;
                }
                len =  i+1;
                pbuf = &buf[9-i];
                break;
            case ('x'):             // hexadecimal integer
                _tty_write( "0x", 2, channel );
                for( i=0 ; i<8 ; i++)
                {
                    buf[7-i] = HexaTab[val % 16U];
                    if (!(val /= 16U)) break;
                }
                len =  i+1;
                pbuf = &buf[7-i];
                break;
            case ('s'):             // string
                {
                    char *str = (char*)val;
                    while ( str[len] ) len++;
                    pbuf = (char*)val;
                }
                break;
            default:
                goto printf_text;
        } // end switch

        _tty_write( pbuf, len, channel );
        goto printf_text;
    }
} // end printf()

//////////////////////////////////////////////////////////////////////////////////////
//  These functions are the ISRs that must be executed when an IRQ is activated
//  by the TTY: _tty_isr_XX is associated to TTY channel [XX].
//  It save the character in the communication buffer _tty_get_buf[XX],
//  and set the set/reset variable _tty_get_full[XX].
//  A character is lost if the buffer is full when the ISR is executed.
//////////////////////////////////////////////////////////////////////////////////////
in_drivers void _tty_isr_indexed(size_t index)
{
    unsigned int base = (unsigned int)&seg_tty_base;
    unsigned int offset = (index*TTY_SPAN + TTY_READ) << 2;

    _tty_channel[index].get_buf  = _byte_extended_read(CLUSTER_IO, base + offset);
    _tty_channel[index].get_full = 1;               // signals character available
}

in_drivers void _tty_isr()    { _tty_isr_indexed(0); }

in_drivers void _tty_isr_00() { _tty_isr_indexed(0); }
in_drivers void _tty_isr_01() { _tty_isr_indexed(1); }
in_drivers void _tty_isr_02() { _tty_isr_indexed(2); }
in_drivers void _tty_isr_03() { _tty_isr_indexed(3); }
in_drivers void _tty_isr_04() { _tty_isr_indexed(4); }
in_drivers void _tty_isr_05() { _tty_isr_indexed(5); }
in_drivers void _tty_isr_06() { _tty_isr_indexed(6); }
in_drivers void _tty_isr_07() { _tty_isr_indexed(7); }
in_drivers void _tty_isr_08() { _tty_isr_indexed(8); }
in_drivers void _tty_isr_09() { _tty_isr_indexed(9); }
in_drivers void _tty_isr_10() { _tty_isr_indexed(10); }
in_drivers void _tty_isr_11() { _tty_isr_indexed(11); }
in_drivers void _tty_isr_12() { _tty_isr_indexed(12); }
in_drivers void _tty_isr_13() { _tty_isr_indexed(13); }
in_drivers void _tty_isr_14() { _tty_isr_indexed(14); }
in_drivers void _tty_isr_15() { _tty_isr_indexed(15); }
in_drivers void _tty_isr_16() { _tty_isr_indexed(16); }
in_drivers void _tty_isr_17() { _tty_isr_indexed(17); }
in_drivers void _tty_isr_18() { _tty_isr_indexed(18); }
in_drivers void _tty_isr_19() { _tty_isr_indexed(19); }
in_drivers void _tty_isr_20() { _tty_isr_indexed(20); }
in_drivers void _tty_isr_21() { _tty_isr_indexed(21); }
in_drivers void _tty_isr_22() { _tty_isr_indexed(22); }
in_drivers void _tty_isr_23() { _tty_isr_indexed(23); }
in_drivers void _tty_isr_24() { _tty_isr_indexed(24); }
in_drivers void _tty_isr_25() { _tty_isr_indexed(25); }
in_drivers void _tty_isr_26() { _tty_isr_indexed(26); }
in_drivers void _tty_isr_27() { _tty_isr_indexed(27); }
in_drivers void _tty_isr_28() { _tty_isr_indexed(28); }
in_drivers void _tty_isr_29() { _tty_isr_indexed(29); }
in_drivers void _tty_isr_30() { _tty_isr_indexed(30); }
in_drivers void _tty_isr_31() { _tty_isr_indexed(31); }


//////////////////////////////////////////////////////////////////////////////////////////
//   BLOCK_DEVICE (IOC)
//////////////////////////////////////////////////////////////////////////////////////////
// The block size is 512 bytes.
// The functions below use the three variables _ioc_lock _ioc_done,
// and _ioc_status for synchronisation.
// - As the IOC component can be used by several programs running in parallel,
// the _ioc_lock variable guaranties exclusive access to the device.
// The _ioc_read() and _ioc_write() functions use atomic LL/SC to get the lock.
// and set _ioc_lock to a non zero value.
// The _ioc_write() and _ioc_read() functions are blocking, polling the _ioc_lock
// variable until the device is available.
// - When the tranfer is completed, the ISR routine activated by the IOC IRQ
// set the _ioc_done variable to a non-zero value. Possible address errors detected
// by the IOC peripheral are reported by the ISR in the _ioc_status variable.
// The _ioc_completed() function is polling the _ioc_done variable, waiting for
// tranfer conpletion. When the completion is signaled, the _ioc_completed() function
// reset the _ioc_done variable to zero, and releases the _ioc_lock variable.
///////////////////////////////////////////////////////////////////////////////////////
//  If USE_IOC_RDK is set, we access a "virtual" block device controler implemented
//  as a memory-mapped segment in cluster [0,0] at address seg_ramdisk_base.
//  The tranfer being fully synchronous, the IOC interrupt is not activated.
///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
// This blocking function is used by the _ioc_read() and _ioc_write() functions
// to get _ioc_lock using LL/SC.
///////////////////////////////////////////////////////////////////////////////////////
in_drivers void _ioc_get_lock()
{
    register unsigned int*  plock = (unsigned int*)&_ioc_lock;

    asm volatile (
            "1:                         \n"
            "ll     $2,     0(%0)       \n" // $2 <= _ioc_lock
            "bnez   $2,     1b          \n" // retry  if busy
            "li     $3,     1           \n" // prepare argument for sc
            "sc     $3,     0(%0)       \n" // try to set _ioc_busy
            "beqz   $3,     1b          \n" // retry if not atomic
            ::"r"(plock) :"$2","$3");
}

//////////////////////////////////////////////////////////////////////////////////////
// Transfer data from a memory buffer to the block_device.
// - lba    : first block index on the disk
// - buffer : base address of the memory buffer
// - count  : number of blocks to be transfered
// - ext    : cluster index for the memory buffer
///////////////////////////////////////////////////////////////////////////////////////
in_drivers void _ioc_write( size_t   lba,
                            void*    buffer,
                            size_t   count,
                            size_t   ext )
{
    // get the lock
    _ioc_get_lock();

    if ( USE_IOC_RDK )  // we use an extended_memcpy
    {
        unsigned int  src_address = (unsigned int)buffer;
        unsigned int  src_cluster = ext;
        unsigned int  dst_address = (unsigned int)&seg_ramdisk_base + lba*512;
        unsigned int  dst_cluster = 0;

        _extended_memcpy( dst_cluster,
                          dst_address,
                          src_cluster,
                          src_address,
                          count*512 );

        _ioc_status = BLOCK_DEVICE_WRITE_SUCCESS;
        _ioc_done   = 1;

        return;
    }

    unsigned int base = (unsigned int)&seg_ioc_base;
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_BUFFER*4,     (unsigned int)buffer );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_BUFFER_EXT*4, ext );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_COUNT*4,      count );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_LBA*4,        lba );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_IRQ_ENABLE*4, 1 );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_OP*4,         BLOCK_DEVICE_WRITE );
}

///////////////////////////////////////////////////////////////////////////////////////
// Transfer data from a file on the block device to a memory buffer.
// - lba    : first block index on the disk
// - buffer : base address of the memory buffer
// - count  : number of blocks to be transfered
// - ext    : cluster index for the memory buffer
///////////////////////////////////////////////////////////////////////////////////////
in_drivers void _ioc_read( size_t   lba,
                           void*    buffer,
                           size_t   count,
                           size_t   ext )
{
    // get the lock
    _ioc_get_lock();

    if ( USE_IOC_RDK )  // we use an extended_memcpy
    {
        unsigned int  dst_address = (unsigned int)buffer;
        unsigned int  dst_cluster = ext;
        unsigned int  src_address = (unsigned int)&seg_ramdisk_base + lba*512;
        unsigned int  src_cluster = 0;

        _extended_memcpy( dst_cluster,
                          dst_address,
                          src_cluster,
                          src_address,
                          count*512 );

        _ioc_status = BLOCK_DEVICE_READ_SUCCESS;
        _ioc_done   = 1;

        return;
    }

    const unsigned int base = (unsigned int)&seg_ioc_base;
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_BUFFER*4,     (unsigned int)buffer );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_BUFFER_EXT*4, ext );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_COUNT*4,      count );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_LBA*4,        lba );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_IRQ_ENABLE*4, 1 );
    _word_extended_write( CLUSTER_IO, base + BLOCK_DEVICE_OP*4,         BLOCK_DEVICE_READ );
}

in_drivers inline unsigned int _ioc_get_blocksize() {
    const unsigned int base = (unsigned int)&seg_ioc_base;
    return _word_extended_read( CLUSTER_IO, base + BLOCK_DEVICE_BLOCK_SIZE*4 );
}

///////////////////////////////////////////////////////////////////////////////////////
// This blocking function cheks completion of an I/O transfer and reports errors.
// It returns 0 if the transfer is successfully completed.
// It returns -1 if an error has been reported.
///////////////////////////////////////////////////////////////////////////////////////
in_drivers void _ioc_completed()
{
    // waiting for completion
    while (_ioc_done == 0)  asm volatile("nop");

    // reset synchronisation variables
    _ioc_done = 0;
    _ioc_lock = 0;

    if( (_ioc_status != BLOCK_DEVICE_READ_SUCCESS) &&
        (_ioc_status != BLOCK_DEVICE_WRITE_SUCCESS) )
    {
        _tty_get_lock( 0 );
        _tty_puts( "ERROR in _ioc_completed()\n");
        _tty_release_lock( 0 );
        _exit();
    }
}

//////////////////////////////////////////////////////////////////////////////////////
//  This ISR must be executed when an IRQ is activated by IOC to signal completion.
//  It acknowledge the IRQ using the ioc base address, save the status in _ioc_status,
//  and set the _ioc_done variable to signal completion.
//  This variable is defined in the drivers.c file.
//////////////////////////////////////////////////////////////////////////////////////
in_drivers void _ioc_isr()
{
    unsigned int base = (unsigned int)&seg_ioc_base;

    _ioc_status = _word_extended_read( CLUSTER_IO, base + BLOCK_DEVICE_STATUS*4 );
    _ioc_done   = 1;       // signals completion
}

//////////////////////////////////////////////////////////////////////////////////////
//  FRAME_BUFFER
//////////////////////////////////////////////////////////////////////////////////////
// The _fb_sync_write & _fb_sync_read functions use a memcpy strategy to implement
// the transfer between a data buffer and the frame buffer.
// They are blocking until completion of the transfer.
//////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////
//  _fb_sync_write()
// Transfer data from an user buffer to the frame_buffer device with a memcpy.
// - offset : offset (in bytes) in the frame buffer
// - buffer : base address of the memory buffer
// - length : number of bytes to be transfered
// - ext    : cluster_xy for the user buffer
//////////////////////////////////////////////////////////////////////////////////////
in_drivers void _fb_sync_write( unsigned int  offset,
                                unsigned int  buffer,
                                unsigned int  length,
                                unsigned int  ext )
{
    unsigned int  src_address = buffer;
    unsigned int  src_cluster = ext;
    unsigned int  dst_address = (unsigned int)&seg_fbf_base + offset;

    _extended_memcpy( CLUSTER_IO,
                      dst_address,
                      src_cluster,
                      src_address,
                      length );
}

///////////////////////////////////////////////////////////////////////////////////////
//  _fb_sync_read()
// Transfer data from the frame_buffer device to an user buffer with a memcpy.
// - offset : offset (in bytes) in the frame buffer
// - buffer : base address of the memory buffer
// - length : number of bytes to be transfered
// - ext    : cluster_xy for the user buffer
//////////////////////////////////////////////////////////////////////////////////////
in_drivers void  _fb_sync_read( unsigned int  offset,
                                unsigned int  buffer,
                                unsigned int  length,
                                unsigned int  ext )
{
    unsigned int  dst_address = buffer;
    unsigned int  dst_cluster = ext;
    unsigned int  src_address = (unsigned int)&seg_fbf_base + offset;

    _extended_memcpy( dst_cluster,
                      dst_address,
                      CLUSTER_IO,
                      src_address,
                      length );
}

//////////////////////////////////////////////////////////////////////////////////////
//  This ISR must be executed when an IRQ is activated by MEMC to signal
//  an error detected by the TSAR memory cache after a write transaction.
//  It displays an error message on the TTY terminal allocated to the processor
//  executing the ISR.
//////////////////////////////////////////////////////////////////////////////////////
in_drivers void _mmc_isr()
{
    //int*         mmc_address = (int*)&seg_mmc_base;
    unsigned int cluster_xy  = _procid() / NB_PROCS_MAX;

    _tty_printf( "WRITE ERROR signaled by Memory Cache in cluster %x\n", cluster_xy );
}

///////////////////////////////////////////////////////////////////////////////////////
// Release a software spin-lock
///////////////////////////////////////////////////////////////////////////////////////
in_drivers void _release_lock(size_t index)

{
    if( index >= NB_LOCKS )
    {
        _tty_get_lock( 0 );
        _tty_puts( "ERROR in _release_lock()" );
        _tty_release_lock( 0 );
        _exit();
    }

    _spin_lock[index] = 0;
}

///////////////////////////////////////////////////////////////////////////////////////
// Try to take a software spin-lock.
// This is a blocking call, as there is a busy-waiting loop,
// until the lock is granted to the requester.
// There is an internal delay of about 100 cycles between
// two successive lock read, to avoid bus saturation.
///////////////////////////////////////////////////////////////////////////////////////
in_drivers void _get_lock(size_t index)
{
    if( index >= NB_LOCKS )
    {
        _tty_get_lock( 0 );
        _tty_puts( "ERROR in _get_lock()" );
        _tty_release_lock( 0 );
        _exit();
    }

    register int   delay = ((_proctime() +_procid()) & 0xF) << 4;
    register int * plock = (int *) &_spin_lock[index];

    asm volatile ("_locks_llsc:             \n"
                  "ll   $2,    0(%0)        \n"     // $2 <= _locks_lock
                  "bnez $2,    _locks_delay \n"     // random delay if busy
                  "li   $3,    1            \n"     // prepare argument for sc
                  "sc   $3,    0(%0)        \n"     // try to set _locks_busy
                  "bnez $3,    _locks_ok    \n"     // exit if atomic
                  "_locks_delay:            \n"
                  "move $4,    %1           \n"     // $4 <= delay
                  "_locks_loop:             \n"
                  "addi $4,    $4,    -1    \n"     // $4 <= $4 - 1
                  "beqz $4,    _locks_loop  \n"     // test end delay
                  "j           _locks_llsc  \n"     // retry
                  "_locks_ok:           \n"
                  ::"r"(plock),"r"(delay):"$2","$3","$4");
}


//////////////////////////////////////////////////////////////////////////////////////
// This function makes a cooperative initialisation of the barrier:
// - barrier_count[index] <= N
// - barrier_lock[index]  <= 0
// All tasks try to initialize the barrier, but the initialisation
// is done by only one task, using LL/SC instructions.
// This cooperative initialisation is questionnable,
// because the barrier can ony be initialised once...
//////////////////////////////////////////////////////////////////////////////////////
in_drivers void _barrier_init(unsigned int index, unsigned int value)
{
    register int* pinit     = (int*)&_barrier_value[index];
    register int* pcount    = (int*)&_barrier_count[index];
    register int* plock     = (int*)&_barrier_lock[index];

    if ( index >= NB_BARRIERS )
    {
        _tty_get_lock( 0 );
        _tty_puts( "ERROR in _barrier_init()" );
        _tty_release_lock( 0 );
        _exit();
    }

    // parallel initialisation using atomic instructions LL/SC
    asm volatile ("_barrier_init_test:              \n"
                  "ll   $2,     0(%0)               \n" // read barrier_value
                  "bnez $2,     _barrier_init_done  \n"
                  "move $3,     %3                  \n"
                  "sc   $3,     0(%0)               \n" // try to write barrier_value
                  "beqz $3,     _barrier_init_test  \n"
                  "move $3, %3                      \n"
                  "sw   $3, 0(%1)                   \n" // barrier_count <= barrier_value
                  "move $3, $0                      \n" //
                  "sw   $3, 0(%2)                   \n" // barrier_lock <= 0
                  "_barrier_init_done:          \n"
                  ::"r"(pinit),"r"(pcount),"r"(plock),"r"(value):"$2","$3");
}

//////////////////////////////////////////////////////////////////////////////////////
// This blocking function uses a busy_wait technics (on the barrier_lock value),
// because the GIET does not support dynamic scheduling/descheduling of tasks.
// The barrier state is actually defined by two variables:
// _barrier_count[index] define the number of particpants that are waiting
// _barrier_lock[index] define the bool variable whose value is polled
// The last participant change the value of _barrier_lock[index] to release the barrier...
// There is at most 16 independant barriers, and an error is returned
// if the barrier index is larger than 15.
//////////////////////////////////////////////////////////////////////////////////////
in_drivers void _barrier_wait(unsigned int index)
{
    register int*   pcount      = (int*)&_barrier_count[index];
    register int    count;
    int             lock        = _barrier_lock[index];

    if ( index >= NB_BARRIERS )
    {
        _tty_get_lock( 0 );
        _tty_puts( "ERROR in _barrier_wait()" );
        _tty_release_lock( 0 );
        _exit();
    }

    // parallel decrement _barrier_count[index] using atomic instructions LL/SC
    // input : pointer on _barrier_count[index]
    // output : count = _barrier_count[index] (before decrementation)
    asm volatile ("_barrier_decrement:                  \n"
                  "ll   %0,     0(%1)                   \n"
                  "addi $3,     %0,     -1              \n"
                  "sc   $3,     0(%1)                   \n"
                  "beqz $3,     _barrier_decrement      \n"
                  :"=&r"(count)
                  :"r"(pcount)
                  :"$2","$3");

    // the last task re-initializes the barrier_ count variable
    // and the barrier_lock variable, waking up all other waiting tasks

    if ( count == 1 )    // last task
    {
        _barrier_count[index] = _barrier_value[index];
        asm volatile( "sync" );
        _barrier_lock[index]   = (lock == 0) ? 1 : 0;
    }
    else        // other tasks
    {
        while ( lock == _barrier_lock[index] );
    }
}


// Local Variables:
// tab-width: 4;
// c-basic-offset: 4;
// c-file-offsets:((innamespace . 0)(inline-open . 0));
// indent-tabs-mode: nil;
// End:
//
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

