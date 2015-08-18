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

#include <stdint.h>
#include <iostream>
#include <fcntl.h>
#include "vci_block_device_tsar.h"
#include "block_device_tsar.h"

namespace soclib { namespace caba {

#define tmpl(t) template<typename vci_param> t VciBlockDeviceTsar<vci_param>

using namespace soclib::caba;
using namespace soclib::common;

////////////////////////
tmpl(void)::transition()
{
    if(p_resetn.read() == false)
    {
        r_initiator_fsm   = M_IDLE;
        r_target_fsm      = T_IDLE;
        r_irq_enable      = true;
        r_go              = false;
        return;
    }

    //////////////////////////////////////////////////////////////////////////////
    // The Target FSM controls the following registers:
    // r_target_fsm, r_irq_enable, r_nblocks, r_buf adress, r_lba, r_go, r_read
    //////////////////////////////////////////////////////////////////////////////

    switch(r_target_fsm) {
    ////////////
    case T_IDLE:
    {
        if ( p_vci_target.cmdval.read() )
        {
            r_srcid = p_vci_target.srcid.read();
            r_trdid = p_vci_target.trdid.read();
            r_pktid = p_vci_target.pktid.read();
            sc_dt::sc_uint<vci_param::N> address = p_vci_target.address.read();

            bool found = false;
            std::list<soclib::common::Segment>::iterator seg;
            for ( seg = m_seglist.begin() ; seg != m_seglist.end() ; seg++ )
            {
                if ( seg->contains(address) ) found = true;
            }

            bool     read = (p_vci_target.cmd.read() == vci_param::CMD_READ);
            uint32_t cell = (uint32_t)((address & 0x3F)>>2);
            bool     pending = (r_initiator_fsm.read() != M_IDLE);

            if     ( !read && not found )                         r_target_fsm = T_WRITE_ERROR;
            else if(  read && not found )                         r_target_fsm = T_READ_ERROR;
            else if( !read && not p_vci_target.eop.read() )       r_target_fsm = T_WRITE_ERROR;
            else if(  read && not p_vci_target.eop.read() )       r_target_fsm = T_READ_ERROR;
            else if( !read && pending )                           r_target_fsm = T_WRITE_ERROR;
            else if( !read && (cell == BLOCK_DEVICE_BUFFER) )     r_target_fsm = T_WRITE_BUFFER;
            else if(  read && (cell == BLOCK_DEVICE_BUFFER) )     r_target_fsm = T_READ_BUFFER;
            else if( !read && (cell == BLOCK_DEVICE_BUFFER_EXT) ) r_target_fsm = T_WRITE_BUFFER_EXT;
            else if(  read && (cell == BLOCK_DEVICE_BUFFER_EXT) ) r_target_fsm = T_READ_BUFFER_EXT;
            else if( !read && (cell == BLOCK_DEVICE_COUNT) )      r_target_fsm = T_WRITE_COUNT;
            else if(  read && (cell == BLOCK_DEVICE_COUNT) )      r_target_fsm = T_READ_COUNT;
            else if( !read && (cell == BLOCK_DEVICE_LBA) )        r_target_fsm = T_WRITE_LBA;
            else if(  read && (cell == BLOCK_DEVICE_LBA) )        r_target_fsm = T_READ_LBA;
            else if( !read && (cell == BLOCK_DEVICE_OP) )         r_target_fsm = T_WRITE_OP;
            else if(  read && (cell == BLOCK_DEVICE_STATUS) )     r_target_fsm = T_READ_STATUS;
            else if( !read && (cell == BLOCK_DEVICE_IRQ_ENABLE) ) r_target_fsm = T_WRITE_IRQEN;
            else if(  read && (cell == BLOCK_DEVICE_IRQ_ENABLE) ) r_target_fsm = T_READ_IRQEN;
            else if(  read && (cell == BLOCK_DEVICE_SIZE) )       r_target_fsm = T_READ_SIZE;
            else if(  read && (cell == BLOCK_DEVICE_BLOCK_SIZE) ) r_target_fsm = T_READ_BLOCK;

            // get write data value for both 32 bits and 64 bits data width
            if( (vci_param::B == 8) and (p_vci_target.be.read() == 0xF0) )
                r_tdata = (uint32_t)(p_vci_target.wdata.read()>>32);
            else
                r_tdata = p_vci_target.wdata.read();
        }
        break;
    }
    ////////////////////
    case T_WRITE_BUFFER:
    {
        if (p_vci_target.rspack.read() )
        {
#if SOCLIB_MODULE_DEBUG
std::cout << "  <BDEV_TGT WRITE_BUFFER> value = " << r_tdata.read() << std::endl;
#endif
            r_buf_address = (r_buf_address.read() & 0xFFFFFFFF00000000ULL) |
                ((uint64_t)r_tdata.read());
            r_target_fsm  = T_IDLE;
        }
        break;
    }
    ////////////////////////
    case T_WRITE_BUFFER_EXT:
    {
        if (p_vci_target.rspack.read() )
        {
#if SOCLIB_MODULE_DEBUG
std::cout << "  <BDEV_TGT WRITE_BUFFER_EXT> value = " << r_tdata.read() << std::endl;
#endif
            r_buf_address = (r_buf_address.read() & 0x00000000FFFFFFFFULL) |
                ((uint64_t)r_tdata.read() << 32);
            r_target_fsm  = T_IDLE;
        }
        break;
    }
    ///////////////////
    case T_WRITE_COUNT:
    {
        if (p_vci_target.rspack.read() )
        {
#if SOCLIB_MODULE_DEBUG
std::cout << "  <BDEV_TGT WRITE_COUNT> value = " << r_tdata.read() << std::endl;
#endif
            r_nblocks    = (uint32_t)r_tdata.read();
            r_target_fsm = T_IDLE;
        }
        break;
    }
    /////////////////
    case T_WRITE_LBA:
    {
        if (p_vci_target.rspack.read() )
        {
#if SOCLIB_MODULE_DEBUG
std::cout << "  <BDEV_TGT WRITE_LBA> value = " << r_tdata.read() << std::endl;
#endif
            r_lba        = (uint32_t)r_tdata.read();
            r_target_fsm = T_IDLE;
        }
        break;
    }
    ////////////////
    case T_WRITE_OP:
    {
        if ( p_vci_target.rspack.read() )
        {
            if ( ((uint32_t)r_tdata.read() == BLOCK_DEVICE_READ) and
                 (r_initiator_fsm.read() == M_IDLE) )
            {

#if SOCLIB_MODULE_DEBUG
std::cout << "  <BDEV_TGT WRITE_OP> value = READ" << std::endl;
#endif
                r_read = true;
                r_go   = true;
            }
            else if ( ((uint32_t)r_tdata.read() == BLOCK_DEVICE_WRITE) and
                      (r_initiator_fsm.read() == M_IDLE) )
            {

#if SOCLIB_MODULE_DEBUG
std::cout << "  <BDEV_TGT WRITE_OP> value = WRITE" << std::endl;
#endif
                r_read = false;
                r_go   = true;
            }
            else
            {

#if SOCLIB_MODULE_DEBUG
std::cout << "  <BDEV_TGT WRITE_OP> value = SOFT RESET" << std::endl;
#endif
                r_go   = false;
            }
            r_target_fsm = T_IDLE;
        }
        break;
    }
    ///////////////////
    case T_WRITE_IRQEN:
    {
        if ( p_vci_target.rspack.read() )
        {

#if SOCLIB_MODULE_DEBUG
std::cout << "  <BDEV_TGT WRITE_IRQEN> value = " << r_tdata.read() << std::endl;
#endif
            r_target_fsm = T_IDLE;
            r_irq_enable = (r_tdata.read() != 0);
        }
        break;
    }
    ///////////////////
    case T_READ_BUFFER:
    case T_READ_BUFFER_EXT:
    case T_READ_COUNT:
    case T_READ_LBA:
    case T_READ_IRQEN:
    case T_READ_SIZE:
    case T_READ_BLOCK:
    case T_READ_ERROR:
    case T_WRITE_ERROR:
    {
        if ( p_vci_target.rspack.read() ) r_target_fsm = T_IDLE;
        break;
    }
    ///////////////////
    case T_READ_STATUS:
    {
        if ( p_vci_target.rspack.read() )
        {
            r_target_fsm = T_IDLE;
            if( (r_initiator_fsm == M_READ_SUCCESS ) ||
                (r_initiator_fsm == M_READ_ERROR   ) ||
                (r_initiator_fsm == M_WRITE_SUCCESS) ||
                (r_initiator_fsm == M_WRITE_ERROR  ) ) r_go = false;
        }
        break;
    }
    } // end switch target fsm

    //////////////////////////////////////////////////////////////////////////////
    // The initiator FSM executes a loop, transfering one block per iteration.
    // Each block is split in bursts, and the number of bursts depends
    // on the memory buffer alignment on a burst boundary:
    // - If buffer aligned, all burst have the same length (m_words_per burst)
    //   and the number of bursts is (m_bursts_per_block).
    // - If buffer not aligned, the number of bursts is (m_bursts_per_block + 1)
    //   and first and last burst are shorter, because all words in a burst
    //   must be contained in a single cache line.
    //   first burst => nwords = m_words_per_burst - offset
    //   last  burst => nwords = offset
    //   other burst => nwords = m_words_per_burst
    //////////////////////////////////////////////////////////////////////////////

    switch( r_initiator_fsm.read() ) {
    ////////////
    case M_IDLE:    // check buffer alignment to compute the number of bursts
    {
        if ( r_go.read() )
        {
            r_index         = 0;
            r_block_count   = 0;
            r_burst_count   = 0;
            r_words_count   = 0;
            r_latency_count = m_latency;

            // compute r_burst_offset (zero when buffer aligned)
            r_burst_offset = (uint32_t)((r_buf_address.read()>>2) % m_words_per_burst);

            // start tranfer
            if ( r_read.read() )    r_initiator_fsm = M_READ_BLOCK;
            else                    r_initiator_fsm = M_WRITE_BURST;
        }
        break;
    }
    //////////////////
    case M_READ_BLOCK:  // read one block from disk after waiting m_latency cycles
    {
        if ( r_latency_count.read() == 0 )
        {
            r_latency_count = m_latency;
            ::lseek(m_fd, (r_lba + r_block_count)*m_words_per_block*4, SEEK_SET);
            if( ::read(m_fd, r_local_buffer, m_words_per_block*4) < 0 )
            {
                r_initiator_fsm = M_READ_ERROR;
            }
            else
            {
                r_burst_count   = 0;
                r_words_count   = 0;
                r_initiator_fsm = M_READ_BURST;
            }

/*
if ( r_lba.read() == 0 )
{
    std::cout << "***** Block content after read for lba "
              << std::hex << r_lba.read() << " **************" << std::endl;
    for ( size_t line=0 ; line<16 ; line++ )
    {
        for ( size_t word=0 ; word<8 ; word++ )
        {
            std::cout << std::hex << r_local_buffer[line*8 + word] << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "**********************************************************" 
              << std::endl;
}
*/
        }
        else
        {
            r_latency_count = r_latency_count.read() - 1;
        }
        break;
    }
    //////////////////
    case M_READ_BURST:  // Compute number of words and number of flits in the burst
                        // The number of flits can be smaller than the number of words
                        // in case of 8 bytes flits...
    {
        uint32_t nwords;
        uint32_t offset = r_burst_offset.read();

        if ( offset )                  // buffer not aligned
        {
            if ( r_burst_count.read() == 0 ) nwords = m_words_per_burst - offset;
            else if ( r_burst_count.read() == m_bursts_per_block ) nwords = offset;
            else nwords = m_words_per_burst;
        }
        else                           // buffer aligned
        {
            nwords = m_words_per_burst;
        }

        r_burst_nwords  = nwords;
        r_initiator_fsm = M_READ_CMD;
        break;
    }
    ////////////////
    case M_READ_CMD:    // Send a multi-flits VCI WRITE command
    {
        if ( p_vci_initiator.cmdack.read() )
        {
            uint32_t nwords = r_burst_nwords.read() - r_words_count.read();

            if ( vci_param::B == 4 )    // one word per flit
            {
                if ( nwords <= 1 )      // last flit
                {
                    r_initiator_fsm = M_READ_RSP;
                    r_words_count = 0;
                }
                else                    // not the last flit
                {
                    r_words_count = r_words_count.read() + 1;
                }

                // compute next word address and next local buffer index
                r_buf_address = r_buf_address.read() + 4;
                r_index       = r_index.read() + 1;
            }
            else                        // 2 words per flit
            {
                if ( nwords <= 2 )      // last flit
                {
                    r_initiator_fsm = M_READ_RSP;
                    r_words_count = 0;
                }
                else                    // not the last flit
                {
                    r_words_count = r_words_count.read() + 2;
                }

                // compute next word address and next local buffer index
                if ( nwords == 1 )
                {
                    r_buf_address = r_buf_address.read() + 4;
                    r_index       = r_index.read() + 1;
                }
                else
                {
                    r_buf_address = r_buf_address.read() + 8;
                    r_index       = r_index.read() + 2;
                }
            }
        }
        break;
    }
    ////////////////
    case M_READ_RSP:    // Wait a single flit VCI WRITE response
    {
        if ( p_vci_initiator.rspval.read() )
        {
            bool aligned = (r_burst_offset.read() == 0);

            if ( (p_vci_initiator.rerror.read()&0x1) != 0 )
            {
                r_initiator_fsm = M_READ_ERROR;
            }
            else if ( (not aligned and (r_burst_count.read() == m_bursts_per_block)) or
                      (aligned and (r_burst_count.read() == (m_bursts_per_block-1))) )
            {
                if ( r_block_count.read() == (r_nblocks.read()-1) ) // last burst of last block
                {
                    r_initiator_fsm = M_READ_SUCCESS;
                }
                else                                              // last burst not last block
                {
                    r_index          = 0;
                    r_burst_count    = 0;
                    r_block_count    = r_block_count.read() + 1;
                    r_initiator_fsm  = M_READ_BLOCK;
                }
            }
            else                                                // not the last burst
            {
                r_burst_count = r_burst_count.read() + 1;
                r_initiator_fsm = M_READ_BURST;
            }
        }
        break;
    }
    ///////////////////
    case M_READ_SUCCESS:
    case M_READ_ERROR:
    {
        if( !r_go ) r_initiator_fsm = M_IDLE;
        break;
    }
    ///////////////////
    case M_WRITE_BURST:  // Compute the number of words in the burst
    {
        uint32_t nwords;
        uint32_t offset = r_burst_offset.read();

        if ( offset )                  // buffer not aligned
        {
            if ( r_burst_count.read() == 0 ) nwords = m_words_per_burst - offset;
            else if ( r_burst_count.read() == m_bursts_per_block ) nwords = offset;
            else nwords = m_words_per_burst;
        }
        else                           // buffer aligned
        {
            nwords = m_words_per_burst;
        }

        r_burst_nwords  = nwords;
        r_initiator_fsm =  M_WRITE_CMD;
        break;
    }
    /////////////////
    case M_WRITE_CMD:   // This is actually a single flit VCI READ command
    {
        if ( p_vci_initiator.cmdack.read() ) r_initiator_fsm = M_WRITE_RSP;
        break;
    }
    /////////////////
    case M_WRITE_RSP:   // This is actually a multi-words VCI READ response
    {
        if ( p_vci_initiator.rspval.read() )
        {
            bool aligned = (r_burst_offset.read() == 0);

            if ( (vci_param::B == 8) and (r_burst_nwords.read() > 1) )
            {
                r_local_buffer[r_index.read()]   = (uint32_t)p_vci_initiator.rdata.read();
                r_local_buffer[r_index.read()+1] = (uint32_t)(p_vci_initiator.rdata.read()>>32);
                r_index = r_index.read() + 2;
            }
            else
            {
                r_local_buffer[r_index.read()]   = (uint32_t)p_vci_initiator.rdata.read();
                r_index = r_index.read() + 1;
            }

            if ( p_vci_initiator.reop.read() )  // last flit of the burst
            {
                r_words_count  = 0;
                r_buf_address = r_buf_address.read() + (r_burst_nwords.read()<<2);

                if( (p_vci_initiator.rerror.read()&0x1) != 0 )
                {
                    r_initiator_fsm = M_WRITE_ERROR;
                }
                else if ( (not aligned and (r_burst_count.read() == m_bursts_per_block)) or
                     (aligned and (r_burst_count.read() == (m_bursts_per_block-1))) ) // last burst
                {
                    r_initiator_fsm  = M_WRITE_BLOCK;
                }
                else                                          // not the last burst
                {
                    r_burst_count = r_burst_count.read() + 1;
                    r_initiator_fsm = M_WRITE_BURST;
                }
            }
            else
            {
                r_words_count = r_words_count.read() + 1;
            }
        }
        break;
    }
    ///////////////////
    case M_WRITE_BLOCK:     // write a block to disk after waiting m_latency cycles
    {
        if ( r_latency_count == 0 )
        {

/*
std::cout << "***** Block content before write for lba "
          << std::hex << r_lba.read() << " ***********" << std::endl;
for ( size_t line=0 ; line<16 ; line++ )
{
    for ( size_t word=0 ; word<8 ; word++ )
    {
        std::cout << std::hex << r_local_buffer[line*8 + word] << " ";
    }
    std::cout << std::endl;
}
std::cout << "**********************************************************" 
          << std::endl;
*/

            r_latency_count = m_latency;
            ::lseek(m_fd, (r_lba + r_block_count)*m_words_per_block*4, SEEK_SET);
            if( ::write(m_fd, r_local_buffer, m_words_per_block*4) < 0 )
            {
                r_initiator_fsm = M_WRITE_ERROR;
            }
            else if ( r_block_count.read() == r_nblocks.read() - 1 )
            {
                r_initiator_fsm = M_WRITE_SUCCESS;
            }
            else
            {
                r_burst_count    = 0;
                r_index          = 0;
                r_block_count    = r_block_count.read() + 1;
                r_initiator_fsm  = M_WRITE_BURST;
            }
        }
        else
        {
            r_latency_count = r_latency_count - 1;
        }
        break;
    }
    /////////////////////
    case M_WRITE_SUCCESS:
    case M_WRITE_ERROR:
    {
        if( !r_go ) r_initiator_fsm = M_IDLE;
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
    case T_READ_STATUS:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        if     (r_initiator_fsm == M_IDLE)          p_vci_target.rdata = BLOCK_DEVICE_IDLE;
        else if(r_initiator_fsm == M_READ_SUCCESS)  p_vci_target.rdata = BLOCK_DEVICE_READ_SUCCESS;
        else if(r_initiator_fsm == M_WRITE_SUCCESS) p_vci_target.rdata = BLOCK_DEVICE_WRITE_SUCCESS;
        else if(r_initiator_fsm == M_READ_ERROR)    p_vci_target.rdata = BLOCK_DEVICE_READ_ERROR;
        else if(r_initiator_fsm == M_WRITE_ERROR)   p_vci_target.rdata = BLOCK_DEVICE_WRITE_ERROR;
        else                                        p_vci_target.rdata = BLOCK_DEVICE_BUSY;
        p_vci_target.rerror = VCI_READ_OK;
        break;
    case T_READ_BUFFER:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = (uint32_t)r_buf_address.read();
        p_vci_target.rerror = VCI_READ_OK;
        break;
    case T_READ_BUFFER_EXT:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = (uint32_t)(r_buf_address.read()>>32);
        p_vci_target.rerror = VCI_READ_OK;
        break;
    case T_READ_COUNT:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = r_nblocks.read();
        p_vci_target.rerror = VCI_READ_OK;
        break;
    case T_READ_LBA:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = r_lba.read();
        p_vci_target.rerror = VCI_READ_OK;
        break;
    case T_READ_IRQEN:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = r_irq_enable.read();
        p_vci_target.rerror = VCI_READ_OK;
        break;
    case T_READ_SIZE:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = m_device_size;
        p_vci_target.rerror = VCI_READ_OK;
        break;
    case T_READ_BLOCK:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = m_words_per_block*4;
        p_vci_target.rerror = VCI_READ_OK;
        break;
    case T_READ_ERROR:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = 0;
        p_vci_target.rerror = VCI_READ_ERROR;
        break;
    case T_WRITE_ERROR:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = 0;
        p_vci_target.rerror = VCI_WRITE_ERROR;
        break;
    default:
        p_vci_target.cmdack = false;
        p_vci_target.rspval = true;
        p_vci_target.rdata  = 0;
        p_vci_target.rerror = VCI_WRITE_OK;
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
    case M_WRITE_CMD:       // It is actually a single flit VCI read command
        p_vci_initiator.rspack  = false;
        p_vci_initiator.cmdval  = true;
        p_vci_initiator.address = (sc_dt::sc_uint<vci_param::N>)r_buf_address.read();
        p_vci_initiator.cmd     = vci_param::CMD_READ;
        p_vci_initiator.pktid   = TYPE_READ_DATA_UNC;
        p_vci_initiator.wdata   = 0;
        p_vci_initiator.be      = 0;
        p_vci_initiator.plen    = (sc_dt::sc_uint<vci_param::K>)(r_burst_nwords.read()<<2);
        p_vci_initiator.eop     = true;
        break;
    case M_READ_CMD:        // It is actually a multi-words VCI WRITE command
        p_vci_initiator.rspack  = false;
        p_vci_initiator.cmdval  = true;
        p_vci_initiator.address = (sc_dt::sc_uint<vci_param::N>)r_buf_address.read();
        p_vci_initiator.cmd     = vci_param::CMD_WRITE;
        p_vci_initiator.pktid   = TYPE_WRITE;
        p_vci_initiator.plen    = (sc_dt::sc_uint<vci_param::K>)(r_burst_nwords.read()<<2);
        if ( (vci_param::B == 8) and ((r_burst_nwords.read() - r_words_count.read()) > 1) )
        {
            p_vci_initiator.wdata = ((uint64_t)r_local_buffer[r_index.read()  ]) +
                                   (((uint64_t)r_local_buffer[r_index.read()+1]) << 32);
            p_vci_initiator.be    = 0xFF;
            p_vci_initiator.eop   = ( (r_burst_nwords.read() - r_words_count.read()) <= 2 );
        }
        else
        {
            p_vci_initiator.wdata = r_local_buffer[r_index.read()];
            p_vci_initiator.be    = 0xF;
            p_vci_initiator.eop   = ( r_words_count.read() == (r_burst_nwords.read() - 1) );
        }
        break;
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

    // IRQ signal
    if ( ((r_initiator_fsm == M_READ_SUCCESS)  ||
          (r_initiator_fsm == M_WRITE_SUCCESS) ||
          (r_initiator_fsm == M_READ_ERROR)    ||
          (r_initiator_fsm == M_WRITE_ERROR) ) &&
         r_irq_enable.read() )
    {

#if SOCLIB_MODULE_DEBUG
        if (p_irq != true)
            std::cout << "  <BDEV_INI send IRQ>" << std::endl;
#endif
        p_irq = true;
    }
    else
    {
        p_irq = false;
    }
} // end GenMoore()

//////////////////////////////////////////////////////////////////////////////
tmpl(/**/)::VciBlockDeviceTsar( sc_core::sc_module_name              name,
                                const soclib::common::MappingTable   &mt,
                                const soclib::common::IntTab         &srcid,
                                const soclib::common::IntTab         &tgtid,
                                const std::string                    &filename,
                                const uint32_t                       block_size,
                                const uint32_t                       burst_size,
                                const uint32_t                       latency)

: caba::BaseModule(name),
    m_seglist(mt.getSegmentList(tgtid)),
    m_srcid(mt.indexForId(srcid)),
    m_words_per_block(block_size/4),
    m_words_per_burst(burst_size/4),
    m_bursts_per_block(block_size/burst_size),
    m_latency(latency),
    p_clk("p_clk"),
    p_resetn("p_resetn"),
    p_vci_initiator("p_vci_initiator"),
    p_vci_target("p_vci_target"),
    p_irq("p_irq")
{
    std::cout << "  - Building VciBlockDeviceTsar " << name << std::endl;

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
            std::cout << "Error in component VciBlockDeviceTsar : " << name
                      << "The base address of segment " << seg->name()
                      << " must be multiple of 64 bytes" << std::endl;
            exit(1);
        }
        if ( seg->size() < 64 )
        {
            std::cout << "Error in component VciBlockDeviceTsar : " << name
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
        std::cout << "Error in component VciBlockDeviceTsar : " << name
                  << " No segment allocated" << std::endl;
        exit(1);
    }

    if( (block_size != 128)  &&
        (block_size != 256)  &&
        (block_size != 512)  &&
        (block_size != 1024) &&
        (block_size != 2048) &&
        (block_size != 4096) )
    {
        std::cout << "Error in component VciBlockDeviceTsar : " << name
                  << " The block size must be 128, 256, 512, 1024, 2048 or 4096 bytes"
                  << std::endl;
        exit(1);
    }

    if( (burst_size != 8 ) &&
        (burst_size != 16) &&
        (burst_size != 32) &&
        (burst_size != 64) )
    {
        std::cout << "Error in component VciBlockDeviceTsar : " << name
                  << " The burst size must be 8, 16, 32 or 64 bytes" << std::endl;
        exit(1);
    }

    if ( (vci_param::B != 4) and (vci_param::B != 8) )
    {
        std::cout << "Error in component VciBlockDeviceTsar : " << name
                  << " The VCI data fields must have 32 bits or 64 bits" << std::endl;
        exit(1);
    }

    m_fd = ::open(filename.c_str(), O_RDWR);
    if ( m_fd < 0 )
    {
        std::cout << "Error in component VciBlockDeviceTsar : " << name
                  << " Unable to open file " << filename << std::endl;
        exit(1);
    }
    m_device_size = lseek(m_fd, 0, SEEK_END) / block_size;

    if ( m_device_size > ((uint64_t)1<<vci_param::N ) )
    {
        std::cout << "Error in component VciBlockDeviceTsar" << name
                  << " The file " << filename
                  << " has more blocks than addressable with the VCI address" << std::endl;
        exit(1);
    }

    r_local_buffer = new uint32_t[m_words_per_block];

} // end constructor

/////////////////////////////////
tmpl(/**/)::~VciBlockDeviceTsar()
{
    ::close(m_fd);
    delete [] r_local_buffer;
}


//////////////////////////
tmpl(void)::print_trace()
{
    const char* initiator_str[] =
    {
        "INI_IDLE",

        "INI_READ_BLOCK",
        "INI_READ_BURST",
        "INI_READ_CMD",
        "INI_READ_RSP",
        "INI_READ_SUCCESS",
        "INI_READ_ERROR",

        "INI_WRITE_BURST",
        "INI_WRITE_CMD",
        "INI_WRITE_RSP",
        "INI_WRITE_BLOCK",
        "INI_WRITE_SUCCESS",
        "INI_WRITE_ERROR",
    };
    const char* target_str[] =
    {
        "TGT_IDLE",
        "TGT_WRITE_BUFFER",
        "TGT_READ_BUFFER",
        "TGT_WRITE_BUFFER_EXT",
        "TGT_READ_BUFFER_EXT",
        "TGT_WRITE_COUNT",
        "TGT_READ_COUNT",
        "TGT_WRITE_LBA",
        "TGT_READ_LBA",
        "TGT_WRITE_OP",
        "TGT_READ_STATUS",
        "TGT_WRITE_IRQEN",
        "TGT_READ_IRQEN",
        "TGT_READ_SIZE",
        "TGT_READ_BLOCK",
        "TGT_READ_ERROR",
        "TGT_WRITE_ERROR ",
    };

    std::cout << "BDEV " << name()
              << " : " << target_str[r_target_fsm.read()]
              << " / " << initiator_str[r_initiator_fsm.read()]
              << " / buf = " << std::hex << r_buf_address.read()
              << " / lba = " << std::hex << r_lba.read()
              << " / block_count = " << std::dec << r_block_count.read()
              << " / burst_count = " << r_burst_count.read()
              << " / word_count = " << r_words_count.read() <<std::endl;
}

}} // end namespace

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

