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
         const Loader                      &loader,
         uint32_t                           frozen_cycles,
         uint32_t                           debug_start_cycle,
         bool                               memc_debug_ok,
         bool                               proc_debug_ok)
            : soclib::caba::BaseModule(insname),
            p_clk("clk"),
            p_resetn("resetn")

{

    n_procs = nb_procs;

    // Vectors of ports definition
    p_cmd_in  = alloc_elems<DspinInput<dspin_cmd_width> >  ("p_cmd_in",  4, 3);
    p_cmd_out = alloc_elems<DspinOutput<dspin_cmd_width> > ("p_cmd_out", 4, 3);
    p_rsp_in  = alloc_elems<DspinInput<dspin_rsp_width> >  ("p_rsp_in",  4, 2);
    p_rsp_out = alloc_elems<DspinOutput<dspin_rsp_width> > ("p_rsp_out", 4, 2);

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

        std::ostringstream swip;
        swip << "wi_proc_" << x_id << "_" << y_id << "_" << p;
        wi_proc[p] = new VciDspinInitiatorWrapper<vci_param_int,
                                                  dspin_cmd_width,
                                                  dspin_rsp_width>(
                     swip.str().c_str(),
                     x_width + y_width + l_width);
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
                     //(cluster_id << l_width) + nb_procs, // CC_GLOBAL_ID
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

    wt_memc = new VciDspinTargetWrapper<vci_param_int,
                                        dspin_cmd_width,
                                        dspin_rsp_width>(
                     "wt_memc",
                     x_width + y_width + l_width);

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
                     nb_procs);                         // number of output IRQs

    wt_xicu = new VciDspinTargetWrapper<vci_param_int,
                                        dspin_cmd_width,
                                        dspin_rsp_width>(
                     "wt_xicu",
                     x_width + y_width + l_width);

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

    wt_mdma = new VciDspinTargetWrapper<vci_param_int,
                                        dspin_cmd_width,
                                        dspin_rsp_width>(
                     "wt_mdma",
                     x_width + y_width + l_width);

    wi_mdma = new VciDspinInitiatorWrapper<vci_param_int,
                                           dspin_cmd_width,
                                           dspin_rsp_width>(
                     "wi_mdma",
                     x_width + y_width + l_width);

    /////////////////////////////////////////////////////////////////////////////
    size_t nb_direct_initiators      = nb_procs + 1;
    size_t nb_direct_targets         = 3;
    if (io)
    {
        nb_direct_initiators         = nb_procs + 3;
        nb_direct_targets            = 10;
    }

    xbar_cmd_d = new DspinLocalCrossbar<dspin_cmd_width>(
                     "xbar_cmd_d",
                     mtd,                          // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, l_width,
                     nb_direct_initiators,         // number of local of sources
                     nb_direct_targets,            // number of local dests 
                     2, 2,                         // fifo depths  
                     true,                         // CMD
                     true,                         // use local routing table 
                     false );                      // no broadcast

    /////////////////////////////////////////////////////////////////////////////
    xbar_rsp_d = new DspinLocalCrossbar<dspin_rsp_width>(
                     "xbar_rsp_d",
                     mtd,                          // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, l_width,
                     nb_direct_targets,            // number of local sources
                     nb_direct_initiators,         // number of local dests
                     2, 2,                         // fifo depths  
                     false,                        // RSP
                     false,                        // don't use local routing table 
                     false );                      // no broadcast

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
    router_cmd = new VirtualDspinRouter<dspin_cmd_width>(
                     "router_cmd",
                     x_id,y_id,                    // coordinate in the mesh
                     x_width, y_width,             // x & y fields width
                     3,                            // nb virtual channels
                     4,4);                         // input & output fifo depths

    /////////////////////////////////////////////////////////////////////////////
    router_rsp = new VirtualDspinRouter<dspin_rsp_width>(
                     "router_rsp",
                     x_id,y_id,                    // coordinates in mesh
                     x_width, y_width,             // x & y fields width
                     2,                            // nb virtual channels
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

        wt_brom = new VciDspinTargetWrapper<vci_param_int,
                                            dspin_cmd_width,
                                            dspin_rsp_width>(
                     "wt_brom",
                     x_width + y_width + l_width);

        /////////////////////////////////////////////
        fbuf = new VciFrameBuffer<vci_param_int>(
                     "fbuf",
                     IntTab(cluster_id, tgtid_fbuf),
                     mtd,
                     xfb, yfb);

        wt_fbuf = new VciDspinTargetWrapper<vci_param_int,
                                            dspin_cmd_width,
                                            dspin_rsp_width>(
                     "wt_fbuf",
                     x_width + y_width + l_width);

        /////////////////////////////////////////////
        bdev = new VciBlockDeviceTsar<vci_param_int>(
                     "bdev",
                     mtd,
                     IntTab(cluster_id, nb_procs + 1),
                     IntTab(cluster_id, tgtid_bdev),
                     disk_name,
                     block_size,
                     64);            // burst size

        wt_bdev = new VciDspinTargetWrapper<vci_param_int,
                                            dspin_cmd_width,
                                            dspin_rsp_width>(
                     "wt_bdev",
                     x_width + y_width + l_width);

        wi_bdev = new VciDspinInitiatorWrapper<vci_param_int,
                                               dspin_cmd_width,
                                               dspin_rsp_width>(
                     "wi_bdev",
                     x_width + y_width + l_width);

        int mac = 0xBEEF0000;
        mnic = new VciMultiNic<vci_param_int>(
                     "mnic",
                     IntTab(cluster_id, tgtid_mnic),
                     mtd,
                     nic_channels,
                     nic_rx_name,
                     nic_tx_name,
                     mac,             // mac_4 address
                     0xBABE );           // mac_2 address

        wt_mnic = new VciDspinTargetWrapper<vci_param_int,
                                           dspin_cmd_width,
                                           dspin_rsp_width>(
                    "wt_mnic",
                    x_width + y_width + l_width);

        /////////////////////////////////////////////
        chbuf = new VciChbufDma<vci_param_int>(
                     "chbuf_dma",
                     mtd,
                     IntTab(cluster_id, nb_procs + 2),
                     IntTab(cluster_id, tgtid_chbuf),
                     64,
                     chbufdma_channels); 

        wt_chbuf = new VciDspinTargetWrapper<vci_param_int,
                                            dspin_cmd_width,
                                            dspin_rsp_width>(
                     "wt_chbuf",
                     x_width + y_width + l_width);

        wi_chbuf = new VciDspinInitiatorWrapper<vci_param_int,
                                            dspin_cmd_width,
                                            dspin_rsp_width>(
                     "wi_chbuf",
                     x_width + y_width + l_width);

        /////////////////////////////////////////////
        std::vector<std::string> vect_names;
        for (size_t tid = 0; tid < nb_ttys; tid++)
        {
            std::ostringstream term_name;
            term_name <<  "tty" << tid;
            vect_names.push_back(term_name.str().c_str());
        }
        mtty = new VciMultiTty<vci_param_int>(
                     "mtty",
                     IntTab(cluster_id, tgtid_mtty),
                     mtd,
                     vect_names);

        wt_mtty = new VciDspinTargetWrapper<vci_param_int,
                                            dspin_cmd_width,
                                            dspin_rsp_width>(
                     "wt_mtty",
                     x_width + y_width + l_width);

        simhelper = new VciSimhelper<vci_param_int>(
                     "sim_helper",
                     IntTab(cluster_id, tgtid_simh),
                     mtd);

        wt_simhelper = new VciDspinTargetWrapper<vci_param_int,
                                                 dspin_cmd_width,
                                                 dspin_rsp_width>(
                     "wt_simhelper",
                     x_width + y_width + l_width);
    }

    ////////////////////////////////////
    // Connections are defined here
    ////////////////////////////////////

    //////////////////////// CMD ROUTER and RSP ROUTER
    router_cmd->p_clk                        (this->p_clk);
    router_cmd->p_resetn                     (this->p_resetn);
    router_rsp->p_clk                        (this->p_clk);
    router_rsp->p_resetn                     (this->p_resetn);

    for (int i = 0; i < 4; i++)
    {
        for (int k = 0; k < 3; k++)
        {
            router_cmd->p_out[i][k]          (this->p_cmd_out[i][k]);
            router_cmd->p_in[i][k]           (this->p_cmd_in[i][k]);
        }

        for (int k = 0; k < 2; k++)
        {
            router_rsp->p_out[i][k]          (this->p_rsp_out[i][k]);
            router_rsp->p_in[i][k]           (this->p_rsp_in[i][k]);
        }
    }

    router_cmd->p_out[4][0]                  (signal_dspin_cmd_g2l_d);
    router_cmd->p_out[4][1]                  (signal_dspin_m2p_g2l_c);
    router_cmd->p_out[4][2]                  (signal_dspin_clack_g2l_c);
    router_cmd->p_in[4][0]                   (signal_dspin_cmd_l2g_d);
    router_cmd->p_in[4][1]                   (signal_dspin_m2p_l2g_c);
    router_cmd->p_in[4][2]                   (signal_dspin_clack_l2g_c);

    router_rsp->p_out[4][0]                  (signal_dspin_rsp_g2l_d);
    router_rsp->p_out[4][1]                  (signal_dspin_p2m_g2l_c);
    router_rsp->p_in[4][0]                   (signal_dspin_rsp_l2g_d);
    router_rsp->p_in[4][1]                   (signal_dspin_p2m_l2g_c);


    std::cout << "  - CMD & RSP routers connected" << std::endl;

    ///////////////////// CMD DSPIN  local crossbar direct
    xbar_cmd_d->p_clk                            (this->p_clk);
    xbar_cmd_d->p_resetn                         (this->p_resetn);
    xbar_cmd_d->p_global_out                     (signal_dspin_cmd_l2g_d);
    xbar_cmd_d->p_global_in                      (signal_dspin_cmd_g2l_d);

    xbar_cmd_d->p_local_out[tgtid_memc]          (signal_dspin_cmd_memc_t);
    xbar_cmd_d->p_local_out[tgtid_xicu]          (signal_dspin_cmd_xicu_t);
    xbar_cmd_d->p_local_out[tgtid_mdma]          (signal_dspin_cmd_mdma_t);

    xbar_cmd_d->p_local_in[nb_procs]             (signal_dspin_cmd_mdma_i);

    for (size_t p = 0; p < nb_procs; p++)
        xbar_cmd_d->p_local_in[p]                (signal_dspin_cmd_proc_i[p]);

    if (io)
    {
        xbar_cmd_d->p_local_out[tgtid_mtty]      (signal_dspin_cmd_mtty_t);
        xbar_cmd_d->p_local_out[tgtid_brom]      (signal_dspin_cmd_brom_t);
        xbar_cmd_d->p_local_out[tgtid_bdev]      (signal_dspin_cmd_bdev_t);
        xbar_cmd_d->p_local_out[tgtid_fbuf]      (signal_dspin_cmd_fbuf_t);
        xbar_cmd_d->p_local_out[tgtid_mnic]      (signal_dspin_cmd_mnic_t);
        xbar_cmd_d->p_local_out[tgtid_chbuf]     (signal_dspin_cmd_chbuf_t);
        xbar_cmd_d->p_local_out[tgtid_simh]      (signal_dspin_cmd_simh_t);

        xbar_cmd_d->p_local_in[nb_procs + 1]     (signal_dspin_cmd_bdev_i);
        xbar_cmd_d->p_local_in[nb_procs + 2]     (signal_dspin_cmd_chbuf_i);
    }

    std::cout << "  - Command Direct crossbar connected" << std::endl;

    //////////////////////// RSP DSPIN  local crossbar direct
    xbar_rsp_d->p_clk                            (this->p_clk);
    xbar_rsp_d->p_resetn                         (this->p_resetn);
    xbar_rsp_d->p_global_out                     (signal_dspin_rsp_l2g_d);
    xbar_rsp_d->p_global_in                      (signal_dspin_rsp_g2l_d);

    xbar_rsp_d->p_local_in[tgtid_memc]           (signal_dspin_rsp_memc_t);
    xbar_rsp_d->p_local_in[tgtid_xicu]           (signal_dspin_rsp_xicu_t);
    xbar_rsp_d->p_local_in[tgtid_mdma]           (signal_dspin_rsp_mdma_t);

    xbar_rsp_d->p_local_out[nb_procs]            (signal_dspin_rsp_mdma_i);

    for (size_t p = 0; p < nb_procs; p++)
        xbar_rsp_d->p_local_out[p]               (signal_dspin_rsp_proc_i[p]);

    if (io)
    {
        xbar_rsp_d->p_local_in[tgtid_mtty]       (signal_dspin_rsp_mtty_t);
        xbar_rsp_d->p_local_in[tgtid_brom]       (signal_dspin_rsp_brom_t);
        xbar_rsp_d->p_local_in[tgtid_bdev]       (signal_dspin_rsp_bdev_t);
        xbar_rsp_d->p_local_in[tgtid_fbuf]       (signal_dspin_rsp_fbuf_t);
        xbar_rsp_d->p_local_in[tgtid_mnic]       (signal_dspin_rsp_mnic_t);
        xbar_rsp_d->p_local_in[tgtid_chbuf]      (signal_dspin_rsp_chbuf_t);
        xbar_rsp_d->p_local_in[tgtid_simh]       (signal_dspin_rsp_simh_t);

        xbar_rsp_d->p_local_out[nb_procs + 1]    (signal_dspin_rsp_bdev_i);
        xbar_rsp_d->p_local_out[nb_procs + 2]    (signal_dspin_rsp_chbuf_i);
    }

    std::cout << "  - Response Direct crossbar connected" << std::endl;

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
        proc[p]->p_irq[0]                   (signal_proc_it[p]);
        for ( size_t j = 1 ; j < 6 ; j++)
        {
            proc[p]->p_irq[j]               (signal_false);
        }

        wi_proc[p]->p_clk                   (this->p_clk);
        wi_proc[p]->p_resetn                (this->p_resetn);
        wi_proc[p]->p_dspin_cmd             (signal_dspin_cmd_proc_i[p]);
        wi_proc[p]->p_dspin_rsp             (signal_dspin_rsp_proc_i[p]);
        wi_proc[p]->p_vci                   (signal_vci_ini_proc[p]);
    }

    std::cout << "  - Processors connected" << std::endl;

    ///////////////////////////////////// XICU
    xicu->p_clk                        (this->p_clk);
    xicu->p_resetn                     (this->p_resetn);
    xicu->p_vci                        (signal_vci_tgt_xicu);
    for (size_t p = 0; p < nb_procs; p++)
    {
        xicu->p_irq[p]                 (signal_proc_it[p]);
    }

#if 1
    if(io)
    {
        xicu->p_hwi[0]             (signal_irq_bdev);
        xicu->p_hwi[1]             (signal_irq_mdma[0]);

        for(size_t i=0; i<4; i++)
            xicu->p_hwi[2+i]        (signal_irq_mtty[i]);
        
        xicu->p_hwi[6]           (signal_irq_memc);
    }
    else
    {
        xicu->p_hwi[1]             (signal_irq_mdma[0]);
        xicu->p_hwi[2]             (signal_irq_memc);
    }

    for(size_t i=0; i < 32; i++)
    {
        if((io && (i > 6)) || (!io && ((i == 0) || (i > 2))))
            xicu->p_hwi[i] (signal_false);
    }
#else
    for (size_t i = 0; i < 32; i++)
    {
        if (io) // I/O cluster
        {
            if      (i < 8)                  xicu->p_hwi[i] (signal_false);
            else if (i < (8 + nb_dmas))      xicu->p_hwi[i] (signal_irq_mdma[i - 8]);
            else if (i < 16)                 xicu->p_hwi[i] (signal_false);
            else if (i < (16 + nb_ttys))     xicu->p_hwi[i] (signal_irq_mtty[i - 16]);
            else if (i < 31)                 xicu->p_hwi[i] (signal_false);
            else                             xicu->p_hwi[i] (signal_irq_bdev);
        }
        else      // other clusters
        {
            if      (i < 8)                  xicu->p_hwi[i] (signal_false);
            else if (i < (8 + nb_dmas))      xicu->p_hwi[i] (signal_irq_mdma[i - 8]);
            else                             xicu->p_hwi[i] (signal_false);
        }
    }
#endif

    // wrapper XICU
    wt_xicu->p_clk                     (this->p_clk);
    wt_xicu->p_resetn                  (this->p_resetn);
    wt_xicu->p_dspin_cmd               (signal_dspin_cmd_xicu_t);
    wt_xicu->p_dspin_rsp               (signal_dspin_rsp_xicu_t);
    wt_xicu->p_vci                     (signal_vci_tgt_xicu);

    std::cout << "  - XICU connected" << std::endl;

    //////////////////////////////////////////////// MEMC
    memc->p_clk                        (this->p_clk);
    memc->p_resetn                     (this->p_resetn);
    memc->p_vci_ixr                    (signal_vci_xram);
    memc->p_vci_tgt                    (signal_vci_tgt_memc);
    memc->p_dspin_p2m                  (signal_dspin_p2m_memc);
    memc->p_dspin_m2p                  (signal_dspin_m2p_memc);
    memc->p_dspin_clack                (signal_dspin_clack_memc);
    memc->p_irq                        (signal_irq_memc);

    // wrapper MEMC
    wt_memc->p_clk                     (this->p_clk);
    wt_memc->p_resetn                  (this->p_resetn);
    wt_memc->p_dspin_cmd               (signal_dspin_cmd_memc_t);
    wt_memc->p_dspin_rsp               (signal_dspin_rsp_memc_t);
    wt_memc->p_vci                     (signal_vci_tgt_memc);

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

    // wrapper tgt MDMA
    wt_mdma->p_clk                     (this->p_clk);
    wt_mdma->p_resetn                  (this->p_resetn);
    wt_mdma->p_dspin_cmd               (signal_dspin_cmd_mdma_t);
    wt_mdma->p_dspin_rsp               (signal_dspin_rsp_mdma_t);
    wt_mdma->p_vci                     (signal_vci_tgt_mdma);

    // wrapper ini MDMA
    wi_mdma->p_clk                     (this->p_clk);
    wi_mdma->p_resetn                  (this->p_resetn);
    wi_mdma->p_dspin_cmd               (signal_dspin_cmd_mdma_i);
    wi_mdma->p_dspin_rsp               (signal_dspin_rsp_mdma_i);
    wi_mdma->p_vci                     (signal_vci_ini_mdma);

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

        // wrapper tgt BDEV
        wt_bdev->p_clk                 (this->p_clk);
        wt_bdev->p_resetn              (this->p_resetn);
        wt_bdev->p_dspin_cmd           (signal_dspin_cmd_bdev_t);
        wt_bdev->p_dspin_rsp           (signal_dspin_rsp_bdev_t);
        wt_bdev->p_vci                 (signal_vci_tgt_bdev);

        // wrapper ini BDEV
        wi_bdev->p_clk                 (this->p_clk);
        wi_bdev->p_resetn              (this->p_resetn);
        wi_bdev->p_dspin_cmd           (signal_dspin_cmd_bdev_i);
        wi_bdev->p_dspin_rsp           (signal_dspin_rsp_bdev_i);
        wi_bdev->p_vci                 (signal_vci_ini_bdev);

        std::cout << "  - BDEV connected" << std::endl;

        // FBUF
        fbuf->p_clk                    (this->p_clk);
        fbuf->p_resetn                 (this->p_resetn);
        fbuf->p_vci                    (signal_vci_tgt_fbuf);

        // wrapper tgt FBUF
        wt_fbuf->p_clk                 (this->p_clk);
        wt_fbuf->p_resetn              (this->p_resetn);
        wt_fbuf->p_dspin_cmd           (signal_dspin_cmd_fbuf_t);
        wt_fbuf->p_dspin_rsp           (signal_dspin_rsp_fbuf_t);
        wt_fbuf->p_vci                 (signal_vci_tgt_fbuf);

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

        // wrapper tgt MNIC
        wt_mnic->p_clk                 (this->p_clk);
        wt_mnic->p_resetn              (this->p_resetn);
        wt_mnic->p_dspin_cmd           (signal_dspin_cmd_mnic_t);
        wt_mnic->p_dspin_rsp           (signal_dspin_rsp_mnic_t);
        wt_mnic->p_vci                 (signal_vci_tgt_mnic);

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

        // wrapper tgt CHBUF
        wt_chbuf->p_clk                 (this->p_clk);
        wt_chbuf->p_resetn              (this->p_resetn);
        wt_chbuf->p_dspin_cmd           (signal_dspin_cmd_chbuf_t);
        wt_chbuf->p_dspin_rsp           (signal_dspin_rsp_chbuf_t);
        wt_chbuf->p_vci                 (signal_vci_tgt_chbuf);

        // wrapper ini CHBUF
        wi_chbuf->p_clk                 (this->p_clk);
        wi_chbuf->p_resetn              (this->p_resetn);
        wi_chbuf->p_dspin_cmd           (signal_dspin_cmd_chbuf_i);
        wi_chbuf->p_dspin_rsp           (signal_dspin_rsp_chbuf_i);
        wi_chbuf->p_vci                 (signal_vci_ini_chbuf);

        std::cout << "  - CHBUF connected" << std::endl;

        // BROM
        brom->p_clk                    (this->p_clk);
        brom->p_resetn                 (this->p_resetn);
        brom->p_vci                    (signal_vci_tgt_brom);

        // wrapper tgt BROM
        wt_brom->p_clk                 (this->p_clk);
        wt_brom->p_resetn              (this->p_resetn);
        wt_brom->p_dspin_cmd           (signal_dspin_cmd_brom_t);
        wt_brom->p_dspin_rsp           (signal_dspin_rsp_brom_t);
        wt_brom->p_vci                 (signal_vci_tgt_brom);

        std::cout << "  - BROM connected" << std::endl;

        // MTTY
        mtty->p_clk                    (this->p_clk);
        mtty->p_resetn                 (this->p_resetn);
        mtty->p_vci                    (signal_vci_tgt_mtty);
        for (size_t i = 0; i < nb_ttys; i++)
        {
            mtty->p_irq[i]             (signal_irq_mtty[i]);
        }

        // wrapper tgt MTTY
        wt_mtty->p_clk                 (this->p_clk);
        wt_mtty->p_resetn              (this->p_resetn);
        wt_mtty->p_dspin_cmd           (signal_dspin_cmd_mtty_t);
        wt_mtty->p_dspin_rsp           (signal_dspin_rsp_mtty_t);
        wt_mtty->p_vci                 (signal_vci_tgt_mtty);


        // Sim Helper
        simhelper->p_clk               (this->p_clk);
        simhelper->p_resetn            (this->p_resetn);
        simhelper->p_vci               (signal_vci_tgt_simh);
        
        // wrapper tgt Sim Helper
        wt_simhelper->p_clk            (this->p_clk);
        wt_simhelper->p_resetn         (this->p_resetn);
        wt_simhelper->p_dspin_cmd      (signal_dspin_cmd_simh_t);
        wt_simhelper->p_dspin_rsp      (signal_dspin_rsp_simh_t);
        wt_simhelper->p_vci            (signal_vci_tgt_simh);

        std::cout << "  - MTTY connected" << std::endl;
   }
} // end constructor



template<size_t dspin_cmd_width,
         size_t dspin_rsp_width,
         typename vci_param_int,
         typename vci_param_ext> TsarXbarCluster<dspin_cmd_width,
                                                 dspin_rsp_width,
                                                 vci_param_int,
                                                 vci_param_ext>::~TsarXbarCluster() {

    dealloc_elems<DspinInput<dspin_cmd_width> > (p_cmd_in, 4, 3);
    dealloc_elems<DspinOutput<dspin_cmd_width> >(p_cmd_out, 4, 3);
    dealloc_elems<DspinInput<dspin_rsp_width> > (p_rsp_in, 4, 2);
    dealloc_elems<DspinOutput<dspin_rsp_width> >(p_rsp_out, 4, 2);

    for (size_t p = 0; p < n_procs; p++)
    {
        delete proc[p];
        delete wi_proc[p];
    }

    delete memc;
    delete wt_memc;
    delete xram;
    delete xicu;
    delete wt_xicu;
    delete mdma;
    delete wt_mdma;
    delete wi_mdma;
    delete xbar_cmd_d;
    delete xbar_rsp_d;
    delete xbar_m2p_c;
    delete xbar_p2m_c;
    delete xbar_clack_c;
    delete router_cmd;
    delete router_rsp;
    if (brom != NULL)
    {
        delete brom;
        delete wt_brom;
        delete fbuf;
        delete wt_fbuf;
        delete bdev;
        delete wt_bdev;
        delete wi_bdev;
        delete mnic;
        delete wt_mnic;
        delete chbuf;
        delete wt_chbuf;
        delete wi_chbuf;
        delete mtty;
        delete wt_mtty;
        delete simhelper;
        delete wt_simhelper;
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



