//////////////////////////////////////////////////////////////////////////////
// File: tsar_iob_cluster.h
// Author: Alain Greiner 
// Copyright: UPMC/LIP6
// Date : april 2013
// This program is released under the GNU public license
//////////////////////////////////////////////////////////////////////////////

#ifndef SOCLIB_CABA_TSAR_IOB_CLUSTER_H
#define SOCLIB_CABA_TSAR_IOB_CLUSTER_H

#include <systemc>
#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstdarg>

#include "gdbserver.h"
#include "mapping_table.h"
#include "mips32.h"
#include "vci_simple_ram.h"
#include "vci_xicu.h"
#include "vci_local_crossbar.h"
#include "dspin_local_crossbar.h"
#include "vci_dspin_initiator_wrapper.h"
#include "vci_dspin_target_wrapper.h"
#include "dspin_router.h"
#include "vci_mwmr_dma.h"
#include "vci_mem_cache.h"
#include "vci_cc_vcache_wrapper.h"
#include "vci_io_bridge.h"
#include "coproc_signals.h"
#include "coproc_gcd.h"
#include "coproc_dct.h"
#include "coproc_cpy.h"

namespace soclib { namespace caba   {

///////////////////////////////////////////////////////////////////////////
template<typename vci_param_int, 
         typename vci_param_ext,
         size_t   dspin_int_cmd_width, 
         size_t   dspin_int_rsp_width,
         size_t   dspin_ram_cmd_width,
         size_t   dspin_ram_rsp_width>
class TsarIobCluster 
///////////////////////////////////////////////////////////////////////////
    : public soclib::caba::BaseModule
{

  public:

    // Ports
    sc_in<bool>                                        p_clk;
    sc_in<bool>                                        p_resetn;

    // Thes two ports are used to connect IOB to IOX nework in top cell
    soclib::caba::VciInitiator<vci_param_ext>*         p_vci_iob_iox_ini;
    soclib::caba::VciTarget<vci_param_ext>*            p_vci_iob_iox_tgt; 

    // These arrays of ports are used to connect the INT & RAM networks in top cell
    soclib::caba::DspinOutput<dspin_int_cmd_width>*    p_dspin_int_cmd_out;
    soclib::caba::DspinInput<dspin_int_cmd_width>*     p_dspin_int_cmd_in;
    soclib::caba::DspinOutput<dspin_int_rsp_width>*    p_dspin_int_rsp_out;
    soclib::caba::DspinInput<dspin_int_rsp_width>*     p_dspin_int_rsp_in;
    soclib::caba::DspinOutput<dspin_int_cmd_width>*    p_dspin_int_m2p_out;
    soclib::caba::DspinInput<dspin_int_cmd_width>*     p_dspin_int_m2p_in;
    soclib::caba::DspinOutput<dspin_int_rsp_width>*    p_dspin_int_p2m_out;
    soclib::caba::DspinInput<dspin_int_rsp_width>*     p_dspin_int_p2m_in;
    soclib::caba::DspinOutput<dspin_int_cmd_width>*    p_dspin_int_cla_out;
    soclib::caba::DspinInput<dspin_int_cmd_width>*     p_dspin_int_cla_in;

    soclib::caba::DspinOutput<dspin_ram_cmd_width>*    p_dspin_ram_cmd_out;
    soclib::caba::DspinInput<dspin_ram_cmd_width>*     p_dspin_ram_cmd_in;
    soclib::caba::DspinOutput<dspin_ram_rsp_width>*    p_dspin_ram_rsp_out;
    soclib::caba::DspinInput<dspin_ram_rsp_width>*     p_dspin_ram_rsp_in;

    // interrupt signals
    sc_signal<bool>                       signal_false;
    sc_signal<bool>                       signal_proc_it[32];
    sc_signal<bool>                       signal_irq_mwmr;
    sc_signal<bool>                       signal_irq_memc;
    
    // Coprocessor signals
    CoprocSignals<uint32_t,uint8_t>       signal_to_coproc[8];
    CoprocSignals<uint32_t,uint8_t>       signal_from_coproc[8];
    sc_signal<uint32_t>                   signal_config_coproc[8];
    sc_signal<uint32_t>                   signal_status_coproc[8];

    // INT network DSPIN signals between DSPIN routers and DSPIN local_crossbars
    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_cmd_l2g_d; 
    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_cmd_g2l_d; 

    DspinSignals<dspin_int_rsp_width>     signal_int_dspin_rsp_l2g_d; 
    DspinSignals<dspin_int_rsp_width>     signal_int_dspin_rsp_g2l_d; 

    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_m2p_l2g_c;
    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_m2p_g2l_c; 

    DspinSignals<dspin_int_rsp_width>     signal_int_dspin_p2m_l2g_c;
    DspinSignals<dspin_int_rsp_width>     signal_int_dspin_p2m_g2l_c;

    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_cla_l2g_c;
    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_cla_g2l_c;

    // INT network VCI signals between VCI components and VCI local crossbar
    VciSignals<vci_param_int>             signal_int_vci_ini_proc[8]; 
    VciSignals<vci_param_int>             signal_int_vci_ini_mwmr; 
    VciSignals<vci_param_int>             signal_int_vci_ini_iobx; 

    VciSignals<vci_param_int>             signal_int_vci_tgt_memc;
    VciSignals<vci_param_int>             signal_int_vci_tgt_xicu;
    VciSignals<vci_param_int>             signal_int_vci_tgt_mwmr;
    VciSignals<vci_param_int>             signal_int_vci_tgt_iobx;

    VciSignals<vci_param_int>             signal_int_vci_l2g;
    VciSignals<vci_param_int>             signal_int_vci_g2l;

    // Coherence DSPIN signals between DSPIN local crossbars and CC components 
    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_m2p_memc;
    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_cla_memc;
    DspinSignals<dspin_int_rsp_width>     signal_int_dspin_p2m_memc;
    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_m2p_proc[8];
    DspinSignals<dspin_int_cmd_width>     signal_int_dspin_cla_proc[8];
    DspinSignals<dspin_int_rsp_width>     signal_int_dspin_p2m_proc[8];

    // RAM network VCI signals between VCI components and VCI/DSPIN wrappers
    VciSignals<vci_param_ext>             signal_ram_vci_ini_memc;
    VciSignals<vci_param_ext>             signal_ram_vci_ini_iobx;
    VciSignals<vci_param_ext>             signal_ram_vci_tgt_xram;

    // RAM network DSPIN signals between VCI/DSPIN wrappers, RAM dspin crossbar
    // and routers 
    DspinSignals<dspin_ram_cmd_width>     signal_ram_dspin_cmd_xram_t;
    DspinSignals<dspin_ram_rsp_width>     signal_ram_dspin_rsp_xram_t;
    DspinSignals<dspin_ram_cmd_width>     signal_ram_dspin_cmd_memc_i;
    DspinSignals<dspin_ram_rsp_width>     signal_ram_dspin_rsp_memc_i;
    DspinSignals<dspin_ram_cmd_width>     signal_ram_dspin_cmd_iob_i;
    DspinSignals<dspin_ram_rsp_width>     signal_ram_dspin_rsp_iob_i;
    DspinSignals<dspin_ram_cmd_width>     signal_ram_dspin_cmd_xbar;
    DspinSignals<dspin_ram_rsp_width>     signal_ram_dspin_rsp_xbar;
    DspinSignals<dspin_ram_cmd_width>     signal_ram_dspin_cmd_false;
    DspinSignals<dspin_ram_rsp_width>     signal_ram_dspin_rsp_false;
  
    //////////////////////////////////////
    // Hardwate Components (pointers)
    //////////////////////////////////////
    VciCcVCacheWrapper<vci_param_int, 
                       dspin_int_cmd_width,
                       dspin_int_rsp_width,
                       GdbServer<Mips32ElIss> >*      proc[8];

    VciMemCache<vci_param_int,
                vci_param_ext, 
                dspin_int_rsp_width, 
                dspin_int_cmd_width>*                 memc;

    VciDspinInitiatorWrapper<vci_param_ext,
                             dspin_ram_cmd_width,
                             dspin_ram_rsp_width>*    memc_ram_wi;

    VciXicu<vci_param_int>*                           xicu;

    VciMwmrDma<vci_param_int>*                        mwmr;

    CoprocGcd*                                        gcd;
    CoprocDct*                                        dct;
    CoprocCpy*                                        cpy;

    VciLocalCrossbar<vci_param_int>*                  int_xbar_d;
    
    VciDspinInitiatorWrapper<vci_param_int,
                             dspin_int_cmd_width,
                             dspin_int_rsp_width>*    int_wi_gate_d;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_int_cmd_width,
                          dspin_int_rsp_width>*       int_wt_gate_d;

    DspinLocalCrossbar<dspin_int_cmd_width>*          int_xbar_m2p_c;
    DspinLocalCrossbar<dspin_int_rsp_width>*          int_xbar_p2m_c;
    DspinLocalCrossbar<dspin_int_cmd_width>*          int_xbar_clack_c;

    DspinRouter<dspin_int_cmd_width>*                 int_router_cmd;
    DspinRouter<dspin_int_rsp_width>*                 int_router_rsp;
    DspinRouter<dspin_int_cmd_width>*                 int_router_m2p;
    DspinRouter<dspin_int_rsp_width>*                 int_router_p2m;
    DspinRouter<dspin_int_cmd_width>*                 int_router_cla;

    VciSimpleRam<vci_param_ext>*                      xram;

    VciDspinTargetWrapper<vci_param_ext,
                          dspin_ram_cmd_width,
                          dspin_ram_rsp_width>*       xram_ram_wt;
    
    DspinRouter<dspin_ram_cmd_width>*                 ram_router_cmd;
    DspinRouter<dspin_ram_rsp_width>*                 ram_router_rsp;

    DspinLocalCrossbar<dspin_ram_cmd_width>*          ram_xbar_cmd;
    DspinLocalCrossbar<dspin_ram_rsp_width>*          ram_xbar_rsp;
    

    // IO Network Components (not instanciated in all clusters)

    VciIoBridge<vci_param_int,
                vci_param_ext>*                       iob;

    VciDspinInitiatorWrapper<vci_param_ext,
                             dspin_ram_cmd_width,
                             dspin_ram_rsp_width>*    iob_ram_wi;

    // cluster constructor
    TsarIobCluster( sc_module_name                     insname,
                    size_t                             nb_procs,   
                    size_t                             x,             // x coordinate
                    size_t                             y,             // y coordinate
                    size_t                             xmax,
                    size_t                             ymax,

                    const soclib::common::MappingTable &mt_int,
                    const soclib::common::MappingTable &mt_ext,
                    const soclib::common::MappingTable &mt_iox,

                    size_t                             x_width,   // x field  bits
                    size_t                             y_width,   // y field  bits
                    size_t                             l_width,   // l field  bits
                    size_t                             p_width,   // p field  bits

                    size_t                             int_memc_tgt_id,
                    size_t                             int_xicu_tgt_id,
                    size_t                             int_mwmr_tgt_id,
                    size_t                             int_iobx_tgt_id,
                    size_t                             int_proc_ini_id,
                    size_t                             int_mwmr_ini_id,
                    size_t                             int_iobx_ini_id,

                    size_t                             ram_xram_tgt_id,
                    size_t                             ram_memc_ini_id,
                    size_t                             ram_iobx_ini_id,

                    bool                               is_io,
                    size_t                             iox_iobx_tgt_id,
                    size_t                             iox_iobx_ini_id,

                    size_t                             memc_ways,
                    size_t                             memc_sets,
                    size_t                             l1_i_ways,
                    size_t                             l1_i_sets, 
                    size_t                             l1_d_ways,
                    size_t                             l1_d_sets,   
                    size_t                             xram_latency, 
                    size_t                             xcu_nb_hwi,
                    size_t                             xcu_nb_pti,
                    size_t                             xcu_nb_wti,
                    size_t                             xcu_nb_irq,

                    size_t                             coproc_type,

                    const Loader                       &loader,  // loader for XRAM

                    uint32_t                           frozen_cycles, 
                    uint32_t                           start_debug_cycle,
                    bool                               memc_debug_ok, 
                    bool                               proc_debug_ok, 
                    bool                               iob0_debug_ok ); 

  protected:

    SC_HAS_PROCESS(TsarIobCluster);

    void init();
 
};

}}

#endif

// Local Variables:
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3
//
