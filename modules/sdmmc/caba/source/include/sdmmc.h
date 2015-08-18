
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
// This component is a SD/MMC block device.
//
// This component can perform data transfers between one single file belonging 
// to the host system and a SPI controller.
// The file name is an argument of the constructor.
//

#ifndef SOCLIB_SDMMC_H
#define SOCLIB_SDMMC_H

#include <stdint.h>
#include <systemc>
#include <unistd.h>
#include "caba_base_module.h"

namespace soclib {
namespace caba {

using namespace sc_core;

class SdMMC
	: public caba::BaseModule
{
private:

    // Registers
    int               spi_fsm;   	 // SPI state register
    int		      spi_shiftreg;	// data shift in/out
    int		      spi_bitcount;
    int		      spi_clk;
    int		      spi_mosi_previous; // sampled MOSI value

    uint8_t	      command;
    uint32_t	      args;
    uint8_t	      cmdcrc;
    int               m_fd;           	 // File descriptor
    uint64_t          m_device_size;  	 // Total number of blocks
    const uint32_t    m_latency;      	 // device latency

    uint8_t	      m_databuf[1 /* reponse */ + 1 /* data tocken */ + 512 /* data block */ + 2 /* CRC */ ];
    uint32_t	      m_datalen_snd; // data size to be sent to host
    uint32_t          m_datalen_rcv; // data size expected from host
    uint32_t	      m_data_idx;
    bool	      m_acmd; // next command will be acmd
    int		      m_sdstate; // sdcard internal state

    // sd states
    enum {
	SD_IDLE = 0,
	SD_READY = 1,
    };

    // methods
    void genMealy();

    void handle_sdmmc_cmd(uint8_t, uint32_t);
    void handle_sdmmc_write(uint8_t, uint32_t);

    //  Master FSM states
    enum {
    S_IDLE               = 0,
    S_RECEIVE_CMD        = 1,
    S_RECEIVE_ARGS_START = 2,
    S_RECEIVE_ARGS       = 3,
    S_RECEIVE_CRC        = 4,
    S_RECEIVE_DATA_WAIT  = 5,
    S_RECEIVE_DATA       = 6,
    S_SEND_DATA      	 = 7,
    };

protected:

    SC_HAS_PROCESS(SdMMC);

public:

    // ports
    sc_in<bool> 					      p_clk;
    sc_in<bool> 					      p_resetn;
    sc_in<bool> 					      p_spi_ss;
    sc_in<bool> 					      p_spi_clk;
    sc_in<bool> 					      p_spi_mosi;
    sc_out<bool> 					      p_spi_miso;

    void print_trace();

    // Constructor   
    SdMMC(
	sc_module_name                      name,
        const std::string                   &filename,
        const uint32_t	                    latency = 0);

    ~SdMMC();

};

}}

#endif /* SOCLIB_SDMMC_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
