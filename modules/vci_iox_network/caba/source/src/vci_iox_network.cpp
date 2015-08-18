/*
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
 *         Alain Greiner <alain.greiner@lip6.fr> 2013
 *
 * Maintainers: alain
 */

////////////////////////////////////////////////////////////////////////////////
// This component emulates an external IO bus such as PCIe or Hypertransport,
// but respect the VCI protocol. It can be attached to one OR SEVERAL clusters,
// using a vci_io_bridge component.
// It is considered as a local interconnect, for the ADDRESS or SRCID
// decoding tables: 
// - the CMD routing_table decodes the local field of the VCI ADDRESS 
//   to return the local target port
// - The RSP routing_table decodes the local field of the VCI SRCID
//   to return the initator port 
// It is implemented as two independant crossbars, for VCI commands and 
// VCI responses respectively.
// - The CMD crossbar has nb_ini input ports, and nb_tgt output ports,
//   including the ports to the vci_io_bridge component(s).
// - The RSP crossbar has nb_tgt input ports, and nb_ini output ports.
//   including the ports to the vci_io_bridge component(s).
// For both crossbars, output ports allocation policy is round robin.
////////////////////////////////////////////////////////////////////////////////

#include <systemc>
#include <cassert>
#include "vci_buffers.h"
#include "../include/vci_iox_network.h"
#include "alloc_elems.h"

namespace soclib { namespace caba {

using soclib::common::alloc_elems;
using soclib::common::dealloc_elems;

using namespace sc_core;

///////////////////////////////////////////////////////////////////////////////////
// The IoXbar class is instanciated twice, to implement the CMD & RSP XBARs
///////////////////////////////////////////////////////////////////////////////////
template<typename pkt_t> class IoXbar
{
    // pkt_t type must be VciCmdBuffer<vci_param> or VciRspBuffer<vci_param>
    // the two following types can be found in these two classes

    typedef typename pkt_t::input_port_t    input_port_t;
    typedef typename pkt_t::output_port_t   output_port_t;

    const bool            m_is_cmd;         // CMD XBAR if true
    const size_t          m_inputs;         // number of inputs 
    const size_t          m_outputs;        // number of outputs 
    void*                 m_rt;             // pointer on routing table (CMD or RSP)

    sc_signal<bool>*      r_out_allocated;  // for each output: allocation state 
    sc_signal<size_t>*    r_out_origin;     // for each output: input port index
    sc_signal<bool>*      r_in_allocated;   // for each input: allocation state 
    sc_signal<size_t>*    r_in_dest;        // for each input: output port index

public:
    ///////////////////////
    IoXbar( bool   is_cmd, 
            size_t nb_inputs,
            size_t nb_outputs,
            void*  rt )
    : m_is_cmd( is_cmd ),
      m_inputs( nb_inputs ),
      m_outputs( nb_outputs ),
      m_rt ( rt )
    {
        r_out_allocated = new sc_signal<bool>[nb_outputs];
        r_out_origin    = new sc_signal<size_t>[nb_outputs];
        r_in_allocated  = new sc_signal<bool>[nb_inputs];
        r_in_dest       = new sc_signal<size_t>[nb_inputs];
    }

    ////////////
    void reset()
    {
        for (size_t i=0 ; i<m_outputs ; ++i) 
        {
            r_out_origin[i]    = 0;
            r_out_allocated[i] = false;
        }
        for (size_t i=0 ; i<m_inputs ; ++i) 
        {
            r_in_dest[i]      = 0;
            r_in_allocated[i] = false;
        }
    }

    //////////////////
    void print_trace()
    {
        if ( m_is_cmd )
        {
            std::cout << "IOX_CMD_XBAR : " << std::dec;
            for( size_t out=0 ; out<m_outputs ; out++)
            {
                if( r_out_allocated[out].read() ) 
                {
                    std::cout << "ini " << r_out_origin[out].read() 
                              << " => tgt " << out << " | ";
                }
            }
        }
        else
        {
            std::cout << "IOX_RSP_XBAR : " << std::dec;
            for( size_t out=0 ; out<m_outputs ; out++)
            {
                if( r_out_allocated[out].read() ) 
                {
                    std::cout << "tgt " << r_out_origin[out].read() 
                              << " => ini " << out << " | ";
                }
            }
        }
        std::cout << std::endl;
    }

    ///////////////////////////////////////////
    void transition( input_port_t  **input_port, 
                     output_port_t **output_port )
    {
        // loop on the output ports
        for ( size_t out = 0; out < m_outputs; out++) 
        {
            if ( r_out_allocated[out].read() )          // possible desallocation
            {
                if ( output_port[out]->toPeerEnd() )    
                {
                    size_t in = r_out_origin[out].read();
                    r_out_allocated[out] = false;
                    r_in_allocated[in]   = false;
                }
            } 
            else                                        // possible allocation
            {
                // loop on the intput ports
                bool found = false;
                for(size_t x = 0 ; (x < m_inputs) and not found ; x++) 
                {
                    size_t in = (x + r_out_origin[out] + 1) % m_inputs;
                    if ( input_port[in]->getVal() )
                    {
                        size_t req;       // requested output port index
                        if ( m_is_cmd ) 
                        {
                            AddressDecodingTable<uint64_t, size_t>* rt = 
                               (AddressDecodingTable<uint64_t, size_t>*)m_rt;
                            req = rt->get_value((uint64_t)(input_port[in]->address.read())); 
                        }
                        else            
                        {
                            AddressDecodingTable<uint32_t, size_t>* rt = 
                               (AddressDecodingTable<uint32_t, size_t>*)m_rt;
                            req = rt->get_value((uint32_t)(input_port[in]->rsrcid.read())); 
                        }
                        // allocate the output port if requested
                        if ( out == req )
                        {
                            r_out_allocated[out] = true;
                            r_in_allocated[in]   = true;
                            r_out_origin[out]    = in;
                            r_in_dest[in]        = out;
                            found                = true;
                        }
                    }
                }  // end loop inputs
            }
        }  // end loop outputs
    } // end transition

    //////////////////////////////////////////////
    void genMealy_ack( input_port_t  **input_port,
                       output_port_t **output_port )
    {
        for ( size_t in = 0 ; in < m_inputs ; in++ )
        {
            size_t out = r_in_dest[in].read();
            bool   ack = r_in_allocated[in].read() and output_port[out]->getAck();
            input_port[in]->setAck(ack);
        }
    }

    ///////////////////////////////////////////////////////////////////////
    void genMealy_val( input_port_t  **input_port, 
                       output_port_t **output_port )
    {
        for( size_t out = 0 ; out < m_outputs ; out++) 
        {
            if (r_out_allocated[out]) 
            {
                size_t in = r_out_origin[out];
                pkt_t tmp;
                tmp.readFrom(*input_port[in]);
                tmp.writeTo(*output_port[out]);
            } 
            else 
            {
                output_port[out]->setVal(false);
            }
        }
    } 

}; // end class IoXbar


///////////////////////////////////////////////////////////////////////////////////
#define tmpl(x) template<typename vci_param> x VciIoxNetwork<vci_param>
///////////////////////////////////////////////////////////////////////////////////

/////////////////////////
tmpl(void)::print_trace()
{
    m_cmd_xbar->print_trace();
    m_rsp_xbar->print_trace();
}

////////////////////////
tmpl(void)::transition()
{
    if ( ! p_resetn.read() ) 
    {
        m_cmd_xbar->reset();
        m_rsp_xbar->reset();
        return;
    }

    m_cmd_xbar->transition( m_ports_to_initiator, m_ports_to_target );
    m_rsp_xbar->transition( m_ports_to_target, m_ports_to_initiator );
}

//////////////////////////////
tmpl(void)::genMealy_cmd_val()
{
    m_cmd_xbar->genMealy_val( m_ports_to_initiator, m_ports_to_target );
}

//////////////////////////////
tmpl(void)::genMealy_cmd_ack()
{
    m_cmd_xbar->genMealy_ack( m_ports_to_initiator, m_ports_to_target );
}

//////////////////////////////
tmpl(void)::genMealy_rsp_val()
{
    m_rsp_xbar->genMealy_val( m_ports_to_target, m_ports_to_initiator );
}

//////////////////////////////
tmpl(void)::genMealy_rsp_ack()
{
    m_rsp_xbar->genMealy_ack( m_ports_to_target, m_ports_to_initiator );
}

///////////////////////////////////////////////////////////////////////////
tmpl(/**/)::VciIoxNetwork( sc_core::sc_module_name             name,
                           const soclib::common::MappingTable  &mt,
                           size_t                              nb_tgt,
                           size_t                              nb_ini )
           : BaseModule(name),
           p_clk("clk"),
           p_resetn("resetn"),
           p_to_tgt(soclib::common::alloc_elems<VciInitiator<vci_param> >(
                          "p_to_tgt", nb_tgt)),
           p_to_ini(soclib::common::alloc_elems<VciTarget<vci_param> >(
                          "p_to_ini", nb_ini)),

           m_nb_tgt( nb_tgt ),
           m_nb_ini( nb_ini ),

           m_cmd_rt( mt.getLocalIndexFromAddress(0) ),
           m_rsp_rt( mt.getLocalIndexFromSrcid(0) )
{
    std::cout << "    Building VciIoxNetwork : " << name << std::endl;

    SC_METHOD(transition);
    dont_initialize();
    sensitive << p_clk.pos();

    SC_METHOD(genMealy_cmd_val);        // controls to targets CMDVAL
    dont_initialize();
    sensitive << p_clk.neg();
    for ( size_t i=0; i<nb_ini; ++i )   sensitive << p_to_ini[i];

    SC_METHOD(genMealy_cmd_ack);        // controls to intiators CMDACK
    dont_initialize();
    sensitive << p_clk.neg();
    for ( size_t i=0; i<nb_tgt; ++i )   sensitive << p_to_tgt[i];

    SC_METHOD(genMealy_rsp_val);        // controls to initiators RSPVAL
    dont_initialize();
    sensitive << p_clk.neg();
    for ( size_t i=0; i<nb_tgt; ++i )   sensitive << p_to_tgt[i];

    SC_METHOD(genMealy_rsp_ack);        // controls to targets RSPACK
    dont_initialize();
    sensitive << p_clk.neg();
    for ( size_t i=0; i<nb_ini; ++i )   sensitive << p_to_ini[i];

    // constructing CMD & RSP XBARs
    m_rsp_xbar = new rsp_xbar_t( false,         // RSP XBAR
                                 nb_tgt,
                                 nb_ini,
                                 &m_rsp_rt );

    m_cmd_xbar = new cmd_xbar_t( true,          // CMD XBAR
                                 nb_ini,
                                 nb_tgt,
                                 &m_cmd_rt );

    // constructing  CMD & RSP XBARs input & output ports (pointers)
    m_ports_to_initiator = new VciTarget<vci_param>*[nb_ini];
    for (size_t g = 0 ; g < nb_ini ; g++) m_ports_to_initiator[g] = &p_to_ini[g];

    m_ports_to_target = new VciInitiator<vci_param>*[nb_tgt];
    for (size_t g = 0 ; g < nb_tgt ; g++) m_ports_to_target[g] = &p_to_tgt[g];
}

////////////////////////////
tmpl(/**/)::~VciIoxNetwork()
{
    soclib::common::dealloc_elems(p_to_ini, m_nb_ini);
    soclib::common::dealloc_elems(p_to_tgt, m_nb_tgt);
}

}}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
