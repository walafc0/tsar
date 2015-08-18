//////////////////////////////////////////////////////////////////////////////
// File: tsar_xbar_cluster.cpp
// Author: Alain Greiner
// Copyright: UPMC/LIP6
// Date : march 2011
// This program is released under the GNU public license
//////////////////////////////////////////////////////////////////////////////
// This file define a TSAR cluster architecture with virtual memory:
// - It uses two virtual_dspin_router as distributed global interconnect
// - It uses four dspin_local_crossbar as local interconnect
// - It uses the vci_cc_vcache_wrapper
// - It uses the vci_mem_cache
// - It contains a private RAM with a variable latency to emulate the L3 cache
// - It can contains 1, 2 or 4 processors
// - Each processor has a private dma channel (vci_multi_dma)
// - It uses the vci_xicu interrupt controller
// - The peripherals MTTY, BDEV, FBUF, MNIC and BROM are in cluster (0,0)
// - The Multi-TTY component controls up to 15 terminals.
// - Each Multi-DMA component controls up to 8 DMA channels.
// - The DMA IRQs are connected to IRQ_IN[8]...IRQ_IN[15]
// - The TTY IRQs are connected to IRQ_IN[16]...IRQ_IN[30]
// - The BDEV IRQ is connected to IRQ_IN[31]
//////////////////////////////////////////////////////////////////////////////////

#include "../include/tsar_xbar_cluster.h"


namespace soclib {
namespace caba  {

////////////////////////////////////////////////////////////////////////////////////
template<size_t dspin_cmd_width,
         size_t dspin_rsp_width,
         typename vci_param_int,
         typename vci_param_ext> TsarXbarCluster<dspin_cmd_width,
                                                 dspin_rsp_width,
                                                 vci_param_int,
                                                 vci_param_ext>::TsarXbarCluster(
////////////////////////////////////////////////////////////////////////////////////
         sc_module_name                     insname,
         size_t                             nb_procs,
         size_t                             nb_ttys,
         size_t                             nb_dmas,
         size_t                             x_id,
         size_t                             y_id,
         size_t                             cluster_id,
         const soclib::common::MappingTable &mtd,
         const soclib::common::MappingTable &mtx,
         size_t                             x_width,
         size_t                             y_width,
         size_t                             l_width,
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
         size_t                             irq_per_processor,
         size_t                             xram_latency,
         bool                               io,
         size_t                             xfb,
         size_t                             yfb,
         char*                              disk_name,
         size_t                             block_size,
         size_t                             nic_channels,
         char*                              nic_rx_name,
         char*                              nic_tx_name,
         uint32_t                           nic_timeout,
         size_t                             chbufdma_channels,
         const Loader &                     loader,
         uint32_t                           frozen_cycles,
         uint32_t                           debug_start_cycle,
         bool                               memc_debug_ok,
         bool                               proc_debug_ok)
            : soclib::caba::BaseModule(insname),
            p_clk("clk"),
            p_resetn("resetn")

{

    n_procs = nb_procs;

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
    // Components definition
    /////////////////////////////////////////////////////////////////////////////

    for (size_t p = 0; p < nb_procs; p++)
    {
        std::ostringstream sproc;
        sproc << "proc_" << x_id << "_" << y_id << "_" << p;
        proc[p] = new VciCcVCacheWrapper<vci_param_int,
                                         dspin_cmd_width,
                                         dspin_rsp_width,
                                         GdbServer<Mips32ElIss> >(
                      sproc.str().c_str(),
                      cluster_id * nb_procs + p,      // GLOBAL PROC_ID
                      mtd,                            // Mapping Table
                      IntTab(cluster_id,p),           // SRCID
                      (cluster_id << l_width) + p,    // CC_GLOBAL_ID
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
                      debug_start_cycle,
                      proc_debug_ok);

    }

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream smemc;
    smemc << "memc_" << x_id << "_" << y_id;
    memc = new VciMemCache<vci_param_int,
                           vci_param_ext,
                           dspin_rsp_width,
                           dspin_cmd_width>(
                     smemc.str().c_str(),
                     mtd,                                // Mapping Table direct space
                     mtx,                                // Mapping Table external space
                     IntTab(cluster_id),                 // SRCID external space
                     IntTab(cluster_id, tgtid_memc),     // TGTID direct space
                     x_width,                            // Number of x bits in platform
                     y_width,                            // Number of y bits in platform
                     memc_ways, memc_sets, 16,           // CACHE SIZE
                     3,                                  // MAX NUMBER OF COPIES
                     4096,                               // HEAP SIZE
                     8,                                  // TRANSACTION TABLE DEPTH
                     8,                                  // UPDATE TABLE DEPTH
                     8,                                  // INVALIDATE TABLE DEPTH
                     debug_start_cycle,
                     memc_debug_ok);


    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream sxram;
    sxram << "xram_" << x_id << "_" << y_id;
    xram = new VciSimpleRam<vci_param_ext>(
                     sxram.str().c_str(),
                     IntTab(cluster_id),
                     mtx,
                     loader,
                     xram_latency);

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream sxicu;
    sxicu << "xicu_" << x_id << "_" << y_id;
    xicu = new VciXicu<vci_param_int>(
                     sxicu.str().c_str(),
                     mtd,                               // mapping table
                     IntTab(cluster_id, tgtid_xicu),    // TGTID_D
                     nb_procs,                          // number of timer IRQs
                     32,                                // number of hard IRQs
                     32,                                // number of soft IRQs
                     nb_procs * irq_per_processor);     // number of output IRQs


    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream smdma;
    smdma << "mdma_" << x_id << "_" << y_id;
    mdma = new VciMultiDma<vci_param_int>(
                     smdma.str().c_str(),
                     mtd,
                     IntTab(cluster_id, nb_procs),        // SRCID
                     IntTab(cluster_id, tgtid_mdma),      // TGTID
                     64,                                  // burst size
                     nb_dmas);                            // number of IRQs


    /////////////////////////////////////////////////////////////////////////////
    size_t nb_direct_initiators      = nb_procs + 1;
    size_t nb_direct_targets         = 3;
    if (io)
    {
        nb_direct_initiators         = nb_procs + 3;
        nb_direct_targets            = 10;
    }

    std::ostringstream sxbar;
    sxbar << "xbar_" << x_id << "_" << y_id;
    xbar_d = new VciLocalCrossbar<vci_param_int>(
                     sxbar.str().c_str(),
                     mtd,                             // mapping table
                     //x_id, y_id,                    // cluster coordinates
                     cluster_id,
                     nb_direct_initiators,            // number of local of sources
                     nb_direct_targets,               // number of local dests 
                     tgtid_memc);                     // Default target

    ////////////// vci_dspin wrappers 
    std::ostringstream swtxbar;
    swtxbar << "wt_xbar_" << x_id << "_" << y_id;
    wt_xbar_d = new VciDspinTargetWrapper<vci_param_int, dspin_cmd_width, dspin_rsp_width>(
                swtxbar.str().c_str(),
                x_width + y_width + l_width);

    std::ostringstream swixbar;
    swixbar << "wi_xbar_" << x_id << "_" << y_id;
    wi_xbar_d = new VciDspinInitiatorWrapper<vci_param_int, dspin_cmd_width, dspin_rsp_width>(
                swixbar.str().c_str(),
                x_width + y_width + l_width );

    /////////////////////////////////////////////////////////////////////////////
    xbar_m2p_c = new DspinLocalCrossbar<dspin_cmd_width>(
                     "xbar_m2p_c",
                     mtd,                          // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, l_width,
                     1,                            // number of local sources
                     nb_procs,                     // number of local targets 
                     2, 2,                         // fifo depths
                     true,                         // CMD
                     false,                        // don't use local routing table
                     true );                       // broadcast

    /////////////////////////////////////////////////////////////////////////////
    xbar_p2m_c = new DspinLocalCrossbar<dspin_rsp_width>(
                     "xbar_p2m_c",
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
    xbar_clack_c = new DspinLocalCrossbar<dspin_cmd_width>(
                     "xbar_clack_c",
                     mtd,                          // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, l_width,
                     1,                            // number of local sources
                     nb_procs,                     // number of local targets 
                     1, 1,                         // fifo depths
                     true,                         // CMD
                     false,                        // don't use local routing table
                     false);                       // broadcast

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

    // IO cluster components
    if (io)
    {
        /////////////////////////////////////////////
        brom = new VciSimpleRom<vci_param_int>(
                     "brom",
                     IntTab(cluster_id, tgtid_brom),
                     mtd,
                     loader);

        /////////////////////////////////////////////
        fbuf = new VciFrameBuffer<vci_param_int>(
                     "fbuf",
                     IntTab(cluster_id, tgtid_fbuf),
                     mtd,
                     xfb, yfb);

        /////////////////////////////////////////////
        bdev = new VciBlockDeviceTsar<vci_param_int>(
                     "bdev",
                     mtd,
                     IntTab(cluster_id, nb_procs + 1),
                     IntTab(cluster_id, tgtid_bdev),
                     disk_name,
                     block_size,
                     64);            // burst size

        int mac = 0xBEEF0000;
        mnic = new VciMultiNic<vci_param_int>(
                     "mnic",
                     IntTab(cluster_id, tgtid_mnic),
                     mtd,
                     nic_channels,
                     mac,             // mac_4 address
                     0xBABE,          // mac_2 address
                     nic_rx_name,
                     nic_tx_name);



        /////////////////////////////////////////////
        chbuf = new VciChbufDma<vci_param_int>(
                     "chbuf_dma",
                     mtd,
                     IntTab(cluster_id, nb_procs + 2),
                     IntTab(cluster_id, tgtid_chbuf),
                     64,
                     chbufdma_channels); 

        /////////////////////////////////////////////
        std::vector<std::string> vect_names;
        for (size_t tid = 0; tid < nb_ttys; tid++)
        {
            std::ostringstream term_name;
            term_name <<  "term" << tid;
            vect_names.push_back(term_name.str().c_str());
        }
        mtty = new VciMultiTty<vci_param_int>(
                     "mtty",
                     IntTab(cluster_id, tgtid_mtty),
                     mtd,
                     vect_names);

        simhelper = new VciSimhelper<vci_param_int>(
                     "sim_helper",
                     IntTab(cluster_id, tgtid_simh),
                     mtd);
    }

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

    wi_xbar_d->p_clk                         (this->p_clk);
    wi_xbar_d->p_resetn                      (this->p_resetn);
    wi_xbar_d->p_vci                         (signal_vci_l2g_d);
    wi_xbar_d->p_dspin_cmd                   (signal_dspin_cmd_l2g_d);
    wi_xbar_d->p_dspin_rsp                   (signal_dspin_rsp_g2l_d);

    std::cout << "  - Wrapper Ini VCI2DSPIN Direct connected" << std::endl;

    wt_xbar_d->p_clk                         (this->p_clk);
    wt_xbar_d->p_resetn                      (this->p_resetn);
    wt_xbar_d->p_vci                         (signal_vci_g2l_d);
    wt_xbar_d->p_dspin_cmd                   (signal_dspin_cmd_g2l_d);
    wt_xbar_d->p_dspin_rsp                   (signal_dspin_rsp_l2g_d);

    std::cout << "  - Wrapper Tgt VCI2DSPIN Direct connected" << std::endl;

    ///////////////////// CMD VCI  local crossbar direct
    xbar_d->p_clk                            (this->p_clk);
    xbar_d->p_resetn                         (this->p_resetn);
    xbar_d->p_target_to_up                   (signal_vci_g2l_d);
    xbar_d->p_initiator_to_up                (signal_vci_l2g_d);

    xbar_d->p_to_target[tgtid_memc]          (signal_vci_tgt_memc);
    xbar_d->p_to_target[tgtid_xicu]          (signal_vci_tgt_xicu);
    xbar_d->p_to_target[tgtid_mdma]          (signal_vci_tgt_mdma);

    xbar_d->p_to_initiator[nb_procs]         (signal_vci_ini_mdma);

    for (size_t p = 0; p < nb_procs; p++)
        xbar_d->p_to_initiator[p]            (signal_vci_ini_proc[p]);

    if (io)
    {
        xbar_d->p_to_target[tgtid_mtty]      (signal_vci_tgt_mtty);
        xbar_d->p_to_target[tgtid_brom]      (signal_vci_tgt_brom);
        xbar_d->p_to_target[tgtid_bdev]      (signal_vci_tgt_bdev);
        xbar_d->p_to_target[tgtid_fbuf]      (signal_vci_tgt_fbuf);
        xbar_d->p_to_target[tgtid_mnic]      (signal_vci_tgt_mnic);
        xbar_d->p_to_target[tgtid_chbuf]     (signal_vci_tgt_chbuf);
        xbar_d->p_to_target[tgtid_simh]      (signal_vci_tgt_simh);

        xbar_d->p_to_initiator[nb_procs + 1] (signal_vci_ini_bdev);
        xbar_d->p_to_initiator[nb_procs + 2] (signal_vci_ini_chbuf);
    }

    std::cout << "  - Direct crossbar connected" << std::endl;

    ////////////////////// M2P DSPIN local crossbar coherence
    xbar_m2p_c->p_clk                            (this->p_clk);
    xbar_m2p_c->p_resetn                         (this->p_resetn);
    xbar_m2p_c->p_global_out                     (signal_dspin_m2p_l2g_c);
    xbar_m2p_c->p_global_in                      (signal_dspin_m2p_g2l_c);
    xbar_m2p_c->p_local_in[0]                    (signal_dspin_m2p_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_m2p_c->p_local_out[p]               (signal_dspin_m2p_proc[p]);

    std::cout << "  - M2P Coherence crossbar connected" << std::endl;

    ////////////////////// CLACK DSPIN local crossbar coherence
    xbar_clack_c->p_clk                          (this->p_clk);
    xbar_clack_c->p_resetn                       (this->p_resetn);
    xbar_clack_c->p_global_out                   (signal_dspin_clack_l2g_c);
    xbar_clack_c->p_global_in                    (signal_dspin_clack_g2l_c);
    xbar_clack_c->p_local_in[0]                  (signal_dspin_clack_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_clack_c->p_local_out[p]             (signal_dspin_clack_proc[p]);

    std::cout << "  - Clack Coherence crossbar connected" << std::endl;

    ////////////////////////// P2M DSPIN local crossbar coherence
    xbar_p2m_c->p_clk                            (this->p_clk);
    xbar_p2m_c->p_resetn                         (this->p_resetn);
    xbar_p2m_c->p_global_out                     (signal_dspin_p2m_l2g_c);
    xbar_p2m_c->p_global_in                      (signal_dspin_p2m_g2l_c);
    xbar_p2m_c->p_local_out[0]                   (signal_dspin_p2m_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_p2m_c->p_local_in[p]                (signal_dspin_p2m_proc[p]);

    std::cout << "  - P2M Coherence crossbar connected" << std::endl;


    //////////////////////////////////// Processors
    for (size_t p = 0; p < nb_procs; p++)
    {
        proc[p]->p_clk                      (this->p_clk);
        proc[p]->p_resetn                   (this->p_resetn);
        proc[p]->p_vci                      (signal_vci_ini_proc[p]);
        proc[p]->p_dspin_m2p                (signal_dspin_m2p_proc[p]);
        proc[p]->p_dspin_p2m                (signal_dspin_p2m_proc[p]);
        proc[p]->p_dspin_clack              (signal_dspin_clack_proc[p]);

        for ( size_t i = 0; i < irq_per_processor; i++)
        {
            proc[p]->p_irq[i]               (signal_proc_it[p*irq_per_processor + i]);
        }
        for ( size_t j = irq_per_processor; j < 6; j++) // 6 = number of irqs in the MIPS
        {
            proc[p]->p_irq[j]               (signal_false);
        }

    }

    std::cout << "  - Processors connected" << std::endl;

    ///////////////////////////////////// XICU
    xicu->p_clk                        (this->p_clk);
    xicu->p_resetn                     (this->p_resetn);
    xicu->p_vci                        (signal_vci_tgt_xicu);
    for (size_t p = 0; p < nb_procs * irq_per_processor; p++)
    {
        xicu->p_irq[p]                 (signal_proc_it[p]);
    }
    for (size_t i = 0; i < 32; i++)
    {
        if (io) // I/O cluster
        {
            if      (i < 8)                  xicu->p_hwi[i] (signal_false);
            else if (i < (8 + nb_dmas))      xicu->p_hwi[i] (signal_irq_mdma[i - 8]);
            else if (i < 16)                 xicu->p_hwi[i] (signal_false);
            else if (i < (16 + nb_ttys))     xicu->p_hwi[i] (signal_irq_mtty[i - 16]);
            else if (i < 30)                 xicu->p_hwi[i] (signal_false);
            else if (i < 31)                 xicu->p_hwi[i] (signal_irq_memc);
            else                             xicu->p_hwi[i] (signal_irq_bdev);
        }
        else      // other clusters
        {
            if      (i < 8)                  xicu->p_hwi[i] (signal_false);
            else if (i < (8 + nb_dmas))      xicu->p_hwi[i] (signal_irq_mdma[i - 8]);
            else if (i < 30)                 xicu->p_hwi[i] (signal_false);
            else if (i < 31)                 xicu->p_hwi[i] (signal_irq_memc);
            else                             xicu->p_hwi[i] (signal_false);
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

    ////////////////////////////////////////////// MDMA
    mdma->p_clk                        (this->p_clk);
    mdma->p_resetn                     (this->p_resetn);
    mdma->p_vci_target                 (signal_vci_tgt_mdma);
    mdma->p_vci_initiator              (signal_vci_ini_mdma);
    for (size_t i = 0; i < nb_dmas; i++)
        mdma->p_irq[i]                 (signal_irq_mdma[i]);

    std::cout << "  - MDMA connected" << std::endl;

    /////////////////////////////// Components in I/O cluster

    if (io)
    {
        // BDEV
        bdev->p_clk                    (this->p_clk);
        bdev->p_resetn                 (this->p_resetn);
        bdev->p_irq                    (signal_irq_bdev);
        bdev->p_vci_target             (signal_vci_tgt_bdev);
        bdev->p_vci_initiator          (signal_vci_ini_bdev);

        std::cout << "  - BDEV connected" << std::endl;

        // FBUF
        fbuf->p_clk                    (this->p_clk);
        fbuf->p_resetn                 (this->p_resetn);
        fbuf->p_vci                    (signal_vci_tgt_fbuf);

        std::cout << "  - FBUF connected" << std::endl;

        // MNIC
        mnic->p_clk                    (this->p_clk);
        mnic->p_resetn                 (this->p_resetn);
        mnic->p_vci                    (signal_vci_tgt_mnic);
        for (size_t i = 0; i < nic_channels; i++)
        {
            mnic->p_rx_irq[i]          (signal_irq_mnic_rx[i]);
            mnic->p_tx_irq[i]          (signal_irq_mnic_tx[i]);
        }

        std::cout << "  - MNIC connected" << std::endl;

        // CHBUF
        chbuf->p_clk                    (this->p_clk);
        chbuf->p_resetn                 (this->p_resetn);
        chbuf->p_vci_target             (signal_vci_tgt_chbuf);
        chbuf->p_vci_initiator          (signal_vci_ini_chbuf);
        for (size_t i = 0; i < chbufdma_channels; i++)
        {
            chbuf->p_irq[i]          (signal_irq_chbuf[i]);
        }

        std::cout << "  - CHBUF connected" << std::endl;

        // BROM
        brom->p_clk                    (this->p_clk);
        brom->p_resetn                 (this->p_resetn);
        brom->p_vci                    (signal_vci_tgt_brom);

        std::cout << "  - BROM connected" << std::endl;

        // MTTY
        mtty->p_clk                    (this->p_clk);
        mtty->p_resetn                 (this->p_resetn);
        mtty->p_vci                    (signal_vci_tgt_mtty);
        for (size_t i = 0; i < nb_ttys; i++)
        {
            mtty->p_irq[i]             (signal_irq_mtty[i]);
        }

        std::cout << "  - MTTY connected" << std::endl;


        // Sim Helper
        simhelper->p_clk               (this->p_clk);
        simhelper->p_resetn            (this->p_resetn);
        simhelper->p_vci               (signal_vci_tgt_simh);
        
        std::cout << "  - SIMH connected" << std::endl;
   }
} // end constructor



template<size_t dspin_cmd_width,
         size_t dspin_rsp_width,
         typename vci_param_int,
         typename vci_param_ext> TsarXbarCluster<dspin_cmd_width,
                                                 dspin_rsp_width,
                                                 vci_param_int,
                                                 vci_param_ext>::~TsarXbarCluster() {

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

    for (size_t p = 0; p < n_procs; p++)
    {
        delete proc[p];
    }

    delete memc;
    delete xram;
    delete xicu;
    delete mdma;
    delete xbar_d;
    delete wt_xbar_d;
    delete wi_xbar_d;
    delete xbar_m2p_c;
    delete xbar_p2m_c;
    delete xbar_clack_c;
    delete router_cmd;
    delete router_rsp;
    if (brom != NULL)
    {
        delete brom;
        delete fbuf;
        delete bdev;
        delete mnic;
        delete chbuf;
        delete mtty;
        delete simhelper;
    }
}


template<size_t dspin_cmd_width,
         size_t dspin_rsp_width,
         typename vci_param_int,
         typename vci_param_ext>
void TsarXbarCluster<dspin_cmd_width,
                     dspin_rsp_width,
                     vci_param_int,
                     vci_param_ext>::trace(sc_core::sc_trace_file * tf, const std::string & name) {

#define __trace(x) sc_core::sc_trace(tf, x, name + "_" + #x)
    __trace(signal_vci_l2g_d);
    __trace(signal_vci_g2l_d);
    __trace(signal_vci_tgt_memc);
    __trace(signal_vci_tgt_xicu);
    __trace(signal_vci_tgt_mdma);
    __trace(signal_vci_ini_mdma);

    for (size_t p = 0; p < n_procs; p++) {
        std::ostringstream signame;
        signame << "vci_ini_proc_" << p;
        sc_core::sc_trace(tf, signal_vci_ini_proc[p], signame.str());
    }
    __trace(signal_vci_tgt_mtty);
    __trace(signal_vci_tgt_brom);
    __trace(signal_vci_tgt_bdev);
    __trace(signal_vci_tgt_fbuf);
    __trace(signal_vci_tgt_simh);
    __trace(signal_vci_ini_bdev);
    __trace(signal_vci_tgt_memc);
    __trace(signal_vci_xram);
#undef trace

}                        

}}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

