//////////////////////////////////////////////////////////////////////////////
// File: tsar_iob_cluster.cpp
// Author: Alain Greiner
// Copyright: UPMC/LIP6
// Date : april 2013
// This program is released under the GNU public license
//////////////////////////////////////////////////////////////////////////////
// Cluster(0,0) & Cluster(xmax-1,ymax-1) contains the IOB0 & IOB1 components.
// These two clusters contain 6 extra components:
// - 1 vci_io_bridge (connected to the 3 networks.
// - 3 vci_dspin_wrapper for the IOB.
// - 2 dspin_local_crossbar for commands and responses.
//////////////////////////////////////////////////////////////////////////////

#include "../include/tsar_iob_cluster.h"

#define MWR_COPROC_CPY  0
#define MWR_COPROC_DCT  1
#define MWR_COPROC_GCD  2

#define tmpl(x) \
   template<typename vci_param_int      , typename vci_param_ext,\
            size_t   dspin_int_cmd_width, size_t   dspin_int_rsp_width,\
            size_t   dspin_ram_cmd_width, size_t   dspin_ram_rsp_width>\
            x TsarIobCluster<\
                  vci_param_int      , vci_param_ext,\
                  dspin_int_cmd_width, dspin_int_rsp_width,\
                  dspin_ram_cmd_width, dspin_ram_rsp_width>

namespace soclib { namespace caba  {

/////////////////////////////////////////////////////////////////////////////
tmpl(/**/)::TsarIobCluster(
/////////////////////////////////////////////////////////////////////////////
                    sc_module_name                     insname,
                    size_t                             nb_procs,
                    size_t                             x_id,
                    size_t                             y_id,
                    size_t                             xmax,
                    size_t                             ymax,

                    const soclib::common::MappingTable &mt_int,
                    const soclib::common::MappingTable &mt_ram,
                    const soclib::common::MappingTable &mt_iox,

                    size_t                             x_width,
                    size_t                             y_width,
                    size_t                             l_width,
                    size_t                             p_width,

                    size_t                             int_memc_tgt_id, // local index
                    size_t                             int_xicu_tgt_id, // local index
                    size_t                             int_mwmr_tgt_id, // local index
                    size_t                             int_iobx_tgt_id, // local index

                    size_t                             int_proc_ini_id, // local index
                    size_t                             int_mwmr_ini_id, // local index
                    size_t                             int_iobx_ini_id, // local index

                    size_t                             ram_xram_tgt_id, // local index
                    size_t                             ram_memc_ini_id, // local index
                    size_t                             ram_iobx_ini_id, // local index

                    bool                               is_io,           // is IO cluster (IOB)?
                    size_t                             iox_iobx_tgt_id, // local_index
                    size_t                             iox_iobx_ini_id, // local index

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
                    size_t                             xcu_nb_out,

                    size_t                             coproc_type,

                    const Loader                      &loader,

                    uint32_t                           frozen_cycles,
                    uint32_t                           debug_start_cycle,
                    bool                               memc_debug_ok,
                    bool                               proc_debug_ok,
                    bool                               iob_debug_ok )
    : soclib::caba::BaseModule(insname),
      p_clk("clk"),
      p_resetn("resetn")
{
    assert( (x_id < xmax) and (y_id < ymax) and 
    "Error in tsar_iob_cluster : Illegal cluster coordinates");

    size_t cluster_id = (x_id<<4) + y_id;

    // Vectors of DSPIN ports for inter-cluster communications
    p_dspin_int_cmd_in  = alloc_elems<DspinInput<dspin_int_cmd_width> >("p_int_cmd_in", 4);
    p_dspin_int_cmd_out = alloc_elems<DspinOutput<dspin_int_cmd_width> >("p_int_cmd_out", 4);

    p_dspin_int_rsp_in  = alloc_elems<DspinInput<dspin_int_rsp_width> >("p_int_rsp_in", 4);
    p_dspin_int_rsp_out = alloc_elems<DspinOutput<dspin_int_rsp_width> >("p_int_rsp_out", 4);

    p_dspin_int_m2p_in  = alloc_elems<DspinInput<dspin_int_cmd_width> >("p_int_m2p_in", 4);
    p_dspin_int_m2p_out = alloc_elems<DspinOutput<dspin_int_cmd_width> >("p_int_m2p_out", 4);

    p_dspin_int_p2m_in  = alloc_elems<DspinInput<dspin_int_rsp_width> >("p_int_p2m_in", 4);
    p_dspin_int_p2m_out = alloc_elems<DspinOutput<dspin_int_rsp_width> >("p_int_p2m_out", 4);

    p_dspin_int_cla_in  = alloc_elems<DspinInput<dspin_int_cmd_width> >("p_int_cla_in", 4);
    p_dspin_int_cla_out = alloc_elems<DspinOutput<dspin_int_cmd_width> >("p_int_cla_out", 4);

    p_dspin_ram_cmd_in  = alloc_elems<DspinInput<dspin_ram_cmd_width> >("p_ext_cmd_in", 4);
    p_dspin_ram_cmd_out = alloc_elems<DspinOutput<dspin_ram_cmd_width> >("p_ext_cmd_out", 4);

    p_dspin_ram_rsp_in  = alloc_elems<DspinInput<dspin_ram_rsp_width> >("p_ext_rsp_in", 4);
    p_dspin_ram_rsp_out = alloc_elems<DspinOutput<dspin_ram_rsp_width> >("p_ext_rsp_out", 4);

    // VCI ports from IOB to IOX network (only in IO clusters)
    if ( is_io )
    {
        p_vci_iob_iox_ini = new soclib::caba::VciInitiator<vci_param_ext>;
        p_vci_iob_iox_tgt = new soclib::caba::VciTarget<vci_param_ext>;
    }

    //////////////////////////////////////////////////////////////////////////////////
    //    Hardware components
    //////////////////////////////////////////////////////////////////////////////////

    ////////////  PROCS  /////////////////////////////////////////////////////////////
    for (size_t p = 0; p < nb_procs; p++)
    {
        std::ostringstream s_proc;
        s_proc << "proc_" << x_id << "_" << y_id << "_" << p;
        proc[p] = new VciCcVCacheWrapper<vci_param_int,
                                         dspin_int_cmd_width,
                                         dspin_int_rsp_width,
                                         GdbServer<Mips32ElIss> >(
                      s_proc.str().c_str(),
                      (cluster_id << p_width) + p,    // GLOBAL PROC_ID
                      mt_int,                         // Mapping Table INT network
                      IntTab(cluster_id,p),           // SRCID
                      (cluster_id << l_width) + p,    // CC_GLOBAL_ID
                      8,                              // ITLB ways
                      8,                              // ITLB sets
                      8,                              // DTLB ways
                      8,                              // DTLB sets
                      l1_i_ways, l1_i_sets, 16,       // ICACHE size
                      l1_d_ways, l1_d_sets, 16,       // DCACHE size
                      4,                              // WBUF nlines
                      4,                              // WBUF nwords
                      x_width,
                      y_width,
                      frozen_cycles,                  // max frozen cycles
                      debug_start_cycle,
                      proc_debug_ok);
    }

    ////////////  MEMC   /////////////////////////////////////////////////////////////
    std::ostringstream s_memc;
    s_memc << "memc_" << x_id << "_" << y_id;
    memc = new VciMemCache<vci_param_int,
                           vci_param_ext,
                           dspin_int_rsp_width,
                           dspin_int_cmd_width>(
                     s_memc.str().c_str(),
                     mt_int,                              // Mapping Table INT network
                     mt_ram,                              // Mapping Table RAM network
                     IntTab(cluster_id, ram_memc_ini_id), // SRCID RAM network
                     IntTab(cluster_id, int_memc_tgt_id), // TGTID INT network
                     x_width,                             // number of bits for x coordinate
                     y_width,                             // number of bits for y coordinate
                     memc_ways, memc_sets, 16,            // CACHE SIZE
                     3,                                   // MAX NUMBER OF COPIES
                     4096,                                // HEAP SIZE
                     8,                                   // TRANSACTION TABLE DEPTH
                     8,                                   // UPDATE TABLE DEPTH
                     8,                                   // INVALIDATE TABLE DEPTH
                     debug_start_cycle,
                     memc_debug_ok );

    std::ostringstream s_wi_memc;
    s_wi_memc << "memc_wi_" << x_id << "_" << y_id;
    memc_ram_wi = new VciDspinInitiatorWrapper<vci_param_ext,
                                               dspin_ram_cmd_width,
                                               dspin_ram_rsp_width>(
                     s_wi_memc.str().c_str(),
                     x_width + y_width + l_width);

    ///////////   XICU  //////////////////////////////////////////////////////////////
    std::ostringstream s_xicu;
    s_xicu << "xicu_" << x_id << "_" << y_id;
    xicu = new VciXicu<vci_param_int>(
                     s_xicu.str().c_str(),
                     mt_int,                              // mapping table INT network
                     IntTab(cluster_id, int_xicu_tgt_id), // TGTID direct space
                     xcu_nb_pti,                          // number of timer IRQs
                     xcu_nb_hwi,                          // number of hard IRQs
                     xcu_nb_wti,                          // number of soft IRQs
                     xcu_nb_out);                         // number of output IRQs

    ////////////  MWMR controller and COPROC  ////////////////////////////////////////
    std::ostringstream s_mwmr;
    std::ostringstream s_copro;
    s_mwmr << "mwmr_" << x_id << "_" << y_id;

    if ( coproc_type ==  MWR_COPROC_CPY) 
    {
        s_copro << "cpy_" << x_id << "_" << y_id;
        cpy = new CoprocCpy( s_copro.str().c_str(), 64 );       // burst size

        mwmr = new VciMwmrDma<vci_param_int>(
                     s_mwmr.str().c_str(),
                     mt_int,
                     IntTab(cluster_id, int_mwmr_ini_id), // SRCID
                     IntTab(cluster_id, int_mwmr_tgt_id), // TGTID
                     1,                                   // nb to_coproc ports
                     1,                                   // nb from_coproc ports
                     1,                                   // nb config registers
                     0,                                   // nb status registers
                     64 );                                // burst size (bytes)
    }
    if ( coproc_type == MWR_COPROC_DCT ) 
    {
        s_copro << "dct_" << x_id << "_" << y_id;
        dct = new CoprocDct( s_copro.str().c_str(), 64 , 16 );  // burst size / latency

        mwmr = new VciMwmrDma<vci_param_int>(
                     s_mwmr.str().c_str(),
                     mt_int,
                     IntTab(cluster_id, int_mwmr_ini_id), // SRCID
                     IntTab(cluster_id, int_mwmr_tgt_id), // TGTID
                     1,                                   // nb to_coproc ports
                     1,                                   // nb from_coproc ports
                     1,                                   // nb config registers
                     0,                                   // nb status registers
                     64 );                                // burst size (bytes)
    }
    if ( coproc_type == MWR_COPROC_GCD ) 
    {
        s_copro << "gcd_" << x_id << "_" << y_id;
        gcd = new CoprocGcd( s_copro.str().c_str(), 64 );       // burst size

        mwmr = new VciMwmrDma<vci_param_int>(
                     s_mwmr.str().c_str(),
                     mt_int,
                     IntTab(cluster_id, int_mwmr_ini_id), // SRCID
                     IntTab(cluster_id, int_mwmr_tgt_id), // TGTID
                     2,                                   // nb to_coproc ports
                     1,                                   // nb from_coproc ports
                     1,                                   // nb config registers
                     0,                                   // nb status registers
                     64 );                                // burst size (bytes)
    }

    ///////////  VCI INT_CMD/RSP LOCAL_XBAR  ////////////////////////////////////// 
    size_t nb_direct_initiators = is_io ? nb_procs + 2 : nb_procs + 1;
    size_t nb_direct_targets    = is_io ? 4 : 3;

    std::ostringstream s_int_xbar_d;
    s_int_xbar_d << "int_xbar_cmd_d_" << x_id << "_" << y_id;
    int_xbar_d = new VciLocalCrossbar<vci_param_int>(
                     s_int_xbar_d.str().c_str(),
                     mt_int,                       // mapping table
                     cluster_id,                   // cluster id
                     nb_direct_initiators,         // number of local initiators
                     nb_direct_targets,            // number of local targets
                     0 );                          // default target

    std::ostringstream s_int_dspin_ini_wrapper_gate_d;
    s_int_dspin_ini_wrapper_gate_d << "int_dspin_ini_wrapper_gate_d_"
                                   << x_id << "_" << y_id;
    int_wi_gate_d = new VciDspinInitiatorWrapper<vci_param_int,
                                           dspin_int_cmd_width,
                                           dspin_int_rsp_width>(
                     s_int_dspin_ini_wrapper_gate_d.str().c_str(),
                     x_width + y_width + l_width);

    std::ostringstream s_int_dspin_tgt_wrapper_gate_d;
    s_int_dspin_tgt_wrapper_gate_d << "int_dspin_tgt_wrapper_gate_d_"
                                   << x_id << "_" << y_id;
    int_wt_gate_d = new VciDspinTargetWrapper<vci_param_int,
                                        dspin_int_cmd_width,
                                        dspin_int_rsp_width>(
                     s_int_dspin_tgt_wrapper_gate_d.str().c_str(),
                     x_width + y_width + l_width);

    ////////////  DSPIN INT_M2P LOCAL_XBAR  ////////////////////////////////////////
    std::ostringstream s_int_xbar_m2p_c;
    s_int_xbar_m2p_c << "int_xbar_m2p_c_" << x_id << "_" << y_id;
    int_xbar_m2p_c = new DspinLocalCrossbar<dspin_int_cmd_width>(
                     s_int_xbar_m2p_c.str().c_str(),
                     mt_int,                       // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, l_width,    // several dests
                     1,                            // number of local sources
                     nb_procs,                     // number of local dests
                     2, 2,                         // fifo depths
                     true,                         // pseudo CMD
                     false,                        // no routing table
                     true );                       // broacast

    ////////////  DSPIN INT_P2M LOCAL_XBAR  ////////////////////////////////////////
    std::ostringstream s_int_xbar_p2m_c;
    s_int_xbar_p2m_c << "int_xbar_p2m_c_" << x_id << "_" << y_id;
    int_xbar_p2m_c = new DspinLocalCrossbar<dspin_int_rsp_width>(
                     s_int_xbar_p2m_c.str().c_str(),
                     mt_int,                       // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, 0,          // only one dest
                     nb_procs,                     // number of local sources
                     1,                            // number of local dests
                     2, 2,                         // fifo depths
                     false,                        // pseudo RSP
                     false,                        // no routing table
                     false );                      // no broacast

    ////////////  DSPIN INT_CLA LOCAL_XBAR  ////////////////////////////////////////
    std::ostringstream s_int_xbar_clack_c;
    s_int_xbar_clack_c << "int_xbar_clack_c_" << x_id << "_" << y_id;
    int_xbar_clack_c = new DspinLocalCrossbar<dspin_int_cmd_width>(
                     s_int_xbar_clack_c.str().c_str(),
                     mt_int,                       // mapping table
                     x_id, y_id,                   // cluster coordinates
                     x_width, y_width, l_width,
                     1,                            // number of local sources
                     nb_procs,                     // number of local targets
                     1, 1,                         // fifo depths
                     true,                         // CMD
                     false,                        // no routing table
                     false);                       // broadcast

    ////////////  DSPIN INT_CMD ROUTER ////////////////////////////////////////////
    std::ostringstream s_int_router_cmd;
    s_int_router_cmd << "int_router_cmd_" << x_id << "_" << y_id;
    int_router_cmd = new DspinRouter<dspin_int_cmd_width>(
                     s_int_router_cmd.str().c_str(),
                     x_id,y_id,                    // coordinate in the mesh
                     x_width, y_width,             // x & y fields width
                     4,4);                         // input & output fifo depths

    ////////////  DSPIN INT_RSP ROUTER ////////////////////////////////////////////
    std::ostringstream s_int_router_rsp;
    s_int_router_rsp << "int_router_rsp_" << x_id << "_" << y_id;
    int_router_rsp = new DspinRouter<dspin_int_rsp_width>(
                     s_int_router_rsp.str().c_str(),
                     x_id,y_id,                    // coordinates in mesh
                     x_width, y_width,             // x & y fields width
                     4,4);                         // input & output fifo depths

    ////////////  DSPIN INT_M2P ROUTER ////////////////////////////////////////////
    std::ostringstream s_int_router_m2p;
    s_int_router_m2p << "int_router_m2p_" << x_id << "_" << y_id;
    int_router_m2p = new DspinRouter<dspin_int_cmd_width>(
                     s_int_router_m2p.str().c_str(),
                     x_id,y_id,                    // coordinate in the mesh
                     x_width, y_width,             // x & y fields width
                     4,4,                          // input & output fifo depths
                     true);                        // broadcast supported

    ////////////  DSPIN INT_P2M ROUTER ////////////////////////////////////////////
    std::ostringstream s_int_router_p2m;
    s_int_router_p2m << "int_router_p2m_" << x_id << "_" << y_id;
    int_router_p2m = new DspinRouter<dspin_int_rsp_width>(
                     s_int_router_p2m.str().c_str(),
                     x_id,y_id,                    // coordinates in mesh
                     x_width, y_width,             // x & y fields width
                     4,4);                         // input & output fifo depths

    ////////////  DSPIN INT_CLA ROUTER ////////////////////////////////////////////
    std::ostringstream s_int_router_cla;
    s_int_router_cla << "int_router_cla_" << x_id << "_" << y_id;
    int_router_cla = new DspinRouter<dspin_int_cmd_width>(
                     s_int_router_cla.str().c_str(),
                     x_id,y_id,                    // coordinate in the mesh
                     x_width, y_width,             // x & y fields width
                     4,4);                         // input & output fifo depths

    //////////////  XRAM  /////////////////////////////////////////////////////////
    std::ostringstream s_xram;
    s_xram << "xram_" << x_id << "_" << y_id;
    xram = new VciSimpleRam<vci_param_ext>(
                     s_xram.str().c_str(),
                     IntTab(cluster_id, ram_xram_tgt_id),
                     mt_ram,
                     loader,
                     xram_latency);

    std::ostringstream s_wt_xram;
    s_wt_xram << "xram_wt_" << x_id << "_" << y_id;
    xram_ram_wt = new VciDspinTargetWrapper<vci_param_ext,
                                            dspin_ram_cmd_width,
                                            dspin_ram_rsp_width>(
                     s_wt_xram.str().c_str(),
                     x_width + y_width + l_width);

    ////////////  DSPIN RAM_CMD ROUTER  ///////////////////////////////////////////
    std::ostringstream s_ram_router_cmd;
    s_ram_router_cmd << "ram_router_cmd_" << x_id << "_" << y_id;
    ram_router_cmd = new DspinRouter<dspin_ram_cmd_width>(
                     s_ram_router_cmd.str().c_str(),
                     x_id, y_id,                     // router coordinates in mesh
                     x_width,                        // x field width in first flit
                     y_width,                        // y field width in first flit
                     4, 4);                          // input & output fifo depths

    ////////////  DSPIN RAM_RSP ROUTER  ///////////////////////////////////////////
    std::ostringstream s_ram_router_rsp;
    s_ram_router_rsp << "ram_router_rsp_" << x_id << "_" << y_id;
    ram_router_rsp = new DspinRouter<dspin_ram_rsp_width>(
                     s_ram_router_rsp.str().c_str(),
                     x_id, y_id,                     // coordinates in mesh
                     x_width,                        // x field width in first flit
                     y_width,                        // y field width in first flit
                     4, 4);                          // input & output fifo depths


    ////////////////////// I/O  CLUSTER ONLY    ///////////////////////////////////
    if ( is_io )
    {
        ///////////  IO_BRIDGE ////////////////////////////////////////////////////
        std::ostringstream s_iob;
        s_iob << "iob_" << x_id << "_" << y_id;
        iob = new VciIoBridge<vci_param_int,
                              vci_param_ext>(
                     s_iob.str().c_str(),
                     mt_ram,                                // EXT network maptab
                     mt_int,                                // INT network maptab
                     mt_iox,                                // IOX network maptab
                     IntTab( cluster_id, int_iobx_tgt_id ), // INT TGTID
                     IntTab( cluster_id, int_iobx_ini_id ), // INT SRCID
                     IntTab( 0         , iox_iobx_tgt_id ), // IOX TGTID
                     IntTab( 0         , iox_iobx_ini_id ), // IOX SRCID
                     16,                                    // cache line words
                     8,                                     // IOTLB ways
                     8,                                     // IOTLB sets
                     debug_start_cycle,
                     iob_debug_ok );

        std::ostringstream s_iob_ram_wi;
        s_iob_ram_wi << "iob_ram_wi_" << x_id << "_" << y_id;
        iob_ram_wi = new VciDspinInitiatorWrapper<vci_param_ext,
                                                  dspin_ram_cmd_width,
                                                  dspin_ram_rsp_width>(
                     s_iob_ram_wi.str().c_str(),
                     vci_param_int::S);

        ////////////  DSPIN RAM_CMD LOCAL_XBAR  ///////////////////////////////////
        std::ostringstream s_ram_xbar_cmd;
        s_ram_xbar_cmd << "s_ram_xbar_cmd_" << x_id << "_" << y_id;
        ram_xbar_cmd = new DspinLocalCrossbar<dspin_ram_cmd_width>(
              s_ram_xbar_cmd.str().c_str(), // name
              mt_ram,                       // mapping table
              x_id, y_id,                   // x, y
              x_width, y_width, l_width,    // x_width, y_width, l_width
              2, 0,                         // local inputs, local outputs
              2, 2,                         // in fifo, out fifo depths
              true,                         // is cmd ?
              false,                        // use routing table ?
              false);                       // support broadcast ?

        ////////////  DSPIN RAM_RSP LOCAL_XBAR  ///////////////////////////////////
        std::ostringstream s_ram_xbar_rsp;
        s_ram_xbar_rsp << "s_ram_xbar_rsp_" << x_id << "_" << y_id;
        ram_xbar_rsp = new DspinLocalCrossbar<dspin_ram_rsp_width>(
              s_ram_xbar_rsp.str().c_str(), // name
              mt_ram,                       // mapping table
              x_id, y_id,                   // x, y
              x_width, y_width, l_width,    // x_width, y_width, l_width
              0, 2,                         // local inputs, local outputs
              2, 2,                         // in fifo, out fifo depths
              false,                        // is cmd ?
              true,                         // use routing table ?
              false);                       // support broadcast ?

    } // end if IO

    ////////////////////////////////////
    // Connections are defined here
    ////////////////////////////////////

    // on coherence network : local srcid[proc] in [0...nb_procs-1]
    //                      : local srcid[memc] = nb_procs

    //////////////////////////////////// Processors
    for (size_t p = 0; p < nb_procs; p++)
    {
        proc[p]->p_clk                           (this->p_clk);
        proc[p]->p_resetn                        (this->p_resetn);

        proc[p]->p_vci                           (signal_int_vci_ini_proc[p]);
        proc[p]->p_dspin_m2p                     (signal_int_dspin_m2p_proc[p]);
        proc[p]->p_dspin_p2m                     (signal_int_dspin_p2m_proc[p]);
        proc[p]->p_dspin_clack                   (signal_int_dspin_cla_proc[p]);

        for ( size_t j = 0 ; j < 6 ; j++)
        {
            if ( j < 4 ) proc[p]->p_irq[j]       (signal_proc_it[4*p + j]);
            else         proc[p]->p_irq[j]       (signal_false);
        }
    }

    std::cout << "  - processors connected" << std::endl;

    ///////////////////////////////////// XICU
    xicu->p_clk                                  (this->p_clk);
    xicu->p_resetn                               (this->p_resetn);
    xicu->p_vci                                  (signal_int_vci_tgt_xicu);
    for ( size_t i=0 ; i < xcu_nb_out  ; i++)
    {
        xicu->p_irq[i]                           (signal_proc_it[i]);
    }
    for ( size_t i=0 ; i < xcu_nb_hwi ; i++)
    {
        if      ( i == 0 )       xicu->p_hwi[i]  (signal_irq_memc);
        else if ( i == 1 )       xicu->p_hwi[i]  (signal_irq_mwmr);
        else                     xicu->p_hwi[i]  (signal_false);
    }

    std::cout << "  - xcu connected" << std::endl;

    ///////////////////////////////////// MEMC
    memc->p_clk                                  (this->p_clk);
    memc->p_resetn                               (this->p_resetn);
    memc->p_vci_ixr                              (signal_ram_vci_ini_memc);
    memc->p_vci_tgt                              (signal_int_vci_tgt_memc);
    memc->p_dspin_p2m                            (signal_int_dspin_p2m_memc);
    memc->p_dspin_m2p                            (signal_int_dspin_m2p_memc);
    memc->p_dspin_clack                          (signal_int_dspin_cla_memc);
    memc->p_irq                                  (signal_irq_memc);

    // wrapper to RAM network
    memc_ram_wi->p_clk                           (this->p_clk);
    memc_ram_wi->p_resetn                        (this->p_resetn);
    memc_ram_wi->p_dspin_cmd                     (signal_ram_dspin_cmd_memc_i);
    memc_ram_wi->p_dspin_rsp                     (signal_ram_dspin_rsp_memc_i);
    memc_ram_wi->p_vci                           (signal_ram_vci_ini_memc);

    std::cout << "  - memc connected" << std::endl;

    //////////////////////////////////// XRAM
    xram->p_clk                                  (this->p_clk);
    xram->p_resetn                               (this->p_resetn);
    xram->p_vci                                  (signal_ram_vci_tgt_xram);

    // wrapper to RAM network
    xram_ram_wt->p_clk                           (this->p_clk);
    xram_ram_wt->p_resetn                        (this->p_resetn);
    xram_ram_wt->p_dspin_cmd                     (signal_ram_dspin_cmd_xram_t);
    xram_ram_wt->p_dspin_rsp                     (signal_ram_dspin_rsp_xram_t);
    xram_ram_wt->p_vci                           (signal_ram_vci_tgt_xram);

    std::cout << "  - xram connected" << std::endl;

    /////////////////////////////////// GCD coprocessor
    if ( coproc_type == MWR_COPROC_GCD )
    {
        gcd->p_clk                               (this->p_clk);
        gcd->p_resetn                            (this->p_resetn);
        gcd->p_opa                               (signal_to_coproc[0]);
        gcd->p_opb                               (signal_to_coproc[1]);
        gcd->p_res                               (signal_from_coproc[0]);
        gcd->p_config                            (signal_config_coproc[0]);

        mwmr->p_clk                              (this->p_clk);
        mwmr->p_resetn                           (this->p_resetn);
        mwmr->p_vci_target                       (signal_int_vci_tgt_mwmr);
        mwmr->p_vci_initiator                    (signal_int_vci_ini_mwmr);
        mwmr->p_to_coproc[0]                     (signal_to_coproc[0]);
        mwmr->p_to_coproc[1]                     (signal_to_coproc[1]);
        mwmr->p_from_coproc[0]                   (signal_from_coproc[0]);
        mwmr->p_config[0]                        (signal_config_coproc[0]);
        mwmr->p_irq                              (signal_irq_mwmr);
    }

    /////////////////////////////////// DCT coprocessor
    if ( coproc_type == MWR_COPROC_DCT )
    {
        dct->p_clk                               (this->p_clk);
        dct->p_resetn                            (this->p_resetn);
        dct->p_in                                (signal_to_coproc[0]);
        dct->p_out                               (signal_from_coproc[0]);
        dct->p_config                            (signal_config_coproc[0]);

        mwmr->p_clk                              (this->p_clk);
        mwmr->p_resetn                           (this->p_resetn);
        mwmr->p_vci_target                       (signal_int_vci_tgt_mwmr);
        mwmr->p_vci_initiator                    (signal_int_vci_ini_mwmr);
        mwmr->p_to_coproc[0]                     (signal_to_coproc[0]);
        mwmr->p_from_coproc[0]                   (signal_from_coproc[0]);
        mwmr->p_config[0]                        (signal_config_coproc[0]);
        mwmr->p_irq                              (signal_irq_mwmr);
    }

    std::cout << "  - coproc connected" << std::endl;

    /////////////////////////////////// CPY coprocessor
    if ( coproc_type == MWR_COPROC_CPY )
    {
        cpy->p_clk                               (this->p_clk);
        cpy->p_resetn                            (this->p_resetn);
        cpy->p_load                              (signal_to_coproc[0]);
        cpy->p_store                             (signal_from_coproc[0]);
        cpy->p_config                            (signal_config_coproc[0]);

        mwmr->p_clk                              (this->p_clk);
        mwmr->p_resetn                           (this->p_resetn);
        mwmr->p_vci_target                       (signal_int_vci_tgt_mwmr);
        mwmr->p_vci_initiator                    (signal_int_vci_ini_mwmr);
        mwmr->p_to_coproc[0]                     (signal_to_coproc[0]);
        mwmr->p_from_coproc[0]                   (signal_from_coproc[0]);
        mwmr->p_config[0]                        (signal_config_coproc[0]);
        mwmr->p_irq                              (signal_irq_mwmr);
    }

    //////////////////////////// RAM NETWORK ROUTERS
    ram_router_cmd->p_clk                        (this->p_clk);
    ram_router_cmd->p_resetn                     (this->p_resetn);
    ram_router_rsp->p_clk                        (this->p_clk);
    ram_router_rsp->p_resetn                     (this->p_resetn);

    for( size_t n=0 ; n<4 ; n++)
    {
        ram_router_cmd->p_out[n]                 (this->p_dspin_ram_cmd_out[n]);
        ram_router_cmd->p_in[n]                  (this->p_dspin_ram_cmd_in[n]);
        ram_router_rsp->p_out[n]                 (this->p_dspin_ram_rsp_out[n]);
        ram_router_rsp->p_in[n]                  (this->p_dspin_ram_rsp_in[n]);
    }

    ram_router_cmd->p_out[4]                     (signal_ram_dspin_cmd_xram_t);
    ram_router_rsp->p_in[4]                      (signal_ram_dspin_rsp_xram_t);

    if ( is_io )
    {
       ram_router_cmd->p_in[4]                   (signal_ram_dspin_cmd_xbar);
       ram_router_rsp->p_out[4]                  (signal_ram_dspin_rsp_xbar);
    }
    else
    {
       ram_router_cmd->p_in[4]                   (signal_ram_dspin_cmd_memc_i);
       ram_router_rsp->p_out[4]                  (signal_ram_dspin_rsp_memc_i);
    }

    ///////////////////////////// INT NETWORK ROUTERS
    int_router_cmd->p_clk                        (this->p_clk);
    int_router_cmd->p_resetn                     (this->p_resetn);
    int_router_rsp->p_clk                        (this->p_clk);
    int_router_rsp->p_resetn                     (this->p_resetn);
    int_router_m2p->p_clk                        (this->p_clk);
    int_router_m2p->p_resetn                     (this->p_resetn);
    int_router_p2m->p_clk                        (this->p_clk);
    int_router_p2m->p_resetn                     (this->p_resetn);
    int_router_cla->p_clk                        (this->p_clk);
    int_router_cla->p_resetn                     (this->p_resetn);

    // loop on N/S/E/W ports
    for (size_t i = 0; i < 4; i++)
    {
        int_router_cmd->p_out[i]                 (this->p_dspin_int_cmd_out[i]);
        int_router_cmd->p_in[i]                  (this->p_dspin_int_cmd_in[i]);

        int_router_rsp->p_out[i]                 (this->p_dspin_int_rsp_out[i]);
        int_router_rsp->p_in[i]                  (this->p_dspin_int_rsp_in[i]);

        int_router_m2p->p_out[i]                 (this->p_dspin_int_m2p_out[i]);
        int_router_m2p->p_in[i]                  (this->p_dspin_int_m2p_in[i]);

        int_router_p2m->p_out[i]                 (this->p_dspin_int_p2m_out[i]);
        int_router_p2m->p_in[i]                  (this->p_dspin_int_p2m_in[i]);

        int_router_cla->p_out[i]                 (this->p_dspin_int_cla_out[i]);
        int_router_cla->p_in[i]                  (this->p_dspin_int_cla_in[i]);
    }

    int_router_cmd->p_out[4]                     (signal_int_dspin_cmd_g2l_d);
    int_router_cmd->p_in[4]                      (signal_int_dspin_cmd_l2g_d);

    int_router_rsp->p_out[4]                     (signal_int_dspin_rsp_g2l_d);
    int_router_rsp->p_in[4]                      (signal_int_dspin_rsp_l2g_d);

    int_router_m2p->p_out[4]                     (signal_int_dspin_m2p_g2l_c);
    int_router_m2p->p_in[4]                      (signal_int_dspin_m2p_l2g_c);

    int_router_p2m->p_out[4]                     (signal_int_dspin_p2m_g2l_c);
    int_router_p2m->p_in[4]                      (signal_int_dspin_p2m_l2g_c);

    int_router_cla->p_out[4]                     (signal_int_dspin_cla_g2l_c);
    int_router_cla->p_in[4]                      (signal_int_dspin_cla_l2g_c);

    std::cout << "  - internal routers connected" << std::endl;


    ///////////////////// CMD DSPIN  local crossbar direct
    int_xbar_d->p_clk                            (this->p_clk);
    int_xbar_d->p_resetn                         (this->p_resetn);
    int_xbar_d->p_initiator_to_up                (signal_int_vci_l2g);
    int_xbar_d->p_target_to_up                   (signal_int_vci_g2l);

    int_xbar_d->p_to_target[int_memc_tgt_id]     (signal_int_vci_tgt_memc);
    int_xbar_d->p_to_target[int_xicu_tgt_id]     (signal_int_vci_tgt_xicu);
    int_xbar_d->p_to_target[int_mwmr_tgt_id]     (signal_int_vci_tgt_mwmr);
    int_xbar_d->p_to_initiator[int_mwmr_ini_id]  (signal_int_vci_ini_mwmr);
    for (size_t p = 0; p < nb_procs; p++)
       int_xbar_d->p_to_initiator[int_proc_ini_id + p] (signal_int_vci_ini_proc[p]);

    if ( is_io )
    {
       int_xbar_d->p_to_target[int_iobx_tgt_id]        (signal_int_vci_tgt_iobx);
       int_xbar_d->p_to_initiator[int_iobx_ini_id]     (signal_int_vci_ini_iobx);
    }

    int_wi_gate_d->p_clk                         (this->p_clk);
    int_wi_gate_d->p_resetn                      (this->p_resetn);
    int_wi_gate_d->p_vci                         (signal_int_vci_l2g);
    int_wi_gate_d->p_dspin_cmd                   (signal_int_dspin_cmd_l2g_d);
    int_wi_gate_d->p_dspin_rsp                   (signal_int_dspin_rsp_g2l_d);

    int_wt_gate_d->p_clk                         (this->p_clk);
    int_wt_gate_d->p_resetn                      (this->p_resetn);
    int_wt_gate_d->p_vci                         (signal_int_vci_g2l);
    int_wt_gate_d->p_dspin_cmd                   (signal_int_dspin_cmd_g2l_d);
    int_wt_gate_d->p_dspin_rsp                   (signal_int_dspin_rsp_l2g_d);

    ////////////////////// M2P DSPIN local crossbar coherence
    int_xbar_m2p_c->p_clk                        (this->p_clk);
    int_xbar_m2p_c->p_resetn                     (this->p_resetn);
    int_xbar_m2p_c->p_global_out                 (signal_int_dspin_m2p_l2g_c);
    int_xbar_m2p_c->p_global_in                  (signal_int_dspin_m2p_g2l_c);
    int_xbar_m2p_c->p_local_in[0]                (signal_int_dspin_m2p_memc);
    for (size_t p = 0; p < nb_procs; p++)
        int_xbar_m2p_c->p_local_out[p]           (signal_int_dspin_m2p_proc[p]);

    ////////////////////////// P2M DSPIN local crossbar coherence
    int_xbar_p2m_c->p_clk                        (this->p_clk);
    int_xbar_p2m_c->p_resetn                     (this->p_resetn);
    int_xbar_p2m_c->p_global_out                 (signal_int_dspin_p2m_l2g_c);
    int_xbar_p2m_c->p_global_in                  (signal_int_dspin_p2m_g2l_c);
    int_xbar_p2m_c->p_local_out[0]               (signal_int_dspin_p2m_memc);
    for (size_t p = 0; p < nb_procs; p++)
        int_xbar_p2m_c->p_local_in[p]            (signal_int_dspin_p2m_proc[p]);

    ////////////////////// CLACK DSPIN local crossbar coherence
    int_xbar_clack_c->p_clk                      (this->p_clk);
    int_xbar_clack_c->p_resetn                   (this->p_resetn);
    int_xbar_clack_c->p_global_out               (signal_int_dspin_cla_l2g_c);
    int_xbar_clack_c->p_global_in                (signal_int_dspin_cla_g2l_c);
    int_xbar_clack_c->p_local_in[0]              (signal_int_dspin_cla_memc);
    for (size_t p = 0; p < nb_procs; p++)
        int_xbar_clack_c->p_local_out[p]         (signal_int_dspin_cla_proc[p]);


    ///////////////////////// IOB exists only in cluster_iob0 & cluster_iob1.
    if ( is_io )
    {
        // IO bridge
        iob->p_clk                                 (this->p_clk);
        iob->p_resetn                              (this->p_resetn);
        iob->p_vci_ini_iox                         (*(this->p_vci_iob_iox_ini));
        iob->p_vci_tgt_iox                         (*(this->p_vci_iob_iox_tgt));
        iob->p_vci_tgt_int                         (signal_int_vci_tgt_iobx);
        iob->p_vci_ini_int                         (signal_int_vci_ini_iobx);
        iob->p_vci_ini_ram                         (signal_ram_vci_ini_iobx);

        // initiator wrapper to RAM network
        iob_ram_wi->p_clk                          (this->p_clk);
        iob_ram_wi->p_resetn                       (this->p_resetn);
        iob_ram_wi->p_dspin_cmd                    (signal_ram_dspin_cmd_iob_i);
        iob_ram_wi->p_dspin_rsp                    (signal_ram_dspin_rsp_iob_i);
        iob_ram_wi->p_vci                          (signal_ram_vci_ini_iobx);

        // crossbar between MEMC and IOB to RAM network
        ram_xbar_cmd->p_clk                        (this->p_clk);
        ram_xbar_cmd->p_resetn                     (this->p_resetn);
        ram_xbar_cmd->p_global_out                 (signal_ram_dspin_cmd_xbar);
        ram_xbar_cmd->p_global_in                  (signal_ram_dspin_cmd_false);
        ram_xbar_cmd->p_local_in[ram_memc_ini_id]  (signal_ram_dspin_cmd_memc_i);
        ram_xbar_cmd->p_local_in[ram_iobx_ini_id]  (signal_ram_dspin_cmd_iob_i);

        ram_xbar_rsp->p_clk                        (this->p_clk);
        ram_xbar_rsp->p_resetn                     (this->p_resetn);
        ram_xbar_rsp->p_global_out                 (signal_ram_dspin_rsp_false);
        ram_xbar_rsp->p_global_in                  (signal_ram_dspin_rsp_xbar);
        ram_xbar_rsp->p_local_out[ram_memc_ini_id] (signal_ram_dspin_rsp_memc_i);
        ram_xbar_rsp->p_local_out[ram_iobx_ini_id] (signal_ram_dspin_rsp_iob_i);
    }

    SC_METHOD(init);

} // end constructor

tmpl(void)::init()
{
   signal_ram_dspin_cmd_false.write = false;
   signal_ram_dspin_rsp_false.read  = true;
} 

}}


// Local Variables:
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3

