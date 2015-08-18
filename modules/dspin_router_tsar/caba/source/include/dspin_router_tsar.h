/* -*- c++ -*-
  *
  * File : dspin_router_tsar.h
  * Copyright (c) UPMC, Lip6
  * Authors : Alain Greiner 
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
  */

////////////////////////////////////////////////////////////////////////////////
// This component implements a variant of the standard (SocLib) DSPIN router:
// The routing function has been modified to handle the special case of
// cluster_iob0 (containing component IOB0) and cluster_iob1 (containing
// component IOB1). In those two cluster, the response router must decode
// both the SRCID global bits AND the SRCID local bits to distinguish
// between the IOB and MEMC initiators.
// This component contains the following modifications:
// - 4 extra constructor arguments,
// - 6 new member variables
// - a modified routing function
//////////////////////////////////////////////////////////////////////////////// 

#ifndef DSPIN_ROUTER_TSAR_H_
#define DSPIN_ROUTER_TSAR_H_

#include <systemc>
#include "caba_base_module.h"
#include "generic_fifo.h"
#include "dspin_interface.h"
#include "alloc_elems.h"

namespace soclib { namespace caba {

using namespace sc_core;

template<int flit_width>
class DspinRouterTsar
: public soclib::caba::BaseModule
{
	// Port indexing
	enum 
    {
		DSPIN_NORTH	= 0,
		DSPIN_SOUTH	= 1,
		DSPIN_EAST	= 2,
		DSPIN_WEST	= 3,
		DSPIN_LOCAL	= 4,
	};

    // Input Port FSM
    enum 
    {
        INFSM_IDLE,
        INFSM_REQ,
        INFSM_ALLOC,
    };

    protected:
    SC_HAS_PROCESS(DspinRouterTsar);

    public:

	// ports
	sc_in<bool>                 p_clk;
	sc_in<bool>                 p_resetn;
	DspinInput<flit_width>      *p_in;
	DspinOutput<flit_width>	    *p_out;

	// constructor
	DspinRouterTsar(
                sc_module_name name, 
                const size_t   x,              // x coordinate
                const size_t   y,              // y cordinate
                const size_t   x_width,        // x field width in first flit
                const size_t   y_width,        // y field width in first flit
                const size_t   in_fifo_depth,  // input fifo depth
                const size_t   out_fifo_depth, // output fifo depth
                const bool     is_iob0,        // cluster containing IOB0
                const bool     is_iob1,        // cluster containing IOB0
                const bool     is_rsp,         // only response router is modified
                const size_t   l_width);       // local srcid width
    private:

    // define the FIFO flit
    typedef struct internal_flit_s 
    {
        sc_uint<flit_width>  data;
        bool                 eop;
    } internal_flit_t;
    
    // registers
	sc_signal<bool>				*r_alloc_out;
	sc_signal<size_t>           *r_index_out;
    sc_signal<int>              *r_fsm_in;
	sc_signal<size_t>           *r_index_in;

	// fifos
	soclib::caba::GenericFifo<internal_flit_t>*  r_fifo_in;
	soclib::caba::GenericFifo<internal_flit_t>*  r_fifo_out;

	// structural parameters
	size_t	                    m_local_x;
	size_t	                    m_local_y;
	size_t	                    m_x_width;
	size_t	                    m_x_shift;
	size_t	                    m_x_mask;
	size_t	                    m_y_width;
	size_t	                    m_y_shift;
	size_t	                    m_y_mask;
	size_t	                    m_l_width;
	size_t	                    m_l_shift;
	size_t	                    m_l_mask;
    bool                        m_is_iob0;
    bool                        m_is_iob1;
    bool                        m_is_rsp;

    // methods 
    void    transition();
    void    genMoore();
    size_t  route( sc_uint<flit_width> data );

    public:

    void    print_trace();
};

}} // end namespace
               
#endif // DSPIN_ROUTER_TSAR_H_

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
