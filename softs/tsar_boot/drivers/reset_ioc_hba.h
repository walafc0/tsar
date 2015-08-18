/**
 * \File     : reset_hba.h
 * \Date     : 01/04/2015
 * \Author   : alain greiner
 * \Copyright (c) UPMC-LIP6
 */

#ifndef _RESET_IOC_HBA_H
#define _RESET_IOC_HBA_H

///////////////////////////////////////////////////////////////////////////////////
// HBA component registers offsets
///////////////////////////////////////////////////////////////////////////////////

enum SoclibMultiAhciRegisters 
{
  HBA_PXCLB            = 0,         // command list base address 32 LSB bits
  HBA_PXCLBU           = 1,         // command list base address 32 MSB bits
  HBA_PXIS             = 4,         // interrupt status
  HBA_PXIE             = 5,         // interrupt enable
  HBA_PXCMD            = 6,         // run
  HBA_PXCI             = 14,        // command bit-vector     
  HBA_SPAN             = 0x400,     // 4 Kbytes per channel => 1024 slots
};

///////////////////////////////////////////////////////////////////////////////////
// structures for one AHCI command table
///////////////////////////////////////////////////////////////////////////////////

typedef struct hba_cmd_header_s // size = 16 bytes
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

} hba_cmd_header_t;

typedef struct hba_cmd_buffer_s // size = 16 bytes
{
    unsigned int        dba;	    // Buffer base address 32 LSB bits
    unsigned int        dbau;	    // Buffer base address 32 MSB bits
    unsigned int        res0;	    // reserved
    unsigned int        dbc;	    // Buffer byte count

} hba_cmd_buffer_t;

typedef struct hba_cmd_table_s  // one command = header + one buffer
{

    hba_cmd_header_t   header;      // contains lba value
    hba_cmd_buffer_t   buffer;      // contains buffer address & size

} hba_cmd_table_t;

///////////////////////////////////////////////////////////////////////////////////
// structure for one AHCI command descriptor
///////////////////////////////////////////////////////////////////////////////////

typedef struct hba_cmd_desc_s  // size = 16 bytes
{
    unsigned char       flag[2];    // W in bit 6 of flag[0]
    unsigned char       prdtl[2];	// Number of buffers
    unsigned int        prdbc;		// Number of bytes actually transfered
    unsigned int        ctba;		// Command data base address 32 LSB bits
    unsigned int        ctbau;		// Command data base address 32 MSB bits

} hba_cmd_desc_t;

///////////////////////////////////////////////////////////////////////////////////
//              access functions  
///////////////////////////////////////////////////////////////////////////////////

int reset_hba_init (); 

int reset_hba_read( unsigned int lba, 
                    void*        buffer, 
                    unsigned int count );
#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

