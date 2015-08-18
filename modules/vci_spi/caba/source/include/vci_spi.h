
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
 * Copyright (c) UPMC, Lip6, SoC
 *         manuel.bouyer@lip6.fr october 2013
 *
 * Maintainers: bouyer
 */

//////////////////////////////////////////////////////////////////////////////////////
// This component is a SPI controller with a VCI interface
// It supports only 32 or 64 bits VCI DATA width, but all addressable registers
// contain 32 bits words. It supports VCI addresss lartger than 32 bits.
//
// This component can perform data transfers between one single file belonging 
// to the host system and a buffer in the memory of the virtual prototype.
// The file name is an argument of the constructor.
// This component has a DMA capability, and is both a target and an initiator.
// The burst size (bytes) must be power of 2.
// The burst size is typically a cache line. 
// The memory buffer must be aligned to a a burst boundary.
// Both read and write transfers are supported. An IRQ is optionally
// asserted when the transfer is completed. 
//

#ifndef SOCLIB_VCI_SPI_H
#define SOCLIB_VCI_SPI_H

#include <stdint.h>
#include <systemc>
#include <unistd.h>
#include "caba_base_module.h"
#include "mapping_table.h"
#include "vci_initiator.h"
#include "vci_target.h"
#include "generic_fifo.h"

namespace soclib {
namespace caba {

using namespace sc_core;

template<typename vci_param>
class VciSpi
	: public caba::BaseModule
{
private:

    // Registers
    sc_signal<int>            	       r_target_fsm;   	   // target fsm state register
    sc_signal<int>                     r_initiator_fsm;    // initiator fsm state register
    sc_signal<int>		       r_spi_fsm;	   // spi engine state
    sc_signal<uint64_t>                r_txrx[2];      	   // data in/out
    sc_signal<uint32_t>                r_divider;      	   // SPI clk divider
    sc_signal<uint8_t>                 r_ss;      	   // SPI slave select
    sc_signal<bool>                    r_ctrl_cpol;	// clock polarity
    sc_signal<bool>                    r_ctrl_cpha;	// clock phase
    sc_signal<bool>                    r_ctrl_ie;	// interrupt enable
    sc_signal<uint8_t>                 r_ctrl_char_len; // number of bits in xfer
    sc_signal<uint64_t>                r_buf_address;  // memory buffer address 
    sc_signal<uint32_t>		       r_dma_count;   // DMA burst count
    sc_signal<bool>		       r_read;	      // DMA read/write

    sc_signal<uint32_t>		       r_burst_word;  // DMA burst word count
    sc_signal<bool>                    r_dma_error;   // DMA error

    sc_signal<bool>                    r_spi_bsy;    // SPI shifter busy
    sc_signal<uint32_t>		       r_spi_bit_count;
    sc_signal<uint32_t>	               r_spi_word_count;
    sc_signal<uint32_t>		       r_spi_clk_counter;
    sc_signal<bool>		       r_spi_clk;
    sc_signal<bool>		       r_spi_clk_previous;
    sc_signal<bool>		       r_spi_clk_ignore;
    sc_signal<bool>		       r_spi_out;
    sc_signal<bool>		       r_spi_done;
    sc_signal<bool>		       r_irq;

    GenericFifo<typename vci_param::data_t> r_dma_fifo_read; // buffer data from SPI to network
    GenericFifo<typename vci_param::data_t> r_dma_fifo_write;// buffer data from network to SPI

    sc_signal<typename vci_param::srcid_t >	r_srcid;   // save srcid
    sc_signal<typename vci_param::trdid_t >	r_trdid;   // save trdid
    sc_signal<typename vci_param::pktid_t >	r_pktid;   // save pktid

    sc_signal<typename vci_param::data_t >	r_rdata;   // save reply

    // structural parameters
    std::list<soclib::common::Segment> m_seglist;
    uint32_t                           m_srcid;        	   // initiator index
    const uint32_t                     m_burst_size;       // number of words in a burst
    const uint32_t                     m_words_per_burst;  // number of words in a burst
    const uint32_t                     m_byte2burst_shift; // log2(burst_size)

    // methods
    void transition();
    void genMoore();

    //  Master FSM states
    enum {
    M_IDLE              = 0,
    M_READ_WAIT         = 1,
    M_READ_CMD          = 2,
    M_READ_RSP          = 3,
    M_INTR              = 4,
    M_WRITE_WAIT        = 5,
    M_WRITE_CMD         = 6,
    M_WRITE_RSP         = 7,
    M_WRITE_END         = 8
    };

    // Target FSM states
    enum {
    T_IDLE              = 0,
    T_RSP_READ		= 1,
    T_RSP_WRITE		= 2,
    T_ERROR_READ	= 3,
    T_ERROR_WRITE	= 4
    };

    // SPI FSM states
    enum {
    S_IDLE		= 0,
    S_DMA_RECEIVE	= 1,
    S_DMA_SEND_START	= 2,
    S_DMA_SEND		= 3,
    S_DMA_SEND_END	= 4,
    S_XMIT		= 5,
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

    SC_HAS_PROCESS(VciSpi);

public:

    // ports
    sc_in<bool> 					      p_clk;
    sc_in<bool> 					      p_resetn;
    sc_out<bool> 					      p_irq;
    soclib::caba::VciInitiator<vci_param> p_vci_initiator;
    soclib::caba::VciTarget<vci_param>    p_vci_target;
    sc_out<bool> 					      p_spi_ss;
    sc_out<bool> 					      p_spi_clk;
    sc_out<bool> 					      p_spi_mosi;
    sc_in<bool> 					      p_spi_miso;

    void print_trace();

    // Constructor   
    VciSpi(
	sc_module_name                      name,
	const soclib::common::MappingTable  &mt,
	const soclib::common::IntTab 	    &srcid,
	const soclib::common::IntTab 	    &tgtid,
        const uint32_t 	                    burst_size = 64);

    ~VciSpi();

};

}}

#endif /* SOCLIB_VCI_SPI_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

