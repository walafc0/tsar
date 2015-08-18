/*
 * global config
 */

#define CONFIG_GDB_SERVER

/*
 * headers
 */

#include <systemc>
#include <sys/time.h>
#include <iostream>
#include <cstdlib>
#include <cstdarg>
#include <inttypes.h>
#include <limits.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef CONFIG_GDB_SERVER
#include "gdbserver.h"
#endif

#include "mapping_table.h"

#include "mips32.h"
#include "vci_mem_cache.h"
#include "vci_cc_vcache_wrapper.h"
#include "vci_block_device_tsar.h"

#include "vci_simple_ram.h"
#include "vci_multi_tty.h"
#include "vci_xicu.h"
#include "vci_framebuffer.h"

#include "dspin_local_crossbar.h"
#include "vci_local_crossbar.h"

/*
 * pf global config
 */

using namespace sc_core;
using namespace soclib::caba;
using namespace soclib::common;

#define    cell_width            4
#define    cell_width_ext        8
#define    address_width         40
#define    plen_width            8
#define    error_width           1
#define    clen_width            1
#define    rflag_width           1
#define    srcid_width           14
#define    pktid_width           4
#define    trdid_width           4
#define    wrplen_width          1

#define    dspin_cmd_width     39
#define    dspin_rsp_width     32

typedef VciParams<cell_width,
        plen_width,
        address_width,
        error_width,
        clen_width,
        rflag_width,
        srcid_width,
        pktid_width,
        trdid_width,
        wrplen_width> vci_param;

typedef VciParams<cell_width_ext,
        plen_width,
        address_width,
        error_width,
        clen_width,
        rflag_width,
        srcid_width,
        pktid_width,
        trdid_width,
        wrplen_width> vci_param_ext;

/*
 * segmentation
 */

#include "segmentation.h"


/*
 * default parameters
 */

struct param_s {
    size_t nr_cpus;
    char *rom_path;
    bool dsk;
    char *dsk_path;
    bool dummy_boot;
    bool framebuffer;
    bool trace_enabled;
    size_t trace_start_cycle;
    uint64_t ncycles;
};

#define PARAM_INITIALIZER   \
{                           \
    .nr_cpus = 1,           \
    .rom_path = NULL,       \
    .dsk = false,           \
    .dsk_path = NULL,       \
    .dummy_boot = false,    \
    .framebuffer = false,   \
    .trace_enabled = false, \
    .trace_start_cycle = 0, \
    .ncycles = 0,           \
}

static inline void print_param(const struct param_s &param)
{
    std::cout << std::endl;
    std::cout << "simulation parameters:" << std::endl;
    std::cout << "  nr_cpus     = " << param.nr_cpus << std::endl;
    std::cout << "  rom         = " << param.rom_path << std::endl;
    std::cout << "  dummy boot  = " << param.dummy_boot << std::endl;
    std::cout << "  framebuffer = " << param.framebuffer << std::endl;
    std::cout << "  dsk         = " << param.dsk << std::endl;
    if (param.dsk)
        std::cout << "    dsk_path  = " << param.dsk_path << std::endl;
    std::cout << "  trace       = " << param.trace_enabled << std::endl;
    if (param.trace_enabled)
        std::cout << "    start cyc = " << param.trace_start_cycle << std::endl;
    if (param.ncycles > 0)
        std::cout << "    ncycles   = " << param.ncycles << std::endl;

    std::cout << std::endl;
}

#define MAX_FROZEN_CYCLES 500000

#define NB_IRQS_PER_CPU 4

/*
 * arguments parsing
 */

void args_parse(unsigned int argc, char *argv[], struct param_s &param)
{
    for (size_t n = 1; n < argc; n = n + 2)
    {
        if ((strcmp(argv[n], "--ncpus") == 0) && ((n + 1) < argc))
        {
            assert((param.nr_cpus = atoi(argv[n + 1]))
                    && "insufficient memory");
        }
        else if ((strcmp(argv[n], "--rom") == 0) && ((n + 1) < argc))
        {
            assert((param.rom_path = strdup(argv[n + 1]))
                    && "insufficient memory");
        }
        else if ((strcmp(argv[n], "--dsk") == 0) && ((n + 1) < argc))
        {
            param.dsk = true;
            assert((param.dsk_path = strdup(argv[n + 1]))
                    && "insufficient memory");
        }
        else if (strcmp(argv[n], "--dummy-boot") == 0)
        {
            param.dummy_boot = true;
            /* we don't have an extra argument */
            n = n - 1;
        }
        else if (strcmp(argv[n], "--framebuffer") == 0)
        {
            param.framebuffer = true;
            /* we don't have an extra argument */
            n = n - 1;
        }
        else if ((strcmp(argv[n], "--trace") == 0) && ((n + 1) < argc))
        {
            param.trace_enabled = true;
            param.trace_start_cycle = atoi(argv[n + 1]);
        }
        else if ((strcmp(argv[n], "--ncycles") == 0) && ((n + 1) < argc))
        {
            param.ncycles = atoll(argv[n + 1]);
        }
        else
        {
            std::cout << "Error: don't understand option " << argv[n] << std::endl;
            std::cout << "Accepted arguments are :" << std::endl;
            std::cout << "--ncpus pathname" << std::endl;
            std::cout << "--rom pathname" << std::endl;
            std::cout << "[--dsk pathname]" << std::endl;
            std::cout << "[--dummy-boot]" << std::endl;
            std::cout << "[--framebuffer]" << std::endl;
            std::cout << "[--trace trace_start_cycle]" << std::endl;
            std::cout << "[--ncycles simulation_cycles]" << std::endl;
            exit(0);
        }
    }

    /* check parameters */
    assert((param.nr_cpus <= 4) && "cannot support more than 4 cpus");
    assert(param.rom_path && "--rom is not optional");

    print_param(param);
}


/*
 * netlist
 */

int _main(int argc, char *argv[])
{
#ifdef _OPENMP
    omp_set_dynamic(false);
    omp_set_num_threads(5);
    std::cerr << "Built with openmp version " << _OPENMP << std::endl;
#endif

    struct param_s param = PARAM_INITIALIZER;

    /* parse arguments */
    args_parse(argc, argv, param);

    /*
     * mapping tables
     */

    /* data mapping table */
    MappingTable maptabp(32, IntTab(0, 16), IntTab(0, srcid_width), 0xF0000000);

    /* ram */
    maptabp.add(Segment("mc_m", MEMC_BASE, MEMC_SIZE, IntTab(0, 0), true));
    maptabp.add(Segment("boot", BOOT_BASE, BOOT_SIZE, IntTab(0, 1), true));

    /* uncached peripherals */
    maptabp.add(Segment("xicu", XICU_BASE, XICU_SIZE, IntTab(0, 2), false));
    maptabp.add(Segment("tty",  MTTY_BASE, MTTY_SIZE, IntTab(0, 3), false));
    maptabp.add(Segment("bd",   BD_BASE,   BD_SIZE,   IntTab(0, 4), false));
    maptabp.add(Segment("fb",   FB_BASE,   FB_SIZE,   IntTab(0, 5), false));

    std::cout << maptabp << std::endl;

    /* xram mapping table */
    MappingTable maptabx(32, IntTab(8), IntTab(8), 0x30000000);
    maptabx.add(Segment("xram", MEMC_BASE, MEMC_SIZE, IntTab(0), false));

    std::cout << maptabx << std::endl;

    /*
     * components
     */

    Loader loader;
    loader.load_file(param.rom_path);
    loader.memory_default(0x5c);

#ifdef CONFIG_GDB_SERVER
    typedef GdbServer<Mips32ElIss> proc_iss;
    proc_iss::set_loader(loader);
#else
    typedef Mips32ElIss proc_iss;
#endif

    if (param.dummy_boot == true)
    {
        /* boot linux image directly */
        uint64_t entry_addr = loader.get_entry_point_address();
        std::cout << "setResetAdress: " << std::hex << entry_addr << std::endl << std::endl;
        proc_iss::setResetAddress(entry_addr);
    }

    VciCcVCacheWrapper<vci_param, dspin_cmd_width, dspin_rsp_width, proc_iss > **proc;
    proc = new VciCcVCacheWrapper<vci_param, dspin_cmd_width, dspin_rsp_width,
         proc_iss >*[param.nr_cpus];
    for (size_t i = 0; i < param.nr_cpus; i++)
    {
        std::ostringstream o;
        o << "ccvache" << "[" << i << "]";
        proc[i] = new VciCcVCacheWrapper<vci_param, dspin_cmd_width,
            dspin_rsp_width, proc_iss >(
                o.str().c_str(),    // name
                i,                  // proc_id
                maptabp,            // direct space
                IntTab(0, i),       // srcid_d
                i,                  // cc_global_id
                8, 8,               // itlb size
                8, 8,               // dtlb size
                4, 64, 16,          // icache size
                4, 64, 16,          // dcache size
                4, 4,               // wbuf size
                0, 0,               // x, y Width
                MAX_FROZEN_CYCLES,  // max frozen cycles
                param.trace_start_cycle,
                param.trace_enabled);
    }

    VciSimpleRam<vci_param_ext> xram("xram", IntTab(0), maptabx, loader);

    VciSimpleRam<vci_param> rom("rom", IntTab(0, 1), maptabp, loader);

    VciMemCache<vci_param, vci_param_ext, dspin_rsp_width, dspin_cmd_width>
        memc("memc",
                maptabp,        // direct space
                maptabx,        // xram space
                IntTab(0),      // xram srcid
                IntTab(0, 0),   // direct tgtid
                0, 0,           // x, y width
                16, 256, 16,    // cache size
                3,              // max copies
                4096, 8, 8, 8,  // HEAP size, TRT size, UPT size, IVT size
                param.trace_start_cycle, param.trace_enabled);

    VciXicu<vci_param> xicu("xicu", maptabp, IntTab(0, 2),
            param.nr_cpus,  // #timers
            3,              // #input hw irqs
            param.nr_cpus,  // #ipis
            param.nr_cpus * NB_IRQS_PER_CPU); // #output irqs

    VciMultiTty<vci_param> mtty("mtty", IntTab(0, 3), maptabp, "vcitty0", NULL);

    VciBlockDeviceTsar<vci_param> *bd = NULL;
    if (param.dsk == true)
        bd = new VciBlockDeviceTsar<vci_param>("bd", maptabp,
                IntTab(0, param.nr_cpus),   // srcid
                IntTab(0, 4),               // tgtid
                param.dsk_path);            // filename

    VciFrameBuffer<vci_param> *fb = NULL;
    if (param.framebuffer == true)
        fb = new VciFrameBuffer<vci_param>("fb", IntTab(0, 5), maptabp,
                FB_XSIZE, FB_YSIZE,     // window size
                FbController::RGB_16);  // color type

    /*
     * Interconnects
     */

    /* data network */
    VciLocalCrossbar<vci_param> xbar_d("xbar_d",
            maptabp,        // mapping table
            0,              // cluster coordinates
            param.nr_cpus + 1, // #src
            6,              // #dst
            1);             // default target

    /* coherence */
    DspinLocalCrossbar<dspin_cmd_width> xbar_m2p_c("xbar_m2p_c",
            maptabp,
            0, 0,
            0, 0,
            srcid_width,
            1, param.nr_cpus,
            2, 2,
            true,
            false,
            true);
    DspinLocalCrossbar<dspin_rsp_width> xbar_p2m_c("xbar_p2m_c",
            maptabp,
            0, 0,
            0, 0,
            0,
            param.nr_cpus, 1,
            2, 2,
            false,
            false,
            false);
    DspinLocalCrossbar<dspin_cmd_width> xbar_clack_c("xbar_clack_c",
            maptabp,
            0, 0,
            0, 0,
            srcid_width,
            1, param.nr_cpus,
            1, 1,
            true,
            false,
            false);

    /*
     * signals
     */

    /* clk and resetn */
    sc_clock signal_clk ("clk");
    sc_signal<bool> signal_resetn("resetn");

    /* irq lines */
    sc_signal<bool> **signal_proc_irq =
        alloc_elems<sc_signal<bool> >("proc_irq", param.nr_cpus, proc_iss::n_irq);
    sc_signal<bool> signal_mtty_irq("mtty_irq");
    sc_signal<bool> signal_bd_irq("bd_irq");
    sc_signal<bool> signal_memc_irq("memc_irq");

    /* vci */
    VciSignals<vci_param> *signal_vci_proc =
        alloc_elems<VciSignals<vci_param> >("vci_proc", param.nr_cpus);
    VciSignals<vci_param> signal_vci_ini_bd ("vci_ini_bd");

    VciSignals<vci_param> signal_vci_memc ("vci_memc");
    VciSignals<vci_param> signal_vci_rom ("vci_rom");
    VciSignals<vci_param> signal_vci_xicu ("vci_xicu");
    VciSignals<vci_param> signal_vci_tty ("vci_tty");
    VciSignals<vci_param> signal_vci_tgt_bd ("vci_tgt_bd");
    VciSignals<vci_param> signal_vci_fb ("vci_fb");

    VciSignals<vci_param_ext> signal_vci_xram ("vci_xram");

    /* fake signals for in/out of cluster */
    VciSignals<vci_param> signal_vci_from_out("vci_from_out");
    VciSignals<vci_param> signal_vci_to_out("vci_to_out");

    /* Coherence DSPIN signals to local crossbar */
    DspinSignals<dspin_cmd_width> signal_dspin_m2p_l2g;
    DspinSignals<dspin_cmd_width> signal_dspin_m2p_g2l;
    DspinSignals<dspin_rsp_width> signal_dspin_p2m_l2g;
    DspinSignals<dspin_rsp_width> signal_dspin_p2m_g2l;
    DspinSignals<dspin_cmd_width> signal_dspin_clack_l2g;
    DspinSignals<dspin_cmd_width> signal_dspin_clack_g2l;

    DspinSignals<dspin_cmd_width> signal_dspin_m2p_memc;
    DspinSignals<dspin_cmd_width> signal_dspin_clack_memc;
    DspinSignals<dspin_rsp_width> signal_dspin_p2m_memc;
    DspinSignals<dspin_cmd_width> *signal_dspin_m2p_proc =
        alloc_elems<DspinSignals<dspin_cmd_width> >("dspin_m2p_proc", param.nr_cpus);
    DspinSignals<dspin_cmd_width> *signal_dspin_clack_proc =
        alloc_elems<DspinSignals<dspin_cmd_width> >("dspin_clack_proc", param.nr_cpus);
    DspinSignals<dspin_rsp_width> *signal_dspin_p2m_proc =
        alloc_elems<DspinSignals<dspin_rsp_width> >("dspin_p2m_proc", param.nr_cpus);

    /*
     * netlist
     */

    /* components */
    for (size_t i = 0; i < param.nr_cpus; i++)
    {
        proc[i]->p_clk(signal_clk);
        proc[i]->p_resetn(signal_resetn);
        for (size_t j = 0; j < proc_iss::n_irq; j++)
            proc[i]->p_irq[j](signal_proc_irq[i][j]);
        proc[i]->p_vci(signal_vci_proc[i]);
        proc[i]->p_dspin_m2p(signal_dspin_m2p_proc[i]);
        proc[i]->p_dspin_p2m(signal_dspin_p2m_proc[i]);
        proc[i]->p_dspin_clack(signal_dspin_clack_proc[i]);
    }

    memc.p_clk(signal_clk);
    memc.p_resetn(signal_resetn);
    memc.p_irq(signal_memc_irq);
    memc.p_vci_tgt(signal_vci_memc);
    memc.p_dspin_p2m(signal_dspin_p2m_memc);
    memc.p_dspin_m2p(signal_dspin_m2p_memc);
    memc.p_dspin_clack(signal_dspin_clack_memc);
    memc.p_vci_ixr(signal_vci_xram);

    rom.p_clk(signal_clk);
    rom.p_resetn(signal_resetn);
    rom.p_vci(signal_vci_rom);

    xicu.p_resetn(signal_resetn);
    xicu.p_clk(signal_clk);
    xicu.p_vci(signal_vci_xicu);
    xicu.p_hwi[0](signal_mtty_irq);
    xicu.p_hwi[1](signal_bd_irq);
    xicu.p_hwi[2](signal_memc_irq);
    for (size_t i = 0; i < param.nr_cpus; i++) {
        xicu.p_irq[i * NB_IRQS_PER_CPU + 0](signal_proc_irq[i][0]);
        xicu.p_irq[i * NB_IRQS_PER_CPU + 1](signal_proc_irq[i][1]);
        xicu.p_irq[i * NB_IRQS_PER_CPU + 2](signal_proc_irq[i][2]);
        xicu.p_irq[i * NB_IRQS_PER_CPU + 3](signal_proc_irq[i][3]);
    }

    mtty.p_clk(signal_clk);
    mtty.p_resetn(signal_resetn);
    mtty.p_vci(signal_vci_tty);
    mtty.p_irq[0](signal_mtty_irq);

    if (param.dsk == true)
    {
        bd->p_clk(signal_clk);
        bd->p_resetn(signal_resetn);
        bd->p_vci_target(signal_vci_tgt_bd);
        bd->p_vci_initiator(signal_vci_ini_bd);
        bd->p_irq(signal_bd_irq);
    }

    if (param.framebuffer == true)
    {
        fb->p_clk(signal_clk);
        fb->p_resetn(signal_resetn);
        fb->p_vci(signal_vci_fb);
    }

    xram.p_clk(signal_clk);
    xram.p_resetn(signal_resetn);
    xram.p_vci(signal_vci_xram);

    /* interconnects */
    xbar_d.p_clk(signal_clk);
    xbar_d.p_resetn(signal_resetn);
    xbar_d.p_target_to_up(signal_vci_from_out);
    xbar_d.p_initiator_to_up(signal_vci_to_out);
    for (size_t i = 0; i < param.nr_cpus; i++)
        xbar_d.p_to_initiator[i](signal_vci_proc[i]);
    xbar_d.p_to_initiator[param.nr_cpus](signal_vci_ini_bd);
    xbar_d.p_to_target[0](signal_vci_memc);
    xbar_d.p_to_target[1](signal_vci_rom);
    xbar_d.p_to_target[2](signal_vci_xicu);
    xbar_d.p_to_target[3](signal_vci_tty);
    xbar_d.p_to_target[4](signal_vci_tgt_bd);
    xbar_d.p_to_target[5](signal_vci_fb);

    xbar_m2p_c.p_clk(signal_clk);
    xbar_m2p_c.p_resetn(signal_resetn);
    xbar_m2p_c.p_global_out(signal_dspin_m2p_l2g);
    xbar_m2p_c.p_global_in(signal_dspin_m2p_g2l);
    xbar_m2p_c.p_local_in[0](signal_dspin_m2p_memc);
    for (size_t i = 0; i < param.nr_cpus; i++)
        xbar_m2p_c.p_local_out[i](signal_dspin_m2p_proc[i]);

    xbar_clack_c.p_clk(signal_clk);
    xbar_clack_c.p_resetn(signal_resetn);
    xbar_clack_c.p_global_out(signal_dspin_clack_l2g);
    xbar_clack_c.p_global_in(signal_dspin_clack_g2l);
    xbar_clack_c.p_local_in[0](signal_dspin_clack_memc);
    for (size_t i = 0; i < param.nr_cpus; i++)
        xbar_clack_c.p_local_out[i](signal_dspin_clack_proc[i]);

    xbar_p2m_c.p_clk(signal_clk);
    xbar_p2m_c.p_resetn(signal_resetn);
    xbar_p2m_c.p_global_out(signal_dspin_p2m_l2g);
    xbar_p2m_c.p_global_in(signal_dspin_p2m_g2l);
    xbar_p2m_c.p_local_out[0](signal_dspin_p2m_memc);
    for (size_t i = 0; i < param.nr_cpus; i++)
        xbar_p2m_c.p_local_in[i](signal_dspin_p2m_proc[i]);

    /*
     * simulation
     */

    for (size_t i = 0; i < param.nr_cpus; i++)
        proc[i]->iss_set_debug_mask(0);

    sc_start(sc_time(0, SC_NS));
    signal_resetn = false;

    sc_start(sc_time(1, SC_NS));
    signal_resetn = true;

    /* network boundaries initialization */
    signal_dspin_m2p_l2g.write = false;
    signal_dspin_m2p_l2g.read = true;
    signal_dspin_m2p_g2l.write = false;
    signal_dspin_m2p_g2l.read = true;

    if (param.ncycles > 0)
    {
        for (size_t n = 1; n < param.ncycles; n++)
        {
            if (param.trace_enabled and (n > param.trace_start_cycle))
            {
                std::cout << "****************** cycle " << std::dec << n
                    << " ************************************************" << std::endl;

                proc[0]->print_trace();
                memc.print_trace();

                signal_vci_proc[0].print_trace("signal_vci_proc[0]");
                signal_vci_memc.print_trace("signal_vci_memc");

                signal_dspin_m2p_memc.print_trace("signal_m2p_memc");
                signal_dspin_clack_memc.print_trace("signal_clack_memc");
                signal_dspin_p2m_memc.print_trace("signal_p2m_memc");
                for (size_t i = 0; i < param.nr_cpus; i++)
                {
                    std::ostringstream o;
                    o << "signal_m2p_proc" << "[" << i << "]";
                    signal_dspin_m2p_proc[i].print_trace(o.str().c_str());
                }
            }
            sc_start(sc_core::sc_time(1, SC_NS));
        }
    } else {
        uint64_t n;
        struct timeval t1, t2;

        gettimeofday(&t1, NULL);

        for (n = 1; ; n++) {
            /* stats display */
            if ((n % 5000000) == 0) {
                gettimeofday(&t2, NULL);

                uint64_t ms1 = (uint64_t)t1.tv_sec * 1000ULL +
                    (uint64_t)t1.tv_usec / 1000;
                uint64_t ms2 = (uint64_t)t2.tv_sec * 1000ULL +
                    (uint64_t)t2.tv_usec / 1000;
                std::cerr << "platform clock frequency "
                    << (double)5000000 / (double)(ms2 - ms1) << "Khz" << std::endl;

                gettimeofday(&t1, NULL);
            }

            sc_start(sc_core::sc_time(1, SC_NS));
        };
    }

    return EXIT_SUCCESS;

}

int sc_main (int argc, char *argv[])
{
    try {
        return _main(argc, argv);
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unknown exception occured" << std::endl;
        throw;
    }
    return EXIT_FAILURE;
}

