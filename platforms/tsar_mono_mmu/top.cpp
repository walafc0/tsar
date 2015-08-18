
#include <systemc>
#include <sys/time.h>
#include <iostream>
#include <cstdlib>
#include <cstdarg>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "mapping_table.h"
#include "mips32.h"
#include "vci_simple_ram.h"
#include "vci_multi_tty.h"
#include "vci_mem_cache.h"
#include "vci_cc_vcache_wrapper.h"
#include "vci_logger.h"
#include "vci_xicu.h"
#include "vci_multi_dma.h"
#include "vci_simhelper.h"
#include "dspin_local_crossbar.h"
#include "vci_dspin_initiator_wrapper.h"
#include "vci_dspin_target_wrapper.h"

	// Define VCI parameters
#define    cell_width            4
#define    cell_width_ext        8
#define    address_width         32
#define    plen_width            8
#define    error_width           1
#define    clen_width            1
#define    rflag_width           1
#define    srcid_width           14
#define    pktid_width           4
#define    trdid_width           4
#define    wrplen_width          1

#define    dspin_cmd_width	 39
#define    dspin_rsp_width	 32
//   segments definition in direct space

#define    XICU_BASE    0xd8200000
#define    XICU_SIZE    0x00001000

#define    MDMA_BASE    0xe8000000
#define    MDMA_SIZE    0x00000014

#define    MTTY_BASE    0xd0200000
#define    MTTY_SIZE    0x00000010

#define    EXIT_BASE    0xe0000000
#define    EXIT_SIZE    0x00000010

#define    BOOT_BASE    0xbfc00000
#define    BOOT_SIZE    0x00040000

#define    MEMC_BASE    0x00000000
#define    MEMC_SIZE    0x02000000

int _main(int argc, char *argv[])
{
	using namespace sc_core;
	// Avoid repeating these everywhere
	using soclib::common::IntTab;
	using soclib::common::Segment;
	using namespace soclib::caba;
#ifdef _OPENMP
	omp_set_dynamic(false);
	omp_set_num_threads(1);
	std::cerr << "Built with openmp version " << _OPENMP << std::endl;
#endif
	char	soft_name[256]  = "test.elf";   // pathname to binary code
	size_t	ncycles = 1000000000;   // max number of simulation cycles

	bool    trace_ok = false;
	size_t  from_cycle = 0;		// debug start cycle
	size_t  max_frozen = 100000;	// max number of frozen cycles

    /////////////// command line arguments ////////////////
    if (argc > 1)
    {
        for( int n=1 ; n<argc ; n=n+2 )
        {
            if( (strcmp(argv[n],"-NCYCLES") == 0) && (n+1<argc) )
            {
                ncycles = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-SOFT") == 0) && (n+1<argc) )
            {
                strcpy(soft_name, argv[n+1]);
            }
            else if( (strcmp(argv[n],"-TRACE") == 0) && (n+1<argc) )
            {
                trace_ok = true;
                from_cycle = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-MAXFROZEN") == 0) && (n+1<argc) )
            {
                max_frozen = atoi(argv[n+1]);
            }
            else
            {
                std::cout << "   Arguments on the command line are (key,value) couples." << std::endl;
                std::cout << "   The order is not important." << std::endl;
                std::cout << "   Accepted arguments are :" << std::endl << std::endl;
                std::cout << "     -SOFT pathname_for_embedded_soft" << std::endl;
                std::cout << "     -NCYCLES number_of_simulated_cycles" << std::endl;
                std::cout << "     -TRACE debug_start_cycle" << std::endl;
                exit(0);
            }
        }
    }

	typedef soclib::caba::VciParams<cell_width,
					plen_width,
					address_width,
					error_width,
					clen_width,
					rflag_width,
					srcid_width,
					pktid_width,
					trdid_width,
					wrplen_width> vci_param;
	typedef soclib::caba::VciParams<cell_width_ext,
					plen_width,
					address_width,
					error_width,
					clen_width,
					rflag_width,
					srcid_width,
					pktid_width,
					trdid_width,
					wrplen_width> vci_param_ext;
	typedef soclib::common::Mips32ElIss proc_iss;

	// Direct DSPIN signals to local crossbars
	DspinSignals<dspin_cmd_width>     signal_dspin_cmd_proc0_i;
	DspinSignals<dspin_rsp_width>     signal_dspin_rsp_proc0_i;
	DspinSignals<dspin_cmd_width>     signal_dspin_cmd_dma_i;
	DspinSignals<dspin_rsp_width>     signal_dspin_rsp_dma_i;

	DspinSignals<dspin_cmd_width>     signal_dspin_cmd_memc_t;
	DspinSignals<dspin_rsp_width>     signal_dspin_rsp_memc_t;
	DspinSignals<dspin_cmd_width>     signal_dspin_cmd_xicu_t;
	DspinSignals<dspin_rsp_width>     signal_dspin_rsp_xicu_t;
	DspinSignals<dspin_cmd_width>     signal_dspin_cmd_dma_t;
	DspinSignals<dspin_rsp_width>     signal_dspin_rsp_dma_t;
	DspinSignals<dspin_cmd_width>     signal_dspin_cmd_tty_t;
	DspinSignals<dspin_rsp_width>     signal_dspin_rsp_tty_t;
	DspinSignals<dspin_cmd_width>     signal_dspin_cmd_rom_t;
	DspinSignals<dspin_rsp_width>     signal_dspin_rsp_rom_t;
	DspinSignals<dspin_cmd_width>     signal_dspin_cmd_simh_t;
	DspinSignals<dspin_rsp_width>     signal_dspin_rsp_simh_t;

	// Coherence DSPIN signals to local crossbar
	DspinSignals<dspin_cmd_width>	signal_dspin_cmd_l2g_c;
	DspinSignals<dspin_cmd_width>	signal_dspin_cmd_g2l_c;
	DspinSignals<dspin_rsp_width>	signal_dspin_rsp_l2g_c;
	DspinSignals<dspin_rsp_width>	signal_dspin_rsp_g2l_c;
	DspinSignals<dspin_cmd_width>	signal_dspin_m2p_l2g_c;
	DspinSignals<dspin_cmd_width>	signal_dspin_m2p_g2l_c;
	DspinSignals<dspin_rsp_width>	signal_dspin_p2m_l2g_c;
	DspinSignals<dspin_rsp_width>	signal_dspin_p2m_g2l_c;
	DspinSignals<dspin_cmd_width>	signal_dspin_clack_l2g_c;
	DspinSignals<dspin_cmd_width>	signal_dspin_clack_g2l_c;

	DspinSignals<dspin_cmd_width>	signal_dspin_m2p_memc;
	DspinSignals<dspin_cmd_width>	signal_dspin_clack_memc;
	DspinSignals<dspin_rsp_width>	signal_dspin_p2m_memc;
	DspinSignals<dspin_cmd_width>	signal_dspin_m2p_proc[1];
	DspinSignals<dspin_cmd_width>	signal_dspin_clack_proc[1];
	DspinSignals<dspin_rsp_width>	signal_dspin_p2m_proc[1];

	// Mapping table

	soclib::common::MappingTable maptabp(32, IntTab(0, 16), IntTab(0, srcid_width), 0xF0000000);
	
	maptabp.add(Segment("mc_m" , MEMC_BASE , MEMC_SIZE , IntTab(0, 0), true));
	maptabp.add(Segment("boot", BOOT_BASE, BOOT_SIZE, IntTab(0, 1), true));
	maptabp.add(Segment("tty"  , MTTY_BASE  , MTTY_SIZE  , IntTab(0, 2), false));
	maptabp.add(Segment("xicu" , XICU_BASE , XICU_SIZE , IntTab(0, 3), false));
	maptabp.add(Segment("dma", MDMA_BASE, MDMA_SIZE, IntTab(0, 4), false));
	maptabp.add(Segment("simh", EXIT_BASE, EXIT_SIZE, IntTab(0, 5), false));

	std::cout << maptabp << std::endl;

	soclib::common::MappingTable maptabx(32, IntTab(8), IntTab(8), 0x30000000);
	maptabx.add(Segment("xram" , MEMC_BASE , MEMC_SIZE , IntTab(0), false));
	
	
	std::cout << maptabx << std::endl;

	// Signals

	sc_clock	signal_clk ("clk");
	sc_signal<bool> signal_resetn("resetn");

	sc_signal<bool> signal_proc0_it0("signal_proc0_it0");
	sc_signal<bool> signal_proc0_it1("signal_proc0_it1");
	sc_signal<bool> signal_proc0_it2("signal_proc0_it2");
	sc_signal<bool> signal_proc0_it3("signal_proc0_it3");
	sc_signal<bool> signal_proc0_it4("signal_proc0_it4");
	sc_signal<bool> signal_proc0_it5("signal_proc0_it5");

	soclib::caba::VciSignals<vci_param> signal_vci_ini_rw_proc0
	    ("vci_ini_rw_proc0");
	soclib::caba::VciSignals<vci_param> signal_vci_tty
	    ("signal_vci_tty");
	soclib::caba::VciSignals<vci_param> signal_vci_simh
	    ("signal_vci_simh");

	soclib::caba::VciSignals<vci_param> signal_vci_xicu
	    ("signal_vci_xicu");
	soclib::caba::VciSignals<vci_param> signal_vci_dmai
	    ("signal_vci_dmai");
	soclib::caba::VciSignals<vci_param> signal_vci_dmat
	    ("signal_vci_dmat");
	soclib::caba::VciSignals<vci_param> signal_vci_tgt_rom
	    ("vci_tgt_rom");

	soclib::caba::VciSignals<vci_param_ext> signal_vci_ixr_memc
	    ("vci_ixr_memc");
	soclib::caba::VciSignals<vci_param> signal_vci_tgt_memc
	    ("vci_tgt_memc");
	sc_signal<bool> signal_icu_irq0("signal_icu_irq0");
	sc_signal<bool> signal_icu_irq1("signal_icu_irq1");
	sc_signal<bool> signal_icu_irq2("signal_icu_irq2");

	soclib::common::Loader loader("test.elf");

        //                                  init_rw   init_c   tgt
	soclib::caba::VciCcVCacheWrapper<vci_param, dspin_cmd_width, dspin_rsp_width, proc_iss > *proc0;
	soclib::caba::VciSimpleRam<vci_param> *rom;
	soclib::caba::VciSimpleRam<vci_param_ext> *xram;
	soclib::caba::VciMemCache<vci_param, vci_param_ext, dspin_rsp_width, dspin_cmd_width> *memc;
	soclib::caba::VciXicu<vci_param> *vcixicu;
	soclib::caba::VciMultiTty<vci_param> *vcitty;
	soclib::caba::VciSimhelper<vci_param> *vcisimh;
	soclib::caba::VciMultiDma<vci_param> *vcidma;
	DspinLocalCrossbar<dspin_cmd_width>* xbar_cmd_d;
	DspinLocalCrossbar<dspin_rsp_width>* xbar_rsp_d;
	DspinLocalCrossbar<dspin_cmd_width>* xbar_m2p_c;
	DspinLocalCrossbar<dspin_rsp_width>* xbar_p2m_c;
	DspinLocalCrossbar<dspin_cmd_width>* xbar_clack_c;

	proc0 = new soclib::caba::VciCcVCacheWrapper<vci_param, dspin_cmd_width, dspin_rsp_width, proc_iss >
	  ("proc0", 0, maptabp, IntTab(0, 0), 0,
	    8,8,8,8,4,64,16,4,64,16,4, 4, 0, 0, max_frozen, from_cycle, trace_ok);
	VciDspinInitiatorWrapper<vci_param, dspin_cmd_width, dspin_rsp_width>
	    wi_proc0("wi_proc0", srcid_width);

	vcisimh = new soclib::caba::VciSimhelper<vci_param>
	  ("vcisimh",	IntTab(0, 5), maptabp);
	VciDspinTargetWrapper<vci_param, dspin_cmd_width, dspin_rsp_width>
	    wt_simh("wt_simh", srcid_width);

	vcidma = new soclib::caba::VciMultiDma<vci_param>
	  ("vcidma", maptabp, IntTab(0, 1), IntTab(0, 4), 64, 1);//(1<<(vci_param::K-1)));
	VciDspinInitiatorWrapper<vci_param, dspin_cmd_width, dspin_rsp_width>
	    wi_dma("wi_dma", srcid_width);
	VciDspinTargetWrapper<vci_param, dspin_cmd_width, dspin_rsp_width>
	    wt_dma("wt_dma", srcid_width);

	vcitty = new soclib::caba::VciMultiTty<vci_param>
	  ("vcitty",	IntTab(0, 2), maptabp, "vcitty0", NULL);
	VciDspinTargetWrapper<vci_param, dspin_cmd_width, dspin_rsp_width>
	    wt_tty("wt_tty", srcid_width);

	rom = new soclib::caba::VciSimpleRam<vci_param>
	  ("rom", IntTab(0, 1), maptabp, loader);
	VciDspinTargetWrapper<vci_param, dspin_cmd_width, dspin_rsp_width>
	    wt_rom("wt_rom", srcid_width);

	vcixicu = new soclib::caba::VciXicu<vci_param>
	  ("vcixicu", maptabp, IntTab(0, 3), 4 /* npti */, 2 /* nhwi */, 4 /* nwti */, 6 /* nirq */);
	VciDspinTargetWrapper<vci_param, dspin_cmd_width, dspin_rsp_width>
	    wt_xicu("wt_xicu", srcid_width);

	xram = new soclib::caba::VciSimpleRam<vci_param_ext>
	  ("xram", IntTab(0), maptabx, loader);
	memc = new soclib::caba::VciMemCache<vci_param, vci_param_ext, dspin_rsp_width, dspin_cmd_width>
	  ("memc",maptabp,maptabx,IntTab(0),IntTab(0, 0), 0, 0,16,256,16, 3, 4096, 8, 8, 8, from_cycle, trace_ok);
	VciDspinTargetWrapper<vci_param, dspin_cmd_width, dspin_rsp_width>
	    wt_memc("wt_memc", srcid_width);

	xbar_cmd_d = new DspinLocalCrossbar<dspin_cmd_width>(
		"xbar_cmd_d",
		maptabp,			// mapping table
		0, 0,				// cluster coordinates
		0, 0, srcid_width,
		2,				// number of local of sources
		6,				// number of local dests
		2, 2,			 	// fifo depths
		true,			 	// CMD
		true,				 // use local routing table
		false );			// no broadcast

	xbar_rsp_d = new DspinLocalCrossbar<dspin_rsp_width>(
		"xbar_rsp_d",
		maptabp,			// mapping table
		0, 0,				// cluster coordinates
		0, 0, srcid_width,
		6,				// number of local sources
		2,			 	// number of local dests
		2, 2,			 	// fifo depths
		false,				// RSP
		false,				// don't use local routing table
		false );			// no broadcast

	xbar_m2p_c = new DspinLocalCrossbar<dspin_cmd_width>(
		"xbar_m2p_c", maptabp, 0, 0, 0, 0, srcid_width,
		1, 1, 2, 2, true, false, true);
	xbar_p2m_c =  new DspinLocalCrossbar<dspin_rsp_width>(
		"xbar_p2m_c", maptabp, 0, 0, 0, 0, 0,
		1, 1, 2, 2, false, false, false);
	xbar_clack_c = new DspinLocalCrossbar<dspin_cmd_width>(
		"xbar_clack_c", maptabp, 0, 0, 0, 0, srcid_width,
		1, 1, 1, 1, true, false, false);
#ifdef VCI_LOGGER_ON_L1
	soclib::caba::VciLogger<vci_param> vci_logger0("vci_logger0",maptabp);
#endif
#ifdef VCI_LOGGER_ON_ROM
	soclib::caba::VciLogger<vci_param> vci_logger1("vci_logger1",maptabp);
#endif
#ifdef VCI_LOGGER_ON_L1_TGT
	soclib::caba::VciLogger<vci_param> vci_logger2("vci_logger2",maptabp);
#endif
	proc0->p_clk(signal_clk);
	proc0->p_resetn(signal_resetn);
	proc0->p_irq[0](signal_proc0_it0);
	proc0->p_irq[1](signal_proc0_it1);
	proc0->p_irq[2](signal_proc0_it2);
	proc0->p_irq[3](signal_proc0_it3);
	proc0->p_irq[4](signal_proc0_it4);
	proc0->p_irq[5](signal_proc0_it5);
	proc0->p_vci(signal_vci_ini_rw_proc0);
	proc0->p_dspin_m2p(signal_dspin_m2p_proc[0]);
	proc0->p_dspin_p2m(signal_dspin_p2m_proc[0]);
	proc0->p_dspin_clack(signal_dspin_clack_proc[0]);
	wi_proc0.p_clk(signal_clk);
	wi_proc0.p_resetn(signal_resetn);
	wi_proc0.p_vci(signal_vci_ini_rw_proc0);
	wi_proc0.p_dspin_cmd(signal_dspin_cmd_proc0_i);
	wi_proc0.p_dspin_rsp(signal_dspin_rsp_proc0_i);

	rom->p_clk(signal_clk);
	rom->p_resetn(signal_resetn);
	rom->p_vci(signal_vci_tgt_rom);
	wt_rom.p_clk(signal_clk);
	wt_rom.p_resetn(signal_resetn);
	wt_rom.p_vci(signal_vci_tgt_rom);
	wt_rom.p_dspin_cmd(signal_dspin_cmd_rom_t);
	wt_rom.p_dspin_rsp(signal_dspin_rsp_rom_t);

	vcixicu->p_resetn(signal_resetn);
	vcixicu->p_clk(signal_clk);
	vcixicu->p_vci(signal_vci_xicu);
	vcixicu->p_hwi[0](signal_icu_irq0);
	vcixicu->p_hwi[1](signal_icu_irq1);
	vcixicu->p_hwi[2](signal_icu_irq2);
	vcixicu->p_irq[0](signal_proc0_it0);
	vcixicu->p_irq[1](signal_proc0_it1);
	vcixicu->p_irq[2](signal_proc0_it2);
	vcixicu->p_irq[3](signal_proc0_it3);
	vcixicu->p_irq[4](signal_proc0_it4);
	vcixicu->p_irq[5](signal_proc0_it5);
	wt_xicu.p_clk(signal_clk);
	wt_xicu.p_resetn(signal_resetn);
	wt_xicu.p_vci(signal_vci_xicu);
	wt_xicu.p_dspin_cmd(signal_dspin_cmd_xicu_t);
	wt_xicu.p_dspin_rsp(signal_dspin_rsp_xicu_t);

	vcidma->p_clk(signal_clk);
	vcidma->p_resetn(signal_resetn);
	vcidma->p_irq[0](signal_icu_irq1);
	vcidma->p_vci_target(signal_vci_dmat);
	vcidma->p_vci_initiator(signal_vci_dmai);
	wt_dma.p_clk(signal_clk);
	wt_dma.p_resetn(signal_resetn);
	wt_dma.p_vci(signal_vci_dmat);
	wt_dma.p_dspin_cmd(signal_dspin_cmd_dma_t);
	wt_dma.p_dspin_rsp(signal_dspin_rsp_dma_t);
	wi_dma.p_clk(signal_clk);
	wi_dma.p_resetn(signal_resetn);
	wi_dma.p_vci(signal_vci_dmai);
	wi_dma.p_dspin_cmd(signal_dspin_cmd_dma_i);
	wi_dma.p_dspin_rsp(signal_dspin_rsp_dma_i);


#ifdef VCI_LOGGER_ON_L1
  vci_logger0.p_clk(signal_clk);
  vci_logger0.p_resetn(signal_resetn);
  vci_logger0.p_vci(signal_vci_ini_rw_proc0);
#endif

#ifdef VCI_LOGGER_ON_ROM
  vci_logger1.p_clk(signal_clk);
  vci_logger1.p_resetn(signal_resetn);
  vci_logger1.p_vci(signal_vci_tgt_rom);
#endif

#ifdef VCI_LOGGER_ON_L1_TGT
  vci_logger2.p_clk(signal_clk);
  vci_logger2.p_resetn(signal_resetn);
#endif

	vcitty->p_clk(signal_clk);
	vcitty->p_resetn(signal_resetn);
	vcitty->p_vci(signal_vci_tty);
	vcitty->p_irq[0](signal_icu_irq0);
	wt_tty.p_clk(signal_clk);
	wt_tty.p_resetn(signal_resetn);
	wt_tty.p_vci(signal_vci_tty);
	wt_tty.p_dspin_cmd(signal_dspin_cmd_tty_t);
	wt_tty.p_dspin_rsp(signal_dspin_rsp_tty_t);

	vcisimh->p_clk(signal_clk);
	vcisimh->p_resetn(signal_resetn);
	vcisimh->p_vci(signal_vci_simh);
	wt_simh.p_clk(signal_clk);
	wt_simh.p_resetn(signal_resetn);
	wt_simh.p_vci(signal_vci_simh);
	wt_simh.p_dspin_cmd(signal_dspin_cmd_simh_t);
	wt_simh.p_dspin_rsp(signal_dspin_rsp_simh_t);

	memc->p_clk(signal_clk);
	memc->p_resetn(signal_resetn);
	memc->p_irq(signal_icu_irq2);
	memc->p_vci_tgt(signal_vci_tgt_memc);
	memc->p_dspin_p2m(signal_dspin_p2m_memc);
	memc->p_dspin_m2p(signal_dspin_m2p_memc);
	memc->p_dspin_clack(signal_dspin_clack_memc);
	memc->p_vci_ixr(signal_vci_ixr_memc);
	wt_memc.p_clk(signal_clk);
	wt_memc.p_resetn(signal_resetn);
	wt_memc.p_vci(signal_vci_tgt_memc);
	wt_memc.p_dspin_cmd(signal_dspin_cmd_memc_t);
	wt_memc.p_dspin_rsp(signal_dspin_rsp_memc_t);

	xram->p_clk(signal_clk);
	xram->p_resetn(signal_resetn);
	xram->p_vci(signal_vci_ixr_memc);
	

	xbar_cmd_d->p_clk(signal_clk);
	xbar_cmd_d->p_resetn(signal_resetn);
	xbar_rsp_d->p_clk(signal_clk);
	xbar_rsp_d->p_resetn(signal_resetn);
	xbar_m2p_c->p_clk(signal_clk);
	xbar_m2p_c->p_resetn(signal_resetn);
	xbar_clack_c->p_clk(signal_clk);
	xbar_clack_c->p_resetn(signal_resetn);
	xbar_p2m_c->p_clk(signal_clk);
	xbar_p2m_c->p_resetn(signal_resetn);

	xbar_cmd_d->p_global_out(signal_dspin_cmd_l2g_c);
	xbar_cmd_d->p_global_in(signal_dspin_cmd_g2l_c);
	xbar_rsp_d->p_global_out(signal_dspin_rsp_l2g_c);
	xbar_rsp_d->p_global_in(signal_dspin_rsp_g2l_c);
	xbar_m2p_c->p_global_out(signal_dspin_m2p_l2g_c);
	xbar_m2p_c->p_global_in(signal_dspin_m2p_g2l_c);
	xbar_clack_c->p_global_out(signal_dspin_clack_l2g_c);
	xbar_clack_c->p_global_in(signal_dspin_clack_g2l_c);
	xbar_p2m_c->p_global_out(signal_dspin_p2m_l2g_c);
	xbar_p2m_c->p_global_in(signal_dspin_p2m_g2l_c);

	xbar_cmd_d->p_local_in[0](signal_dspin_cmd_proc0_i);
	xbar_cmd_d->p_local_in[1](signal_dspin_cmd_dma_i);
	xbar_cmd_d->p_local_out[0](signal_dspin_cmd_memc_t);
	xbar_cmd_d->p_local_out[1](signal_dspin_cmd_rom_t);
	xbar_cmd_d->p_local_out[2](signal_dspin_cmd_tty_t);
	xbar_cmd_d->p_local_out[3](signal_dspin_cmd_xicu_t);
	xbar_cmd_d->p_local_out[4](signal_dspin_cmd_dma_t);
	xbar_cmd_d->p_local_out[5](signal_dspin_cmd_simh_t);

	xbar_rsp_d->p_local_out[0](signal_dspin_rsp_proc0_i);
	xbar_rsp_d->p_local_out[1](signal_dspin_rsp_dma_i);
	xbar_rsp_d->p_local_in[0](signal_dspin_rsp_memc_t);
	xbar_rsp_d->p_local_in[1](signal_dspin_rsp_rom_t);
	xbar_rsp_d->p_local_in[2](signal_dspin_rsp_tty_t);
	xbar_rsp_d->p_local_in[3](signal_dspin_rsp_xicu_t);
	xbar_rsp_d->p_local_in[4](signal_dspin_rsp_dma_t);
	xbar_rsp_d->p_local_in[5](signal_dspin_rsp_simh_t);

	xbar_m2p_c->p_local_in[0](signal_dspin_m2p_memc);
	xbar_m2p_c->p_local_out[0](signal_dspin_m2p_proc[0]);

	xbar_clack_c->p_local_in[0](signal_dspin_clack_memc);
	xbar_clack_c->p_local_out[0](signal_dspin_clack_proc[0]);

	xbar_p2m_c->p_local_out[0](signal_dspin_p2m_memc);
	xbar_p2m_c->p_local_in[0](signal_dspin_p2m_proc[0]);

	sc_start(sc_core::sc_time(0, SC_NS));
	signal_resetn = false;

	sc_start(sc_core::sc_time(1, SC_NS));
	signal_resetn = true;


	for ( size_t n=1 ; n<ncycles ; n++ )
	{
		if ( trace_ok and (n > from_cycle) )
		{
		    std::cout << "****************** cycle " << std::dec << n
			      << " ************************************************" << std::endl;
		    proc0->print_trace();
		    memc->print_trace();
		    signal_vci_ini_rw_proc0.print_trace("signal_vci_ini_rw_proc0");
		    signal_vci_tgt_memc.print_trace("signal_vci_tgt_memc");
		    signal_vci_ixr_memc.print_trace("signal_vci_ixr_memc");
		}
		sc_start(sc_core::sc_time(1, SC_NS));
	}
	return EXIT_FAILURE;

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
	return 1;
}

