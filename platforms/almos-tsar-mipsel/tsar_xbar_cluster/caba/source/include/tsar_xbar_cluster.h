//////////////////////////////////////////////////////////////////////////////
// File: tsar_xbar_cluster.h
// Author: Alain Greiner
// Copyright: UPMC/LIP6
// Date : march 2013
// This program is released under the GNU public license
//////////////////////////////////////////////////////////////////////////////

#ifndef SOCLIB_CABA_TSAR_XBAR_CLUSTER_H
#define SOCLIB_CABA_TSAR_XBAR_CLUSTER_H

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
#include "vci_simple_rom.h"
#include "vci_xicu.h"
#include "dspin_local_crossbar.h"
#include "vci_dspin_initiator_wrapper.h"
#include "vci_dspin_target_wrapper.h"
#include "virtual_dspin_router.h"
#include "vci_multi_tty.h"
#include "vci_multi_nic.h"
#include "vci_chbuf_dma.h"
#include "vci_block_device_tsar.h"
#include "vci_framebuffer.h"
#include "vci_multi_dma.h"
#include "vci_mem_cache.h"
#include "vci_cc_vcache_wrapper.h"
#include "vci_simhelper.h"

namespace soclib { namespace caba {

///////////////////////////////////////////////////////////////////////////
template<size_t dspin_cmd_width,
         size_t dspin_rsp_width,
         typename vci_param_int,
         typename vci_param_ext>  class TsarXbarCluster
///////////////////////////////////////////////////////////////////////////
    : public soclib::caba::BaseModule
{
    public:

    // Used in destructor
    size_t n_procs;

    // Ports
    sc_in<bool>                                     p_clk;
    sc_in<bool>                                     p_resetn;
    soclib::caba::DspinOutput<dspin_cmd_width>      **p_cmd_out;
    soclib::caba::DspinInput<dspin_cmd_width>       **p_cmd_in;
    soclib::caba::DspinOutput<dspin_rsp_width>      **p_rsp_out;
    soclib::caba::DspinInput<dspin_rsp_width>       **p_rsp_in;

    // interrupt signals
    sc_signal<bool>         signal_false;
    sc_signal<bool>         signal_irq_memc;
    sc_signal<bool>         signal_proc_it[8];
    sc_signal<bool>         signal_irq_mdma[8];
    sc_signal<bool>         signal_irq_mtty[23];
    sc_signal<bool>         signal_irq_mnic_rx[8];  // unused
    sc_signal<bool>         signal_irq_mnic_tx[8];  // unused
    sc_signal<bool>         signal_irq_chbuf[8];  // unused
    sc_signal<bool>         signal_irq_bdev;

    // DSPIN signals between DSPIN routers and local_crossbars
    DspinSignals<dspin_cmd_width>   signal_dspin_cmd_l2g_d;
    DspinSignals<dspin_cmd_width>   signal_dspin_cmd_g2l_d;
    DspinSignals<dspin_cmd_width>   signal_dspin_m2p_l2g_c;
    DspinSignals<dspin_cmd_width>   signal_dspin_m2p_g2l_c;
    DspinSignals<dspin_cmd_width>   signal_dspin_clack_l2g_c;
    DspinSignals<dspin_cmd_width>   signal_dspin_clack_g2l_c;
    DspinSignals<dspin_rsp_width>   signal_dspin_rsp_l2g_d;
    DspinSignals<dspin_rsp_width>   signal_dspin_rsp_g2l_d;
    DspinSignals<dspin_rsp_width>   signal_dspin_p2m_l2g_c;
    DspinSignals<dspin_rsp_width>   signal_dspin_p2m_g2l_c;

    // Direct VCI signals to VCI/DSPIN wrappers
    VciSignals<vci_param_int>       signal_vci_ini_proc[8];
    VciSignals<vci_param_int>       signal_vci_ini_mdma;
    VciSignals<vci_param_int>       signal_vci_ini_bdev;
    VciSignals<vci_param_int>       signal_vci_ini_chbuf;

    VciSignals<vci_param_int>       signal_vci_tgt_memc;
    VciSignals<vci_param_int>       signal_vci_tgt_xicu;
    VciSignals<vci_param_int>       signal_vci_tgt_mdma;
    VciSignals<vci_param_int>       signal_vci_tgt_mtty;
    VciSignals<vci_param_int>       signal_vci_tgt_bdev;
    VciSignals<vci_param_int>       signal_vci_tgt_brom;
    VciSignals<vci_param_int>       signal_vci_tgt_fbuf;
    VciSignals<vci_param_int>       signal_vci_tgt_mnic;
    VciSignals<vci_param_int>       signal_vci_tgt_chbuf;
    VciSignals<vci_param_int>       signal_vci_tgt_simh;

    // Direct DSPIN signals to local crossbars
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_proc_i[8];
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_proc_i[8];
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_mdma_i;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_mdma_i;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_bdev_i;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_bdev_i;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_chbuf_i;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_chbuf_i;

    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_memc_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_memc_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_xicu_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_xicu_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_mdma_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_mdma_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_mtty_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_mtty_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_bdev_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_bdev_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_brom_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_brom_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_fbuf_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_fbuf_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_mnic_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_mnic_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_chbuf_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_chbuf_t;
    DspinSignals<dspin_cmd_width>     signal_dspin_cmd_simh_t;
    DspinSignals<dspin_rsp_width>     signal_dspin_rsp_simh_t;

    // Coherence DSPIN signals to local crossbar
    DspinSignals<dspin_cmd_width>     signal_dspin_m2p_memc;
    DspinSignals<dspin_cmd_width>     signal_dspin_clack_memc;
    DspinSignals<dspin_rsp_width>     signal_dspin_p2m_memc;
    DspinSignals<dspin_cmd_width>     signal_dspin_m2p_proc[8];
    DspinSignals<dspin_cmd_width>     signal_dspin_clack_proc[8];
    DspinSignals<dspin_rsp_width>     signal_dspin_p2m_proc[8];

    // external RAM to MEMC VCI signal
    VciSignals<vci_param_ext>         signal_vci_xram;

    // Components

    VciCcVCacheWrapper<vci_param_int,
                       dspin_cmd_width,
                       dspin_rsp_width,
                       GdbServer<Mips32ElIss> >*  proc[8];

    VciDspinInitiatorWrapper<vci_param_int,
                             dspin_cmd_width,
                             dspin_rsp_width>*    wi_proc[8];

    VciMemCache<vci_param_int,
                vci_param_ext,
                dspin_rsp_width,
                dspin_cmd_width>*                 memc;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_memc;

    VciXicu<vci_param_int>*                       xicu;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_xicu;

    VciMultiDma<vci_param_int>*                   mdma;

    VciDspinInitiatorWrapper<vci_param_int,
                             dspin_cmd_width,
                             dspin_rsp_width>*    wi_mdma;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_mdma;

    VciSimpleRam<vci_param_ext>*                  xram;

    VciSimpleRom<vci_param_int>*                  brom;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_brom;

    VciMultiTty<vci_param_int>*                   mtty;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_mtty;

    VciSimhelper<vci_param_int>*                  simhelper;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_simhelper;


    VciFrameBuffer<vci_param_int>*                fbuf;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_fbuf;

    VciMultiNic<vci_param_int>*                   mnic;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_mnic;

    VciChbufDma<vci_param_int>*                   chbuf;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_chbuf;

    VciDspinInitiatorWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wi_chbuf;

    VciBlockDeviceTsar<vci_param_int>*            bdev;

    VciDspinInitiatorWrapper<vci_param_int,
                             dspin_cmd_width,
                             dspin_rsp_width>*    wi_bdev;

    VciDspinTargetWrapper<vci_param_int,
                          dspin_cmd_width,
                          dspin_rsp_width>*       wt_bdev;

    DspinLocalCrossbar<dspin_cmd_width>*          xbar_cmd_d;
    DspinLocalCrossbar<dspin_rsp_width>*          xbar_rsp_d;
    DspinLocalCrossbar<dspin_cmd_width>*          xbar_m2p_c;
    DspinLocalCrossbar<dspin_rsp_width>*          xbar_p2m_c;
    DspinLocalCrossbar<dspin_cmd_width>*          xbar_clack_c;

    VirtualDspinRouter<dspin_cmd_width>*          router_cmd;
    VirtualDspinRouter<dspin_rsp_width>*          router_rsp;

    TsarXbarCluster( sc_module_name                     insname,
                     size_t                             nb_procs,      // processors
                     size_t                             nb_ttys,       // TTY terminals
                     size_t                             nb_dmas,       //  DMA channels
                     size_t                             x,             // x coordinate
                     size_t                             y,             // y coordinate
                     size_t                             cluster,       // y + ymax*x
                     const soclib::common::MappingTable &mtd,          // internal
                     const soclib::common::MappingTable &mtx,          // external
                     size_t                             x_width,       // x field bits
                     size_t                             y_width,       // y field bits
                     size_t                             l_width,       // l field bits
                     size_t                             tgtid_memc,
                     size_t                             tgtid_xicu,
                     size_t                             tgtid_mdma,
                     size_t                             tgtid_fbuf,
                     size_t                             tgtid_mtty,
                     size_t                             tgtid_brom,
                     size_t                             tgtid_mnic,
                     size_t                             tgtid_chbuf,
                     size_t                             tgtid_bdev,
                     size_t                             tgtid_simh,
                     size_t                             memc_ways,
                     size_t                             memc_sets,
                     size_t                             l1_i_ways,
                     size_t                             l1_i_sets,
                     size_t                             l1_d_ways,
                     size_t                             l1_d_sets,
                     size_t                             xram_latency,  // external ram
                     bool                               io,            // I/O cluster
                     size_t                             xfb,           // fbf pixels
                     size_t                             yfb,           // fbf lines
                     char*                              disk_name,     // virtual disk
                     size_t                             block_size,    // block size
                     size_t                             nic_channels,  // number channels
                     char*                              nic_rx_name,   // filename rx
                     char*                              nic_tx_name,   // filename tx
                     uint32_t                           nic_timeout,   // cycles
                     size_t                             chbufdma_channels,  // number channels
                     const Loader                       &loader,
                     uint32_t                           frozen_cycles,
                     uint32_t                           start_debug_cycle,
                     bool                               memc_debug_ok,
                     bool                               proc_debug_ok);

    ~TsarXbarCluster();

};
}}

#endif
