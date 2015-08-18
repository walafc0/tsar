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
 *	 manuel.bouyer@lip6.fr october 2013
 *
 * Maintainers: bouyer
 */

#include <stdint.h>
#include <iostream>
#include <arithmetics.h>
#include <fcntl.h>
#include "vci_spi.h"
#include "vcispi.h"

namespace soclib { namespace caba {

#define tmpl(t) template<typename vci_param> t VciSpi<vci_param>

using namespace soclib::caba;
using namespace soclib::common;

////////////////////////
tmpl(void)::transition()
{

    bool s_dma_bsy = (r_initiator_fsm != M_IDLE);
    if(p_resetn.read() == false) 
    {
	r_initiator_fsm   = M_IDLE;
	r_target_fsm      = T_IDLE;
	r_spi_fsm	  = S_IDLE;
	r_ss	          = 0;
	r_divider	  = 0xffff;
	r_ctrl_char_len   = 0;
	r_ctrl_ie	  = false;
	r_ctrl_cpol       = false;
	r_ctrl_cpha       = false;
	r_spi_bsy         = false;
	r_dma_count       = 0;
	r_dma_error       = false;
	r_spi_clk_counter = 0xffff;
	r_spi_clk	  = 0;
	r_spi_done        = false;

	r_irq		  = false;
	r_read		  = false;

	r_dma_fifo_read.init();
	r_dma_fifo_write.init();

	return;
    } 

    //////////////////////////////////////////////////////////////////////////////
    // The Target FSM controls the following registers:
    // r_target_fsm, r_irq_enable, r_nblocks, r_buf adress, r_lba, r_go, r_read
    //////////////////////////////////////////////////////////////////////////////

    if (r_spi_done)
	r_spi_bsy = false;

    switch(r_target_fsm) {
    ////////////
    case T_IDLE:
    {
	if ( p_vci_target.cmdval.read() ) 
	{ 
	    r_srcid = p_vci_target.srcid.read();
	    r_trdid = p_vci_target.trdid.read();
	    r_pktid = p_vci_target.pktid.read();
	    uint32_t wdata = p_vci_target.wdata.read();
	    sc_dt::sc_uint<vci_param::N> address = p_vci_target.address.read();

	    bool found = false;
	    std::list<soclib::common::Segment>::iterator seg;
	    for ( seg = m_seglist.begin() ; seg != m_seglist.end() ; seg++ ) 
	    {
		if ( seg->contains(address) ) found = true;
	    }
 

	    if (not found) {
		if (p_vci_target.cmd.read() == vci_param::CMD_WRITE)
	    	    r_target_fsm = T_ERROR_WRITE;
		else
	    	    r_target_fsm = T_ERROR_READ;
	    } else if (p_vci_target.cmd.read() != vci_param::CMD_READ &&
		       p_vci_target.cmd.read() != vci_param::CMD_WRITE) {
	    	r_target_fsm = T_ERROR_READ;
	    } else {
		bool     write  = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) & !r_spi_bsy &!s_dma_bsy;
		uint32_t cell   = (uint32_t)((address & 0x3F)>>2);
		switch(cell) {
		case SPI_DATA_TXRX0:
		    r_rdata = r_txrx[0] & (uint64_t)0x00000000ffffffffULL;
		    if (write) {
			r_txrx[0] =
			   (r_txrx[0] & (uint64_t)0xffffffff00000000ULL) |
			   ((uint64_t)wdata);
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		case SPI_DATA_TXRX1:
		    r_rdata = r_txrx[0] >> 32;
		    if (write) {
			r_txrx[0] =
			    (r_txrx[0] & (uint64_t)0x00000000ffffffffULL) |
			    ((uint64_t)wdata << 32);
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		case SPI_DATA_TXRX2:
		    r_rdata = r_txrx[1] & (uint64_t)0x00000000ffffffffULL;
		    if (write) {
			r_txrx[1] =
			   (r_txrx[1] & (uint64_t)0xffffffff00000000ULL) |
			   ((uint64_t)wdata);
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		case SPI_DATA_TXRX3:
		    r_rdata = r_txrx[1] >> 32;
		    if (write) {
		        r_txrx[1] =
			    (r_txrx[1] & (uint64_t)0x00000000ffffffffULL) |
			    ((uint64_t)wdata << 32);
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		case SPI_CTRL:
		{
		    uint32_t data = 0;
		    if (r_ctrl_cpol.read()) 
			data |= SPI_CTRL_CPOL;
		    if (r_ctrl_cpha.read()) 
			data |= SPI_CTRL_CPHA;
		    if (r_ctrl_ie.read()) 
			data |= SPI_CTRL_IE_EN;
		    if (r_spi_bsy.read()) 
			data |= SPI_CTRL_GO_BSY;
		    if (s_dma_bsy) 
			data |= SPI_CTRL_DMA_BSY;
		    if (r_dma_error) 
			data |= SPI_CTRL_DMA_ERR;
		    data |= (uint32_t)r_ctrl_char_len.read();
		    r_rdata = data;
		    if (write) {
			r_ctrl_cpol = ((wdata & SPI_CTRL_CPOL) != 0);
			r_ctrl_cpha = ((wdata & SPI_CTRL_CPHA) != 0);
			r_ctrl_ie  = ((wdata & SPI_CTRL_IE_EN) != 0);
			if (wdata & SPI_CTRL_GO_BSY) 
				r_spi_bsy = true;
			r_ctrl_char_len = (wdata & SPI_CTRL_CHAR_LEN_MASK);
#ifdef SOCLIB_MODULE_DEBUG
			if ((wdata & SPI_CTRL_GO_BSY) != 0) {
			    std::cout << name() << " start xfer " << std::dec << (int)r_ctrl_char_len.read() << " data " << std::hex << r_txrx[1] << " " << r_txrx[0] << std::endl;
			}
#endif
		    } else {
			r_irq = false;
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		}
		case SPI_DIVIDER:
		    r_rdata = r_divider.read();
		    if (write) {
#ifdef SOCLIB_MODULE_DEBUG
		        std::cout << name() << " divider set to " << std::dec << wdata << std::endl;
#endif
			r_divider = wdata;
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		case SPI_SS:
		    r_rdata = r_ss.read();
		    if (write) {
			r_ss = wdata;
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		case SPI_DMA_BASE:
		    r_rdata = r_buf_address.read();
		    if (write) {
			r_buf_address = (r_buf_address & (uint64_t)0xffffffff00000000) | wdata;
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		case SPI_DMA_BASEH:
		    r_rdata = r_buf_address >> 32;
		    if (write) {
			r_buf_address = (r_buf_address & (uint64_t)0x00000000ffffffff) | ((uint64_t)wdata << 32);
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		case SPI_DMA_COUNT:
		    r_rdata = (r_dma_count.read() << m_byte2burst_shift) |
			r_read;
		    if (write) {
			r_read = (wdata & 0x1);
			r_dma_count = wdata >> m_byte2burst_shift;
			r_ctrl_char_len = vci_param::B * 8;
		    }
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_RSP_WRITE : T_RSP_READ;
		    break;
		default:
		    r_target_fsm = (p_vci_target.cmd.read() == vci_param::CMD_WRITE) ? T_ERROR_WRITE : T_ERROR_READ;
		    break;
		}
	    }
	}
	break;
    }
    ////////////////////
    case T_RSP_READ:
    case T_RSP_WRITE:
    case T_ERROR_READ:
    case T_ERROR_WRITE:
	if (p_vci_target.rspack.read() ) {
	    r_target_fsm  = T_IDLE;
	}
	break;
    } // end switch target fsm


	

    //////////////////////////////////////////////////////////////////////////////
    // the SPI FSM controls SPI signals
    //////////////////////////////////////////////////////////////////////////////
    if (r_spi_bsy == false)
	r_spi_done = false;
    switch (r_spi_fsm) {
    case S_IDLE:
	r_spi_clk_counter = r_divider.read();
	r_spi_clk = 0;
	r_spi_clk_previous = r_ctrl_cpha;
	r_spi_clk_ignore = r_ctrl_cpha;
	r_spi_bit_count = r_ctrl_char_len;
	if (r_dma_count != 0) {
		if (r_read.read())
			r_spi_fsm = S_DMA_SEND_START;
		else
			r_spi_fsm = S_DMA_RECEIVE;
	} else if (r_spi_bsy.read() && !r_spi_done.read()) {
	    r_spi_fsm = S_XMIT;
	    r_spi_out = (r_txrx[(r_ctrl_char_len -1)/ 64] >> ((r_ctrl_char_len - 1) % 64)) & (uint64_t)0x0000000000000001ULL;
	}
	break;
    case S_DMA_RECEIVE:
    {
	r_spi_clk_counter = r_divider.read();
	r_spi_clk = 0;
	r_spi_clk_previous = r_ctrl_cpha;
	r_spi_clk_ignore = r_ctrl_cpha;
	r_spi_bit_count = r_ctrl_char_len;
	if (r_initiator_fsm != M_WRITE_RSP || !p_vci_initiator.rspval.read()) {
	    if (r_dma_fifo_write.rok()) {
	        typename vci_param::data_t v = r_dma_fifo_write.read();
	        r_dma_fifo_write.simple_get();
	        r_txrx[0] = v;
	        r_spi_out = (v >> ((vci_param::B * 8) - 1)) & 0x1;
	        r_spi_fsm = S_XMIT;
	    } else if (r_initiator_fsm == M_WRITE_END) {
	        r_spi_fsm = S_IDLE;
	    }
	}
	break;
    }
    case S_DMA_SEND_START:
	r_spi_word_count = (r_dma_count << (m_byte2burst_shift - 2)) - 1;
	r_spi_out = 1;
	r_txrx[0] = 0xffffffff;
	r_spi_fsm = S_XMIT;
	break;
    case S_DMA_SEND:
	r_spi_out = 1;
	r_spi_clk_counter = r_divider.read();
	r_spi_clk = 0;
	r_spi_clk_previous = r_ctrl_cpha;
	r_spi_clk_ignore = r_ctrl_cpha;
	r_spi_bit_count = r_ctrl_char_len;
	if (r_initiator_fsm != M_READ_CMD) {
	    if (r_dma_fifo_read.wok()) {
	        r_dma_fifo_read.simple_put(
		    (typename vci_param::data_t)r_txrx[0]);
	        r_spi_word_count = r_spi_word_count - 1;
		r_txrx[0] = 0xffffffff;
	        if ( r_spi_word_count == 0 ) {
		    r_spi_fsm = S_DMA_SEND_END;
	        } else {
		    r_spi_fsm = S_XMIT;
	        }
	    }
	}
	break;
    case S_DMA_SEND_END:
	if (r_initiator_fsm == M_IDLE)
		r_spi_fsm = S_IDLE;
	break;
    case S_XMIT:
      {
	bool s_clk_sample;
	// on clock transition, sample input line, and shift data
	s_clk_sample = r_spi_clk ^ r_ctrl_cpha;
	if (!r_spi_clk_ignore) {
	    if (r_spi_clk_previous == 0 && s_clk_sample == 1) {
		// low to high transition: shift and sample
		r_txrx[1] = (r_txrx[1] << 1) | (r_txrx[0] >> 63);
		r_txrx[0] = (r_txrx[0] << 1) | p_spi_miso;
		r_spi_bit_count = r_spi_bit_count - 1;
	    } else if (r_spi_clk_previous == 1 && s_clk_sample == 0) {
		// high to low transition: change output, or stop
		if (r_spi_bit_count == 0) {
		    if (r_initiator_fsm != M_IDLE) {
			if (r_read)
			    r_spi_fsm = S_DMA_SEND;
			else
			    r_spi_fsm = S_DMA_RECEIVE;
		    } else {
		        r_spi_fsm = S_IDLE;
		        r_irq = r_ctrl_ie;
		        r_spi_done = true;
		    }
#ifdef SOCLIB_MODULE_DEBUG0
		    std::cout << name() << " end xfer " << std::dec << (int)r_ctrl_char_len.read() << " data " << std::hex << r_txrx[1] << " " << r_txrx[0] << std::endl;
#endif
		} else {
		    r_spi_out = (r_txrx[(r_ctrl_char_len -1)/ 64] >> ((r_ctrl_char_len - 1) % 64)) & (uint64_t)0x0000000000000001ULL;
		}
	    }
	}
	r_spi_clk_previous = s_clk_sample;
	// generate the SPI clock
	if (r_spi_clk_counter.read() == 0) {
	    r_spi_clk_counter = r_divider.read();
	    r_spi_clk = !r_spi_clk.read();
	    r_spi_clk_ignore = false;
	} else {
	    r_spi_clk_counter = r_spi_clk_counter.read() - 1;
	}
	break;
      }
    }
    //////////////////////////////////////////////////////////////////////////////
    // The initiator FSM executes a loop, transfering one burst per iteration.
    // data comes from or goes to fifos, the other end of the fifos is
    // feed by or eaten by the SPI fsm.
    //////////////////////////////////////////////////////////////////////////////

    switch( r_initiator_fsm.read() ) {
    ////////////
    case M_IDLE: 	// check buffer alignment to compute the number of bursts
    {
	if ( r_dma_count != 0 )
	{
	    // start transfer
	    if ( r_read.read() )    r_initiator_fsm = M_READ_WAIT;
	    else		    r_initiator_fsm = M_WRITE_WAIT;
	}
	break;
    }
    case M_READ_WAIT:  // wait for the FIFO to be full
	if (!r_dma_fifo_read.wok()) {
		r_burst_word = m_words_per_burst - 1;
		r_initiator_fsm = M_READ_CMD;
	}
	break;
    ////////////////
    case M_READ_CMD:	// Send a multi-flits VCI WRITE command
    {
	if ( p_vci_initiator.cmdack.read() )
	{
	    if ( r_burst_word == 0 )      // last flit
	    {
		r_initiator_fsm = M_READ_RSP;
	    }
	    else		    // not the last flit
	    {
		r_burst_word = r_burst_word.read() - 1;
	    }

	    r_dma_fifo_read.simple_get(); // consume one fifo word
	    // compute next word address
	    r_buf_address = r_buf_address.read() + vci_param::B;
	}
	break;
    }
    ////////////////
    case M_READ_RSP: 	// Wait a single flit VCI WRITE response
    {
	if ( p_vci_initiator.rspval.read() )
	{
	    if ( (p_vci_initiator.rerror.read()&0x1) != 0 ) 
	    {
	        r_burst_word = 0;
		r_dma_count = 0;
		r_dma_error = true;
	        r_initiator_fsm = M_INTR;
#ifdef SOCLIB_MODULE_DEBUG
		std::cout << "vci_bd M_READ_ERROR" << std::endl;
#endif
	    }
	    else if ( r_spi_fsm == S_DMA_SEND_END ) // last burst
	    {
		r_dma_count = 0;
		r_initiator_fsm = M_INTR;
		r_dma_error = false;
#ifdef SOCLIB_MODULE_DEBUG
		std::cout << "vci_bd M_READ_SUCCESS" << std::endl;
#endif
	    }
	    else // keep on reading
	    {
		r_dma_count = r_dma_count - 1;
		r_initiator_fsm  = M_READ_WAIT;
	    }
	}
	break;
    }
    ///////////////////
    case M_INTR: 
	r_initiator_fsm = M_IDLE;
	r_irq = true;
	break;
    ///////////////////
    case M_WRITE_WAIT:  // wait for the FIFO to be empty
	if (!r_dma_fifo_write.rok()) {
	    r_burst_word = m_words_per_burst - 1;
	    r_dma_count = r_dma_count - 1;
	    r_initiator_fsm = M_WRITE_CMD;
	}
	break;
    /////////////////
    case M_WRITE_CMD:	// This is actually a single flit VCI READ command
    {
	if ( p_vci_initiator.cmdack.read() ) r_initiator_fsm = M_WRITE_RSP;
	break;
    }
    /////////////////
    case M_WRITE_RSP:	// This is actually a multi-words VCI READ response
    {
	if ( p_vci_initiator.rspval.read() )
	{
	    typename vci_param::data_t v = p_vci_initiator.rdata.read();
	    typename vci_param::data_t f = 0;
	    // byte-swap
	    for (int i = 0; i < (vci_param::B * 8); i += 8) {
		f |= ((v >> i) & 0xff) << ((vci_param::B * 8) - 8 - i);
	    }
	    r_dma_fifo_write.simple_put(f);
	    r_burst_word = r_burst_word.read() - 1;
	    if ( p_vci_initiator.reop.read() )  // last flit of the burst
	    {
		r_buf_address = r_buf_address.read() + m_burst_size;

		if( (p_vci_initiator.rerror.read()&0x1) != 0 ) 
		{
		    r_dma_count = 0;
		    r_dma_error = 1;
		    r_initiator_fsm = M_WRITE_END;
#ifdef SOCLIB_MODULE_DEBUG
		    std::cout << "vci_bd M_WRITE_ERROR" << std::endl;
#endif
		}
		else if ( r_dma_count.read() == 0) // last burst
		{
		    r_dma_error = 0;
		    r_initiator_fsm  = M_WRITE_END;
		}
		else					  // not the last burst
		{
		    r_initiator_fsm = M_WRITE_WAIT;
		}
	    }
	}
	break;
    }
    /////////////////
    case M_WRITE_END:	// wait for the write to be complete
    {
	if (r_spi_fsm == S_IDLE) { // write complete
	    r_initiator_fsm  = M_INTR;
	}
	break;
    }
  } // end switch r_initiator_fsm
}  // end transition

//////////////////////
tmpl(void)::genMoore()
{
    // p_vci_target port   
    p_vci_target.rsrcid = (sc_dt::sc_uint<vci_param::S>)r_srcid.read();
    p_vci_target.rtrdid = (sc_dt::sc_uint<vci_param::T>)r_trdid.read();
    p_vci_target.rpktid = (sc_dt::sc_uint<vci_param::P>)r_pktid.read();
    p_vci_target.reop   = true;

    switch(r_target_fsm) {
    case T_IDLE:
	p_vci_target.cmdack = true;
	p_vci_target.rspval = false;
	p_vci_target.rdata  = 0;
	break;
    case T_RSP_READ:
	p_vci_target.cmdack = false;
	p_vci_target.rspval = true;
	p_vci_target.rdata = r_rdata;
	p_vci_target.rerror = VCI_READ_OK;
	break;
    case T_RSP_WRITE:
	p_vci_target.cmdack = false;
	p_vci_target.rspval = true;
	p_vci_target.rdata  = 0;
	p_vci_target.rerror = VCI_WRITE_OK;
	break;
    case T_ERROR_READ:
	p_vci_target.cmdack = false;
	p_vci_target.rspval = true;
	p_vci_target.rdata  = 0;
	p_vci_target.rerror = VCI_READ_ERROR;
	break;
    case T_ERROR_WRITE:
	p_vci_target.cmdack = false;
	p_vci_target.rspval = true;
	p_vci_target.rdata  = 0;
	p_vci_target.rerror = VCI_WRITE_ERROR;
	break;
    } // end switch target fsm

    // p_vci_initiator port
    p_vci_initiator.srcid  = (sc_dt::sc_uint<vci_param::S>)m_srcid;
    p_vci_initiator.trdid  = 0;
    p_vci_initiator.contig = true;
    p_vci_initiator.cons   = false;
    p_vci_initiator.wrap   = false;
    p_vci_initiator.cfixed = false;
    p_vci_initiator.clen   = 0;

    switch (r_initiator_fsm) {
    case M_WRITE_CMD:		// It is actually a single flit VCI read command
	p_vci_initiator.rspack  = false;
	p_vci_initiator.cmdval  = true;
	p_vci_initiator.address = (sc_dt::sc_uint<vci_param::N>)r_buf_address.read();
	p_vci_initiator.cmd     = vci_param::CMD_READ;
	p_vci_initiator.pktid   = TYPE_READ_DATA_UNC; 
	p_vci_initiator.wdata   = 0;
	p_vci_initiator.be      = 0;
	p_vci_initiator.plen    = (sc_dt::sc_uint<vci_param::K>)(m_burst_size);
	p_vci_initiator.eop     = true;
	break;
    case M_READ_CMD:		// It is actually a multi-words VCI WRITE command 
    {
	typename vci_param::data_t v = 0;
	typename vci_param::data_t f;
	p_vci_initiator.rspack  = false;
	p_vci_initiator.cmdval  = true;
	p_vci_initiator.address = (sc_dt::sc_uint<vci_param::N>)r_buf_address.read();
	p_vci_initiator.cmd     = vci_param::CMD_WRITE;
	p_vci_initiator.pktid   = TYPE_WRITE;
	p_vci_initiator.plen    = (sc_dt::sc_uint<vci_param::K>)(m_burst_size);
	f = r_dma_fifo_read.read();
	// byte-swap
	for (int i = 0; i < (vci_param::B * 8); i += 8) {
		v |= ((f >> i) & 0xff) << ((vci_param::B * 8) - 8 - i);
	}
	p_vci_initiator.wdata = v;
	p_vci_initiator.eop   = ( r_burst_word.read() == 0);
	if (vci_param::B == 8)
	{
	    p_vci_initiator.be    = 0xFF;
	}
	else
	{
	    p_vci_initiator.be    = 0xF;
	}
	break;
    }
    case M_READ_RSP:
    case M_WRITE_RSP:
	p_vci_initiator.rspack  = true;
	p_vci_initiator.cmdval  = false;
	break;
    default:
	p_vci_initiator.rspack  = false;
	p_vci_initiator.cmdval  = false;
	break;
    }

    // SPI signals
    p_spi_ss = ((r_ss & 0x1) == 0);
    switch(r_spi_fsm) {
    default:
	p_spi_mosi = r_spi_out;
	p_spi_clk = 0;
	break;
    case S_XMIT:
      {
	bool s_clk_sample = r_spi_clk ^ r_ctrl_cpha;
	p_spi_clk = r_spi_clk ^ r_ctrl_cpol;
	if (s_clk_sample == 0) {
	    // clock low: get data directly from shift register
	    // as r_spi_out may be delayed by one clock cycle
	    p_spi_mosi = (r_txrx[(r_ctrl_char_len -1)/ 64] >> ((r_ctrl_char_len - 1) % 64)) & (uint64_t)0x0000000000000001ULL;
	} else {
	    // clock high: get data from saved value, as the shift register
	    // may have changed
	    p_spi_mosi = r_spi_out;
	}
	break;
      }
    }

    // IRQ signal
    p_irq = r_irq;
} // end GenMoore()

//////////////////////////////////////////////////////////////////////////////
tmpl(/**/)::VciSpi( sc_core::sc_module_name	      name, 
				const soclib::common::MappingTable   &mt,
				const soclib::common::IntTab	 &srcid,
				const soclib::common::IntTab	 &tgtid,
				const uint32_t		       burst_size)

: caba::BaseModule(name),
	m_seglist(mt.getSegmentList(tgtid)),
	m_srcid(mt.indexForId(srcid)),
	m_burst_size(burst_size),
	m_words_per_burst(burst_size / vci_param::B),
	m_byte2burst_shift(soclib::common::uint32_log2(burst_size)),
	p_clk("p_clk"),
	p_resetn("p_resetn"),
	p_vci_initiator("p_vci_initiator"),
	p_vci_target("p_vci_target"),
	p_irq("p_irq"),
	p_spi_ss("p_spi_ss"),
	p_spi_clk("p_spi_clk"),
	p_spi_mosi("p_spi_mosi"),
	p_spi_miso("p_spi_miso"),

	r_dma_fifo_read("r_dma_fifo_read", burst_size / vci_param::B), // one cache line
	r_dma_fifo_write("r_dma_fifo_read", burst_size / vci_param::B) // one cache line
{
    std::cout << "  - Building VciSpi " << name << std::endl;

	SC_METHOD(transition);
    dont_initialize();
    sensitive << p_clk.pos();

	SC_METHOD(genMoore);
    dont_initialize();
    sensitive << p_clk.neg();

    size_t nbsegs = 0;
    std::list<soclib::common::Segment>::iterator seg;
    for ( seg = m_seglist.begin() ; seg != m_seglist.end() ; seg++ ) 
    {
	nbsegs++;
	
	    if ( (seg->baseAddress() & 0x0000003F) != 0 ) 
	    {
		    std::cout << "Error in component VciSpi : " << name 
			      << "The base address of segment " << seg->name()
		      << " must be multiple of 64 bytes" << std::endl;
		    exit(1);
	    }
	    if ( seg->size() < 64 ) 
	    {
		    std::cout << "Error in component VciSpi : " << name 
			  << "The size of segment " << seg->name()
		      << " cannot be smaller than 64 bytes" << std::endl;
		    exit(1);
	    }
	std::cout << "    => segment " << seg->name()
		  << " / base = " << std::hex << seg->baseAddress()
		  << " / size = " << seg->size() << std::endl; 
    }

    if( nbsegs == 0 )
    {
		std::cout << "Error in component VciSpi : " << name
			  << " No segment allocated" << std::endl;
		exit(1);
    }

    if( (burst_size != 8 ) && 
		(burst_size != 16) && 
		(burst_size != 32) && 
		(burst_size != 64) )
	{
		std::cout << "Error in component VciSpi : " << name 
			  << " The burst size must be 8, 16, 32 or 64 bytes" << std::endl;
		exit(1);
	}

	if ( (vci_param::B != 4) and (vci_param::B != 8) )
	{
		std::cout << "Error in component VciSpi : " << name	      
			  << " The VCI data fields must have 32 bits or 64 bits" << std::endl;
		exit(1);
	}

} // end constructor

tmpl(/**/)::~VciSpi()
{
}


//////////////////////////
tmpl(void)::print_trace()
{
	const char* initiator_str[] = 
    {
		"M_IDLE",

		"M_READ_WAIT",
		"M_READ_CMD",
		"M_READ_RSP",
		"M_INTR",

		"M_WRITE_WAIT",
		"M_WRITE_CMD",
		"M_WRITE_RSP",
		"M_WRITE_END",
	};
	const char* target_str[] = 
        {
		"T_IDLE",
		"T_RSP_READ",
		"T_RSP_WRITE",
		"T_ERROR_READ",
		"T_ERROR_WRITE",
	};
	const char* spi_str[] = 
        {
		"S_IDLE",
		"S_DMA_RECEIVE",
		"S_DMA_SEND_START",
		"S_DMA_SEND",
		"S_DMA_SEND_END",
		"S_XMIT",
	};

	std::cout << name() << " _TGT : " << target_str[r_target_fsm.read()] 
	    << std::endl;
	std::cout << name() << " _SPI : " << spi_str[r_spi_fsm.read()] 
	    << " clk_counter " << r_spi_clk_counter.read()
	    << " r_spi_bit_count " << r_spi_bit_count.read() 
	    << " r_spi_bsy " << (int)r_spi_bsy.read() << std::endl;
	std::cout << name() << " _SPI : "
	    << " r_spi_clk " << r_spi_clk.read()
	    << " cpol " << r_ctrl_cpol.read()
	    << " cpha " << r_ctrl_cpha.read()
	    << " r_spi_clk_ignore " << r_spi_clk_ignore.read()
	    << " r_txrx 0x" << std::hex
	    << r_txrx[1].read() << " " << r_txrx[0].read()

	    << std::endl;
	std::cout << name() << "  _INI : " << initiator_str[r_initiator_fsm.read()] 
	  << "  buf = " << std::hex << r_buf_address.read()
	  << "  burst = " << r_burst_word.read() 
	  << "  count = " << r_dma_count.read() 
	  << "  spi_count = " << r_spi_word_count.read() 
	  <<std::endl; 
}

}} // end namespace

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

