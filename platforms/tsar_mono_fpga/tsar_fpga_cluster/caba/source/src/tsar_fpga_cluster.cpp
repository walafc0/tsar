//////////////////////////////////////////////////////////////////////////////
// File: tsar_fpga_cluster.cpp
// Author: Alain Greiner
// Copyright: UPMC/LIP6
// Date : february 2014
// This program is released under the GNU public license
//////////////////////////////////////////////////////////////////////////////

#include "../include/tsar_fpga_cluster.h"

namespace soclib {
namespace caba  {

////////////////////////////////////////////////////////////////////////////////////
template<size_t dspin_cmd_width,
    size_t dspin_rsp_width,
    typename vci_param_int,
    typename vci_param_ext> TsarFpgaCluster<dspin_cmd_width,
    dspin_rsp_width,
    vci_param_int,
    vci_param_ext>::TsarFpgaCluster(
            ////////////////////////////////////////////////////////////////////////
            sc_module_name insname,
            size_t nb_procs,
            const soclib::common::MappingTable &mtd,
            const soclib::common::MappingTable &mtx,
            uint32_t reset_address,
            size_t x_width, size_t y_width, size_t l_width,
            size_t tgtid_memc,
            size_t tgtid_xicu,
            size_t tgtid_mtty,
            size_t tgtid_bdev,
            size_t tgtid_xrom,
            const char* disk_pathname,
            size_t memc_ways, size_t memc_sets,
            size_t l1_i_ways, size_t l1_i_sets,
            size_t l1_d_ways, size_t l1_d_sets,
            size_t xram_latency,
            const Loader &loader,
            uint32_t frozen_cycles,
            uint32_t trace_start_cycle,
            bool trace_proc_ok, uint32_t trace_proc_id,
            bool trace_memc_ok )
                : soclib::caba::BaseModule(insname),
                m_nprocs(nb_procs),
                p_clk("clk"),
                p_resetn("resetn")

{
    /////////////////////////////////////////////////////////////////////////////
    // Components definition and allocation
    /////////////////////////////////////////////////////////////////////////////

    // The processor is a MIPS32 wrapped in the GDB server
    // the reset address is defined by the reset_address argument
    typedef GdbServer<Mips32ElIss> mips_iss;
    mips_iss::setResetAddress( reset_address );

    for (size_t p = 0; p < nb_procs; p++)
    {
        bool trace_ok = trace_proc_ok and (trace_proc_id == p);

        std::ostringstream sproc;
        sproc << "proc_" << p;
        proc[p] = new VciCcVCacheWrapper<vci_param_int,
                                         dspin_cmd_width, dspin_rsp_width,
                                         mips_iss > (
                sproc.str().c_str(),
                p,                              // GLOBAL PROC_ID
                mtd,                            // Mapping Table
                IntTab(0,p),                    // SRCID
                p,                              // GLOBAL_CC_ID
                8, 8,                           // ITLB ways & sets
                8, 8,                           // DTLB ways & sets
                l1_i_ways, l1_i_sets, 16,       // ICACHE size
                l1_d_ways, l1_d_sets, 16,       // DCACHE size
                4, 4,                           // WBUF lines & words
                x_width, y_width,
                frozen_cycles,                  // max frozen cycles
                trace_start_cycle, trace_ok );
    }

    /////////////////////////////////////////////////////////////////////////////
    memc = new VciMemCache<vci_param_int, vci_param_ext,
                           dspin_rsp_width, dspin_cmd_width>(
             "memc",
             mtd,                                // Mapping Table direct space
             mtx,                                // Mapping Table external space
             IntTab(0),                          // SRCID external space
             IntTab(0, tgtid_memc),              // TGTID direct space
             x_width, y_width,                   // Number of x,y bits in platform
             memc_ways, memc_sets, 16,           // CACHE SIZE
             3,                                  // MAX NUMBER OF COPIES
             4096,                               // HEAP SIZE
             8, 8, 8,                            // TRT, UPT, IVT DEPTH
             trace_start_cycle,
             trace_memc_ok );

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream sxram;
    xram = new VciSimpleRam<vci_param_ext>(
            "xram",
            IntTab(0),
            mtx,
            loader,
            xram_latency);

    /////////////////////////////////////////////////////////////////////////////
    std::ostringstream sxicu;
    xicu = new VciXicu<vci_param_int>(
            "xicu",
            mtd,                               // mapping table
            IntTab(0, tgtid_xicu),             // TGTID_D
            16,                                // number of timer IRQs
            16,                                // number of hard IRQs
            16,                                // number of soft IRQs
            16 );                              // number of output IRQs

    /////////////////////////////////////////////////////////////////////////////
    size_t nb_initiators = nb_procs + 1;
    size_t nb_targets    = 5;

    std::ostringstream s_xbar_cmd;
    xbar_cmd = new VciLocalCrossbar<vci_param_int>(
            s_xbar_cmd.str().c_str(),
            mtd,                          // mapping table
            0,                            // cluster id
            nb_initiators,                // number of local initiators
            nb_targets,                   // number of local targets 
            0 );                          // default target

    wi_gate = new VciDspinInitiatorWrapper<vci_param_int,
                                           dspin_cmd_width, dspin_rsp_width>(
            "wi_gate",
            x_width + y_width + l_width);

    wt_gate = new VciDspinTargetWrapper<vci_param_int,
                                        dspin_cmd_width, dspin_rsp_width>(
            "wt_gate",
            x_width + y_width + l_width);

    /////////////////////////////////////////////////////////////////////////////
    xbar_m2p = new DspinLocalCrossbar<dspin_cmd_width>(
            "xbar_m2p",
            mtd,                          // mapping table
            0, 0,                         // cluster coordinates
            x_width, y_width, l_width,
            1,                            // number of local sources
            nb_procs,                     // number of local dests 
            2, 2,                         // fifo depths
            true,                         // CMD
            false,                        // don't use local routing table
            true );                       // broadcast

    /////////////////////////////////////////////////////////////////////////////
    xbar_p2m = new DspinLocalCrossbar<dspin_rsp_width>(
            "xbar_p2m",
            mtd,                          // mapping table
            0, 0,                         // cluster coordinates
            x_width, y_width, 0,          // l_width unused on p2m network
            nb_procs,                     // number of local sources
            1,                            // number of local dests
            2, 2,                         // fifo depths
            false,                        // RSP
            false,                        // don't use local routing table
            false );                      // no broadcast 

    /////////////////////////////////////////////////////////////////////////////
    xbar_cla = new DspinLocalCrossbar<dspin_cmd_width>(
            "xbar_cla",
            mtd,                          // mapping table
            0, 0,                         // cluster coordinates
            x_width, y_width, l_width,
            1,                            // number of local sources
            nb_procs,                     // number of local dests 
            2, 2,                         // fifo depths
            true,                         // CMD
            false,                        // don't use local routing table
            false );                      // no broadcast

    /////////////////////////////////////////////
    bdev = new VciBlockDeviceTsar<vci_param_int>(
            "bdev",
            mtd,
            IntTab(0, nb_procs),
            IntTab(0, tgtid_bdev),
            disk_pathname,
            512,
            64 );            // burst size

    /////////////////////////////////////////////
    mtty = new VciMultiTty<vci_param_int>(
            "mtty",
            IntTab(0, tgtid_mtty),
            mtd,
            "tty", NULL );

    /////////////////////////////////////////////
    xrom = new VciSimpleRom<vci_param_int>(
            "xrom",
            IntTab(0, tgtid_xrom),
            mtd,
            loader,
            0 );

    std::cout << std::endl;

    ////////////////////////////////////
    // Connections are defined here
    ////////////////////////////////////

    // CMD DSPIN local crossbar direct
    xbar_cmd->p_clk(this->p_clk);
    xbar_cmd->p_resetn(this->p_resetn);
    xbar_cmd->p_initiator_to_up(signal_vci_l2g);
    xbar_cmd->p_target_to_up(signal_vci_g2l);

    xbar_cmd->p_to_target[tgtid_memc](signal_vci_tgt_memc);
    xbar_cmd->p_to_target[tgtid_xicu](signal_vci_tgt_xicu);
    xbar_cmd->p_to_target[tgtid_mtty](signal_vci_tgt_mtty);
    xbar_cmd->p_to_target[tgtid_bdev](signal_vci_tgt_bdev);
    xbar_cmd->p_to_target[tgtid_xrom](signal_vci_tgt_xrom);

    for (size_t p = 0; p < nb_procs; p++)
    {
        xbar_cmd->p_to_initiator[p](signal_vci_ini_proc[p]);
    }
    xbar_cmd->p_to_initiator[nb_procs](signal_vci_ini_bdev);

    wi_gate->p_clk(this->p_clk);
    wi_gate->p_resetn(this->p_resetn);
    wi_gate->p_vci(signal_vci_l2g);
    wi_gate->p_dspin_cmd(p_cmd_out);
    wi_gate->p_dspin_rsp(p_rsp_in);

    wt_gate->p_clk(this->p_clk);
    wt_gate->p_resetn(this->p_resetn);
    wt_gate->p_vci(signal_vci_g2l);
    wt_gate->p_dspin_cmd(p_cmd_in);
    wt_gate->p_dspin_rsp(p_rsp_out);

    std::cout << "  - CMD & RSP Direct crossbar connected" << std::endl;

    // M2P DSPIN local crossbar coherence
    xbar_m2p->p_clk(this->p_clk);
    xbar_m2p->p_resetn(this->p_resetn);
    xbar_m2p->p_global_out(p_m2p_out);
    xbar_m2p->p_global_in(p_m2p_in);
    xbar_m2p->p_local_in[0](signal_dspin_m2p_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_m2p->p_local_out[p](signal_dspin_m2p_proc[p]);

    std::cout << "  - M2P Coherence crossbar connected" << std::endl;

    ////////////////////////// P2M DSPIN local crossbar coherence
    xbar_p2m->p_clk(this->p_clk);
    xbar_p2m->p_resetn(this->p_resetn);
    xbar_p2m->p_global_out(p_p2m_out);
    xbar_p2m->p_global_in(p_p2m_in);
    xbar_p2m->p_local_out[0](signal_dspin_p2m_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_p2m->p_local_in[p](signal_dspin_p2m_proc[p]);

    std::cout << "  - P2M Coherence crossbar connected" << std::endl;

    ////////////////////// CLACK DSPIN local crossbar coherence
    xbar_cla->p_clk(this->p_clk);
    xbar_cla->p_resetn(this->p_resetn);
    xbar_cla->p_global_out(p_cla_out);
    xbar_cla->p_global_in(p_cla_in);
    xbar_cla->p_local_in[0](signal_dspin_clack_memc);
    for (size_t p = 0; p < nb_procs; p++)
        xbar_cla->p_local_out[p](signal_dspin_clack_proc[p]);

    std::cout << "  - CLA Coherence crossbar connected" << std::endl;

    //////////////////////////////////// Processors
    for (size_t p = 0; p < nb_procs; p++)
    {
        proc[p]->p_clk(this->p_clk);
        proc[p]->p_resetn(this->p_resetn);
        proc[p]->p_vci(signal_vci_ini_proc[p]);
        proc[p]->p_dspin_m2p(signal_dspin_m2p_proc[p]);
        proc[p]->p_dspin_p2m(signal_dspin_p2m_proc[p]);
        proc[p]->p_dspin_clack(signal_dspin_clack_proc[p]);

        for ( size_t j = 0 ; j < 6 ; j++)
        {
            if ( j < 4 ) proc[p]->p_irq[j](signal_proc_irq[4*p + j]);
            else         proc[p]->p_irq[j](signal_false);
        }
    }

    std::cout << "  - Processors connected" << std::endl;

    ///////////////////////////////////// XICU
    xicu->p_clk(this->p_clk);
    xicu->p_resetn(this->p_resetn);
    xicu->p_vci(signal_vci_tgt_xicu);

    for (size_t i = 0 ; i < 16  ; i++)
    {
        xicu->p_irq[i](signal_proc_irq[i]);
    }

    for (size_t i = 0; i < 16; i++)
    {
        if      (i == 8)  xicu->p_hwi[i] (signal_irq_memc);
        else if (i == 9)  xicu->p_hwi[i] (signal_irq_bdev);
        else if (i == 10) xicu->p_hwi[i] (signal_irq_mtty);
        else              xicu->p_hwi[i] (signal_false);
    }

    std::cout << "  - XICU connected" << std::endl;

    // MEMC
    memc->p_clk(this->p_clk);
    memc->p_resetn(this->p_resetn);
    memc->p_irq(signal_irq_memc);
    memc->p_vci_ixr(signal_vci_xram);
    memc->p_vci_tgt(signal_vci_tgt_memc);
    memc->p_dspin_p2m(signal_dspin_p2m_memc);
    memc->p_dspin_m2p(signal_dspin_m2p_memc);
    memc->p_dspin_clack(signal_dspin_clack_memc);

    std::cout << "  - MEMC connected" << std::endl;

    // XRAM
    xram->p_clk(this->p_clk);
    xram->p_resetn(this->p_resetn);
    xram->p_vci(signal_vci_xram);

    std::cout << "  - XRAM connected" << std::endl;

    // BDEV
    bdev->p_clk(this->p_clk);
    bdev->p_resetn(this->p_resetn);
    bdev->p_irq(signal_irq_bdev);
    bdev->p_vci_target(signal_vci_tgt_bdev);
    bdev->p_vci_initiator(signal_vci_ini_bdev);

    std::cout << "  - BDEV connected" << std::endl;

    // MTTY (single channel)
    mtty->p_clk(this->p_clk);
    mtty->p_resetn(this->p_resetn);
    mtty->p_vci(signal_vci_tgt_mtty);
    mtty->p_irq[0](signal_irq_mtty);

    std::cout << "  - MTTY connected" << std::endl;

    // XROM
    xrom->p_clk(this->p_clk);
    xrom->p_resetn(this->p_resetn);
    xrom->p_vci(signal_vci_tgt_xrom);

    std::cout << "  - XROM connected" << std::endl;
} // end constructor

template<size_t dspin_cmd_width, size_t dspin_rsp_width,
         typename vci_param_int, typename vci_param_ext>
         TsarFpgaCluster<dspin_cmd_width, dspin_rsp_width,
                         vci_param_int, vci_param_ext>::~TsarFpgaCluster()
{
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
    delete wi_gate;
    delete wt_gate;
    delete bdev;
    delete mtty;
    delete xrom;
}

}}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4



