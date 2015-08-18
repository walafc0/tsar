/*
 -*- c++ -*-
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

#include <stdint.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <cassert>
#include "sdmmc.h"

namespace soclib { namespace caba {

using namespace soclib::caba;

////////////////////////
void SdMMC::genMealy()
{
    if(p_resetn.read() == false) 
    {
        spi_fsm  = S_IDLE;
	m_acmd	   = false;	 
	m_sdstate  = SD_IDLE;
        return;
    } 
    if (p_spi_ss.read()) {
	if (spi_fsm != S_IDLE) {
		std::cerr << name() << " deselect but not idle, state "
		<< std::dec << spi_fsm << " last cmd " << (int)command
		<< " args " << std::hex << args << std::dec
		<< " bitcount " << (int)spi_bitcount
		<< " idx " << m_data_idx << " len_snd " << m_datalen_snd
		<< " len_rcv " << m_datalen_rcv << std::endl;
	}
	spi_fsm  = S_IDLE;
	spi_clk = p_spi_clk;
	spi_mosi_previous = p_spi_mosi;
	return;
    }

    switch(spi_fsm) {
    case S_IDLE:
	if (p_spi_clk.read() == 1 && spi_clk == 0) {
		// rising edge
		command = (command << 1) | spi_mosi_previous;
		spi_bitcount = 6;
		spi_fsm = S_RECEIVE_CMD;
	}
        break;
    case S_RECEIVE_CMD:
	if (p_spi_clk.read() == 1 && spi_clk == 0) {
		// rising edge
		command = (command << 1) | spi_mosi_previous;
		if (spi_bitcount == 0) {
			if ((command & 0x80) == 0) {
				spi_fsm = S_RECEIVE_ARGS_START;
			} else {
#ifdef SOCLIB_MODULE_DEBUG0
				std::cout << name() << " S_RECEIVE_CMD " << std::hex << (int)command << std::endl;
#endif
				spi_fsm = S_IDLE;
			}
		} else {
		    spi_bitcount = spi_bitcount - 1;
		}
	}
	break;
    case S_RECEIVE_ARGS_START:
	if (p_spi_clk.read() == 1 && spi_clk == 0) {
		// rising edge
		args = (args << 1) | spi_mosi_previous;
		spi_bitcount = 30;
		spi_fsm = S_RECEIVE_ARGS;
	}
        break;
    case S_RECEIVE_ARGS:
	if (p_spi_clk.read() == 1 && spi_clk == 0) {
		// rising edge
		args = (args << 1) | spi_mosi_previous;
		if (spi_bitcount == 0) {
			spi_bitcount = 7;
			spi_fsm = S_RECEIVE_CRC;
		} else {
		    spi_bitcount = spi_bitcount - 1;
		}
	}
        break;
    case S_RECEIVE_CRC:
	if (p_spi_clk.read() == 1 && spi_clk == 0) {
		// rising edge
		cmdcrc = (cmdcrc << 1) | spi_mosi_previous;
		if (spi_bitcount == 0) {
			handle_sdmmc_cmd(command, args);
			spi_bitcount = 0; // SEND_DATA will reset it
			spi_fsm = S_SEND_DATA;
			m_data_idx = 0;
		} else {
			spi_bitcount = spi_bitcount - 1;
		}
	}
	break;
	
    case S_SEND_DATA:
	if (p_spi_clk.read() == 0 && spi_clk == 1) {
		// falling edge
		if (spi_bitcount == 0) {
			if (m_data_idx != m_datalen_snd) {	
				spi_shiftreg = m_databuf[m_data_idx];
				spi_bitcount = 7;
				spi_fsm = S_SEND_DATA;
				m_data_idx++;
#ifdef SOCLIB_MODULE_DEBUG0
		        std::cout << name() << " S_SEND_DATA " << std::dec << m_datalen_snd << " idx " << m_data_idx << " " << std::hex << (uint32_t)m_databuf[m_data_idx] << std::endl;
#endif
			} else if (m_datalen_rcv != 0) {
				spi_fsm = S_RECEIVE_DATA_WAIT;
				spi_bitcount = 7;
				m_data_idx = 0;
			} else {
				spi_fsm = S_IDLE;
			}
		} else {
			spi_bitcount = spi_bitcount - 1;
			spi_shiftreg = spi_shiftreg << 1;
		}
	}
	break;
    case S_RECEIVE_DATA_WAIT:
	if (p_spi_clk.read() == 1 && spi_clk == 0) {
	    // rising edge
	    uint8_t s_data;
	    s_data = (m_databuf[0] << 1) | spi_mosi_previous;
	    m_databuf[0] = s_data;
	    if (spi_bitcount == 0) {
#ifdef SOCLIB_MODULE_DEBUG
	std::cout << name() << " S_RECEIVE_DATA_WAIT " << std::dec << (int)s_data << std::endl;
#endif
		    spi_bitcount = 7;
		    if (s_data == 0xfe) { // data start token
			spi_fsm = S_RECEIVE_DATA;
			m_data_idx = 1;
		    } else {
#ifdef SOCLIB_MODULE_DEBUG
			std::cout << name() << " S_RECEIVE_DATA_WAIT " << std::hex << (int)s_data << std::endl;
#endif
			spi_fsm = S_RECEIVE_DATA_WAIT;
		}
	    } else {
	        spi_bitcount = spi_bitcount - 1;
	    }
	}
	break;
	case S_RECEIVE_DATA:
	    if (p_spi_clk.read() == 1 && spi_clk == 0) {
		// rising edge
		m_databuf[m_data_idx] = (m_databuf[m_data_idx] << 1) | spi_mosi_previous;
		if (spi_bitcount == 0) {
		    m_data_idx++;
		    if (m_data_idx != m_datalen_rcv) {
		    	spi_fsm = S_RECEIVE_DATA;
		    	spi_bitcount = 7;
		    } else {
		        handle_sdmmc_write(command, args);
		        if (m_datalen_snd > 0) {
		            spi_bitcount = 0; // SEND_DATA will reset it
		            spi_fsm = S_SEND_DATA;
		            m_data_idx = 0;
		        } else {
		            spi_fsm = S_IDLE;
		        }
		    }
		} else {
			spi_bitcount = spi_bitcount - 1;
		}
	    }
	    break;
    }

//// now genrate output signal

    switch(spi_fsm) {
    case S_IDLE:
	p_spi_miso = !p_spi_ss.read();
        break;
    case S_SEND_DATA:
	p_spi_miso = (spi_shiftreg & 0x80) != 0;
	break;
    default:
	p_spi_miso = !p_spi_ss.read();
        break;
    }
    spi_clk = p_spi_clk.read();
    spi_mosi_previous = p_spi_mosi;
} // end GenMealy()


//////////////////////
void SdMMC::handle_sdmmc_cmd(uint8_t cmd, uint32_t data)
{
	m_datalen_rcv = 0;
	m_databuf[0] = 0x04; // illegal command
	m_datalen_snd = 1;

	if (m_sdstate == SD_IDLE)
		m_databuf[0] |= 0x01; // idle

	if ((cmd & 0x40) == 0) {
		//illegal command
		return;
	}
	cmd &= 0x3f;
	if (m_acmd) {
#ifdef SOCLIB_MODULE_DEBUG0
	std::cout << name() << " new acmd " << std::dec << (int)cmd << " args " << std::hex << data << " crc " << (int)cmdcrc << std::endl;
#endif
	    m_acmd = false;
	    switch (cmd) {
	    case 41:
		m_databuf[0] = 0x0; // card ready
		m_datalen_snd = 1;
		m_sdstate = SD_READY;
		break;
	    case 51:
		// send SCR
		m_databuf[ 0] = (m_sdstate == SD_IDLE) ? 0x1 : 0x0; // R1
		m_databuf[ 1] = 0xfe; // data token
		m_databuf[ 2] = 0x00; // SCR_STRUCTURE / SD_SPEC
		m_databuf[ 3] = 0x05; // DATA_STAT_AFTER_ERASE, SD_SECURITY, SD_BUS_WIDTHS
		m_databuf[ 4] = 0;    // SD_SPEC3, EX_SECURITY, SD_SPEC4
		m_databuf[ 5] = 0;    // CMD_SUPPORT
		m_databuf[ 6] = 0;    // vendor specific
		m_databuf[ 7] = 0;    // vendor specific
		m_databuf[ 8] = 0;    // vendor specific
		m_databuf[ 9] = 0;    // vendor specific
		m_databuf[10] = 0x0;  // CRC16
		m_databuf[11] = 0x0;  // CRC16
		m_datalen_snd = 12;
		break;
	    default:
		std::cout << name() << " unknown acmd " << std::dec
		    << (int)cmd << std::endl;
		break; // return illegal command
	    }
	} else {
#ifdef SOCLIB_MODULE_DEBUG0
	std::cout << name() << " new cmd " << std::dec << (int)cmd << " args " << std::hex << data << " crc " << (int)cmdcrc << std::endl;
#endif
	    switch (cmd) {
	    case 0:
		m_databuf[0] = 0x1;
		m_datalen_snd = 1;
		m_sdstate = SD_IDLE;
		break;
	    case 8:
		// reply with illegal command for now
		break;
	    case 9:
	      {
		// send CSD
		// we use a block len of 1024
		uint32_t csize = ((m_device_size + (512 * 1024) - 1) / (512 * 1024)) - 1;
		m_databuf[ 0]  = (m_sdstate == SD_IDLE) ? 0x1 : 0x0; // R1
		m_databuf[ 1]  = 0xfe; // data token
		m_databuf[ 2]  = 0x00; // CSD_STRUCTURE
		m_databuf[ 3]  = 0xe;  // TAAC
		m_databuf[ 4]  = 0;    // NSAC
		m_databuf[ 5]  = 0x32; // TRAN_SPEED
		m_databuf[ 6]  = 0x5b; // CCC_H
		m_databuf[ 7]  = 0x5a; // CCC_L + READ_BL_LEN
		m_databuf[ 8]  = 0x80; // READ_BL_PARTIAL, R/W_BLK_MISALIGN, DSR_IMP
		m_databuf[ 8] |= (csize >> 10) & 0x03; // CSIZE[12-11]
		m_databuf[ 9]  = (csize >>  2) & 0xff; // CSIZE[10-2]
		m_databuf[10]  = (csize <<  6) & 0xc0; // CSIZE[1-0]
		m_databuf[10] |= 0;    // R_CURR_MIN, R_CURR_MAX
		m_databuf[11]  = 0x3;  // W_CURR_MIN, W_CURR_MAX, CSIZE_MULT[2-1];
		m_databuf[12]  = 0xff; // CSIZE_MULT[1], ERASE_BLK_EN, ERASE_SECTOR_SIZE[6-1]
		m_databuf[13]  = 0x80; // ERASE_SECTOR_SIZE[0]. WP_GRP_SIZE
		m_databuf[14]  = 0x0a; // WP_GRP_ENABLE, R2W_FACTOR, WRITE_BL_LEN[2-3]
		m_databuf[15]  = 0x40; // WRITE_BL_LEN[0-1], WR_BL_PARTIAL
		m_databuf[16]  = 0;    // FILE_FORMAT
		m_databuf[17]  = 0x1;  // CRC7
		m_databuf[18]  = 0x0;  // CRC16
		m_databuf[19]  = 0x0;  // CRC16
		m_datalen_snd  = 20;
		break;
	      }
	    case 10:
		// send CID
		m_databuf[ 0] = (m_sdstate == SD_IDLE) ? 0x1 : 0x0; // R1
		m_databuf[ 1] = 0xfe; // data token
		m_databuf[ 2] = 0xda; // MID
		m_databuf[ 3] = 'P';  // OID
		m_databuf[ 4] = '6';  // OID
		m_databuf[ 5] = 's';  // PNM
		m_databuf[ 6] = 'o';  // PNM
		m_databuf[ 7] = 'c';  // PNM
		m_databuf[ 8] = 's';  // PNM
		m_databuf[ 9] = 'd';  // PNM
		m_databuf[10] = 0x01; // PRV
		m_databuf[11] = 0xde; // PSN
		m_databuf[12] = 0xad; // PSN
		m_databuf[13] = 0xbe; // PSN
		m_databuf[14] = 0xef; // PSN
		m_databuf[15] = 10;   // MDT
		m_databuf[16] = 13;   // MDT
		m_databuf[17] = 0x1;  // CRC7
		m_databuf[18] = 0x0;  // CRC16
		m_databuf[19] = 0x0;  // CRC16
		m_datalen_snd = 20;
		break;
	    case 16:
		// set block size
		if (m_sdstate != SD_IDLE && data == 512) {
			m_databuf[0] = 0x00;
			m_datalen_snd = 1;
		} // else illegal command
		break;
	    case 17:
	      {
		int ret;
		// read data block
		if (m_sdstate == SD_IDLE) {
			// return illegal command
			return;
		}
		if (data >= m_device_size) {
			std::cerr << name() << " read: request " << data
			    << " past end of file " << m_device_size << std::endl;
			m_databuf[0] = 0x00; // R1 OK
			m_databuf[1] = 0x08; // error tocken "out of range"
			m_datalen_snd = 2;
			return;
		}
		do {
			if (lseek(m_fd, data, SEEK_SET) < 0) {
				std::cerr << name() << " lseek: " <<
				  strerror(errno) << std::endl;
				m_databuf[0] = 0x00; // R1 OK
				m_databuf[1] = 0x02; // error tocken "CC err"
				m_datalen_snd = 2;
				return;
			}
			ret = read(m_fd, &m_databuf[2], 512);
		} while (ret < 0 && errno == EINTR);
		if (ret < 0) {
			std::cerr << name() << " read: " <<
			  strerror(errno) << std::endl;
			m_databuf[0] = 0x00; // R1 OK
			m_databuf[1] = 0x04; // error tocken "card ECC failed"
			m_datalen_snd = 2;
			return;
		}
		m_databuf[514] = m_databuf[515] = 0; // XXX CRC
		m_databuf[0] = 0x0; // R1
		m_databuf[1] = 0xfe; // start block tocken
		m_datalen_snd = 516;
		break;
	      }
	    case 24:
	      {
		// write data block
		if (m_sdstate == SD_IDLE) {
			// return illegal command
			return;
		}
#ifdef SOCLIB_MODULE_DEBUG
	std::cout << name() << " new cmd write " << std::dec << (int)cmd << " args " << std::hex << data << std::endl;
#endif
		m_databuf[0] = 0x0; // R1
		m_datalen_snd = 1;
		m_datalen_rcv = 512 + 2 + 1; // data + tocken + CRC
		break;
	      }
	    case 55:
		// app-specific command follow
		m_acmd = true;
		m_databuf[0] = (m_sdstate == SD_IDLE) ? 0x1 : 0x0;
		m_datalen_snd = 1;
		break;
	    case 58:
		// send OCR
		m_databuf[4] = (m_sdstate == SD_IDLE) ? 0x1 : 0x0; // R1
		m_databuf[3] = 0x80; // power up complete, SDSC
		m_databuf[2] = 0xff; // all voltages supported
		m_databuf[1] = 0x00; 
		m_databuf[0] = 0x00; 
		m_datalen_snd = 5;
		break;
	    default:
		std::cout << name() << " unknown cmd " << std::dec
		    << (int)cmd << std::endl;
		break; // return illegal command
	    }
	}
}

void SdMMC::handle_sdmmc_write(uint8_t cmd, uint32_t data)
{
	m_datalen_rcv = 0;
	cmd &= 0x3f;
#ifdef SOCLIB_MODULE_DEBUG
	std::cout << name() << " cmd write " << std::dec << (int)cmd << " args " << std::hex << data << std::endl;
#endif
	switch(cmd) {
	    case 24:
	      {
		int ret;
		// write data block
		assert(m_sdstate != SD_IDLE && "can't write in idle state");
		if (data >= m_device_size) {
			std::cerr << name() << " write: request " << data
			    << " past end of file " << m_device_size << std::endl;
			m_databuf[0] = 0xd; // write error
			m_datalen_snd = 1;
			return;
		}
		do {
			if (lseek(m_fd, data, SEEK_SET) < 0) {
				std::cerr << name() << " lseek: " <<
				  strerror(errno) << std::endl;
				m_databuf[0] = 0xd; // write error
				m_datalen_snd = 1;
				return;
			}
			ret = write(m_fd, &m_databuf[1], 512);
		} while (ret < 0 && errno == EINTR);
		if (ret < 0) {
			std::cerr << name() << " write: " <<
			  strerror(errno) << std::endl;
			m_databuf[0] = 0xd; // write error
			m_datalen_snd = 1;
			return;
		}
		m_databuf[0] = 0x5; // write complete
		m_databuf[1] = 0x0; // busy
		m_datalen_snd = 2;
		break;
	      }
	    default:
		std::cerr << name() << " unkown write cmd " << std::dec <<
		    (int)cmd << std::endl;
		m_databuf[0] = 0xd; // write error;
		m_datalen_snd = 1;
	}
	return;
}

//////////////////////////////////////////////////////////////////////////////
SdMMC::SdMMC( sc_core::sc_module_name              name, 
                                const std::string                    &filename,
                                const uint32_t                       latency)

: caba::BaseModule(name),
	m_latency(latency),
	p_clk("p_clk"),
	p_resetn("p_resetn"),
	p_spi_ss("p_spi_ss"),
	p_spi_clk("p_spi_clk"),
	p_spi_mosi("p_spi_mosi"),
	p_spi_miso("p_spi_miso")
{
    std::cout << "  - Building SdMMC " << name << std::endl;

    SC_METHOD(genMealy);
    dont_initialize();
    sensitive << p_clk.neg();
    sensitive << p_resetn;
    sensitive << p_spi_ss;
    sensitive << p_spi_clk;

    m_fd = ::open(filename.c_str(), O_RDWR);
    if ( m_fd < 0 ) 
    {
	    std::cout << "Error in component SdMMC : " << name 
	              << " Unable to open file " << filename << std::endl;
	    exit(1);
    }
    m_device_size = lseek(m_fd, 0, SEEK_END);

} // end constructor

SdMMC::~SdMMC()
{
}


//////////////////////////
void SdMMC::print_trace()
{
	const char* spi_str[] = 
    {
		"S_IDLE",
                "S_RECEIVE_CMD",
                "S_RECEIVE_ARGS_START",
                "S_RECEIVE_ARGS",
                "S_RECEIVE_CRC",
                "S_RECEIVE_DATA_START",
                "S_RECEIVE_DATA",
                "S_SEND_DATA",
                "S_NOP",
	};
	if (spi_clk != p_spi_clk.read()) {
	std::cout << name() << " SPI_FSM : " << spi_str[spi_fsm] 
	    << std::dec
	    << " clk " << spi_clk << "->" << p_spi_clk << " ss " << p_spi_ss
	    << " mosi " << p_spi_mosi << " miso " << p_spi_miso
	    << std::endl;
	std::cout << "         spi_shiftreg: " << std::hex << (int)spi_shiftreg
	    << " spi_bitcount: " << (int)spi_bitcount
	    << std::endl;
        }
}
}} // end namespace

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

