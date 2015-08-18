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
 *         Alain Greiner <nipo@ssji.net>, 2013
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

#ifndef VCI_IOX_NETWORK_H
#define VCI_IOX_NETWORK_H

#include <systemc>
#include "caba_base_module.h"
#include "vci_initiator.h"
#include "vci_target.h"
#include "vci_buffers.h"
#include "mapping_table.h"
#include "address_decoding_table.h"

namespace soclib { namespace caba {

using namespace sc_core;
using namespace soclib::common;

template<typename pkt_t> class IoXbar;

///////////////////////////////////////
template<typename vci_param>
class VciIoxNetwork
///////////////////////////////////////
    : public BaseModule
{

public:

    sc_in<bool>                              p_clk;
    sc_in<bool>                              p_resetn;
    VciInitiator<vci_param>*                 p_to_tgt;
    VciTarget<vci_param>*                    p_to_ini;

private:

    typedef IoXbar<VciCmdBuffer<vci_param> > cmd_xbar_t;
    typedef IoXbar<VciRspBuffer<vci_param> > rsp_xbar_t;

    size_t                                   m_nb_tgt;
    size_t                                   m_nb_ini;

    VciInitiator<vci_param>**                m_ports_to_target;
    VciTarget<vci_param>**                   m_ports_to_initiator;

  	rsp_xbar_t*                              m_rsp_xbar;  // RSP crossbar
  	cmd_xbar_t*                              m_cmd_xbar;  // CMD crossbar

    AddressDecodingTable<uint64_t,size_t>    m_cmd_rt;    // routing table for CMD
    AddressDecodingTable<uint32_t,size_t>    m_rsp_rt;    // routing table for RSP

    void transition();

    void genMealy_cmd_val();
    void genMealy_cmd_ack();
    void genMealy_rsp_val();
    void genMealy_rsp_ack();

protected:
    SC_HAS_PROCESS(VciIoxNetwork);

public:
    void print_trace();

    VciIoxNetwork( sc_module_name                      name,
				   const soclib::common::MappingTable  &mt,
				   size_t                              nb_tgt,
				   size_t                              nb_ini );

    ~VciIoxNetwork();
};

}}

#endif 

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
