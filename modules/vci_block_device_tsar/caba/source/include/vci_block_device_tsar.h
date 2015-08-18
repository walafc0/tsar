
/* -*- c++ -*-
 *
 * SOCLIB_LGPL_HEADER_BEGIN
 * 
 * This file is part of SoCLib, GNU LGPLv2.1.
 * 
 * SoCLib is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 of the License.
 * 
 * SoCLib is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with SoCLib; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * SOCLIB_LGPL_HEADER_END
 *
 * Copyright (c) UPMC, Lip6, Asim
 *         alain.greiner@lip6.fr april 2011
 *
 * Maintainers: alain
 */

//////////////////////////////////////////////////////////////////////////////////////
// This component is a simplified disk controller with a VCI interface.
// It supports only 32 or 64 bits VCI DATA width, but all addressable registers
// contain 32 bits words. It supports VCI addresss lartger than 32 bits.
//
// This component can perform data transfers between one single file (belonging 
// to the host system) and a buffer in the memory of the virtual prototype.
// The name of the file containing the disk image is a constructor argument.
// This component has a DMA capability, and is both a target and an initiator.
// The block size (bytes), and the burst size (bytes) must be power of 2.
// The burst size is typically a cache line. 
// The memory buffer is not constrained to be aligned on a burst boundary,
// but must be aligned on a 32 bits word boundary. 
// Both read and write transfers are supported. An IRQ is optionally
// asserted when the transfer is completed. 
//
// As a target this block device controler contains 9 32 bits memory mapped registers,
// taking 36 bytes in the address space.
// - BLOCK_DEVICE_BUFFER        0x00 (read/write)  Memory buffer base address (LSB bits)
// - BLOCK_DEVICE_COUNT         0x04 (read/write)  Number of blocks to be transfered.
// - BLOCK_DEVICE_LBA           0x08 (read/write)  Index of first block in the file.
// - BLOCK_DEVICE_OP            0x0C (write-only)  Writing here starts the operation.
// - BLOCK_DEVICE_STATUS        0x10 (read-only)   Block Device status.
// - BLOCK_DEVICE_IRQ_ENABLE    0x14 (read/write)  IRQ enabled if non zero.
// - BLOCK_DEVICE_SIZE          0x18 (read-only)   Number of addressable blocks.
// - BLOCK_DEVICE_BLOCK_SIZE    0x1C (read_only)   Block size in bytes.
// - BLOCK_DEVICE_BUFFER_EXT    0x20 (read/write)  Memory buffer base address (MSB bits)
//
// The following operations codes are supported: 
// - BLOCK_DEVICE_NOOP          No operation
// - BLOCK_DEVICE_READ          From block device to memory
// - BLOCK_DEVICE_WRITE         From memory to block device 
//
// The BLOCK_DEVICE_STATUS is actually defined by the initiator FSM state.
// The following values are defined for device status:
// -BLOCK_DEVICE_IDLE           0
// -BLOCK_DEVICE_BUSY           1
// -BLOCK_DEVICE_READ_SUCCESS   2
// -BLOCK_DEVICE_WRITE_SUCCESS  3
// -BLOCK_DEVICE_READ_ERROR     4
// -BLOCK_DEVICE_WRITE_ERROR    5
//
// In the 4 states READ_ERROR, READ_SUCCESS, WRITE_ERROR, WRITE_SUCCESS,
// the IRQ is asserted (if it is enabled).
// A read access to the BLOCK_DEVICE_STATUS in these 4 states reset 
// the initiator FSM state to IDLE, and acknowledge the IRQ.
// Any write access to registers BUFFER, COUNT, LBA, OP is ignored
// if the device is not IDLE.
///////////////////////////////////////////////////////////////////////////

#ifndef SOCLIB_VCI_BLOCK_DEVICE_TSAR_H
#define SOCLIB_VCI_BLOCK_DEVICE_TSAR_H

#include <stdint.h>
#include <systemc>
#include <unistd.h>
#include "caba_base_module.h"
#include "mapping_table.h"
#include "vci_initiator.h"
#include "vci_target.h"

namespace soclib {
namespace caba {

using namespace sc_core;

template<typename vci_param>
class VciBlockDeviceTsar
	: public caba::BaseModule
{
private:

    // Registers
    sc_signal<int>            	       r_target_fsm;   	   // target fsm state register
    sc_signal<int>                     r_initiator_fsm;    // initiator fsm state register
    sc_signal<bool>                    r_irq_enable;   	   // default value is true
    sc_signal<uint32_t>                r_nblocks;      	   // number of blocks in transfer
    sc_signal<uint64_t>                r_buf_address;  	   // memory buffer address 
    sc_signal<uint32_t>                r_lba;          	   // first block index
    sc_signal<bool>                    r_read;         	   // requested operation
    sc_signal<uint32_t>                r_index;        	   // word index in local buffer
    sc_signal<uint32_t>                r_latency_count;    // latency counter 
    sc_signal<uint32_t>                r_words_count;      // word counter (in a burst)
    sc_signal<uint32_t>                r_burst_count;      // burst counter (in a block)
    sc_signal<uint32_t>                r_block_count;  	   // block counter (in a transfer)
    sc_signal<uint32_t>                r_burst_offset;     // number of non aligned words
    sc_signal<uint32_t>                r_burst_nwords;     // number of words in a burst
    sc_signal<bool>                    r_go;           	   // command from T_FSM to M_FSM

    sc_signal<typename vci_param::srcid_t >	r_srcid;   // save srcid
    sc_signal<typename vci_param::trdid_t >	r_trdid;   // save trdid
    sc_signal<typename vci_param::pktid_t >	r_pktid;   // save pktid
    sc_signal<typename vci_param::data_t >	r_tdata;   // save wdata

    uint32_t*                          r_local_buffer; 	   // capacity is one block 

    // structural parameters
    std::list<soclib::common::Segment> m_seglist;
    uint32_t                           m_srcid;        	   // initiator index
    int                                m_fd;           	   // File descriptor
    uint64_t                           m_device_size;  	   // Total number of blocks
    const uint32_t                     m_words_per_block;  // number of words in a block
    const uint32_t                     m_words_per_burst;  // number of words in a burst
    const uint32_t                     m_bursts_per_block; // number of bursts in a block
    const uint32_t                     m_latency;      	   // device latency

    // methods
    void transition();
    void genMoore();

    //  Master FSM states
    enum {
    M_IDLE              = 0,

    M_READ_BLOCK        = 1,
    M_READ_BURST        = 2,
    M_READ_CMD          = 3,
    M_READ_RSP          = 4,
    M_READ_SUCCESS      = 5,
    M_READ_ERROR        = 6,

    M_WRITE_BURST       = 7,
    M_WRITE_CMD         = 8,
    M_WRITE_RSP         = 9,
    M_WRITE_BLOCK       = 10,
    M_WRITE_SUCCESS     = 11,
    M_WRITE_ERROR       = 12,
    };

    // Target FSM states
    enum {
    T_IDLE              = 0,
    T_WRITE_BUFFER      = 1,
    T_READ_BUFFER       = 2,
    T_WRITE_BUFFER_EXT  = 3,
    T_READ_BUFFER_EXT   = 4,
    T_WRITE_COUNT       = 5,
    T_READ_COUNT        = 6,
    T_WRITE_LBA         = 7,
    T_READ_LBA          = 8,
    T_WRITE_OP          = 9,
    T_READ_STATUS       = 10,
    T_WRITE_IRQEN       = 11,
    T_READ_IRQEN        = 12,
    T_READ_SIZE         = 13,
    T_READ_BLOCK        = 14,
    T_READ_ERROR        = 15,
    T_WRITE_ERROR       = 16,
    };

    // Error codes values
    enum {
    VCI_READ_OK		= 0,
    VCI_READ_ERROR	= 1,
    VCI_WRITE_OK	= 2,
    VCI_WRITE_ERROR	= 3,
    };

    /* transaction type, pktid field */
    enum transaction_type_e
    {
      // b3 unused
      // b2 READ / NOT READ
      // Si READ
      //  b1 DATA / INS
      //  b0 UNC / MISS
      // Si NOT READ
      //  b1 accès table llsc type SW / other
      //  b2 WRITE/CAS/LL/SC
      TYPE_READ_DATA_UNC          = 0x0,
      TYPE_READ_DATA_MISS         = 0x1,
      TYPE_READ_INS_UNC           = 0x2,
      TYPE_READ_INS_MISS          = 0x3,
      TYPE_WRITE                  = 0x4,
      TYPE_CAS                    = 0x5,
      TYPE_LL                     = 0x6,
      TYPE_SC                     = 0x7
    };

protected:

    SC_HAS_PROCESS(VciBlockDeviceTsar);

public:

    // ports
    sc_in<bool> 					      p_clk;
    sc_in<bool> 					      p_resetn;
    soclib::caba::VciInitiator<vci_param> p_vci_initiator;
    soclib::caba::VciTarget<vci_param>    p_vci_target;
    sc_out<bool> 					      p_irq;

    void print_trace();

    // Constructor   
    VciBlockDeviceTsar(
		sc_module_name                      name,
		const soclib::common::MappingTable 	&mt,
		const soclib::common::IntTab 		&srcid,
		const soclib::common::IntTab 		&tgtid,
        const std::string                   &filename,
        const uint32_t 	                    block_size = 512,
        const uint32_t 	                    burst_size = 64,
        const uint32_t	                    latency = 0);

    ~VciBlockDeviceTsar();

};

}}

#endif /* SOCLIB_VCI_BLOCK_DEVICE_TSAR_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

