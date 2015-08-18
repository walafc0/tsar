/////////////////////////////////////////////////////////////////////////
// File: top.cpp (for tsar_mono_fpga)
// Author: Cesar Fuguet
// Copyright: UPMC/LIP6
// Date : March 2015
// This program is released under the GNU public license
/////////////////////////////////////////////////////////////////////////
#include <systemc>
#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstdarg>
#include <stdint.h>

#include "gdbserver.h"
#include "mapping_table.h"
#include "tsar_fpga_cluster.h"
#include "vci_local_crossbar.h"
#include "vci_dspin_initiator_wrapper.h"
#include "vci_dspin_target_wrapper.h"
#include "vci_multi_tty.h"
#include "vci_block_device_tsar.h"
#include "vci_framebuffer.h"
#include "alloc_elems.h"

#include "hard_config.h"

///////////////////////////////////////////////////
//               Parallelisation
///////////////////////////////////////////////////
#define USE_OPENMP _OPENMP

#if USE_OPENMP
#include <omp.h>
#endif

///////////////////////////////////////////////////
//  cluster index (from x,y coordinates)
///////////////////////////////////////////////////

#define cluster(x,y)   ((y) + ((x) << Y_WIDTH))

///////////////////////////////////////////////////////////
//          DSPIN parameters
///////////////////////////////////////////////////////////

#define dspin_cmd_width      39
#define dspin_rsp_width      32

///////////////////////////////////////////////////////////
//          VCI parameters
///////////////////////////////////////////////////////////

#define vci_cell_width_int    4
#define vci_cell_width_ext    8
#define vci_address_width     40
#define vci_plen_width        8
#define vci_rerror_width      1
#define vci_clen_width        1
#define vci_rflag_width       1
#define vci_srcid_width       14
#define vci_pktid_width       4
#define vci_trdid_width       4
#define vci_wrplen_width      1


///////////////////////////////////////////////////////////////////////////////////////
//    Secondary Hardware Parameters
///////////////////////////////////////////////////////////////////////////////////////

#define XMAX                  X_SIZE   // actual number of columns in 2D mesh
#define YMAX                  Y_SIZE   // actual number of rows in 2D mesh

#define XRAM_LATENCY          0

#define MEMC_WAYS             16
#define MEMC_SETS             256

#define L1_IWAYS              4
#define L1_ISETS              64

#define L1_DWAYS              4
#define L1_DSETS              64

#define BDEV_IMAGE_NAME       "../../../giet_vm/hdd/virt_hdd.dmg"

#define ROM_SOFT_NAME         "../../softs/tsar_boot/preloader.elf"

#define NORTH                 0
#define SOUTH                 1
#define EAST                  2
#define WEST                  3

///////////////////////////////////////////////////////////////////////////////////////
//     DEBUG Parameters default values
///////////////////////////////////////////////////////////////////////////////////////

#define MAX_FROZEN_CYCLES     500000

///////////////////////////////////////////////////////////////////////////////////////
//     LOCAL TGTID & SRCID definition
// For all components:  global TGTID = global SRCID = cluster_index
///////////////////////////////////////////////////////////////////////////////////////

#define MEMC_TGTID            0
#define XICU_TGTID            1
#define XROM_TGTID            2
#define MTTY_TGTID            3
#define BDEV_TGTID            4
#define FBUF_TGTID            5

#define BDEV_SRCID            NB_PROCS_MAX

bool stop_called = false;

#define SIMULATION_PERIOD     5000000

/////////////////////////////////
int _main(int argc, char *argv[])
{
    using namespace sc_core;
    using namespace soclib::caba;
    using namespace soclib::common;

    uint64_t ncycles           = UINT64_MAX;         // max simulated cycles
    size_t   threads           = 1;                  // simulator's threads number
    bool     trace_ok          = false;              // trace activated
    uint32_t trace_from        = 0;                  // trace start cycle
    bool     trace_proc_ok     = false;              // detailed proc trace activated
    size_t   trace_memc_ok     = false;              // detailed memc trace activated
    size_t   trace_memc_id     = 0;                  // index of memc to be traced
    size_t   trace_proc_id     = 0;                  // index of proc to be traced
    char     soft_name[256]    = ROM_SOFT_NAME;      // pathname for ROM binary code
    char     disk_name[256]    = BDEV_IMAGE_NAME;    // pathname for DISK image
    uint32_t frozen_cycles     = MAX_FROZEN_CYCLES;  // for debug
    uint64_t simulation_period = SIMULATION_PERIOD;

    ////////////// command line arguments //////////////////////
    if (argc > 1)
    {
        for (int n = 1; n < argc; n = n + 2)
        {
            if ((strcmp(argv[n], "-NCYCLES") == 0) && (n + 1 < argc))
            {
                ncycles = (uint64_t) strtol(argv[n + 1], NULL, 0);
            }
            else if ((strcmp(argv[n],"-DEBUG") == 0) && (n + 1 < argc))
            {
                trace_ok = true;
                trace_from = (uint32_t) strtol(argv[n + 1], NULL, 0);
                simulation_period = 1;
            }
            else if ((strcmp(argv[n], "-MEMCID") == 0) && (n + 1 < argc))
            {
                trace_memc_ok = true;
                trace_memc_id = (size_t) strtol(argv[n + 1], NULL, 0);
                size_t x = trace_memc_id >> Y_WIDTH;
                size_t y = trace_memc_id & ((1<<Y_WIDTH)-1);

                assert( (x < XMAX) and (y < (YMAX)) and
                        "MEMCID parameter refers a not valid memory cache");
            }
            else if ((strcmp(argv[n], "-PROCID") == 0) && (n + 1 < argc))
            {
                trace_proc_ok = true;
                trace_proc_id = (size_t) strtol(argv[n + 1], NULL, 0);
                size_t cluster_xy = trace_proc_id >> P_WIDTH ;
                size_t x          = cluster_xy >> Y_WIDTH;
                size_t y          = cluster_xy & ((1<<Y_WIDTH)-1);
                size_t l          = trace_proc_id & ((1<<P_WIDTH)-1) ;

                assert( (x < XMAX) and (y < YMAX) and (l < NB_PROCS_MAX) and
                        "PROCID parameter refers a not valid processor");
            }
            else if ((strcmp(argv[n], "-ROM") == 0) && ((n + 1) < argc))
            {
                strcpy(soft_name, argv[n + 1]);
            }
            else if ((strcmp(argv[n], "-DISK") == 0) && ((n + 1) < argc))
            {
                strcpy(disk_name, argv[n + 1]);
            }
            else if ((strcmp(argv[n], "-THREADS") == 0) && ((n + 1) < argc))
            {
                threads = (size_t) strtol(argv[n + 1], NULL, 0);
                threads = (threads < 1) ? 1 : threads;
            }
            else if ((strcmp(argv[n], "-FROZEN") == 0) && (n + 1 < argc))
            {
                frozen_cycles = (uint32_t) strtol(argv[n + 1], NULL, 0);
            }
            else
            {
                std::cout << "   Arguments are (key,value) couples." << std::endl;
                std::cout << "   The order is not important." << std::endl;
                std::cout << "   Accepted arguments are :" << std::endl << std::endl;
                std::cout << "     - NCYCLES number_of_simulated_cycles" << std::endl;
                std::cout << "     - DEBUG debug_start_cycle" << std::endl;
                std::cout << "     - ROM path to ROM image" << std::endl;
                std::cout << "     - DISK path to disk image" << std::endl;
                std::cout << "     - THREADS simulator's threads number" << std::endl;
                std::cout << "     - FROZEN max_number_of_lines" << std::endl;
                std::cout << "     - PERIOD number_of_cycles between trace" << std::endl;
                std::cout << "     - MEMCID index_memc_to_be_traced" << std::endl;
                std::cout << "     - PROCID index_proc_to_be_traced" << std::endl;
                exit(0);
            }
        }
    }

    // checking hardware parameters
    assert( (X_SIZE == 1) && (Y_SIZE == 1) );
    assert( (X_WIDTH == 4) && (Y_WIDTH == 4) );
    assert( (P_WIDTH == 2) );
    assert( (NB_PROCS_MAX <= 4));
    assert( (NB_TTY_CHANNELS == 1));

    std::cout << std::endl;
    std::cout << " - XMAX             = " << XMAX << std::endl;
    std::cout << " - YMAX             = " << YMAX << std::endl;
    std::cout << " - NB_PROCS_MAX     = " << NB_PROCS_MAX <<  std::endl;
    std::cout << " - NB_TTY_CHANNELS  = " << NB_TTY_CHANNELS <<  std::endl;
    std::cout << " - MEMC_WAYS        = " << MEMC_WAYS << std::endl;
    std::cout << " - MEMC_SETS        = " << MEMC_SETS << std::endl;
    std::cout << " - RAM_LATENCY      = " << XRAM_LATENCY << std::endl;
    std::cout << " - MAX_FROZEN       = " << frozen_cycles << std::endl;
    std::cout << " - MAX_CYCLES       = " << ncycles << std::endl;
    std::cout << " - RESET_ADDRESS    = " << RESET_ADDRESS << std::endl;
    std::cout << " - SOFT_FILENAME    = " << soft_name << std::endl;
    std::cout << " - DISK_IMAGENAME   = " << disk_name << std::endl;
    std::cout << std::endl;

    // Internal and External VCI parameters definition
    typedef soclib::caba::VciParams<vci_cell_width_int,
            vci_plen_width,
            vci_address_width,
            vci_rerror_width,
            vci_clen_width,
            vci_rflag_width,
            vci_srcid_width,
            vci_pktid_width,
            vci_trdid_width,
            vci_wrplen_width> vci_param_int;

    typedef soclib::caba::VciParams<vci_cell_width_ext,
            vci_plen_width,
            vci_address_width,
            vci_rerror_width,
            vci_clen_width,
            vci_rflag_width,
            vci_srcid_width,
            vci_pktid_width,
            vci_trdid_width,
            vci_wrplen_width> vci_param_ext;

#if USE_OPENMP
    omp_set_dynamic(false);
    omp_set_num_threads(threads);
    std::cerr << "Built with openmp version " << _OPENMP << std::endl;
    std::cout << " - OPENMP THREADS   = " << threads << std::endl;
    std::cout << std::endl;
#endif


    ///////////////////////////////////////
    //  Direct Network Mapping Table
    ///////////////////////////////////////

    MappingTable maptabd(vci_address_width,
            IntTab(X_WIDTH + Y_WIDTH, 16 - X_WIDTH - Y_WIDTH),
            IntTab(X_WIDTH + Y_WIDTH, vci_srcid_width - X_WIDTH - Y_WIDTH),
            0x00FF000000ULL);

    maptabd.add(Segment("seg_xicu", SEG_XCU_BASE, SEG_XCU_SIZE,
                IntTab(cluster(0,0),XICU_TGTID), false));

    maptabd.add(Segment("seg_mcfg", SEG_MMC_BASE, SEG_MMC_SIZE,
                IntTab(cluster(0,0),MEMC_TGTID), false));

    maptabd.add(Segment("seg_memc", SEG_RAM_BASE, SEG_RAM_SIZE,
                IntTab(cluster(0,0),MEMC_TGTID), true));

    maptabd.add(Segment("seg_mtty", SEG_TTY_BASE, SEG_TTY_SIZE,
                IntTab(cluster(0,0),MTTY_TGTID), false));

    maptabd.add(Segment("seg_bdev", SEG_IOC_BASE, SEG_IOC_SIZE,
                IntTab(cluster(0,0),BDEV_TGTID), false));

    maptabd.add(Segment("seg_brom", SEG_ROM_BASE, SEG_ROM_SIZE,
                IntTab(cluster(0,0),XROM_TGTID), true));

    maptabd.add(Segment("seg_fbuf", SEG_FBF_BASE, SEG_FBF_SIZE,
                IntTab(cluster(0,0),FBUF_TGTID), false));

    std::cout << maptabd << std::endl;

    /////////////////////////////////////////////////
    // Ram network mapping table
    /////////////////////////////////////////////////

    MappingTable maptabx(vci_address_width,
                         IntTab(X_WIDTH+Y_WIDTH),
                         IntTab(X_WIDTH+Y_WIDTH),
                         0x00FF000000ULL);

    maptabx.add(Segment("seg_xram", SEG_RAM_BASE, SEG_RAM_SIZE,
                IntTab(cluster(0,0)), false));

    std::cout << maptabx << std::endl;

    ////////////////////
    // Signals
    ///////////////////

    sc_clock signal_clk("clk");
    sc_signal<bool> signal_resetn("resetn");

    ////////////////////////////
    //      Loader
    ////////////////////////////

#if USE_IOC_RDK
    std::ostringstream ramdisk_name;
    ramdisk_name << disk_name << "@" << std::hex << SEG_RDK_BASE << ":";
    soclib::common::Loader loader( soft_name, ramdisk_name.str().c_str() );
#else
    soclib::common::Loader loader( soft_name );
#endif

    loader.memory_default(0x55);

    typedef soclib::common::GdbServer<soclib::common::Mips32ElIss> proc_iss;
    proc_iss::set_loader( loader );

    //////////////////////////////////////////////////////////////
    // cluster construction
    //////////////////////////////////////////////////////////////
    TsarFpgaCluster<dspin_cmd_width, dspin_rsp_width,
                    vci_param_int, vci_param_ext> fpga_cluster (
                        "tsar_fpga_cluster",
                        NB_PROCS_MAX,
                        maptabd, maptabx,
                        RESET_ADDRESS,
                        X_WIDTH, Y_WIDTH,
                        vci_srcid_width - X_WIDTH - Y_WIDTH,   // l_id width,
                        MEMC_TGTID,
                        XICU_TGTID,
                        MTTY_TGTID,
                        BDEV_TGTID,
                        XROM_TGTID,
                        disk_name,
                        MEMC_WAYS, MEMC_SETS,
                        L1_IWAYS, L1_ISETS, L1_DWAYS, L1_DSETS,
                        XRAM_LATENCY,
                        loader,
                        frozen_cycles, trace_from,
                        trace_proc_ok, trace_proc_id,
                        trace_memc_ok );

    DspinSignals<dspin_cmd_width> signal_dspin_bound_cmd_in;
    DspinSignals<dspin_cmd_width> signal_dspin_bound_cmd_out;
    DspinSignals<dspin_rsp_width> signal_dspin_bound_rsp_in;
    DspinSignals<dspin_rsp_width> signal_dspin_bound_rsp_out;
    DspinSignals<dspin_cmd_width> signal_dspin_bound_m2p_in;
    DspinSignals<dspin_cmd_width> signal_dspin_bound_m2p_out;
    DspinSignals<dspin_rsp_width> signal_dspin_bound_p2m_in;
    DspinSignals<dspin_rsp_width> signal_dspin_bound_p2m_out;
    DspinSignals<dspin_cmd_width> signal_dspin_bound_cla_in;
    DspinSignals<dspin_cmd_width> signal_dspin_bound_cla_out;

    // Cluster clock & reset
    fpga_cluster.p_clk(signal_clk);
    fpga_cluster.p_resetn(signal_resetn);
    fpga_cluster.p_cmd_in(signal_dspin_bound_cmd_in);
    fpga_cluster.p_cmd_out(signal_dspin_bound_cmd_out);
    fpga_cluster.p_rsp_in(signal_dspin_bound_rsp_in);
    fpga_cluster.p_rsp_out(signal_dspin_bound_rsp_out);
    fpga_cluster.p_m2p_in(signal_dspin_bound_m2p_in);
    fpga_cluster.p_m2p_out(signal_dspin_bound_m2p_out);
    fpga_cluster.p_p2m_in(signal_dspin_bound_p2m_in);
    fpga_cluster.p_p2m_out(signal_dspin_bound_p2m_out);
    fpga_cluster.p_cla_in(signal_dspin_bound_cla_in);
    fpga_cluster.p_cla_out(signal_dspin_bound_cla_out);

    ////////////////////////////////////////////////////////
    //   Simulation
    ///////////////////////////////////////////////////////

    sc_start(sc_core::sc_time(0, SC_NS));
    signal_resetn = false;

    // set cluster gateway signals default values
    signal_dspin_bound_cmd_in.write = false;
    signal_dspin_bound_cmd_in.read  = true;
    signal_dspin_bound_cmd_out.write = false;
    signal_dspin_bound_cmd_out.read  = true;

    signal_dspin_bound_rsp_in.write = false;
    signal_dspin_bound_rsp_in.read  = true;
    signal_dspin_bound_rsp_out.write = false;
    signal_dspin_bound_rsp_out.read  = true;

    signal_dspin_bound_m2p_in.write = false;
    signal_dspin_bound_m2p_in.read  = true;
    signal_dspin_bound_m2p_out.write = false;
    signal_dspin_bound_m2p_out.read  = true;

    signal_dspin_bound_p2m_in.write = false;
    signal_dspin_bound_p2m_in.read  = true;
    signal_dspin_bound_p2m_out.write = false;
    signal_dspin_bound_p2m_out.read  = true;

    signal_dspin_bound_cla_in.write = false;
    signal_dspin_bound_cla_in.read  = true;
    signal_dspin_bound_cla_out.write = false;
    signal_dspin_bound_cla_out.read  = true;

    sc_start(sc_core::sc_time(1, SC_NS));
    signal_resetn = true;

    // simulation loop
    for (uint64_t n = 0; n < ncycles && !stop_called; n += simulation_period)
    {
        // trace display
        if ( trace_ok and (n > trace_from) )
        {
            std::cout << "****************** cycle " << std::dec << n ;
            std::cout << " ********************************************" << std::endl;

            if ( trace_proc_ok )
            {
                std::ostringstream proc_signame;
                proc_signame << "[SIG]PROC_" << trace_proc_id ;
                fpga_cluster.proc[trace_proc_id]->print_trace(1);
                fpga_cluster.signal_vci_ini_proc[trace_proc_id].print_trace(proc_signame.str());

                fpga_cluster.xicu->print_trace(0);
                fpga_cluster.signal_vci_tgt_xicu.print_trace("[SIG]XICU");

                for (int p = 0; p < NB_PROCS_MAX; p++)
                {
                    if ( fpga_cluster.signal_proc_irq[p*IRQ_PER_PROCESSOR].read() )
                    {
                        std::cout << "### IRQ_PROC_" << p << std::endl;
                    }
                }
            }

            if ( trace_memc_ok )
            {
                fpga_cluster.memc->print_trace();
                fpga_cluster.signal_vci_tgt_memc.print_trace("[SEG]MEMC");
                fpga_cluster.signal_vci_xram.print_trace("[SEG]XRAM");
            }

            fpga_cluster.bdev->print_trace();
            fpga_cluster.signal_vci_tgt_bdev.print_trace("[SIG]BDEV_0_0");
            fpga_cluster.signal_vci_ini_bdev.print_trace("[SIG]BDEV_0_0");
        }  // end trace

        struct timeval t1,t2;
        if (gettimeofday(&t1, NULL) != 0) return EXIT_FAILURE;
        sc_start(sc_core::sc_time(simulation_period, SC_NS));
        if (gettimeofday(&t2, NULL) != 0) return EXIT_FAILURE;

        // stats display
        if (!trace_ok)
        {
            uint64_t ms1 = (uint64_t)t1.tv_sec * 1000ULL +
                           (uint64_t)t1.tv_usec / 1000;
            uint64_t ms2 = (uint64_t)t2.tv_sec * 1000ULL +
                           (uint64_t)t2.tv_usec / 1000;
            std::cerr << "platform clock frequency "
                      << (double) simulation_period / (double) (ms2 - ms1)
                      << "Khz" << std::endl;
        }
    }

    return EXIT_SUCCESS;
}

void handler(int dummy = 0)
{
    stop_called = true;
    sc_stop();
}

void voidhandler(int dummy = 0) {}

int sc_main (int argc, char *argv[])
{
    signal(SIGINT, handler);
    signal(SIGPIPE, voidhandler);

    try {
        return _main(argc, argv);
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unknown exception occured" << std::endl;
        throw;
    }
    return 1;
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
