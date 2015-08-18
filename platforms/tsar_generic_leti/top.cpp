/////////////////////////////////////////////////////////////////////////
// File: top.cpp (for tsar_generic_leti platform)
// Author: Alain Greiner
// Copyright: UPMC/LIP6
// Date : february 2013 / updated january 2015
// This program is released under the GNU public license
/////////////////////////////////////////////////////////////////////////
// This file define a generic TSAR architecture, fully compatible
// with the VLSI Hardware prototype developped by CEA-LETI and LIP6
// in the framework of the SHARP project.
//
// The processor is a MIPS32 processor wrapped in a GDB server
// (this is defined in the tsar_leti_cluster).
//
// The main hardware parameters are the mesh size (X_SIZE & Y_SIZE),
// and the number of processors per cluster (NB_PROCS_MAX).
// The NB_PROCS_MAX parameter cannot be larger than 4.
// Others parameters are the frame buffer size, the disk controller type
// (BDV or HBA), the number of TTY channels, the number of NIC channels,
// and the number of CMA channels. 
//
// All external peripherals are located in cluster[X_SIZE-1][Y_SIZE-1],
// and are connected to an IO bus (implemented as a vci_local_crossbar):
// - DISK : block device controller (BDV / HBA)
// - MNIC : multi-channel ethernet controller
// - CDMA : multi-channel chained buffer dma controller
// - MTTY : multi-channel tty controller
// - FBUF : frame buffer controller
// - IOPI : HWI to SWI translator 
//
// This IO bus is directly connected to the north ports of the CMD/RSP
// routers in cluster[X_SIZE-1][y_SIZE-2] through VCI/DSPIN wrappers.
// All other clusters in the upper row are empty: no processors,
// no ram, no routers. 
// The X_SIZE parameter must be larger than 0, but no larger than 16.
// The Y_SIZE parameter must be larger than 1, but no larger than 16.
//
// We don't use an external ROM, as the boot code is (pre)loaded
// in RAM in cluster[0][0] at address 0x0.
//
// An optional RAMDISK of 32 Mbytes can be used in RAM of cluster[0][0].
//
// The physical address space is 40 bits.
// The 8 address MSB bits define the cluster index.
//
// Besides the processors, each cluster contains:
// - 5 L1/L2 DSPIN routers implementing 5 separated NOCs
// - 1 vci_mem_cache
// - 1 vci_xicu
// - 1 vci_simple_ram (to emulate the L3 cache).
//
// Each processor receives 4 consecutive IRQ lines from the local XICU.
// The number of PTI and WTI IRQs is bounded to 16.
//
// In all clusters, the MEMC IRQ line (signaling a late write error)
// is connected to XICU HWI[8]
//
// For all external peripherals, the hardware interrupts (HWI) are
// translated to write interrupts (WTI) by the iopic component:
// - IOPIC HWI[1:0]     connected to IRQ_NIC_RX[1:0]
// - IOPIC HWI[3:2]     connected to IRQ_NIC_TX[1:0]
// - IOPIC HWI[7:4]     connected to IRQ_CMA_TX[3:0]]
// - IOPIC HWI[8]       connected to IRQ_DISK
// - IOPIC HWI[15:9]    unused       (grounded)
// - IOPIC HWI[23:16]   connected to IRQ_TTY_RX[7:0]]
// - IOPIC HWI[31:24]   connected to IRQ_TTY_TX[7:0]]
//
// The cluster internal architecture is defined in file tsar_leti_cluster,
// that must be considered as an extension of this top.cpp file.
////////////////////////////////////////////////////////////////////////////
// The following parameters must be defined in the hard_config.h file :
// - X_WIDTH          : number of bits for x coordinate (must be 4)
// - Y_WIDTH          : number of bits for y coordinate (must be 4)
// - P_WIDTH          : number of bits for local processor coordinate
// - X_SIZE           : number of clusters in a row (1,2,4,8,16)
// - Y_SIZE           : number of clusters in a column (1,2,4,8)
// - NB_PROCS_MAX     : number of processors per cluster (1, 2 or 4)
// - NB_CMA_CHANNELS  : number of CMA channels in I/0 cluster (4 max)
// - NB_TTY_CHANNELS  : number of TTY channels in I/O cluster (8 max)
// - NB_NIC_CHANNELS  : number of NIC channels in I/O cluster (2 max)
// - FBUF_X_SIZE      : number of pixels per line for frame buffer
// - FBUF_Y_SIZE      : number of lines for frame buffer
// - XCU_NB_HWI       : number of XCU HWIs (must be 16)
// - XCU_NB_PTI       : number of XCU PTIs (must be 16)
// - XCU_NB_WTI       : number of XCU WTIs (must be 16)
// - XCU_NB_OUT       : number of XCU output (must be 16)
// - USE_IOC_XYZ      : IOC type (XYZ in HBA / BDV / SDC / RDK)
//
// Some other hardware parameters are not used when compiling the OS,
// and are only defined in this top.cpp file:
// - XRAM_LATENCY     : external ram latency
// - L1_IWAYS         : L1 cache instruction number of ways
// - L1_ISETS         : L1 cache instruction number of sets
// - L1_DWAYS         : L1 cache data number of ways
// - L1_DSETS         : L1 cache data number of sets
// - DISK_IMAGE_NAME  : pathname for block device disk image
/////////////////////////////////////////////////////////////////////////
// General policy for 40 bits physical address decoding:
// All physical segments base addresses are multiple of 1 Mbytes
// (=> the 24 LSB bits = 0, and the 16 MSB bits define the target)
// The (X_WIDTH + Y_WIDTH) MSB bits (left aligned) define
// the cluster index, and the LADR bits define the local index:
//      |X_ID|Y_ID|  LADR |     OFFSET          |
//      |  4 |  4 |   8   |       24            |
/////////////////////////////////////////////////////////////////////////
// General policy for 14 bits SRCID decoding:
// Each component is identified by (x_id, y_id, l_id) tuple.
//      |X_ID|Y_ID| L_ID |
//      |  4 |  4 |  6   |
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

#include "tsar_leti_cluster.h"
#include "vci_local_crossbar.h"
#include "vci_dspin_initiator_wrapper.h"
#include "vci_dspin_target_wrapper.h"
#include "vci_multi_tty.h"
#include "vci_multi_nic.h"
#include "vci_chbuf_dma.h"
#include "vci_block_device_tsar.h"
#include "vci_multi_ahci.h"
#include "vci_framebuffer.h"
#include "vci_iopic.h"

#include "alloc_elems.h"

///////////////////////////////////////////////////
// Main hardware parameters values
///////////////////////////////////////////////////

#include "hard_config.h"

///////////////////////////////////////////////////////////////////////////////////////
//    Secondary Hardware Parameters
///////////////////////////////////////////////////////////////////////////////////////

#define XMAX                  X_SIZE         // actual number of columns in 2D mesh
#define YMAX                  (Y_SIZE - 1)   // actual number of rows in 2D mesh

#define XRAM_LATENCY          0

#define MEMC_WAYS             16
#define MEMC_SETS             256

#define L1_IWAYS              4
#define L1_ISETS              64

#define L1_DWAYS              4
#define L1_DSETS              64

#define DISK_IMAGE_NAME       "../../../giet_vm/hdd/virt_hdd.dmg"

#define ROM_SOFT_NAME         "../../softs/tsar_boot/preloader.elf"

#define NORTH                 0
#define SOUTH                 1
#define EAST                  2
#define WEST                  3

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
//     DEBUG Parameters default values
///////////////////////////////////////////////////////////////////////////////////////

#define MAX_FROZEN_CYCLES     500000
#define SIM_STEP              500000
#define STATS_STEP            (2*SIM_STEP) 

///////////////////////////////////////////////////////////////////////////////////////
//     LOCAL TGTID & SRCID definition
// For all components:  global TGTID = global SRCID = cluster_index
///////////////////////////////////////////////////////////////////////////////////////

#define MEMC_TGTID            0
#define XICU_TGTID            1
#define MTTY_TGTID            2
#define DISK_TGTID            3
#define FBUF_TGTID            4
#define MNIC_TGTID            5
#define CDMA_TGTID            6
#define IOPI_TGTID            7

#define DISK_SRCID            NB_PROCS_MAX
#define CDMA_SRCID            NB_PROCS_MAX + 1
#define IOPI_SRCID            NB_PROCS_MAX + 2

bool stop_called = false;

/////////////////////////////////
int _main(int argc, char *argv[])
{
   using namespace sc_core;
   using namespace soclib::caba;
   using namespace soclib::common;

   uint32_t ncycles           = 0xFFFFFFFF;         // max simulated cycles
   size_t   threads           = 1;                  // simulator's threads number
   bool     trace_ok          = false;              // trace activated
   uint32_t trace_from        = 0;                  // trace start cycle
   bool     trace_proc_ok     = false;              // detailed proc trace activated
   size_t   trace_memc_ok     = false;              // detailed memc trace activated
   size_t   trace_memc_id     = 0;                  // index of memc to be traced
   size_t   trace_proc_id     = 0;                  // index of proc to be traced
   char     soft_name[256]    = ROM_SOFT_NAME;      // pathname for ROM binary code
   char     disk_name[256]    = DISK_IMAGE_NAME;    // pathname for DISK image
   uint32_t frozen_cycles     = MAX_FROZEN_CYCLES;  // for debug
   struct   timeval t1,t2;
   uint64_t ms1,ms2;

   ////////////// command line arguments //////////////////////
   if (argc > 1)
   {
      for (int n = 1; n < argc; n = n + 2)
      {
         if ((strcmp(argv[n], "-NCYCLES") == 0) && (n + 1 < argc))
         {
            ncycles = (uint64_t) strtol(argv[n + 1], NULL, 0);
         }
         else if ((strcmp(argv[n],"-SOFT") == 0) && (n + 1 < argc))
         {
            strcpy(soft_name, argv[n + 1]);
         }
         else if ((strcmp(argv[n],"-DISK") == 0) && (n + 1 < argc))
         {
            strcpy(disk_name, argv[n + 1]);
         }
         else if ((strcmp(argv[n],"-DEBUG") == 0) && (n + 1 < argc))
         {
            trace_ok = true;
            trace_from = (uint32_t) strtol(argv[n + 1], NULL, 0);
         }
         else if ((strcmp(argv[n], "-MEMCID") == 0) && (n + 1 < argc))
         {
            trace_memc_ok = true;
            trace_memc_id = (size_t) strtol(argv[n + 1], NULL, 0);
            size_t x = trace_memc_id >> Y_WIDTH;
            size_t y = trace_memc_id & ((1<<Y_WIDTH)-1);

            assert( (x < XMAX) and (y < (YMAX)) and
                  "MEMCID parameter doesxn't fit valid XMAX/YMAX");
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
            std::cout << "     - THREADS simulator's threads number" << std::endl;
            std::cout << "     - FROZEN max_number_of_lines" << std::endl;
            std::cout << "     - MEMCID index_memc_to_be_traced" << std::endl;
            std::cout << "     - PROCID index_proc_to_be_traced" << std::endl;
            exit(0);
         }
      }
   }

    // checking hardware parameters
    assert( ((X_SIZE <= 16) and (X_SIZE > 0)) and
            "Illegal X_SIZE parameter" );

    assert( ((Y_SIZE <= 16) and (Y_SIZE > 1)) and
            "Illegal Y_SIZE parameter" );

    assert( (P_WIDTH <= 2) and
            "P_WIDTH parameter cannot be larger than 2" );

    assert( (NB_PROCS_MAX <= 4) and
            "Illegal NB_PROCS_MAX parameter" );

    assert( (XCU_NB_HWI == 16) and
            "XCU_NB_HWI must be 16" );

    assert( (XCU_NB_PTI == 16) and
            "XCU_NB_PTI must be 16" );

    assert( (XCU_NB_WTI == 16) and
            "XCU_NB_WTI must be 16" );

    assert( (XCU_NB_OUT == 16) and
            "XCU_NB_OUT must be 16" );
   
    assert( (NB_CMA_CHANNELS <= 4) and
            "The NB_CMA_CHANNELS parameter cannot be larger than 4" );

    assert( (NB_TTY_CHANNELS <= 8) and
            "The NB_TTY_CHANNELS parameter cannot be larger than 8" );

    assert( (NB_NIC_CHANNELS <= 2) and
            "The NB_NIC_CHANNELS parameter cannot be larger than 2" );

    assert( (vci_address_width == 40) and
            "VCI address width with the GIET must be 40 bits" );

    assert( (X_WIDTH == 4) and (Y_WIDTH == 4) and
            "You must have X_WIDTH == Y_WIDTH == 4");

    std::cout << std::endl;

    std::cout << " - XMAX             = " << XMAX << std::endl
              << " - YMAX             = " << YMAX << std::endl
              << " - NB_PROCS_MAX     = " << NB_PROCS_MAX <<  std::endl
              << " - NB_TTY_CHANNELS  = " << NB_TTY_CHANNELS <<  std::endl
              << " - NB_NIC_CHANNELS  = " << NB_NIC_CHANNELS <<  std::endl
              << " - NB_CMA_CHANNELS  = " << NB_CMA_CHANNELS <<  std::endl
              << " - MEMC_WAYS        = " << MEMC_WAYS << std::endl
              << " - MEMC_SETS        = " << MEMC_SETS << std::endl
              << " - RAM_LATENCY      = " << XRAM_LATENCY << std::endl
              << " - MAX_FROZEN       = " << frozen_cycles << std::endl
              << " - MAX_CYCLES       = " << ncycles << std::endl
              << " - RESET_ADDRESS    = " << RESET_ADDRESS << std::endl
              << " - SOFT_FILENAME    = " << soft_name << std::endl
              << " - DISK_IMAGENAME   = " << disk_name << std::endl
              << " - OPENMP THREADS   = " << threads << std::endl
              << " - DEBUG_PROCID     = " << trace_proc_id << std::endl
              << " - DEBUG_MEMCID     = " << trace_memc_id << std::endl;

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
#endif


   ///////////////////////////////////////
   //  Direct Network Mapping Table
   ///////////////////////////////////////

   MappingTable maptabd(vci_address_width,
                        IntTab(X_WIDTH + Y_WIDTH, 16 - X_WIDTH - Y_WIDTH),
                        IntTab(X_WIDTH + Y_WIDTH, vci_srcid_width - X_WIDTH - Y_WIDTH),
                        0x00FF000000ULL);

   // replicated segments
   for (size_t x = 0; x < XMAX; x++)
   {
      for (size_t y = 0; y < (YMAX) ; y++)
      {
         sc_uint<vci_address_width> offset;
         offset = ((sc_uint<vci_address_width>)cluster(x,y)) << 32;

         std::ostringstream    si;
         si << "seg_xicu_" << x << "_" << y;
         maptabd.add(Segment(si.str(), SEG_XCU_BASE + offset, SEG_XCU_SIZE,
                  IntTab(cluster(x,y),XICU_TGTID), false));

         std::ostringstream    sd;
         sd << "seg_mcfg_" << x << "_" << y;
         maptabd.add(Segment(sd.str(), SEG_MMC_BASE + offset, SEG_MMC_SIZE,
                  IntTab(cluster(x,y),MEMC_TGTID), false));

         std::ostringstream    sh;
         sh << "seg_memc_" << x << "_" << y;
         maptabd.add(Segment(sh.str(), SEG_RAM_BASE + offset, SEG_RAM_SIZE,
                  IntTab(cluster(x,y),MEMC_TGTID), true));
      }
   }

   // segments for peripherals in cluster(0,0)
   maptabd.add(Segment("seg_tty0", SEG_TTY_BASE, SEG_TTY_SIZE,
               IntTab(cluster(0,0),MTTY_TGTID), false));

   maptabd.add(Segment("seg_ioc0", SEG_IOC_BASE, SEG_IOC_SIZE,
               IntTab(cluster(0,0),DISK_TGTID), false));

   // segments for peripherals in cluster_io (XMAX-1,YMAX)
   sc_uint<vci_address_width> offset;
   offset = ((sc_uint<vci_address_width>)cluster(XMAX-1,YMAX)) << 32;

   maptabd.add(Segment("seg_mtty", SEG_TTY_BASE + offset, SEG_TTY_SIZE,
               IntTab(cluster(XMAX-1, YMAX),MTTY_TGTID), false));

   maptabd.add(Segment("seg_fbuf", SEG_FBF_BASE + offset, SEG_FBF_SIZE,
               IntTab(cluster(XMAX-1, YMAX),FBUF_TGTID), false));

   maptabd.add(Segment("seg_disk", SEG_IOC_BASE + offset, SEG_IOC_SIZE,
               IntTab(cluster(XMAX-1, YMAX),DISK_TGTID), false));

   maptabd.add(Segment("seg_mnic", SEG_NIC_BASE + offset, SEG_NIC_SIZE,
               IntTab(cluster(XMAX-1, YMAX),MNIC_TGTID), false));

   maptabd.add(Segment("seg_cdma", SEG_CMA_BASE + offset, SEG_CMA_SIZE,
               IntTab(cluster(XMAX-1, YMAX),CDMA_TGTID), false));

   maptabd.add(Segment("seg_iopi", SEG_PIC_BASE + offset, SEG_PIC_SIZE,
               IntTab(cluster(XMAX-1, YMAX),IOPI_TGTID), false));

   std::cout << maptabd << std::endl;

    /////////////////////////////////////////////////
    // Ram network mapping table
    /////////////////////////////////////////////////

    MappingTable maptabx(vci_address_width,
                         IntTab(X_WIDTH+Y_WIDTH),
                         IntTab(X_WIDTH+Y_WIDTH),
                         0x00FF000000ULL);

    for (size_t x = 0; x < XMAX; x++)
    {
        for (size_t y = 0; y < (YMAX) ; y++)
        {
            sc_uint<vci_address_width> offset;
            offset = (sc_uint<vci_address_width>)cluster(x,y)
                      << (vci_address_width-X_WIDTH-Y_WIDTH);

            std::ostringstream sh;
            sh << "x_seg_memc_" << x << "_" << y;

            maptabx.add(Segment(sh.str(), SEG_RAM_BASE + offset,
                     SEG_RAM_SIZE, IntTab(cluster(x,y)), false));
        }
    }
    std::cout << maptabx << std::endl;

    ////////////////////
    // Signals
    ///////////////////

    sc_clock                          signal_clk("clk");
    sc_signal<bool>                   signal_resetn("resetn");

    // IRQs from external peripherals
    sc_signal<bool>                   signal_irq_disk;
    sc_signal<bool>                   signal_irq_mnic_rx[NB_NIC_CHANNELS];
    sc_signal<bool>                   signal_irq_mnic_tx[NB_NIC_CHANNELS];
    sc_signal<bool>                   signal_irq_mtty_rx[NB_TTY_CHANNELS];
    sc_signal<bool>                   signal_irq_cdma[NB_CMA_CHANNELS];
    sc_signal<bool>                   signal_irq_false;

   // Horizontal inter-clusters DSPIN signals
   DspinSignals<dspin_cmd_width>** signal_dspin_h_cmd_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cmd_inc", XMAX-1, YMAX);
   DspinSignals<dspin_cmd_width>** signal_dspin_h_cmd_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cmd_dec", XMAX-1, YMAX);

   DspinSignals<dspin_rsp_width>** signal_dspin_h_rsp_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_rsp_inc", XMAX-1, YMAX);
   DspinSignals<dspin_rsp_width>** signal_dspin_h_rsp_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_rsp_dec", XMAX-1, YMAX);

   DspinSignals<dspin_cmd_width>** signal_dspin_h_m2p_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_m2p_inc", XMAX-1, YMAX);
   DspinSignals<dspin_cmd_width>** signal_dspin_h_m2p_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_m2p_dec", XMAX-1, YMAX);

   DspinSignals<dspin_rsp_width>** signal_dspin_h_p2m_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_p2m_inc", XMAX-1, YMAX);
   DspinSignals<dspin_rsp_width>** signal_dspin_h_p2m_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_p2m_dec", XMAX-1, YMAX);

   DspinSignals<dspin_cmd_width>** signal_dspin_h_cla_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cla_inc", XMAX-1, YMAX);
   DspinSignals<dspin_cmd_width>** signal_dspin_h_cla_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cla_dec", XMAX-1, YMAX);

   // Vertical inter-clusters DSPIN signals
   DspinSignals<dspin_cmd_width>** signal_dspin_v_cmd_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cmd_inc", XMAX, YMAX-1);
   DspinSignals<dspin_cmd_width>** signal_dspin_v_cmd_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cmd_dec", XMAX, YMAX-1);

   DspinSignals<dspin_rsp_width>** signal_dspin_v_rsp_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_rsp_inc", XMAX, YMAX-1);
   DspinSignals<dspin_rsp_width>** signal_dspin_v_rsp_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_rsp_dec", XMAX, YMAX-1);

   DspinSignals<dspin_cmd_width>** signal_dspin_v_m2p_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_m2p_inc", XMAX, YMAX-1);
   DspinSignals<dspin_cmd_width>** signal_dspin_v_m2p_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_m2p_dec", XMAX, YMAX-1);

   DspinSignals<dspin_rsp_width>** signal_dspin_v_p2m_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_p2m_inc", XMAX, YMAX-1);
   DspinSignals<dspin_rsp_width>** signal_dspin_v_p2m_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_p2m_dec", XMAX, YMAX-1);

   DspinSignals<dspin_cmd_width>** signal_dspin_v_cla_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cla_inc", XMAX, YMAX-1);
   DspinSignals<dspin_cmd_width>** signal_dspin_v_cla_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cla_dec", XMAX, YMAX-1);

   // Mesh boundaries DSPIN signals (Most of those signals are not used...)
   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_cmd_in =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_cmd_in" , XMAX, YMAX, 4);
   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_cmd_out =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_cmd_out", XMAX, YMAX, 4);

   DspinSignals<dspin_rsp_width>*** signal_dspin_bound_rsp_in =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_bound_rsp_in" , XMAX, YMAX, 4);
   DspinSignals<dspin_rsp_width>*** signal_dspin_bound_rsp_out =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_bound_rsp_out", XMAX, YMAX, 4);

   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_m2p_in =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_m2p_in" , XMAX, YMAX, 4);
   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_m2p_out =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_m2p_out", XMAX, YMAX, 4);

   DspinSignals<dspin_rsp_width>*** signal_dspin_bound_p2m_in =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_bound_p2m_in" , XMAX, YMAX, 4);
   DspinSignals<dspin_rsp_width>*** signal_dspin_bound_p2m_out =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_bound_p2m_out", XMAX, YMAX, 4);

   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_cla_in =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_cla_in" , XMAX, YMAX, 4);
   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_cla_out =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_cla_out", XMAX, YMAX, 4);

   // VCI signals for iobus and peripherals
   VciSignals<vci_param_int>    signal_vci_ini_disk("signal_vci_ini_disk");
   VciSignals<vci_param_int>    signal_vci_ini_cdma("signal_vci_ini_cdma");
   VciSignals<vci_param_int>    signal_vci_ini_iopi("signal_vci_ini_iopi");

   VciSignals<vci_param_int>*   signal_vci_ini_proc =
       alloc_elems<VciSignals<vci_param_int> >("signal_vci_ini_proc", NB_PROCS_MAX );

   VciSignals<vci_param_int>    signal_vci_tgt_memc("signal_vci_tgt_memc");
   VciSignals<vci_param_int>    signal_vci_tgt_xicu("signal_vci_tgt_xicu");
   VciSignals<vci_param_int>    signal_vci_tgt_disk("signal_vci_tgt_disk");
   VciSignals<vci_param_int>    signal_vci_tgt_mtty("signal_vci_tgt_mtty");
   VciSignals<vci_param_int>    signal_vci_tgt_fbuf("signal_vci_tgt_fbuf");
   VciSignals<vci_param_int>    signal_vci_tgt_mnic("signal_vci_tgt_mnic");
   VciSignals<vci_param_int>    signal_vci_tgt_cdma("signal_vci_tgt_cdma");
   VciSignals<vci_param_int>    signal_vci_tgt_iopi("signal_vci_tgt_iopi");

   VciSignals<vci_param_int>    signal_vci_cmd_to_noc("signal_vci_cmd_to_noc");
   VciSignals<vci_param_int>    signal_vci_cmd_from_noc("signal_vci_cmd_from_noc");

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
   // mesh construction: XMAX * YMAX clusters
   //////////////////////////////////////////////////////////////

   TsarLetiCluster<dspin_cmd_width,
                   dspin_rsp_width,
                   vci_param_int,
                   vci_param_ext>*          clusters[XMAX][YMAX];

#if USE_OPENMP
#pragma omp parallel
    {
#pragma omp for
#endif
        for (size_t i = 0; i  < (XMAX * YMAX); i++)
        {
            size_t x = i / (YMAX);
            size_t y = i % (YMAX);

#if USE_OPENMP
#pragma omp critical
            {
#endif
            std::cout << std::endl;
            std::cout << "Cluster_" << std::dec << x << "_" << y
                      << " with cluster_xy = " << std::hex << cluster(x,y) << std::endl;
            std::cout << std::endl;

            std::ostringstream cluster_name;
            cluster_name <<  "cluster_" << std::dec << x << "_" << y;

            clusters[x][y] = new TsarLetiCluster<dspin_cmd_width,
                                                 dspin_rsp_width,
                                                 vci_param_int,
                                                 vci_param_ext>
            (
                cluster_name.str().c_str(),
                NB_PROCS_MAX,
                x,
                y,
                cluster(x,y),
                maptabd,
                maptabx,
                RESET_ADDRESS,
                X_WIDTH,
                Y_WIDTH,
                vci_srcid_width - X_WIDTH - Y_WIDTH,   // l_id width,
                P_WIDTH,
                MEMC_TGTID,
                XICU_TGTID,
                MTTY_TGTID,
                DISK_TGTID,
                disk_name,
                MEMC_WAYS,
                MEMC_SETS,
                L1_IWAYS,
                L1_ISETS,
                L1_DWAYS,
                L1_DSETS,
                XRAM_LATENCY,
                loader,
                frozen_cycles,
                trace_from,
                trace_proc_ok,
                trace_proc_id,
                trace_memc_ok,
                trace_memc_id
            );

#if USE_OPENMP
            } // end critical
#endif
        } // end for
#if USE_OPENMP
    }
#endif

#if USE_PIC

    //////////////////////////////////////////////////////////////////
    // IO bus and external peripherals in cluster[X_SIZE-1][Y_SIZE-1]
    // - 6 local targets    : FBF, TTY, CMA, NIC, PIC, IOC
    // - 3 local initiators : IOC, CMA, PIC
    // There is no PROC, no MEMC and no XICU in this cluster,
    // but the crossbar has (NB_PROCS_MAX + 3) intiators and
    // 8 targets, in order to use the same SRCID and TGTID space
    // (same mapping table for the internal components,
    //  and for the external peripherals)
    //////////////////////////////////////////////////////////////////

    std::cout << std::endl;
    std::cout << " Building IO cluster (external peripherals)" << std::endl;
    std::cout << std::endl;

    size_t cluster_io = cluster(XMAX-1, YMAX);

    //////////// vci_local_crossbar
    VciLocalCrossbar<vci_param_int>*
    iobus = new VciLocalCrossbar<vci_param_int>(
                "iobus",
                maptabd,                      // mapping table
                cluster_io,                   // cluster_xy
                NB_PROCS_MAX + 3,             // number of local initiators
                8,                            // number of local targets
                DISK_TGTID );                 // default target index

    //////////// vci_framebuffer
    VciFrameBuffer<vci_param_int>*
    fbuf = new VciFrameBuffer<vci_param_int>(
                "fbuf",
                IntTab(cluster_io, FBUF_TGTID),
                maptabd,
                FBUF_X_SIZE, FBUF_Y_SIZE );

#if ( USE_IOC_HBA )

    ////////////  vci_multi_ahci
    std::vector<std::string> filenames;
    filenames.push_back(disk_name);           // one single disk
    VciMultiAhci<vci_param_int>*  
    disk = new VciMultiAhci<vci_param_int>( 
                "disk",
                maptabd,
                IntTab(cluster_io, DISK_SRCID),
                IntTab(cluster_io, DISK_TGTID),
                filenames,
                512,                          // block size
                64,                           // burst size (bytes)
                0 );                          // disk latency

#elif ( USE_IOC_BDV or USE_IOC_SDC )

    ////////////  vci_block_device
    VciBlockDeviceTsar<vci_param_int>*
    disk = new VciBlockDeviceTsar<vci_param_int>(
                "disk",
                maptabd,
                IntTab(cluster_io, DISK_SRCID),
                IntTab(cluster_io, DISK_TGTID),
                disk_name,
                512,                          // block size
                64,                           // burst size (bytes)
                0 );                          // disk latency
#endif

    //////////// vci_multi_nic
    VciMultiNic<vci_param_int>*
    mnic = new VciMultiNic<vci_param_int>(
             "mnic",
                IntTab(cluster_io, MNIC_TGTID),
                maptabd,
                NB_NIC_CHANNELS,
                0,                // default MAC_4 address
                0,                // default MAC_2 address
                1,                // NIC_MODE_SYNTHESIS
                12 );             // INTER_FRAME_GAP

    ///////////// vci_chbuf_dma
    VciChbufDma<vci_param_int>*
    cdma = new VciChbufDma<vci_param_int>(
                "cdma",
                maptabd,
                IntTab(cluster_io, CDMA_SRCID),
                IntTab(cluster_io, CDMA_TGTID),
                64,                               // burst size
                NB_CMA_CHANNELS,
                4 );                              // number of pipelined bursts

    ////////////// vci_multi_tty
    std::vector<std::string> vect_names;
    for (size_t id = 0; id < NB_TTY_CHANNELS; id++)
    {
        std::ostringstream term_name;
        term_name <<  "ext_" << id;
        vect_names.push_back(term_name.str().c_str());
    }

    VciMultiTty<vci_param_int>*
    mtty = new VciMultiTty<vci_param_int>(
                "mtty",
                IntTab(cluster_io, MTTY_TGTID),
                maptabd,
                vect_names );

    ///////////// vci_iopic
    VciIopic<vci_param_int>*
    iopic = new VciIopic<vci_param_int>(
                "iopic",
                maptabd,
                IntTab(cluster_io, IOPI_SRCID),
                IntTab(cluster_io, IOPI_TGTID),
                32 );

    ////////////// vci_dspin wrappers
    VciDspinTargetWrapper<vci_param_int, dspin_cmd_width, dspin_rsp_width>*
    wt_iobus = new VciDspinTargetWrapper<vci_param_int, dspin_cmd_width, dspin_rsp_width>(
                "wt_iobus",
                vci_srcid_width );

    VciDspinInitiatorWrapper<vci_param_int, dspin_cmd_width, dspin_rsp_width>*
    wi_iobus = new VciDspinInitiatorWrapper<vci_param_int, dspin_cmd_width, dspin_rsp_width>(
                "wi_iobus",
                vci_srcid_width );

    ///////////////////////////////////////////////////////////////
    //     IObus  Net-list
    ///////////////////////////////////////////////////////////////

    // iobus
    iobus->p_clk                       (signal_clk);
    iobus->p_resetn                    (signal_resetn);

    iobus->p_target_to_up              (signal_vci_cmd_from_noc);
    iobus->p_initiator_to_up           (signal_vci_cmd_to_noc);

    iobus->p_to_target[MEMC_TGTID]     (signal_vci_tgt_memc);
    iobus->p_to_target[XICU_TGTID]     (signal_vci_tgt_xicu);
    iobus->p_to_target[MTTY_TGTID]     (signal_vci_tgt_mtty);
    iobus->p_to_target[FBUF_TGTID]     (signal_vci_tgt_fbuf);
    iobus->p_to_target[MNIC_TGTID]     (signal_vci_tgt_mnic);
    iobus->p_to_target[DISK_TGTID]     (signal_vci_tgt_disk);
    iobus->p_to_target[CDMA_TGTID]     (signal_vci_tgt_cdma);
    iobus->p_to_target[IOPI_TGTID]     (signal_vci_tgt_iopi);

    for( size_t p=0 ; p<NB_PROCS_MAX ; p++ )
    {
        iobus->p_to_initiator[p]       (signal_vci_ini_proc[p]);
    }
    iobus->p_to_initiator[DISK_SRCID]  (signal_vci_ini_disk);
    iobus->p_to_initiator[CDMA_SRCID]  (signal_vci_ini_cdma);
    iobus->p_to_initiator[IOPI_SRCID]  (signal_vci_ini_iopi);

    std::cout << "  - IOBUS connected" << std::endl;

    // disk
    disk->p_clk                        (signal_clk);
    disk->p_resetn                     (signal_resetn);
    disk->p_vci_target                 (signal_vci_tgt_disk);
    disk->p_vci_initiator              (signal_vci_ini_disk);
#if USE_IOC_HBA
    disk->p_channel_irq[0]             (signal_irq_disk);
#else
    disk->p_irq                        (signal_irq_disk);
#endif

    std::cout << "  - DISK connected" << std::endl;

    // frame_buffer
    fbuf->p_clk                        (signal_clk);
    fbuf->p_resetn                     (signal_resetn);
    fbuf->p_vci                        (signal_vci_tgt_fbuf);

    std::cout << "  - FBUF connected" << std::endl;

    // multi_nic
    mnic->p_clk                        (signal_clk);
    mnic->p_resetn                     (signal_resetn);
    mnic->p_vci                        (signal_vci_tgt_mnic);
    for ( size_t i=0 ; i<NB_NIC_CHANNELS ; i++ )
    {
         mnic->p_rx_irq[i]             (signal_irq_mnic_rx[i]);
         mnic->p_tx_irq[i]             (signal_irq_mnic_tx[i]);
    }

    std::cout << "  - MNIC connected" << std::endl;

    // chbuf_dma
    cdma->p_clk                        (signal_clk);
    cdma->p_resetn                     (signal_resetn);
    cdma->p_vci_target                 (signal_vci_tgt_cdma);
    cdma->p_vci_initiator              (signal_vci_ini_cdma);
    for ( size_t i=0 ; i<NB_CMA_CHANNELS ; i++)
    {
        cdma->p_irq[i]                 (signal_irq_cdma[i]);
    }

    std::cout << "  - CDMA connected" << std::endl;

    // multi_tty
    mtty->p_clk                        (signal_clk);
    mtty->p_resetn                     (signal_resetn);
    mtty->p_vci                        (signal_vci_tgt_mtty);
    for ( size_t i=0 ; i<NB_TTY_CHANNELS ; i++ )
    {
        mtty->p_irq[i]              	(signal_irq_mtty_rx[i]);
    }

    std::cout << "  - MTTY connected" << std::endl;

    // iopic
    // NB_NIC_CHANNELS <= 2
    // NB_CMA_CHANNELS <= 4
    // NB_TTY_CHANNELS <= 16
    iopic->p_clk                       (signal_clk);
    iopic->p_resetn                    (signal_resetn);
    iopic->p_vci_target                (signal_vci_tgt_iopi);
    iopic->p_vci_initiator             (signal_vci_ini_iopi);
    for ( size_t i=0 ; i<32 ; i++)
    {
       if     (i < NB_NIC_CHANNELS)    iopic->p_hwi[i] (signal_irq_mnic_rx[i]);
       else if(i < 2 )                 iopic->p_hwi[i] (signal_irq_false);
       else if(i < 2+NB_NIC_CHANNELS)  iopic->p_hwi[i] (signal_irq_mnic_tx[i-2]);
       else if(i < 4 )                 iopic->p_hwi[i] (signal_irq_false);
       else if(i < 4+NB_CMA_CHANNELS)  iopic->p_hwi[i] (signal_irq_cdma[i-4]);
       else if(i < 8)                  iopic->p_hwi[i] (signal_irq_false);
       else if(i == 8)                 iopic->p_hwi[i] (signal_irq_disk);
       else if(i < 16)                 iopic->p_hwi[i] (signal_irq_false);
       else if(i < 16+NB_TTY_CHANNELS) iopic->p_hwi[i] (signal_irq_mtty_rx[i-16]);
       else                            iopic->p_hwi[i] (signal_irq_false);
    }

    std::cout << "  - IOPIC connected" << std::endl;

    // vci/dspin wrappers
    wi_iobus->p_clk                    (signal_clk);
    wi_iobus->p_resetn                 (signal_resetn);
    wi_iobus->p_vci                    (signal_vci_cmd_to_noc);
    wi_iobus->p_dspin_cmd              (signal_dspin_bound_cmd_in[XMAX-1][YMAX-1][NORTH]);
    wi_iobus->p_dspin_rsp              (signal_dspin_bound_rsp_out[XMAX-1][YMAX-1][NORTH]);

    // vci/dspin wrappers
    wt_iobus->p_clk                    (signal_clk);
    wt_iobus->p_resetn                 (signal_resetn);
    wt_iobus->p_vci                    (signal_vci_cmd_from_noc);
    wt_iobus->p_dspin_cmd              (signal_dspin_bound_cmd_out[XMAX-1][YMAX-1][NORTH]);
    wt_iobus->p_dspin_rsp              (signal_dspin_bound_rsp_in[XMAX-1][YMAX-1][NORTH]);

#endif  // USE_PIC

    // Clock & RESET for clusters
    for (size_t x = 0; x < (XMAX); x++)
    {
        for (size_t y = 0; y < (YMAX); y++)
        {
            clusters[x][y]->p_clk                    (signal_clk);
            clusters[x][y]->p_resetn                 (signal_resetn);
        }
    }

    // Inter Clusters horizontal connections
    if (XMAX > 1)
    {
        for (size_t x = 0; x < (XMAX-1); x++)
        {
            for (size_t y = 0; y < (YMAX); y++)
            {
                clusters[x][y]->p_cmd_out[EAST]      (signal_dspin_h_cmd_inc[x][y]);
                clusters[x+1][y]->p_cmd_in[WEST]     (signal_dspin_h_cmd_inc[x][y]);
                clusters[x][y]->p_cmd_in[EAST]       (signal_dspin_h_cmd_dec[x][y]);
                clusters[x+1][y]->p_cmd_out[WEST]    (signal_dspin_h_cmd_dec[x][y]);

                clusters[x][y]->p_rsp_out[EAST]      (signal_dspin_h_rsp_inc[x][y]);
                clusters[x+1][y]->p_rsp_in[WEST]     (signal_dspin_h_rsp_inc[x][y]);
                clusters[x][y]->p_rsp_in[EAST]       (signal_dspin_h_rsp_dec[x][y]);
                clusters[x+1][y]->p_rsp_out[WEST]    (signal_dspin_h_rsp_dec[x][y]);

                clusters[x][y]->p_m2p_out[EAST]      (signal_dspin_h_m2p_inc[x][y]);
                clusters[x+1][y]->p_m2p_in[WEST]     (signal_dspin_h_m2p_inc[x][y]);
                clusters[x][y]->p_m2p_in[EAST]       (signal_dspin_h_m2p_dec[x][y]);
                clusters[x+1][y]->p_m2p_out[WEST]    (signal_dspin_h_m2p_dec[x][y]);

                clusters[x][y]->p_p2m_out[EAST]      (signal_dspin_h_p2m_inc[x][y]);
                clusters[x+1][y]->p_p2m_in[WEST]     (signal_dspin_h_p2m_inc[x][y]);
                clusters[x][y]->p_p2m_in[EAST]       (signal_dspin_h_p2m_dec[x][y]);
                clusters[x+1][y]->p_p2m_out[WEST]    (signal_dspin_h_p2m_dec[x][y]);

                clusters[x][y]->p_cla_out[EAST]      (signal_dspin_h_cla_inc[x][y]);
                clusters[x+1][y]->p_cla_in[WEST]     (signal_dspin_h_cla_inc[x][y]);
                clusters[x][y]->p_cla_in[EAST]       (signal_dspin_h_cla_dec[x][y]);
                clusters[x+1][y]->p_cla_out[WEST]    (signal_dspin_h_cla_dec[x][y]);
            }
        }
    }
    std::cout << std::endl << "Horizontal connections done" << std::endl;

    // Inter Clusters vertical connections
    if (YMAX > 1)
    {
        for (size_t y = 0; y < (YMAX-1); y++)
        {
            for (size_t x = 0; x < XMAX; x++)
            {
                clusters[x][y]->p_cmd_out[NORTH]     (signal_dspin_v_cmd_inc[x][y]);
                clusters[x][y+1]->p_cmd_in[SOUTH]    (signal_dspin_v_cmd_inc[x][y]);
                clusters[x][y]->p_cmd_in[NORTH]      (signal_dspin_v_cmd_dec[x][y]);
                clusters[x][y+1]->p_cmd_out[SOUTH]   (signal_dspin_v_cmd_dec[x][y]);

                clusters[x][y]->p_rsp_out[NORTH]     (signal_dspin_v_rsp_inc[x][y]);
                clusters[x][y+1]->p_rsp_in[SOUTH]    (signal_dspin_v_rsp_inc[x][y]);
                clusters[x][y]->p_rsp_in[NORTH]      (signal_dspin_v_rsp_dec[x][y]);
                clusters[x][y+1]->p_rsp_out[SOUTH]   (signal_dspin_v_rsp_dec[x][y]);

                clusters[x][y]->p_m2p_out[NORTH]     (signal_dspin_v_m2p_inc[x][y]);
                clusters[x][y+1]->p_m2p_in[SOUTH]    (signal_dspin_v_m2p_inc[x][y]);
                clusters[x][y]->p_m2p_in[NORTH]      (signal_dspin_v_m2p_dec[x][y]);
                clusters[x][y+1]->p_m2p_out[SOUTH]   (signal_dspin_v_m2p_dec[x][y]);

                clusters[x][y]->p_p2m_out[NORTH]     (signal_dspin_v_p2m_inc[x][y]);
                clusters[x][y+1]->p_p2m_in[SOUTH]    (signal_dspin_v_p2m_inc[x][y]);
                clusters[x][y]->p_p2m_in[NORTH]      (signal_dspin_v_p2m_dec[x][y]);
                clusters[x][y+1]->p_p2m_out[SOUTH]   (signal_dspin_v_p2m_dec[x][y]);

                clusters[x][y]->p_cla_out[NORTH]     (signal_dspin_v_cla_inc[x][y]);
                clusters[x][y+1]->p_cla_in[SOUTH]    (signal_dspin_v_cla_inc[x][y]);
                clusters[x][y]->p_cla_in[NORTH]      (signal_dspin_v_cla_dec[x][y]);
                clusters[x][y+1]->p_cla_out[SOUTH]   (signal_dspin_v_cla_dec[x][y]);
            }
        }
    }
    std::cout << std::endl << "Vertical connections done" << std::endl;

    // East & West boundary cluster connections
    for (size_t y = 0; y < (YMAX); y++)
    {
        clusters[0][y]->p_cmd_in[WEST]           (signal_dspin_bound_cmd_in[0][y][WEST]);
        clusters[0][y]->p_cmd_out[WEST]          (signal_dspin_bound_cmd_out[0][y][WEST]);
        clusters[XMAX-1][y]->p_cmd_in[EAST]    (signal_dspin_bound_cmd_in[XMAX-1][y][EAST]);
        clusters[XMAX-1][y]->p_cmd_out[EAST]   (signal_dspin_bound_cmd_out[XMAX-1][y][EAST]);

        clusters[0][y]->p_rsp_in[WEST]           (signal_dspin_bound_rsp_in[0][y][WEST]);
        clusters[0][y]->p_rsp_out[WEST]          (signal_dspin_bound_rsp_out[0][y][WEST]);
        clusters[XMAX-1][y]->p_rsp_in[EAST]    (signal_dspin_bound_rsp_in[XMAX-1][y][EAST]);
        clusters[XMAX-1][y]->p_rsp_out[EAST]   (signal_dspin_bound_rsp_out[XMAX-1][y][EAST]);

        clusters[0][y]->p_m2p_in[WEST]           (signal_dspin_bound_m2p_in[0][y][WEST]);
        clusters[0][y]->p_m2p_out[WEST]          (signal_dspin_bound_m2p_out[0][y][WEST]);
        clusters[XMAX-1][y]->p_m2p_in[EAST]    (signal_dspin_bound_m2p_in[XMAX-1][y][EAST]);
        clusters[XMAX-1][y]->p_m2p_out[EAST]   (signal_dspin_bound_m2p_out[XMAX-1][y][EAST]);

        clusters[0][y]->p_p2m_in[WEST]           (signal_dspin_bound_p2m_in[0][y][WEST]);
        clusters[0][y]->p_p2m_out[WEST]          (signal_dspin_bound_p2m_out[0][y][WEST]);
        clusters[XMAX-1][y]->p_p2m_in[EAST]    (signal_dspin_bound_p2m_in[XMAX-1][y][EAST]);
        clusters[XMAX-1][y]->p_p2m_out[EAST]   (signal_dspin_bound_p2m_out[XMAX-1][y][EAST]);

        clusters[0][y]->p_cla_in[WEST]           (signal_dspin_bound_cla_in[0][y][WEST]);
        clusters[0][y]->p_cla_out[WEST]          (signal_dspin_bound_cla_out[0][y][WEST]);
        clusters[XMAX-1][y]->p_cla_in[EAST]    (signal_dspin_bound_cla_in[XMAX-1][y][EAST]);
        clusters[XMAX-1][y]->p_cla_out[EAST]   (signal_dspin_bound_cla_out[XMAX-1][y][EAST]);
    }

    std::cout << std::endl << "West & East boundaries connections done" << std::endl;

    // North & South boundary clusters connections
    for (size_t x = 0; x < XMAX; x++)
    {
        clusters[x][0]->p_cmd_in[SOUTH]          (signal_dspin_bound_cmd_in[x][0][SOUTH]);
        clusters[x][0]->p_cmd_out[SOUTH]         (signal_dspin_bound_cmd_out[x][0][SOUTH]);
        clusters[x][YMAX-1]->p_cmd_in[NORTH]   (signal_dspin_bound_cmd_in[x][YMAX-1][NORTH]);
        clusters[x][YMAX-1]->p_cmd_out[NORTH]  (signal_dspin_bound_cmd_out[x][YMAX-1][NORTH]);

        clusters[x][0]->p_rsp_in[SOUTH]          (signal_dspin_bound_rsp_in[x][0][SOUTH]);
        clusters[x][0]->p_rsp_out[SOUTH]         (signal_dspin_bound_rsp_out[x][0][SOUTH]);
        clusters[x][YMAX-1]->p_rsp_in[NORTH]   (signal_dspin_bound_rsp_in[x][YMAX-1][NORTH]);
        clusters[x][YMAX-1]->p_rsp_out[NORTH]  (signal_dspin_bound_rsp_out[x][YMAX-1][NORTH]);

        clusters[x][0]->p_m2p_in[SOUTH]          (signal_dspin_bound_m2p_in[x][0][SOUTH]);
        clusters[x][0]->p_m2p_out[SOUTH]         (signal_dspin_bound_m2p_out[x][0][SOUTH]);
        clusters[x][YMAX-1]->p_m2p_in[NORTH]   (signal_dspin_bound_m2p_in[x][YMAX-1][NORTH]);
        clusters[x][YMAX-1]->p_m2p_out[NORTH]  (signal_dspin_bound_m2p_out[x][YMAX-1][NORTH]);

        clusters[x][0]->p_p2m_in[SOUTH]          (signal_dspin_bound_p2m_in[x][0][SOUTH]);
        clusters[x][0]->p_p2m_out[SOUTH]         (signal_dspin_bound_p2m_out[x][0][SOUTH]);
        clusters[x][YMAX-1]->p_p2m_in[NORTH]   (signal_dspin_bound_p2m_in[x][YMAX-1][NORTH]);
        clusters[x][YMAX-1]->p_p2m_out[NORTH]  (signal_dspin_bound_p2m_out[x][YMAX-1][NORTH]);

        clusters[x][0]->p_cla_in[SOUTH]          (signal_dspin_bound_cla_in[x][0][SOUTH]);
        clusters[x][0]->p_cla_out[SOUTH]         (signal_dspin_bound_cla_out[x][0][SOUTH]);
        clusters[x][YMAX-1]->p_cla_in[NORTH]   (signal_dspin_bound_cla_in[x][YMAX-1][NORTH]);
        clusters[x][YMAX-1]->p_cla_out[NORTH]  (signal_dspin_bound_cla_out[x][YMAX-1][NORTH]);
    }

    std::cout << std::endl << "North & South boundaries connections done" << std::endl;

    std::cout << std::endl;

    ////////////////////////////////////////////////////////
    //   Simulation
    ///////////////////////////////////////////////////////

    sc_start(sc_core::sc_time(0, SC_NS));
    signal_resetn    = false;
    signal_irq_false = false;

    // set network boundaries signals default values
    // for all boundary clusters but the IO cluster
    for (size_t x = 0; x < XMAX ; x++)
    {
        for (size_t y = 0; y < YMAX ; y++)
        {
            for (size_t face = 0; face < 4; face++)
            {
                if ( (x != XMAX-1) or (y != YMAX-1) or (face != NORTH) )
                {
                    signal_dspin_bound_cmd_in [x][y][face].write = false;
                    signal_dspin_bound_cmd_in [x][y][face].read  = true;
                    signal_dspin_bound_cmd_out[x][y][face].write = false;
                    signal_dspin_bound_cmd_out[x][y][face].read  = true;

                    signal_dspin_bound_rsp_in [x][y][face].write = false;
                    signal_dspin_bound_rsp_in [x][y][face].read  = true;
                    signal_dspin_bound_rsp_out[x][y][face].write = false;
                    signal_dspin_bound_rsp_out[x][y][face].read  = true;
                }

                signal_dspin_bound_m2p_in [x][y][face].write = false;
                signal_dspin_bound_m2p_in [x][y][face].read  = true;
                signal_dspin_bound_m2p_out[x][y][face].write = false;
                signal_dspin_bound_m2p_out[x][y][face].read  = true;

                signal_dspin_bound_p2m_in [x][y][face].write = false;
                signal_dspin_bound_p2m_in [x][y][face].read  = true;
                signal_dspin_bound_p2m_out[x][y][face].write = false;
                signal_dspin_bound_p2m_out[x][y][face].read  = true;

                signal_dspin_bound_cla_in [x][y][face].write = false;
                signal_dspin_bound_cla_in [x][y][face].read  = true;
                signal_dspin_bound_cla_out[x][y][face].write = false;
                signal_dspin_bound_cla_out[x][y][face].read  = true;
            }
        }
    }

#if USE_PIC == 0
    signal_dspin_bound_cmd_in[XMAX-1][YMAX-1][NORTH].write = false;
    signal_dspin_bound_rsp_out[XMAX-1][YMAX-1][NORTH].read = true;
    signal_dspin_bound_cmd_out[XMAX-1][YMAX-1][NORTH].read = true;
    signal_dspin_bound_rsp_in[XMAX-1][YMAX-1][NORTH].write = false;
#endif

    // set default values for VCI signals connected to unused ports on iobus
    signal_vci_tgt_memc.rspval = false;
    signal_vci_tgt_xicu.rspval = false;
    for ( size_t p = 0 ; p < NB_PROCS_MAX ; p++ ) signal_vci_ini_proc[p].cmdval = false;

    sc_start(sc_core::sc_time(1, SC_NS));
    signal_resetn = true;

    if (gettimeofday(&t1, NULL) != 0)
    {
        perror("gettimeofday");
        return EXIT_FAILURE;
    }

    // simulation loop
    for (uint64_t n = 1; n < ncycles && !stop_called; n+=SIM_STEP)
    {
        // Monitor a specific address for L1 cache
        // clusters[0][0]->proc[0]->cache_monitor(0x110002C078ULL);

        // Monitor a specific address for L2 cache
        // clusters[0][0]->memc->cache_monitor( 0x0000201E00ULL );

        // Monitor a specific address for one XRAM
        // clusters[0][0]->xram->start_monitor( 0x0000201E00ULL , 64);

        // stats display
        if( ((n-1) % STATS_STEP) == 0)
        {
            if ( n > SIM_STEP+1 )
            {
               if (gettimeofday(&t2, NULL) != 0)
               {
                   perror("gettimeofday");
                   return EXIT_FAILURE;
               }

               ms1 = (uint64_t) t1.tv_sec * 1000ULL + (uint64_t) t1.tv_usec / 1000;
               ms2 = (uint64_t) t2.tv_sec * 1000ULL + (uint64_t) t2.tv_usec / 1000;
               std::cerr << "platform clock frequency "
                         << (double) STATS_STEP / (double) (ms2 - ms1) << "Khz" << std::endl;

               if (gettimeofday(&t1, NULL) != 0)
               {
                   perror("gettimeofday");
                   return EXIT_FAILURE;
               }
            }
        }

        // trace display
        if ( trace_ok and (n > trace_from) )
        {
            std::cout << "****************** cycle " << std::dec << n ;
            std::cout << " ********************************************" << std::endl;

            size_t l = 0;
            size_t x = 0;
            size_t y = 0;

            if ( trace_proc_ok )
            {
                l = trace_proc_id & ((1<<P_WIDTH)-1) ;
                x = (trace_proc_id >> P_WIDTH) >> Y_WIDTH ;
                y = (trace_proc_id >> P_WIDTH) & ((1<<Y_WIDTH) - 1);

                std::ostringstream proc_signame;
                proc_signame << "[SIG]PROC_" << x << "_" << y << "_" << l ;
                clusters[x][y]->proc[l]->print_trace(1);
                clusters[x][y]->signal_vci_ini_proc[l].print_trace(proc_signame.str());

                std::ostringstream xicu_signame;
                xicu_signame << "[SIG]XICU_" << x << "_" << y ;
                clusters[x][y]->xicu->print_trace(0);
                clusters[x][y]->signal_vci_tgt_xicu.print_trace(xicu_signame.str());
                
                if ( clusters[x][y]->signal_proc_irq[0] ) 
                   std::cout << "### IRQ_PROC_" << x << "_" << y << "_0" << std::endl;
                if ( clusters[x][y]->signal_proc_irq[4] ) 
                   std::cout << "### IRQ_PROC_" << x << "_" << y << "_1" << std::endl;
                if ( clusters[x][y]->signal_proc_irq[8] ) 
                   std::cout << "### IRQ_PROC_" << x << "_" << y << "_2" << std::endl;
                if ( clusters[x][y]->signal_proc_irq[12] ) 
                   std::cout << "### IRQ_PROC_" << x << "_" << y << "_3" << std::endl;
            }

            if ( trace_memc_ok )
            {
                x = trace_memc_id >> Y_WIDTH;
                y = trace_memc_id & ((1<<Y_WIDTH) - 1);

                std::ostringstream smemc;
                smemc << "[SIG]MEMC_" << x << "_" << y;
                std::ostringstream sxram;
                sxram << "[SIG]XRAM_" << x << "_" << y;

                clusters[x][y]->memc->print_trace();
                clusters[x][y]->signal_vci_tgt_memc.print_trace(smemc.str());
                clusters[x][y]->signal_vci_xram.print_trace(sxram.str());
            }

            // trace coherence signals
            // clusters[0][0]->signal_dspin_m2p_proc[0].print_trace("[CC_M2P_0_0]");
            // clusters[0][1]->signal_dspin_m2p_proc[0].print_trace("[CC_M2P_0_1]");
            // clusters[1][0]->signal_dspin_m2p_proc[0].print_trace("[CC_M2P_1_0]");
            // clusters[1][1]->signal_dspin_m2p_proc[0].print_trace("[CC_M2P_1_1]");

            // clusters[0][0]->signal_dspin_p2m_proc[0].print_trace("[CC_P2M_0_0]");
            // clusters[0][1]->signal_dspin_p2m_proc[0].print_trace("[CC_P2M_0_1]");
            // clusters[1][0]->signal_dspin_p2m_proc[0].print_trace("[CC_P2M_1_0]");
            // clusters[1][1]->signal_dspin_p2m_proc[0].print_trace("[CC_P2M_1_1]");

            // trace xbar(s) m2p
            // clusters[0][0]->xbar_m2p->print_trace();
            // clusters[1][0]->xbar_m2p->print_trace();
            // clusters[0][1]->xbar_m2p->print_trace();
            // clusters[1][1]->xbar_m2p->print_trace();

            // trace router(s) m2p
            // clusters[0][0]->router_m2p->print_trace();
            // clusters[1][0]->router_m2p->print_trace();
            // clusters[0][1]->router_m2p->print_trace();
            // clusters[1][1]->router_m2p->print_trace();

#if USE_PIC
            // trace external ioc
            disk->print_trace();
            signal_vci_tgt_disk.print_trace("[SIG]DISK_TGT");
            signal_vci_ini_disk.print_trace("[SIG]DISK_INI");

            // trace external iopic
            iopic->print_trace();
            signal_vci_tgt_iopi.print_trace("[SIG]IOPI_TGT");
            signal_vci_ini_iopi.print_trace("[SIG]IOPI_INI");

            // trace external interrupts
            if (signal_irq_disk)   std::cout << "### IRQ_DISK" << std::endl;
#else
            clusters[0][0]->disk->print_trace();
            clusters[0][0]->signal_vci_tgt_disk.print_trace("[SIG]DISK_0_0");
            clusters[0][0]->signal_vci_ini_disk.print_trace("[SIG]DISK_0_0");
#endif

        }  // end trace

        sc_start(sc_core::sc_time(SIM_STEP, SC_NS));
    }
    // Free memory
    for (size_t i = 0 ; i  < (XMAX * YMAX) ; i++)
    {
        size_t x = i / (YMAX);
        size_t y = i % (YMAX);
        delete clusters[x][y];
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
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3
