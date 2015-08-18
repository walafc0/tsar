/**
 * \File     : reset_ioc_hba.c
 * \Date     : 23/11/2013
 * \Author   : alain greiner
 * \Copyright (c) UPMC-LIP6
 */

#include <reset_ioc_hba.h>
#include <reset_tty.h>
#include <reset_inval.h>
#include <io.h>
#include <defs.h>

#define   HBA_POLLING_TIMEOUT    1000000

///////////////////////////////////////////////////////////////////////////////
//               Global variables
///////////////////////////////////////////////////////////////////////////////

// command descriptor (one single command)
static hba_cmd_desc_t  hba_cmd_desc  __attribute__((aligned(64)));

// command table (one single command)
static hba_cmd_table_t  hba_cmd_table __attribute__((aligned(64)));

// IOC/HBA device base address
static int* const ioc_address = (int* const)SEG_IOC_BASE;

///////////////////////////////////////////////////////////////////////////////
//      Extern functions
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// This function register one command in both the command descriptor
// and the command data, and updates the HBA_PXCI register.
// The addresses are supposed to be identity mapping (vaddr == paddr).
// return 0 if success, -1 if error
///////////////////////////////////////////////////////////////////////////////
int reset_hba_read( unsigned int lba,  
                    void*        buffer,
                    unsigned int count )   
{
    unsigned int       pxci;           // HBA_PXCI register value
    unsigned int       pxis;           // HBA_PXIS register value

#if RESET_DEBUG
reset_puts("\n[DEBUG HBA] reset_hba_read() : buffer = ");
reset_putx( (unsigned int)buffer );
reset_puts(" / nblocks = ");
reset_putd( count );
reset_puts(" / lba = ");
reset_putd( lba );
reset_puts("\n");
#endif

    // check buffer alignment
    if( (unsigned int)buffer & 0x3F )
    {
        reset_puts("\n[RESET ERROR] in reset_hba_read() ");
        reset_puts("buffer not aligned on 64 bytes: base = "); 
        reset_putx( (unsigned int)buffer );
        reset_puts("\n"); 
        return -1;
    }

    // set command data header: lba value
    hba_cmd_table.header.lba0 = (char)lba;
    hba_cmd_table.header.lba1 = (char)(lba>>8);
    hba_cmd_table.header.lba2 = (char)(lba>>16);
    hba_cmd_table.header.lba3 = (char)(lba>>24);
    hba_cmd_table.header.lba4 = 0;
    hba_cmd_table.header.lba5 = 0;

    // set command data buffer: address and size)
    hba_cmd_table.buffer.dba  = (unsigned int)(buffer);
    hba_cmd_table.buffer.dbau = 0;
    hba_cmd_table.buffer.dbc  = count*512;

    // set command descriptor: one single buffer / read access
    hba_cmd_desc.prdtl[0] = 1;
    hba_cmd_desc.prdtl[1] = 0;
    hba_cmd_desc.flag[0]  = 0;  // read
   
#if RESET_DEBUG
reset_puts("\n[DEBUG HBA] reset_hba_read() : command registered\n");
#endif

#if USE_IOB
    // update external memory for command table
    reset_L2_sync( &hba_cmd_table , 32 );

    // update external memory for command descriptor
    reset_L2_sync( &hba_cmd_desc , 16 );
#endif

    // start transfer
    iowrite32( &ioc_address[HBA_PXCI] , 1 );

#if RESET_DEBUG
reset_puts("\n[DEBUG HBA] reset_hba_read() : MULTI_AHCI controler activated\n");
#endif

#if (RESET_HARD_CC == 0) || USE_IOB
    // inval buffer in L1 cache
    reset_L1_inval( buffer , count * 512 );
#endif

#if USE_IOB
    // inval buffer in  L2 cache
    reset_L2_inval( buffer , count * 512 );
#endif

    // poll PXCI until command completed by HBA
    unsigned int iter = HBA_POLLING_TIMEOUT;
    do
    {
        pxci = ioread32( &ioc_address[HBA_PXCI] );
        iter--;
        if (iter == 0 )
        {
            reset_puts("\n[SDC ERROR] in reset_sdc_read() : polling timeout\n");
            return 1;
        }
    }
    while( pxci ); 
             
    // get PXIS register
    pxis = ioread32( &ioc_address[HBA_PXIS] );

    // reset PXIS register
    iowrite32( &ioc_address[HBA_PXIS] , 0 );

    // reset PXCI register : we use only command slot[0] in PXCI
    iowrite32( &ioc_address[HBA_PXCI] , 0 );

    // check error status 
    if ( pxis & 0x40000000 ) 
    {
        reset_puts("[RESET ERROR] in reset_hba_read() : "
                   " status error returned by HBA\n");
        return 1;
    }

    return 0;
} // end reset_hba_read()


///////////////////////////////////////////////////////////////////////////////
// This function initialises both the HBA registers and the 
// memory structures used by the AHCI peripheral (one single command).
///////////////////////////////////////////////////////////////////////////////
int reset_hba_init()
{
    // initialise the command descriptor
    hba_cmd_desc.ctba  = (unsigned int)&hba_cmd_table;
    hba_cmd_desc.ctbau = 0;

#if USE_IOB
    // update external memory for the commande descriptor
    reset_L2_sync( &hba_cmd_desc , sizeof( hba_cmd_desc_t ) );
#endif
    // initialise HBA registers 
    iowrite32( &ioc_address[HBA_PXCLB]  , (unsigned int)&hba_cmd_desc );
    iowrite32( &ioc_address[HBA_PXCLBU] , 0 );
    iowrite32( &ioc_address[HBA_PXIE]   , 0 );
    iowrite32( &ioc_address[HBA_PXIS]   , 0 );
    iowrite32( &ioc_address[HBA_PXCI]   , 0 );
    iowrite32( &ioc_address[HBA_PXCMD]  , 1 );

#if RESET_DEBUG
reset_puts("\n[DEBUG HBA] reset_hba_init() : AHCI init done\n");
#endif

    return 0;
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

