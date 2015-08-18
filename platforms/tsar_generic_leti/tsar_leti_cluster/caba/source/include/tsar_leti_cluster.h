//////////////////////////////////////////////////////////////////////////////
// File: tsar_leti_cluster.h
// Author: Alain Greiner
// Copyright: UPMC/LIP6
// Date : march 2013
// This program is released under the GNU public license
//////////////////////////////////////////////////////////////////////////////

#ifndef SOCLIB_CABA_TSAR_LETI_CLUSTER_H
#define SOCLIB_CABA_TSAR_LETI_CLUSTER_H

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
#include "vci_multi_tty.h"
#include "vci_block_device_tsar.h"
#include "vci_mem_cache.h"
#include "vci_cc_vcache_wrapper.h"

namespace soclib { namespace caba {

///////////////////////////////////////////////////////////////////////////
template<size_t dspin_cmd_width,
         size_t dspin_rsp_width,
         typename vci_param_int,
         typename vci_param_ext>  class TsarLetiCluster
///////////////////////////////////////////////////////////////////////////
    : public soclib::caba::BaseModule
{
    public:

    // Used in destructor
    size_t m_nprocs;

    // Ports
    sc_in<bool>                                     p_clk;
    sc_in<bool>                                     p_resetn;

    soclib::caba::DspinOutput<dspin_cmd_width>      *p_cmd_out;
    soclib::caba::DspinInput<dspin_cmd_width>       *p_cmd_in;

    soclib::caba::DspinOutput<dspin_rsp_width>      *p_rsp_out;
    soclib::caba::DspinInput<dspin_rsp_width>       *p_rsp_in;

    soclib::caba::DspinOutput<dspin_cmd_width>      *p_m2p_out;
    soclib::caba::DspinInput<dspin_cmd_width>       *p_m2p_in;

    soclib::caba::DspinOutput<dspin_rsp_width>      *p_p2m_out;
    soclib::caba::DspinInput<dspin_rsp_width>       *p_p2m_in;

    soclib::caba::DspinOutput<dspin_cmd_width>      *p_cla_out;
    soclib::caba::DspinInput<dspin_cmd_width>       *p_cla_in;

    // interrupt signals
    sc_signal<bool>         signal_false;
    sc_signal<bool>         signal_proc_irq[16];
    sc_signal<bool>         signal_irq_mtty;
    sc_signal<bool>         signal_irq_memc;
    sc_signal<bool>         signal_irq_bdev;

    // DSPIN signals between DSPIN routers and local_crossbars
    DspinSignals<dspin_cmd_width>   signal_dspin_cmd_l2g_d;
    DspinSignals<dspin_cmd_width>   signal_dspin_cmd_g2l_d;
    DspinSignals<dspin_rsp_width>   signal_dspin_rsp_l2g_d;
    DspinSignals<dspin_rsp_width>   signal_dspin_rsp_g2l_d;
    DspinSignals<dspin_cmd_width>   signal_dspin_m2p_l2g_c;
    DspinSignals<dspin_cmd_width>   signal_dspin_m2p_g2l_c;
    DspinSignals<dspin_cmd_width>   signal_dspin_clack_l2g_c;
    DspinSignals<dspin_cmd_width>   signal_dspin_clack_g2l_c;
    DspinSignals<dspin_rsp_width>   signal_dspin_p2m_l2g_c;
    DspinSignals<dspin_rsp_width>   signal_dspin_p2m_g2l_c;

    // Direct VCI signals
    VciSignals<vci_param_int>       signal_vci_ini_proc[4];
    VciSignals<vci_param_int>       signal_vci_ini_mdma;
    VciSignals<vci_param_int>       signal_vci_ini_bdev;
    VciSignals<vci_param_int>       signal_vci_ini_chbuf;

    VciSignals<vci_param_int>       signal_vci_tgt_memc;
    VciSignals<vci_param_int>       signal_vci_tgt_xicu;
    VciSignals<vci_param_int>       signal_vci_tgt_mdma;
    VciSignals<vci_param_int>       signal_vci_tgt_mtty;
    VciSignals<vci_param_int>       signal_vci_tgt_bdev;
    VciSignals<vci_param_int>       signal_vci_tgt_fbuf;
    VciSignals<vci_param_int>       signal_vci_tgt_mnic;
    VciSignals<vci_param_int>       signal_vci_tgt_chbuf;
    VciSignals<vci_param_int>       signal_vci_tgt_simh;

    VciSignals<vci_param_int>       signal_vci_g2l;
    VciSignals<vci_param_int>       signal_vci_l2g;

    // Coherence DSPIN signals to local crossbar
    DspinSignals<dspin_cmd_width>     signal_dspin_m2p_memc;
    DspinSignals<dspin_cmd_width>     signal_dspin_clack_memc;
    DspinSignals<dspin_rsp_width>     signal_dspin_p2m_memc;
    DspinSignals<dspin_cmd_width>     signal_dspin_m2p_proc[4];
    DspinSignals<dspin_cmd_width>     signal_dspin_clack_proc[4];
    DspinSignals<dspin_rsp_width>     signal_dspin_p2m_proc[4];

    // external RAM to MEMC VCI signal
    VciSignals<vci_param_ext>         signal_vci_xram;

    // Components

    VciCcVCacheWrapper<vci_param_int,
                       dspin_cmd_width,
                       dspin_rsp_width,
                       GdbServer<Mips32ElIss> >*  proc[4];

    VciMemCache<vci_param_int,
                vci_param_ext,
                dspin_rsp_width,
                dspin_cmd_width>*                 memc;

    VciXicu<vci_param_int>*                       xicu;

    VciSimpleRam<vci_param_ext>*                  xram;

    VciMultiTty<vci_param_int>*                   mtty;

    VciBlockDeviceTsar<vci_param_int>*            bdev;

    VciLocalCrossbar<vci_param_int>*              xbar_cmd;

    VciDspinInitiatorWrapper<vci_param_int,
                             dspin_cmd_width,
                             dspin_rsp_width>*    wi_gate;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_gate;

    DspinLocalCrossbar<dspin_cmd_width>*          xbar_m2p;
    DspinLocalCrossbar<dspin_rsp_width>*          xbar_p2m;
    DspinLocalCrossbar<dspin_cmd_width>*          xbar_cla;

    DspinRouter<dspin_cmd_width>*                 router_cmd;
    DspinRouter<dspin_rsp_width>*                 router_rsp;
    DspinRouter<dspin_cmd_width>*                 router_m2p;
    DspinRouter<dspin_rsp_width>*                 router_p2m;
    DspinRouter<dspin_cmd_width>*                 router_cla;

    TsarLetiCluster( sc_module_name                     insname,
                     size_t                             nb_procs,      // processors
                     size_t                             x,             // x coordinate
                     size_t                             y,             // y coordinate
                     size_t                             cluster,       // y + ymax*x
                     const soclib::common::MappingTable &mtd,          // internal
                     const soclib::common::MappingTable &mtx,          // external
                     uint32_t                           reset_address, // boot address
                     size_t                             x_width,       // x field bits
                     size_t                             y_width,       // y field bits
                     size_t                             l_width,       // l field bits
                     size_t                             p_width,       // p field bits
                     size_t                             tgtid_memc,
                     size_t                             tgtid_xicu,
                     size_t                             tgtid_mtty,
                     size_t                             tgtid_bdev,
                     const char*                        disk_pathname,
                     size_t                             memc_ways,
                     size_t                             memc_sets,
                     size_t                             l1_i_ways,
                     size_t                             l1_i_sets,
                     size_t                             l1_d_ways,
                     size_t                             l1_d_sets,
                     size_t                             xram_latency,  // external ram
                     const Loader                       &loader,
                     uint32_t                           frozen_cycles,
                     uint32_t                           trace_start_cycle,
                     bool                               trace_proc_ok,
                     uint32_t                           trace_proc_id,
                     bool                               trace_memc_ok,
                     uint32_t                           trace_memc_id );

    ~TsarLetiCluster();

};
}}

#endif
