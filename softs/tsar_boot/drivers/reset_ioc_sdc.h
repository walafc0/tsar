/**
 * \File     : reset_ioc_sdc.h
 * \Date     : 31/O4/2015
 * \Author   : alain greiner
 * \Copyright (c) UPMC-LIP6
 */

///////////////////////////////////////////////////////////////////////////////////
// This driver supports the SocLib VciAhciSdc component, that is a single channel,
// block oriented, SD card contrôler, respecting the AHCI standard.
// This driver supports only SD Cards V2 and higher, and the block
// size must be 512 bytes.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _RESET_SDC_IOC_H_
#define _RESET_SDC_IOC_H_

/////////////////////////////////////////////////////////////////////////////
//    SDC Addressable Registers (up to 64 registers)
/////////////////////////////////////////////////////////////////////////////

enum SoclibSdcRegisters
{
    SDC_PERIOD       = 32,          // system cycles       / Write-Only
    SDC_CMD_ID       = 33,          // command index       / Write-Only
    SDC_CMD_ARG      = 34,          // command argument    / Write-Only
    SDC_RSP_STS      = 35,          // response status     / Read-Only
};

/////////////////////////////////////////////////////////////////////////////
//    Software supported SDC commands
/////////////////////////////////////////////////////////////////////////////

enum SoclibSdcCommands
{
    SDC_CMD0         = 0,           // Soft reset
    SDC_CMD3         = 3,           // Relative Card Address
    SDC_CMD7         = 7,           // Toggle mode
    SDC_CMD8         = 8,           // Voltage info 
    SDC_CMD41        = 41,          // Operation Condition
};

enum SoclibSdcErrorCodes
{
    SDC_ERROR_LBA    = 0x40000000,  // LBA larger tnan SD card capacity
    SDC_ERROR_CRC    = 0x00800000,  // CRC error reported by SD card
    SDC_ERROR_CMD    = 0x00400000,  // command notsupported by SD card
};
           
///////////////////////////////////////////////////////////////////////////////
//      Various SD Card constants
///////////////////////////////////////////////////////////////////////////////

#define SDC_CMD8_ARGUMENT   0x00000155  // VHS = 2.7-3.6 V / check = 0x55
#define SDC_CMD41_ARGUMENT  0x40000000  // High Capacity Host Support 
#define SDC_CMD41_RSP_BUSY  0x80000000  // Card Busy when 0     
#define SDC_CMD41_RSP_CCS   0x40000000  // High Capacity when 1
 
/////////////////////////////////////////////////////////////////////////////
//    AHCI Addressable Registers 
/////////////////////////////////////////////////////////////////////////////

enum SoclibAhciRegisters 
{
    AHCI_PXCLB       = 0,           // command list base address 32 LSB bits
    AHCI_PXCLBU      = 1,           // command list base address 32 MSB bits
    AHCI_PXIS        = 4,           // interrupt status
    AHCI_PXIE        = 5,           // interrupt enable
    AHCI_PXCMD       = 6,           // run
    AHCI_PXCI        = 14,          // command bit-vector     
};

/////////////////////////////////////////////////////////////////////////////
// AHCI structures for Command List
/////////////////////////////////////////////////////////////////////////////

/////// command descriptor  ///////////////////////
typedef struct ahci_cmd_desc_s  // size = 16 bytes
{
    unsigned char       flag[2];    // W in bit 6 of flag[0]
    unsigned char       prdtl[2];	// Number of buffers
    unsigned int        prdbc;		// Number of bytes actually transfered
    unsigned int        ctba;		// Command Table base address 32 LSB bits
    unsigned int        ctbau;		// Command Table base address 32 MSB bits
} ahci_cmd_desc_t;


/////////////////////////////////////////////////////////////////////////////
// AHCI structures for Command Table
/////////////////////////////////////////////////////////////////////////////

/////// command header  ///////////////////////////////
typedef struct ahci_cmd_header_s     // size = 16 bytes
{
    unsigned int        res0;       // reserved	
    unsigned char	    lba0;	    // LBA 7:0
    unsigned char	    lba1;	    // LBA 15:8
    unsigned char	    lba2;	    // LBA 23:16
    unsigned char	    res1;	    // reserved
    unsigned char	    lba3;	    // LBA 31:24
    unsigned char	    lba4;	    // LBA 39:32
    unsigned char	    lba5;	    // LBA 47:40
    unsigned char	    res2;	    // reserved
    unsigned int        res3;       // reserved	
} ahci_cmd_header_t;

/////// Buffer Descriptor //////////////////////////
typedef struct ahci_cmd_buffer_s // size = 16 bytes
{
    unsigned int        dba;	    // Buffer base address 32 LSB bits
    unsigned int        dbau;	    // Buffer base address 32 MSB bits
    unsigned int        res0;	    // reserved
    unsigned int        dbc;	    // Buffer bytes count
} ahci_cmd_buffer_t;

/////// command table /////////////////////////////////
typedef struct ahci_cmd_table_s     // size = 32 bytes
{
    ahci_cmd_header_t   header;     // contains LBA value
    ahci_cmd_buffer_t   buffer;     // contains buffer descriptor
} ahci_cmd_table_t;


///////////////////////////////////////////////////////////////////////////////
// This function initializes the AHCI_SDC controller and the SD Card.
// Returns 0 if success, > 0 if failure
///////////////////////////////////////////////////////////////////////////////

unsigned int reset_sdc_init();

///////////////////////////////////////////////////////////////////////////////
// Transfer data between the block device and a memory buffer. 
// - lba       : first block index on the block device
// - buffer    : base address of the memory buffer
// - count     : number of blocks to be transfered.
// Returns 0 if success, > 0 if error.
///////////////////////////////////////////////////////////////////////////////

unsigned int reset_sdc_read( unsigned int   lba,
                             void*          buffer,
                             unsigned int   count);

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
