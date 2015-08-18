//////////////////////////////////////////////////////////////////////////////
// File: tsar_leti_cluster.cpp
// Author: Alain Greiner
// Copyright: UPMC/LIP6
// Date : february 2014
// This program is released under the GNU public license
//////////////////////////////////////////////////////////////////////////////

#include "../include/tsar_leti_cluster.h"

namespace soclib {
namespace caba  {

////////////////////////////////////////////////////////////////////////////////////
template<size_t dspin_cmd_width,
         size_t dspin_rsp_width,
         typename vci_param_int,
         typename vci_param_ext> TsarLetiCluster<dspin_cmd_width,
                                                 dspin_rsp_width,
                                                 vci_param_int,
                                                 vci_param_ext>::TsarLetiCluster(
////////////////////////////////////////////////////////////////////////////////////
         sc_module_name                     insname,
         size_t                             nb_procs,
         size_t                             x_id,
         size_t                             y_id,
         size_t                             cluster_xy,
         const soclib::common::MappingTable &mtd,
         const soclib::common::MappingTable &mtx,
         uint32_t                           reset_address,
         size_t                             x_width,
         size_t                             y_width,
         size_t                             l_width,
         size_t                             p_width,
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
         size_t                             xram_latency,
         const Loader                      &loader,
         uint32_t                           frozen_cycles,
         uint32_t                           trace_start_cycle,
         bool                               trace_proc_ok,
         uint32_t                           trace_proc_id,
         bool                               trace_memc_ok,
         uint32_t                           trace_memc_id )
            : soclib::caba::BaseModule(insname),
            m_nprocs(nb_procs),
            p_clk("clk"),
            p_resetn("resetn")

{
    /////////////////////////////////////////////////////////////////////////////
    // Vectors of ports definition and allocation
    /////////////////////////////////////////////////////////////////////////////

    p_cmd_in  = alloc_elems<DspinInput<dspin_cmd_width> >  ("p_cmd_in",  4);
    p_cmd_out = alloc_elems<DspinOutput<dspin_cmd_width> > ("p_cmd_out", 4);

    p_rsp_in  = alloc_elems<DspinInput<dspin_rsp_width> >  ("p_rsp_in",  4);
    p_rsp_out = alloc_elems<DspinOutput<dspin_rsp_width> > ("p_rsp_out", 4);

    p_m2p_in  = alloc_elems<DspinInput<dspin_cmd_width> >  ("p_m2p_in",  4);
    p_m2p_out = alloc_elems<DspinOutput<dspin_cmd_width> > ("p_m2p_out", 4);

    p_p2m_in  = alloc_elems<DspinInput<dspin_rsp_width> >  ("p_p2m_in",  4);
    p_p2m_out = alloc_elems<DspinOutput<dspin_rsp_width> > ("p_p2m_out", 4);

    p_cla_in  = alloc_elems<DspinInput<dspin_cmd_width> >  ("p_cla_in",  4);
    p_cla_out = alloc_elems<DspinOutput<dspin_cmd_width> > ("p_cla_out", 4);

    /////////////////////////////////////////////////////////////////////////////
    // Components definition and allocation
    /////////////////////////////////////////////////////////////////////////////

    // The processor is a MIPS32 wrapped in the GDB server
    // the reset address is defined by the reset_address argument
    typedef GdbServer<Mips32ElIss> mips_iss;
    mips_iss::setResetAddress( reset_address );

    for (size_t p = 0; p < nb_procs; p++)
    {
        uint32_t global_proc_id  = (cluster_xy << p_width) + p;
        uint32_t global_cc_id    = (cluster_xy << l_width) + p;
        bool     trace_ok        = trace_proc_ok and (trace_proc_id == global_proc_id);

        std::ostringstream sproc;
        sproc << "proc_" << x_id << "_" << y_id << "_" << p;
        proc[p] = new VciCcVCacheWrapper<vci_param_int,
                                         dspin_cmd_width,
                                         dspin_rsp_width,
                                         mips_iss >(
                      sproc.str().c_str(),
                      global_proc_id,                 // GLOBAL PROC_ID
                      mtd,                            // Mapping Table
                      IntTab(cluster_xy,p),           // SRCID
                      global_cc_id,                   // GLOBAL_CC_ID
                      8,                              // ITLB ways
                      8,                              // ITLB sets
                      8,                              // DTLB ways
                      8,                              // DTLB sets
                      l1_i_ways,l1_i_sets, 16,        // ICACHE size
                      l1_d_ways,l1_d_sets, 16,        // DCACHE size
                      4,                              // WBUF nlines
                      4,                              // WBUF nwords
                      x_width,
                      y_width,
                      frozen_cycles,                  // max frozen cycles
                      trace_start_cycle,
                      trace_ok );
    }

    /////////////////////////////////////////////////////////////////////////////
    bool trace_ok = trace_memc_ok and (trace_memc_id == cluster_xy);
    std::ostringstream smemc;
    smemc << "memc_" << x_id << "_" << y_id;
    memc = new VciMemCache<vci_param_int,
                           vci_param_ext,
                           dspin_rsp_width,
                           dspin_cmd_width>(
                     smemc.str().c_str(),
                     mtd,                                // Mapping Table direct space
                     mtx,                                // Mapping Table external space
                     IntTab(cluster_xy),                 // SRCID external space
                     IntTab(cluster_xy, tgtid_memc),     // TGTID direct space
                     x_width,                            // Number of x bits in platform
                     y_width,                            // Number of y bits in platform
                     memc_ways, memc_sets, 16,           // CACHE SIZE
                     3,                                  // MAX NUMBER OF COPIES
                     4096,                               // HEAP SIZE
                     8,                                  // TRANSACTION TABLE DEPTH
                     8,                                  // UPDATE TABLE DEPTH
                     8,                                  // INVALIDATE TABLE DEPTH
                     trace_start_cycle,
                     trace_ok );

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream sxram;
    sxram << "xram_" << x_id << "_" << y_id;
    xram = new VciSimpleRam<vci_param_ext>(
                     sxram.str().c_str(),
                     IntTab(cluster_xy),
                     mtx,
                     loader,
                     xram_latency);

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream sxicu;
    sxicu << "xicu_" << x_id << "_" << y_id;
    xicu = new VciXicu<vci_param_int>(
                     sxicu.str().c_str(),
                     mtd,                               // mapping table
                     IntTab(cluster_xy, tgtid_xicu),    // TGTID_D
                     16,                                // number of timer IRQs
                     16,                                // number of hard IRQs
                     16,                                // number of soft IRQs
                     16 );                              // number of output IRQs

    /////////////////////////////////////////////////////////////////////////////
    size_t nb_initiators = nb_procs;
    size_t nb_targets    = 2;

    if ((x_id == 0) and (y_id == 0))  // cluster(0,0)
    {
        nb_initiators = nb_procs + 1;
        nb_targets    = 4;
    }

    std::ostringstream s_xbar_cmd;
    xbar_cmd = new VciLocalCrossbar<vci_param_int>(
                     s_xbar_cmd.str().c_str(),
                     mtd,                          // mapping table
                     cluster_xy,                   // cluster id
                     nb_initiators,                // number of local initiators
                     nb_targets,                   // number of local targets 
                     0 );                          // default target

    wi_gate = new VciDspinInitiatorWrapper<vci_param_int,
                                           dspin_cmd_width,
                                           dspin_rsp_width>(
                     "wi_gate",
                     x_width + y_width + l_width);

    wt_gate = new VciDspinTargetWrapper<vci_param_int,
                                        dspin_cmd_width,
                                        dspin_rsp_width>(
                     "wt_gate",
                     x_width + y_width + l_width);

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream s_xbar_m2p;
    s_xbar_m2p << "xbar_m2p_" << x_id << "_" << y_id;
    xbar_m2p = new DspinLocalCrossbar<dspin_cmd_width>(
                     s_xbar_m2p.str().c_str(),
                     mtd,                          // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, l_width,
                     1,                            // number of local sources
                     nb_procs,                     // number of local dests 
                     2, 2,                         // fifo depths
                     true,                         // CMD
                     false,                        // don't use local routing table
                     true );                       // broadcast

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream s_xbar_p2m;
    s_xbar_p2m << "xbar_p2m_" << x_id << "_" << y_id;
    xbar_p2m = new DspinLocalCrossbar<dspin_rsp_width>(
                     s_xbar_p2m.str().c_str(),
                     mtd,                          // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, 0,          // l_width unused on p2m network
                     nb_procs,                     // number of local sources
                     1,                            // number of local dests
                     2, 2,                         // fifo depths
                     false,                        // RSP
                     false,                        // don't use local routing table
                     false );                      // no broadcast 

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream s_xbar_cla;
    s_xbar_cla << "xbar_cla_" << x_id << "_" << y_id;
    xbar_cla = new DspinLocalCrossbar<dspin_cmd_width>(
                     s_xbar_cla.str().c_str(),
                     mtd,                          // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, l_width,
                     1,                            // number of local sources
                     nb_procs,                     // number of local dests 
                     2, 2,                         // fifo depths
                     true,                         // CMD
                     false,                        // don't use local routing table
                     false);                       // no broadcast

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream s_router_cmd;
    s_router_cmd << "router_cmd_" << x_id << "_" << y_id;
    router_cmd = new DspinRouter<dspin_cmd_width>(
                     s_router_cmd.str().c_str(),
                     x_id,y_id,                    // coordinate in the mesh
                     x_width, y_width,             // x & y fields width
                     4,4);                         // input & output fifo depths

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream s_router_rsp;
    s_router_rsp << "router_rsp_" << x_id << "_" << y_id;
    router_rsp = new DspinRouter<dspin_rsp_width>(
                     s_router_rsp.str().c_str(),
                     x_id,y_id,                    // coordinates in mesh
                     x_width, y_width,             // x & y fields width
                     4,4);                         // input & output fifo depths

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream s_router_m2p;
    s_router_m2p << "router_m2p_" << x_id << "_" << y_id;
    router_m2p = new DspinRouter<dspin_cmd_width>(
                     s_router_m2p.str().c_str(),
                     x_id,y_id,                    // coordinate in the mesh
                     x_width, y_width,             // x & y fields width
                     4,4,                          // input & output fifo depths
                     true);                        // broadcast supported

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream s_router_p2m;
    s_router_p2m << "router_p2m_" << x_id << "_" << y_id;
    router_p2m = new DspinRouter<dspin_rsp_width>(
                     s_router_p2m.str().c_str(),
                     x_id,y_id,                    // coordinates in mesh
                     x_width, y_width,             // x & y fields width
                     4,4);                         // input & output fifo depths

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream s_router_cla;
    s_router_cla << "router_cla_" << x_id << "_" << y_id;
    router_cla = new DspinRouter<dspin_cmd_width>(
                     s_router_cla.str().c_str(),
                     x_id,y_id,                    // coordinate in the mesh
                     x_width, y_width,             // x & y fields width
                     4,4);                         // input & output fifo depths

    // backup BDV and TTY peripherals in cluster(0,0)
    bdev = NULL;
    mtty = NULL;
    if ((x_id == 0) and (y_id == 0))
    {
        /////////////////////////////////////////////
        bdev = new VciBlockDeviceTsar<vci_param_int>(
                     "bdev",
                     mtd,
                     IntTab(cluster_xy, nb_procs),
                     IntTab(cluster_xy, tgtid_bdev),
                     disk_pathname,
                     512,
                     64 );            // burst size

        /////////////////////////////////////////////
        mtty = new VciMultiTty<vci_param_int>(
                     "mtty",
                     IntTab(cluster_xy, tgtid_mtty),
                     mtd,
                     "tty_backup", NULL );
    }

    std::cout << std::endl;

    ////////////////////////////////////
    // Connections are defined here
    ////////////////////////////////////

    //////////////////////// ROUTERS
    router_cmd->p_clk                      (this->p_clk);
    router_cmd->p_resetn                   (this->p_resetn);
    router_rsp->p_clk                      (this->p_clk);
    router_rsp->p_resetn                   (this->p_resetn);
    router_m2p->p_clk                      (this->p_clk);
    router_m2p->p_resetn                   (this->p_resetn);
    router_p2m->p_clk                      (this->p_clk);
    router_p2m->p_resetn                   (this->p_resetn);
    router_cla->p_clk                      (this->p_clk);
    router_cla->p_resetn                   (this->p_resetn);

    // loop on N/S/E/W ports
    for (size_t i = 0; i < 4; i++)
    {
        router_cmd->p_out[i]               (this->p_cmd_out[i]);
        router_cmd->p_in[i]                (this->p_cmd_in[i]);

        router_rsp->p_out[i]               (this->p_rsp_out[i]);
        router_rsp->p_in[i]                (this->p_rsp_in[i]);

        router_m2p->p_out[i]               (this->p_m2p_out[i]);
        router_m2p->p_in[i]                (this->p_m2p_in[i]);

        router_p2m->p_out[i]               (this->p_p2m_out[i]);
        router_p2m->p_in[i]                (this->p_p2m_in[i]);

        router_cla->p_out[i]               (this->p_cla_out[i]);
        router_cla->p_in[i]                (this->p_cla_in[i]);
    }

    router_cmd->p_out[4]                   (signal_dspin_cmd_g2l_d);
    router_cmd->p_in[4]                    (signal_dspin_cmd_l2g_d);

    router_rsp->p_out[4]                   (signal_dspin_rsp_g2l_d);
    router_rsp->p_in[4]                    (signal_dspin_rsp_l2g_d);

    router_m2p->p_out[4]                   (signal_dspin_m2p_g2l_c);
    router_m2p->p_in[4]                    (signal_dspin_m2p_l2g_c);

    router_p2m->p_out[4]                   (signal_dspin_p2m_g2l_c);
    router_p2m->p_in[4]                    (signal_dspin_p2m_l2g_c);

    router_cla->p_out[4]                   (signal_dspin_clack_g2l_c);
    router_cla->p_in[4]                    (signal_dspin_clack_l2g_c);

    std::cout << "  - routers connected" << std::endl;

    ///////////////////// CMD DSPIN  local crossbar direct
    xbar_cmd->p_clk                            (this->p_clk);
    xbar_cmd->p_resetn                         (this->p_resetn);
    xbar_cmd->p_initiator_to_up                (signal_vci_l2g);
    xbar_cmd->p_target_to_up                   (signal_vci_g2l);

    xbar_cmd->p_to_target[tgtid_memc]          (signal_vci_tgt_memc);
    xbar_cmd->p_to_target[tgtid_xicu]          (signal_vci_tgt_xicu);

    for (size_t p = 0; p < nb_procs; p++)
        xbar_cmd->p_to_initiator[p]            (signal_vci_ini_proc[p]);

    if ((x_id == 0) and (y_id == 0))  // cluster(0,0)
    {
        xbar_cmd->p_to_target[tgtid_mtty]      (signal_vci_tgt_mtty);
        xbar_cmd->p_to_target[tgtid_bdev]      (signal_vci_tgt_bdev);
        xbar_cmd->p_to_initiator[nb_procs]     (signal_vci_ini_bdev);
    }

    wi_gate->p_clk                             (this->p_clk);
    wi_gate->p_resetn                          (this->p_resetn);
    wi_gate->p_vci                             (signal_vci_l2g);
    wi_gate->p_dspin_cmd                       (signal_dspin_cmd_l2g_d);
    wi_gate->p_dspin_rsp                       (signal_dspin_rsp_g2l_d);

    wt_gate->p_clk                             (this->p_clk);
    wt_gate->p_resetn                          (this->p_resetn);
    wt_gate->p_vci                             (signal_vci_g2l);
    wt_gate->p_dspin_cmd                       (signal_dspin_cmd_g2l_d);
    wt_gate->p_dspin_rsp                       (signal_dspin_rsp_l2g_d);

    std::cout << "  - CMD & RSP Direct crossbar connected" << std::endl;

    ////////////////////// M2P DSPIN local crossbar coherence
    xbar_m2p->p_clk                            (this->p_clk);
    xbar_m2p->p_resetn                         (this->p_resetn);
    xbar_m2p->p_global_out                     (signal_dspin_m2p_l2g_c);
    xbar_m2p->p_global_in                      (signal_dspin_m2p_g2l_c);
    xbar_m2p->p_local_in[0]                    (signal_dspin_m2p_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_m2p->p_local_out[p]               (signal_dspin_m2p_proc[p]);

    std::cout << "  - M2P Coherence crossbar connected" << std::endl;

    ////////////////////////// P2M DSPIN local crossbar coherence
    xbar_p2m->p_clk                            (this->p_clk);
    xbar_p2m->p_resetn                         (this->p_resetn);
    xbar_p2m->p_global_out                     (signal_dspin_p2m_l2g_c);
    xbar_p2m->p_global_in                      (signal_dspin_p2m_g2l_c);
    xbar_p2m->p_local_out[0]                   (signal_dspin_p2m_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_p2m->p_local_in[p]                (signal_dspin_p2m_proc[p]);

    std::cout << "  - P2M Coherence crossbar connected" << std::endl;

    ////////////////////// CLACK DSPIN local crossbar coherence
    xbar_cla->p_clk                          (this->p_clk);
    xbar_cla->p_resetn                       (this->p_resetn);
    xbar_cla->p_global_out                   (signal_dspin_clack_l2g_c);
    xbar_cla->p_global_in                    (signal_dspin_clack_g2l_c);
    xbar_cla->p_local_in[0]                  (signal_dspin_clack_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_cla->p_local_out[p]             (signal_dspin_clack_proc[p]);

    std::cout << "  - CLA Coherence crossbar connected" << std::endl;

    //////////////////////////////////// Processors
    for (size_t p = 0; p < nb_procs; p++)
    {
        proc[p]->p_clk                      (this->p_clk);
        proc[p]->p_resetn                   (this->p_resetn);
        proc[p]->p_vci                      (signal_vci_ini_proc[p]);
        proc[p]->p_dspin_m2p                (signal_dspin_m2p_proc[p]);
        proc[p]->p_dspin_p2m                (signal_dspin_p2m_proc[p]);
        proc[p]->p_dspin_clack              (signal_dspin_clack_proc[p]);

        for ( size_t j = 0 ; j < 6 ; j++)
        {
            if ( j < 4 ) proc[p]->p_irq[j]  (signal_proc_irq[4*p + j]);
            else         proc[p]->p_irq[j]  (signal_false);
        }
    }

    std::cout << "  - Processors connected" << std::endl;

    ///////////////////////////////////// XICU
    xicu->p_clk                        (this->p_clk);
    xicu->p_resetn                     (this->p_resetn);
    xicu->p_vci                        (signal_vci_tgt_xicu);

    for (size_t i = 0 ; i < 16  ; i++)
    {
        xicu->p_irq[i]                 (signal_proc_irq[i]);
    }

    for (size_t i = 0; i < 16; i++)
    {
        if ((x_id == 0) and (y_id == 0)) // cluster (0,0)
        {
            if      (i == 8)           xicu->p_hwi[i] (signal_irq_memc);
            else if (i == 9)           xicu->p_hwi[i] (signal_irq_bdev);
            else if (i == 10)          xicu->p_hwi[i] (signal_irq_mtty);
            else                       xicu->p_hwi[i] (signal_false);
        }
        else                             // other clusters
        {
            if      (i == 8)           xicu->p_hwi[i] (signal_irq_memc);
            else                       xicu->p_hwi[i] (signal_false);
        }
    }

    std::cout << "  - XICU connected" << std::endl;

    //////////////////////////////////////////////// MEMC
    memc->p_clk                        (this->p_clk);
    memc->p_resetn                     (this->p_resetn);
    memc->p_irq                        (signal_irq_memc);
    memc->p_vci_ixr                    (signal_vci_xram);
    memc->p_vci_tgt                    (signal_vci_tgt_memc);
    memc->p_dspin_p2m                  (signal_dspin_p2m_memc);
    memc->p_dspin_m2p                  (signal_dspin_m2p_memc);
    memc->p_dspin_clack                (signal_dspin_clack_memc);

    std::cout << "  - MEMC connected" << std::endl;

    /////////////////////////////////////////////// XRAM
    xram->p_clk                        (this->p_clk);
    xram->p_resetn                     (this->p_resetn);
    xram->p_vci                        (signal_vci_xram);

    std::cout << "  - XRAM connected" << std::endl;

    /////////////////////////////// Extra Components in cluster(0,0)

    if ((x_id == 0) and (y_id == 0))
    {
        // BDEV
        bdev->p_clk                    (this->p_clk);
        bdev->p_resetn                 (this->p_resetn);
        bdev->p_irq                    (signal_irq_bdev);
        bdev->p_vci_target             (signal_vci_tgt_bdev);
        bdev->p_vci_initiator          (signal_vci_ini_bdev);

        std::cout << "  - BDEV connected" << std::endl;

        // MTTY (single channel)
        mtty->p_clk                    (this->p_clk);
        mtty->p_resetn                 (this->p_resetn);
        mtty->p_vci                    (signal_vci_tgt_mtty);
        mtty->p_irq[0]                 (signal_irq_mtty);

        std::cout << "  - MTTY connected" << std::endl;
    }
} // end constructor



template<size_t dspin_cmd_width,
         size_t dspin_rsp_width,
         typename vci_param_int,
         typename vci_param_ext> TsarLetiCluster<dspin_cmd_width,
                                                 dspin_rsp_width,
                                                 vci_param_int,
                                                 vci_param_ext>::~TsarLetiCluster() {

    dealloc_elems<DspinInput<dspin_cmd_width> > (p_cmd_in, 4);
    dealloc_elems<DspinOutput<dspin_cmd_width> >(p_cmd_out, 4);

    dealloc_elems<DspinInput<dspin_rsp_width> > (p_rsp_in, 4);
    dealloc_elems<DspinOutput<dspin_rsp_width> >(p_rsp_out, 4);

    dealloc_elems<DspinInput<dspin_cmd_width> > (p_m2p_in, 4);
    dealloc_elems<DspinOutput<dspin_cmd_width> >(p_m2p_out, 4);

    dealloc_elems<DspinInput<dspin_rsp_width> > (p_p2m_in, 4);
    dealloc_elems<DspinOutput<dspin_rsp_width> >(p_p2m_out, 4);

    dealloc_elems<DspinInput<dspin_cmd_width> > (p_cla_in, 4);
    dealloc_elems<DspinOutput<dspin_cmd_width> >(p_cla_out, 4);

    for (size_t p = 0; p < m_nprocs ; p++)
    {
        if ( proc[p] ) delete proc[p];
    }

    delete memc;
    delete xram;
    delete xicu;
    delete xbar_cmd;
    delete xbar_m2p;
    delete xbar_p2m;
    delete xbar_cla;
    delete router_cmd;
    delete router_rsp;
    delete router_m2p;
    delete router_p2m;
    delete router_cla;
    delete wi_gate;
    delete wt_gate;

    if ( bdev )
    {
        delete bdev;
    }

    if ( mtty )
    {
        delete mtty;
    }
}


}}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4



