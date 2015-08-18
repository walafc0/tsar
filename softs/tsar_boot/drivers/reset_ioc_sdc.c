/**
 * \File     : reset_ioc_sdc.c
 * \Date     : 31/04/2015
 * \Author   : Alain Greiner
 * \Copyright (c) UPMC-LIP6
 */

#include <reset_ioc_sdc.h>
#include <reset_tty.h>
#include <reset_inval.h>
#include <reset_utils.h>
#include <io.h>
#include <defs.h>

#define  SDC_POLLING_TIMEOUT  1000000     // Number of retries when polling PXCI

#define  SDC_RSP_TIMEOUT      100         // Number of retries for a config RSP

///////////////////////////////////////////////////////////////////////////////////
//          AHCI related global variables
///////////////////////////////////////////////////////////////////////////////////

// command descriptor (one single command)
static ahci_cmd_desc_t  ahci_cmd_desc  __attribute__((aligned(64)));

// command table (one single command)
static ahci_cmd_table_t  ahci_cmd_table __attribute__((aligned(64)));

// IOC/AHCI device base address
static int* const ioc_address = (int* const)SEG_IOC_BASE;

///////////////////////////////////////////////////////////////////////////////////
//          SD Card related global variables
///////////////////////////////////////////////////////////////////////////////////

// SD card relative address
unsigned int     sdc_rca;

// SD Card Hih Capacity Support when non zero
unsigned int     sdc_sdhc;

///////////////////////////////////////////////////////////////////////////////
// This function sends a command to the SD card and returns the response.
// - index      : CMD index
// - arg        : CMD argument
// - return Card response
///////////////////////////////////////////////////////////////////////////////
static unsigned int reset_sdc_send_cmd ( unsigned int   index,
                                         unsigned int   arg )
{
    unsigned int  sdc_rsp;
    register int  iter = 0;

    // load argument
    iowrite32( &ioc_address[SDC_CMD_ARG] , arg );

    // lauch command
    iowrite32( &ioc_address[SDC_CMD_ID] , index );

    // get response
    do
    {
        sdc_rsp = ioread32( &ioc_address[SDC_RSP_STS] );
        iter++;
    }
    while ( (sdc_rsp == 0xFFFFFFFF) && (iter < SDC_RSP_TIMEOUT) ); 

    return sdc_rsp;
}

/////////////////////////////////////////////////////////////////////////////////
//           Extern functions
/////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// This function initialises both the SD Card and the AHCI registers and 
// memory structures used by the AHCI_SDC peripheral (one single command).
///////////////////////////////////////////////////////////////////////////////
unsigned int reset_sdc_init()
{
    //////////// SD Card initialisation //////////
  
    unsigned int rsp;

    // define the SD card clock period
    iowrite32( &ioc_address[SDC_PERIOD] , 2 );

    // send CMD0 command (soft reset / no argument)
    rsp = reset_sdc_send_cmd( SDC_CMD0 , 0 );
    if ( rsp == 0xFFFFFFFF )
    {
        reset_puts("\n[RESET ERROR] in reset_sdc_init() : no aknowledge to CMD0\n");
        return 1;
    }

#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_init() : SDC_CMD0 done\n");
#endif

    // send CMD8 command 
    rsp = reset_sdc_send_cmd( SDC_CMD8 , SDC_CMD8_ARGUMENT );
    if ( rsp == 0xFFFFFFFF )
    {
        reset_puts("\n[RESET ERROR] in reset_sdc_init() : no response to CMD8\n");
        return 1;
    }
    else if ( rsp != SDC_CMD8_ARGUMENT )
    {
        reset_puts("\n[RESET ERROR] in reset_sdc_init() : bad response to CMD8\n");
        return 1;
    }

#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_init() : SDC_CMD8 done\n");
#endif

    // send CMD41 command to get SDHC
    rsp = reset_sdc_send_cmd( SDC_CMD41 , SDC_CMD41_ARGUMENT );
    if ( rsp == 0xFFFFFFFF )
    {
        reset_puts("\n[RESET ERROR] in reset_sdc_init() : no response to CMD41\n");
        return 1;
    }
    sdc_sdhc = ( (rsp & SDC_CMD41_RSP_CCS) != 0 );

#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_init() : SDC_CMD41 done / sdhc = ");
reset_putd( sdc_sdhc );
reset_puts("\n");
#endif

    // send CMD3 to get RCA
    rsp = reset_sdc_send_cmd( SDC_CMD3 , 0 );
    if ( rsp == 0xFFFFFFFF )
    {
        reset_puts("\n[RESET ERROR] in reset_sdc_init() : no response to CMD3\n");
        return 1;
    }
    sdc_rca = rsp;

#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_init() : SDC_CMD3 done / rca = ");
reset_putd( sdc_rca );
reset_puts("\n");
#endif

    // send CMD7
    rsp = reset_sdc_send_cmd( SDC_CMD7 , sdc_rca );
    if ( rsp == 0xFFFFFFFF )
    {
        reset_puts("\n[RESET ERROR] in reset_sdc_init() : no response to CMD7\n");
        return 1;
    }

#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_init() : SDC_CMD7 done\n");
#endif

    //////////// AHCI interface initialisation  ///////

    // initialise the command descriptor
    ahci_cmd_desc.ctba  = (unsigned int)&ahci_cmd_table;
    ahci_cmd_desc.ctbau = 0;

#if USE_IOB
    // update external memory for the commande descriptor
    reset_L2_sync( &ahci_cmd_desc , sizeof( ahci_cmd_desc_t ) );
#endif

    // initialise AHCI registers 
    iowrite32( &ioc_address[AHCI_PXCLB]  , (unsigned int)&ahci_cmd_desc );
    iowrite32( &ioc_address[AHCI_PXCLBU] , 0 );
    iowrite32( &ioc_address[AHCI_PXIE]   , 0 );
    iowrite32( &ioc_address[AHCI_PXIS]   , 0 );
    iowrite32( &ioc_address[AHCI_PXCI]   , 0 );
    iowrite32( &ioc_address[AHCI_PXCMD]  , 1 );

#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_init() : AHCI init done\n");
#endif

    return 0;

} // end reset_sdc_init()


///////////////////////////////////////////////////////////////////////////////
// This function register one command in both the command descriptor
// and the command data, and updates the HBA_PXCI register.
// The addresses are supposed to be identity mapping (vaddr == paddr).
// return 0 if success, -1 if error
///////////////////////////////////////////////////////////////////////////////
unsigned int reset_sdc_read( unsigned int  lba,
                             void*         buffer,
                             unsigned int  count )
{
    unsigned int       pxci;              // AHCI_PXCI register value
    unsigned int       pxis;              // AHCI_PXIS register value

#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_read() : buffer = ");
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
        reset_puts("\n[SDC ERROR] in reset_sdc_read() : buffer not 64 bytes aligned\n");
        return 1;
    }

    // set command header: lba value
    ahci_cmd_table.header.lba0 = (char)lba;
    ahci_cmd_table.header.lba1 = (char)(lba>>8);
    ahci_cmd_table.header.lba2 = (char)(lba>>16);
    ahci_cmd_table.header.lba3 = (char)(lba>>24);
    ahci_cmd_table.header.lba4 = 0;
    ahci_cmd_table.header.lba5 = 0;

    // set command buffer: address and size)
    ahci_cmd_table.buffer.dba  = (unsigned int)(buffer);
    ahci_cmd_table.buffer.dbau = 0;
    ahci_cmd_table.buffer.dbc  = count*512;

    // set command descriptor: one single buffer / read access
    ahci_cmd_desc.prdtl[0] = 1;
    ahci_cmd_desc.prdtl[1] = 0;
    ahci_cmd_desc.flag[0]  = 0;  // read
   
#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_read() : command registered\n");
#endif

#if USE_IOB
    // update external memory for command table
    reset_L2_sync( &ahci_cmd_table , 32 );

    // update external memory for command descriptor
    reset_L2_sync( &ahci_cmd_desc , 16 );
#endif

    // start transfer
    iowrite32( &ioc_address[AHCI_PXCI] , 1 );

#if RESET_DEBUG
reset_puts("\n[DEBUG SDC] reset_sdc_read() : AHCI_SDC activated at cycle ");
reset_putd( proctime() );
reset_puts("\n");
#endif

#if (RESET_HARD_CC == 0) || USE_IOB
    // inval buffer in L1 cache
    reset_L1_inval( buffer , count * 512 );
#endif

#if USE_IOB
    // inval buffer in  L2 cache
    reset_L2_inval( buffer , count * 512 );
#endif

    // poll PXCI until command completed by AHCI
    unsigned int iter = SDC_POLLING_TIMEOUT;
    do
    {
        pxci = ioread32( &ioc_address[AHCI_PXCI] );
        iter--;
        if (iter == 0 )
        {
            reset_puts("\n[SDC ERROR] in reset_sdc_read() : polling PXCI timeout\n");
            return 1;
        }
    }
    while( pxci ); 
             
    // get PXIS register
    pxis = ioread32( &ioc_address[AHCI_PXIS] );

    // reset PXIS register
    iowrite32( &ioc_address[AHCI_PXIS] , 0 );

    // check error status 
    if ( pxis & 0x40000000 ) 
    {
        reset_puts("\n[RESET ERROR] in reset_sdc_read() : status error\n");
        return 1;
    }

    return 0;

} // end reset_sdc_read()

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
