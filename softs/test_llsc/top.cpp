/////////////////////////////////////////////////////////////////////////
// File: top.cpp 
// Author: Alain Greiner 
// Copyright: UPMC/LIP6
// Date : may 2013
// This program is released under the GNU public license
/////////////////////////////////////////////////////////////////////////
// This file define a generic TSAR architecture.
// The physical address space is 40 bits.
//
// The number of clusters cannot be larger than 256.
// The number of processors per cluster cannot be larger than 8.
// 
// - It uses four dspin_local_crossbar per cluster as local interconnect 
// - It uses two virtual_dspin routers per cluster as global interconnect
// - It uses the vci_cc_vcache_wrapper 
// - It uses the vci_mem_cache
// - It contains one vci_xicu per cluster.
// - It contains one vci_multi_dma per cluster.
// - It contains one vci_simple_ram per cluster to model the L3 cache.
//
// The communication between the MemCache and the Xram is 64 bits.
//
// All clusters are identical, but the cluster 0 (called io_cluster), 
// contains 6 extra components:
// - the boot rom (BROM)
// - the disk controller (BDEV)
// - the multi-channel network controller (MNIC)
// - the multi-channel chained buffer dma controller (CDMA)
// - the multi-channel tty controller (MTTY)
// - the frame buffer controller (FBUF)
//
// It is build with one single component implementing a cluster,
// defined in files tsar_xbar_cluster.* (with * = cpp, h, sd)
//
// The IRQs are connected to XICUs as follow:
// - The IRQ_IN[0] to IRQ_IN[7] ports are not used in all clusters.
// - The DMA IRQs are connected to IRQ_IN[8] to IRQ_IN[15] in all clusters.
// - The TTY IRQs are connected to IRQ_IN[16] to IRQ_IN[30] in I/O cluster.
// - The BDEV IRQ is connected to IRQ_IN[31] in I/O cluster.
// 
// Some hardware parameters are used when compiling the OS, and are used 
// by this top.cpp file. They must be defined in the hard_config.h file :
// - CLUSTER_X        : number of clusters in a row (power of 2)
// - CLUSTER_Y        : number of clusters in a column (power of 2)
// - CLUSTER_SIZE     : size of the segment allocated to a cluster
// - NB_PROCS_MAX     : number of processors per cluster (power of 2)
// - NB_DMA_CHANNELS  : number of DMA channels per cluster (< 9)
// - NB_TTY_CHANNELS  : number of TTY channels in I/O cluster (< 16)
// - NB_NIC_CHANNELS  : number of NIC channels in I/O cluster (< 9)
// 
// Some other hardware parameters are not used when compiling the OS,
// and can be directly defined in this top.cpp file:
// - XRAM_LATENCY     : external ram latency 
// - MEMC_WAYS        : L2 cache number of ways
// - MEMC_SETS        : L2 cache number of sets
// - L1_IWAYS     
// - L1_ISETS    
// - L1_DWAYS   
// - L1_DSETS  
// - FBUF_X_SIZE      : width of frame buffer (pixels)
// - FBUF_Y_SIZE      : heigth of frame buffer (lines)
// - BDEV_SECTOR_SIZE : block size for block drvice
// - BDEV_IMAGE_NAME  : file pathname for block device 
// - NIC_RX_NAME      : file pathname for NIC received packets
// - NIC_TX_NAME      : file pathname for NIC transmited packets
// - NIC_TIMEOUT      : max number of cycles before closing a container
/////////////////////////////////////////////////////////////////////////
// General policy for 40 bits physical address decoding:
// All physical segments base addresses are multiple of 1 Mbytes
// (=> the 24 LSB bits = 0, and the 16 MSB bits define the target) 
// The (x_width + y_width) MSB bits (left aligned) define
// the cluster index, and the LADR bits define the local index:
//      | X_ID  | Y_ID  |---| LADR |     OFFSET          |
//      |x_width|y_width|---|  8   |       24            |
/////////////////////////////////////////////////////////////////////////
// General policy for 14 bits SRCID decoding:
// Each component is identified by (x_id, y_id, l_id) tuple.
//      | X_ID  | Y_ID  |---| L_ID |
//      |x_width|y_width|---|  6   |
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
#include "tsar_xbar_cluster.h"
#include "alloc_elems.h"

///////////////////////////////////////////////////
//      OS
///////////////////////////////////////////////////

//#define USE_ALMOS
#define USE_GIET 

#ifdef USE_ALMOS
#ifdef USE_GIET
#error "Can't use Two different OS"
#endif
#endif

#ifndef USE_ALMOS
#ifndef USE_GIET
#error "You need to specify one OS"
#endif
#endif

///////////////////////////////////////////////////
//               Parallelisation
///////////////////////////////////////////////////
#define USE_OPENMP 0

#if USE_OPENMP
#include <omp.h>
#endif

//  cluster index (computed from x,y coordinates)
#define cluster(x,y)   (y + YMAX * x)

#define min(x, y) (x < y ? x : y)

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

#ifdef USE_ALMOS
#define vci_address_width     32
#endif
#ifdef USE_GIET
#define vci_address_width     40
#endif
#define vci_plen_width        8
#define vci_rerror_width      1
#define vci_clen_width        1
#define vci_rflag_width       1
#define vci_srcid_width       14
#define vci_pktid_width       4
#define vci_trdid_width       4
#define vci_wrplen_width      1

////////////////////////////////////////////////////////////
//    Main Hardware Parameters values         
//////////////////////i/////////////////////////////////////

#ifdef USE_ALMOS
#include "almos/hard_config.h"
#define PREFIX_OS "almos/"
#endif
#ifdef USE_GIET
#include "scripts/soft/hard_config.h"
#define PREFIX_OS "giet_vm/"
#endif

////////////////////////////////////////////////////////////
//    Secondary Hardware Parameters         
//////////////////////i/////////////////////////////////////

#define XMAX                  CLUSTER_X
#define YMAX                  CLUSTER_Y

#define XRAM_LATENCY          0

#define MEMC_WAYS             16
#define MEMC_SETS             256

#define L1_IWAYS              4
#define L1_ISETS              64

#define L1_DWAYS              4
#define L1_DSETS              64

#ifdef USE_ALMOS
#define FBUF_X_SIZE           512
#define FBUF_Y_SIZE           512
#endif
#ifdef USE_GIET
#define FBUF_X_SIZE           128
#define FBUF_Y_SIZE           128
#endif

#ifdef USE_GIET
#define BDEV_SECTOR_SIZE      512
#define BDEV_IMAGE_NAME       PREFIX_OS"display/images.raw"
#endif
#ifdef USE_ALMOS
#define BDEV_SECTOR_SIZE      4096
#define BDEV_IMAGE_NAME       PREFIX_OS"hdd-img.bin"
#endif

#define NIC_RX_NAME           PREFIX_OS"nic/rx_packets.txt"
#define NIC_TX_NAME           PREFIX_OS"nic/tx_packets.txt"
#define NIC_TIMEOUT           10000

#define NORTH                 0
#define SOUTH                 1
#define EAST                  2
#define WEST                  3

////////////////////////////////////////////////////////////
//    Software to be loaded in ROM & RAM         
//////////////////////i/////////////////////////////////////

#ifdef USE_ALMOS
#define soft_name       PREFIX_OS"bootloader.bin",\
                        PREFIX_OS"kernel-soclib.bin@0xbfc10000:D",\
                        PREFIX_OS"arch-info.bib@0xBFC08000:D"
#endif
#ifdef USE_GIET
#define soft_pathname   "scripts/soft/soft.elf"
#endif

////////////////////////////////////////////////////////////
//     DEBUG Parameters default values         
//////////////////////i/////////////////////////////////////

#define MAX_FROZEN_CYCLES     100000000

/////////////////////////////////////////////////////////
//    Physical segments definition
/////////////////////////////////////////////////////////
// There is 3 segments replicated in all clusters
// and 5 specific segments in the "IO" cluster 
// (containing address 0xBF000000)
/////////////////////////////////////////////////////////

#ifdef USE_GIET
   // specific segments in "IO" cluster : absolute physical address
   #define BROM_BASE    0x00BFC00000
   #define BROM_SIZE    0x0000100000   // 1 Mbytes

   #define FBUF_BASE    0x00B2000000
   #define FBUF_SIZE    (FBUF_X_SIZE * FBUF_Y_SIZE * 2)

   #define BDEV_BASE    0x00B3000000
   #define BDEV_SIZE    0x0000001000   // 4 Kbytes

   #define MTTY_BASE    0x00B4000000
   #define MTTY_SIZE    0x0000001000   // 4 Kbytes

   #define MNIC_BASE    0x00B5000000
   #define MNIC_SIZE    0x0000080000   // 512 Kbytes (for 8 channels)

   #define CDMA_BASE    0x00B6000000
   #define CDMA_SIZE    0x0000004000 * NB_CMA_CHANNELS

   // replicated segments : address is incremented by a cluster offset
   //     offset  = cluster(x,y) << (address_width-x_width-y_width);

   #define MEMC_BASE    0x0000000000
   #define MEMC_SIZE    0x0010000000   // 256 Mbytes per cluster

   #define XICU_BASE    0x00B0000000
   #define XICU_SIZE    0x0000001000   // 4 Kbytes

   #define MDMA_BASE    0x00B1000000
   #define MDMA_SIZE    0x0000001000 * NB_DMA_CHANNELS  // 4 Kbytes per channel

   #define SIMH_BASE    0x00B7000000
   #define SIMH_SIZE    0x0000001000
#endif

#ifdef USE_ALMOS
   #define CLUSTER_INC  (0x80000000ULL / (XMAX * YMAX) * 2)

   #define MEMC_BASE    0x0000000000
   #define MEMC_SIZE    min(0x02000000, (0x80000000 / (XMAX * YMAX)))

   #define BROM_BASE    0x00BFC00000
   #define BROM_SIZE    0x0000100000   // 1 Mbytes

   #define XICU_BASE    (MEMC_SIZE)
   #define XICU_SIZE    0x0000001000   // 4 Kbytes

   #define MDMA_BASE    (XICU_BASE + XICU_SIZE)
   #define MDMA_SIZE    0x0000001000 * NB_DMA_CHANNELS  // 4 Kbytes per channel  

   #define BDEV_BASE    ((cluster_io_id * (CLUSTER_INC)) + MDMA_BASE + MDMA_SIZE)
   #define BDEV_SIZE    0x0000001000   // 4 Kbytes

   #define MTTY_BASE    (BDEV_BASE + BDEV_SIZE)
   #define MTTY_SIZE    0x0000001000   // 4 Kbytes

   #define FBUF_BASE    (MTTY_BASE + MTTY_SIZE)
   #define FBUF_SIZE    (FBUF_X_SIZE * FBUF_Y_SIZE * 2) // Should be 0x80000

   // Unused in almos
   #define MNIC_BASE    (FBUF_BASE + FBUF_SIZE)
   #define MNIC_SIZE    0x0000001000

   #define CDMA_BASE    (MNIC_BASE + MNIC_SIZE)
   #define CDMA_SIZE    0x0000004000 * NB_CMA_CHANNELS

#endif


////////////////////////////////////////////////////////////////////
//     TGTID definition in direct space
// For all components:  global TGTID = global SRCID = cluster_index
////////////////////////////////////////////////////////////////////

#define MEMC_TGTID      0
#define XICU_TGTID      1
#define MDMA_TGTID      2
#define MTTY_TGTID      3
#define FBUF_TGTID      4
#define BDEV_TGTID      5
#define MNIC_TGTID      6
#define BROM_TGTID      7
#define CDMA_TGTID      8
#define SIMH_TGTID      9

bool stop_called = false;

/////////////////////////////////
int _main(int argc, char *argv[])
{
   using namespace sc_core;
   using namespace soclib::caba;
   using namespace soclib::common;

#ifdef USE_GIET
   char     soft_name[256]   = soft_pathname;      // pathname to binary code
#endif
   uint64_t ncycles          = 0xFFFFFFFFFFFFFFFF; // simulated cycles
   char     disk_name[256]   = BDEV_IMAGE_NAME;    // pathname to the disk image
   char     nic_rx_name[256] = NIC_RX_NAME;        // pathname to the rx packets file
   char     nic_tx_name[256] = NIC_TX_NAME;        // pathname to the tx packets file
   ssize_t  threads_nr       = 1;                  // simulator's threads number
   bool     debug_ok         = false;              // trace activated
   size_t   debug_period     = 1;                  // trace period
   size_t   debug_memc_id    = 0;                  // index of memc to be traced 
   size_t   debug_proc_id    = 0;                  // index of proc to be traced
   uint32_t debug_from       = 0;                  // trace start cycle
   uint32_t frozen_cycles    = MAX_FROZEN_CYCLES;  // monitoring frozen processor
   size_t   cluster_io_id;                         // index of cluster containing IOs
   struct   timeval t1,t2;
   uint64_t ms1,ms2;

   ////////////// command line arguments //////////////////////
   if (argc > 1)
   {
      for (int n = 1; n < argc; n = n + 2)
      {
         if ((strcmp(argv[n], "-NCYCLES") == 0) && (n + 1 < argc))
         {
            ncycles = atoi(argv[n + 1]);
         }
         else if ((strcmp(argv[n], "-SOFT") == 0) && (n + 1 < argc))
         {
#ifdef USE_ALMOS
            assert( 0 && "Can't define almos soft name" );
#endif
#ifdef USE_GIET
            strcpy(soft_name, argv[n + 1]);
#endif
         }
         else if ((strcmp(argv[n],"-DISK") == 0) && (n + 1 < argc))
         {
            strcpy(disk_name, argv[n + 1]);
         }
         else if ((strcmp(argv[n],"-DEBUG") == 0) && (n + 1 < argc))
         {
            debug_ok = true;
            debug_from = atoi(argv[n + 1]);
         }
         else if ((strcmp(argv[n], "-MEMCID") == 0) && (n + 1 < argc))
         {
            debug_memc_id = atoi(argv[n + 1]);
            assert((debug_memc_id < (XMAX * YMAX)) && 
                   "debug_memc_id larger than XMAX * YMAX" );
         }
         else if ((strcmp(argv[n], "-PROCID") == 0) && (n + 1 < argc))
         {
            debug_proc_id = atoi(argv[n + 1]);
            assert((debug_proc_id < (XMAX * YMAX * NB_PROCS_MAX)) && 
                   "debug_proc_id larger than XMAX * YMAX * NB_PROCS");
         }
         else if ((strcmp(argv[n], "-THREADS") == 0) && ((n + 1) < argc))
         {
            threads_nr = atoi(argv[n + 1]);
            threads_nr = (threads_nr < 1) ? 1 : threads_nr;
         }
         else if ((strcmp(argv[n], "-FROZEN") == 0) && (n + 1 < argc))
         {
            frozen_cycles = atoi(argv[n + 1]);
         }
         else if ((strcmp(argv[n], "-PERIOD") == 0) && (n + 1 < argc))
         {
            debug_period = atoi(argv[n + 1]);
         }
         else
         {
            std::cout << "   Arguments are (key,value) couples." << std::endl;
            std::cout << "   The order is not important." << std::endl;
            std::cout << "   Accepted arguments are :" << std::endl << std::endl;
            std::cout << "     -SOFT pathname_for_embedded_soft" << std::endl;
            std::cout << "     -DISK pathname_for_disk_image" << std::endl;
            std::cout << "     -NCYCLES number_of_simulated_cycles" << std::endl;
            std::cout << "     -DEBUG debug_start_cycle" << std::endl;
            std::cout << "     -THREADS simulator's threads number" << std::endl;
            std::cout << "     -FROZEN max_number_of_lines" << std::endl;
            std::cout << "     -PERIOD number_of_cycles between trace" << std::endl;
            std::cout << "     -MEMCID index_memc_to_be_traced" << std::endl;
            std::cout << "     -PROCID index_proc_to_be_traced" << std::endl;
            exit(0);
         }
      }
   }

    // checking hardware parameters
    assert( ( (XMAX == 1) or (XMAX == 2) or (XMAX == 4) or
              (XMAX == 8) or (XMAX == 16) ) and
              "The XMAX parameter must be 1, 2, 4, 8 or 16" );

    assert( ( (YMAX == 1) or (YMAX == 2) or (YMAX == 4) or
              (YMAX == 8) or (YMAX == 16) ) and
              "The YMAX parameter must be 1, 2, 4, 8 or 16" );

    assert( ( (NB_PROCS_MAX == 1) or (NB_PROCS_MAX == 2) or
              (NB_PROCS_MAX == 4) or (NB_PROCS_MAX == 8) ) and
             "The NB_PROCS_MAX parameter must be 1, 2, 4 or 8" );

    assert( (NB_DMA_CHANNELS < 9) and
            "The NB_DMA_CHANNELS parameter must be smaller than 9" );

    assert( (NB_TTY_CHANNELS < 15) and
            "The NB_TTY_CHANNELS parameter must be smaller than 15" );

    assert( (NB_NIC_CHANNELS < 9) and
            "The NB_NIC_CHANNELS parameter must be smaller than 9" );

#ifdef USE_GIET
    assert( (vci_address_width == 40) and
            "VCI address width with the GIET must be 40 bits" );
#endif

#ifdef USE_ALMOS
    assert( (vci_address_width == 32) and
            "VCI address width with ALMOS must be 32 bits" );
#endif


    std::cout << std::endl;
    std::cout << " - XMAX             = " << XMAX << std::endl;
    std::cout << " - YMAX             = " << YMAX << std::endl;
    std::cout << " - NB_PROCS_MAX     = " << NB_PROCS_MAX <<  std::endl;
    std::cout << " - NB_DMA_CHANNELS  = " << NB_DMA_CHANNELS <<  std::endl;
    std::cout << " - NB_TTY_CHANNELS  = " << NB_TTY_CHANNELS <<  std::endl;
    std::cout << " - NB_NIC_CHANNELS  = " << NB_NIC_CHANNELS <<  std::endl;
    std::cout << " - MEMC_WAYS        = " << MEMC_WAYS << std::endl;
    std::cout << " - MEMC_SETS        = " << MEMC_SETS << std::endl;
    std::cout << " - RAM_LATENCY      = " << XRAM_LATENCY << std::endl;
    std::cout << " - MAX_FROZEN       = " << frozen_cycles << std::endl;
    std::cout << "[PROCS] " << NB_PROCS_MAX * XMAX * YMAX << std::endl;

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
   omp_set_num_threads(threads_nr);
   std::cerr << "Built with openmp version " << _OPENMP << std::endl;
#endif

   // Define parameters depending on mesh size
   size_t   x_width;
   size_t   y_width;

   if      (XMAX == 1) x_width = 0;
   else if (XMAX == 2) x_width = 1;
   else if (XMAX <= 4) x_width = 2;
   else if (XMAX <= 8) x_width = 3;
   else                x_width = 4;

   if      (YMAX == 1) y_width = 0;
   else if (YMAX == 2) y_width = 1;
   else if (YMAX <= 4) y_width = 2;
   else if (YMAX <= 8) y_width = 3;
   else                y_width = 4;


#ifdef USE_ALMOS
   cluster_io_id = 0xbfc00000 >> (vci_address_width - x_width - y_width); // index of cluster containing IOs
#else
   cluster_io_id = 0;
#endif

   /////////////////////
   //  Mapping Tables
   /////////////////////

   // internal network
   MappingTable maptabd(vci_address_width, 
                        IntTab(x_width + y_width, 20 - x_width - y_width), 
                        IntTab(x_width + y_width, vci_srcid_width - x_width - y_width), 
                        0x00FF800000);

   for (size_t x = 0; x < XMAX; x++)
   {
      for (size_t y = 0; y < YMAX; y++)
      {
         sc_uint<vci_address_width> offset;
         offset = (sc_uint<vci_address_width>)cluster(x,y) 
                   << (vci_address_width-x_width-y_width);

         std::ostringstream    si;
         si << "seg_xicu_" << x << "_" << y;
         maptabd.add(Segment(si.str(), XICU_BASE + offset, XICU_SIZE, 
                  IntTab(cluster(x,y),XICU_TGTID), false));

         std::ostringstream    sd;
         sd << "seg_mdma_" << x << "_" << y;
         maptabd.add(Segment(sd.str(), MDMA_BASE + offset, MDMA_SIZE, 
                  IntTab(cluster(x,y),MDMA_TGTID), false));

         std::ostringstream    sh;
         sh << "seg_memc_" << x << "_" << y;
         maptabd.add(Segment(sh.str(), MEMC_BASE + offset, MEMC_SIZE, 
                  IntTab(cluster(x,y),MEMC_TGTID), true));

         if ( cluster(x,y) == cluster_io_id )
         {
            maptabd.add(Segment("seg_mtty", MTTY_BASE, MTTY_SIZE, 
                        IntTab(cluster(x,y),MTTY_TGTID), false));
            maptabd.add(Segment("seg_fbuf", FBUF_BASE, FBUF_SIZE, 
                        IntTab(cluster(x,y),FBUF_TGTID), false));
            maptabd.add(Segment("seg_bdev", BDEV_BASE, BDEV_SIZE, 
                        IntTab(cluster(x,y),BDEV_TGTID), false));
            maptabd.add(Segment("seg_brom", BROM_BASE, BROM_SIZE, 
                        IntTab(cluster(x,y),BROM_TGTID), true));
            maptabd.add(Segment("seg_mnic", MNIC_BASE, MNIC_SIZE, 
                        IntTab(cluster(x,y),MNIC_TGTID), false));
            maptabd.add(Segment("seg_cdma", CDMA_BASE, CDMA_SIZE, 
                        IntTab(cluster(x,y),CDMA_TGTID), false));
            maptabd.add(Segment("seg_simh", SIMH_BASE, SIMH_SIZE, 
                        IntTab(cluster(x,y),SIMH_TGTID), false));
         }
      }
   }
   std::cout << maptabd << std::endl;

   // external network
   MappingTable maptabx(vci_address_width, 
                        IntTab(x_width+y_width), 
                        IntTab(x_width+y_width), 
                        0xFFFF000000ULL);

   for (size_t x = 0; x < XMAX; x++)
   {
      for (size_t y = 0; y < YMAX ; y++)
      { 

         sc_uint<vci_address_width> offset;
         offset = (sc_uint<vci_address_width>)cluster(x,y) 
                   << (vci_address_width-x_width-y_width);

         std::ostringstream sh;
         sh << "x_seg_memc_" << x << "_" << y;

         maptabx.add(Segment(sh.str(), MEMC_BASE + offset, 
                     MEMC_SIZE, IntTab(cluster(x,y)), false));
      }

   }
   std::cout << maptabx << std::endl;

   ////////////////////
   // Signals
   ///////////////////

   sc_clock           signal_clk("clk");
   sc_signal<bool>    signal_resetn("resetn");

   // Horizontal inter-clusters DSPIN signals
   DspinSignals<dspin_cmd_width>*** signal_dspin_h_cmd_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cmd_inc", XMAX-1, YMAX, 3);
   DspinSignals<dspin_cmd_width>*** signal_dspin_h_cmd_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cmd_dec", XMAX-1, YMAX, 3);
   DspinSignals<dspin_rsp_width>*** signal_dspin_h_rsp_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_rsp_inc", XMAX-1, YMAX, 2);
   DspinSignals<dspin_rsp_width>*** signal_dspin_h_rsp_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_rsp_dec", XMAX-1, YMAX, 2);

   // Vertical inter-clusters DSPIN signals
   DspinSignals<dspin_cmd_width>*** signal_dspin_v_cmd_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cmd_inc", XMAX, YMAX-1, 3);
   DspinSignals<dspin_cmd_width>*** signal_dspin_v_cmd_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cmd_dec", XMAX, YMAX-1, 3);
   DspinSignals<dspin_rsp_width>*** signal_dspin_v_rsp_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_rsp_inc", XMAX, YMAX-1, 2);
   DspinSignals<dspin_rsp_width>*** signal_dspin_v_rsp_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_rsp_dec", XMAX, YMAX-1, 2);

   // Mesh boundaries DSPIN signals
   DspinSignals<dspin_cmd_width>**** signal_dspin_false_cmd_in =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_false_cmd_in" , XMAX, YMAX, 4, 3);
   DspinSignals<dspin_cmd_width>**** signal_dspin_false_cmd_out =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_false_cmd_out", XMAX, YMAX, 4, 3);
   DspinSignals<dspin_rsp_width>**** signal_dspin_false_rsp_in =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_false_rsp_in" , XMAX, YMAX, 4, 2);
   DspinSignals<dspin_rsp_width>**** signal_dspin_false_rsp_out =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_false_rsp_out", XMAX, YMAX, 4, 2);


   ////////////////////////////
   //      Loader    
   ////////////////////////////

   soclib::common::Loader loader(soft_name);

   typedef soclib::common::GdbServer<soclib::common::Mips32ElIss> proc_iss;
   proc_iss::set_loader(loader);

   ////////////////////////////
   // Clusters construction
   ////////////////////////////

   TsarXbarCluster<dspin_cmd_width,
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
            size_t x = i / YMAX;
            size_t y = i % YMAX;

#if USE_OPENMP
#pragma omp critical
            {
#endif
            std::cout << std::endl;
            std::cout << "Cluster_" << x << "_" << y << std::endl;
            std::cout << std::endl;

            std::ostringstream sc;
            sc << "cluster_" << x << "_" << y;
            clusters[x][y] = new TsarXbarCluster<dspin_cmd_width,
                                                 dspin_rsp_width,
                                                 vci_param_int,
                                                 vci_param_ext>
            (
                sc.str().c_str(),
                NB_PROCS_MAX,
                NB_TTY_CHANNELS,  
                NB_DMA_CHANNELS, 
                x,
                y,
                cluster(x,y),
                maptabd,
                maptabx,
                x_width,
                y_width,
                vci_srcid_width - x_width - y_width,   // l_id width,
                MEMC_TGTID,
                XICU_TGTID,
                MDMA_TGTID,
                FBUF_TGTID,
                MTTY_TGTID,
                BROM_TGTID,
                MNIC_TGTID,
                CDMA_TGTID,
                BDEV_TGTID,
                SIMH_TGTID,
                MEMC_WAYS,
                MEMC_SETS,
                L1_IWAYS,
                L1_ISETS,
                L1_DWAYS,
                L1_DSETS,
                XRAM_LATENCY,
                (cluster(x,y) == cluster_io_id),
                FBUF_X_SIZE,
                FBUF_Y_SIZE,
                disk_name,
                BDEV_SECTOR_SIZE,
                NB_NIC_CHANNELS,
                nic_rx_name,
                nic_tx_name,
                NIC_TIMEOUT,
                NB_CMA_CHANNELS,
                loader,
                frozen_cycles,
                debug_from,
                debug_ok and (cluster(x,y) == debug_memc_id),
                debug_ok and (cluster(x,y) == debug_proc_id) 
            );

#if USE_OPENMP
            } // end critical
#endif
        } // end for
#if USE_OPENMP
    }
#endif

   ///////////////////////////////////////////////////////////////
   //     Net-list 
   ///////////////////////////////////////////////////////////////

   // Clock & RESET
   for (size_t x = 0; x < (XMAX); x++){
      for (size_t y = 0; y < YMAX; y++){
         clusters[x][y]->p_clk                         (signal_clk);
         clusters[x][y]->p_resetn                      (signal_resetn);
      }
   }

   // Inter Clusters horizontal connections
   if (XMAX > 1){
      for (size_t x = 0; x < (XMAX-1); x++){
         for (size_t y = 0; y < YMAX; y++){
            for (size_t k = 0; k < 3; k++){
               clusters[x][y]->p_cmd_out[EAST][k]      (signal_dspin_h_cmd_inc[x][y][k]);
               clusters[x+1][y]->p_cmd_in[WEST][k]     (signal_dspin_h_cmd_inc[x][y][k]);
               clusters[x][y]->p_cmd_in[EAST][k]       (signal_dspin_h_cmd_dec[x][y][k]);
               clusters[x+1][y]->p_cmd_out[WEST][k]    (signal_dspin_h_cmd_dec[x][y][k]);
            }

            for (size_t k = 0; k < 2; k++){
               clusters[x][y]->p_rsp_out[EAST][k]      (signal_dspin_h_rsp_inc[x][y][k]);
               clusters[x+1][y]->p_rsp_in[WEST][k]     (signal_dspin_h_rsp_inc[x][y][k]);
               clusters[x][y]->p_rsp_in[EAST][k]       (signal_dspin_h_rsp_dec[x][y][k]);
               clusters[x+1][y]->p_rsp_out[WEST][k]    (signal_dspin_h_rsp_dec[x][y][k]);
            }
         }
      }
   }
   std::cout << std::endl << "Horizontal connections established" << std::endl;   

   // Inter Clusters vertical connections
   if (YMAX > 1) {
      for (size_t y = 0; y < (YMAX-1); y++){
         for (size_t x = 0; x < XMAX; x++){
            for (size_t k = 0; k < 3; k++){
               clusters[x][y]->p_cmd_out[NORTH][k]     (signal_dspin_v_cmd_inc[x][y][k]);
               clusters[x][y+1]->p_cmd_in[SOUTH][k]    (signal_dspin_v_cmd_inc[x][y][k]);
               clusters[x][y]->p_cmd_in[NORTH][k]      (signal_dspin_v_cmd_dec[x][y][k]);
               clusters[x][y+1]->p_cmd_out[SOUTH][k]   (signal_dspin_v_cmd_dec[x][y][k]);
            }

            for (size_t k = 0; k < 2; k++){
               clusters[x][y]->p_rsp_out[NORTH][k]     (signal_dspin_v_rsp_inc[x][y][k]);
               clusters[x][y+1]->p_rsp_in[SOUTH][k]    (signal_dspin_v_rsp_inc[x][y][k]);
               clusters[x][y]->p_rsp_in[NORTH][k]      (signal_dspin_v_rsp_dec[x][y][k]);
               clusters[x][y+1]->p_rsp_out[SOUTH][k]   (signal_dspin_v_rsp_dec[x][y][k]);
            }
         }
      }
   }
   std::cout << "Vertical connections established" << std::endl;

   // East & West boundary cluster connections
   for (size_t y = 0; y < YMAX; y++)
   {
      for (size_t k = 0; k < 3; k++)
      {
         clusters[0][y]->p_cmd_in[WEST][k]        (signal_dspin_false_cmd_in[0][y][WEST][k]);
         clusters[0][y]->p_cmd_out[WEST][k]       (signal_dspin_false_cmd_out[0][y][WEST][k]);
         clusters[XMAX-1][y]->p_cmd_in[EAST][k]   (signal_dspin_false_cmd_in[XMAX-1][y][EAST][k]);
         clusters[XMAX-1][y]->p_cmd_out[EAST][k]  (signal_dspin_false_cmd_out[XMAX-1][y][EAST][k]);
      }

      for (size_t k = 0; k < 2; k++)
      {
         clusters[0][y]->p_rsp_in[WEST][k]        (signal_dspin_false_rsp_in[0][y][WEST][k]);
         clusters[0][y]->p_rsp_out[WEST][k]       (signal_dspin_false_rsp_out[0][y][WEST][k]);
         clusters[XMAX-1][y]->p_rsp_in[EAST][k]   (signal_dspin_false_rsp_in[XMAX-1][y][EAST][k]);
         clusters[XMAX-1][y]->p_rsp_out[EAST][k]  (signal_dspin_false_rsp_out[XMAX-1][y][EAST][k]);
      }
   }

   // North & South boundary clusters connections
   for (size_t x = 0; x < XMAX; x++)
   {
      for (size_t k = 0; k < 3; k++)
      {
         clusters[x][0]->p_cmd_in[SOUTH][k]       (signal_dspin_false_cmd_in[x][0][SOUTH][k]);
         clusters[x][0]->p_cmd_out[SOUTH][k]      (signal_dspin_false_cmd_out[x][0][SOUTH][k]);
         clusters[x][YMAX-1]->p_cmd_in[NORTH][k]  (signal_dspin_false_cmd_in[x][YMAX-1][NORTH][k]);
         clusters[x][YMAX-1]->p_cmd_out[NORTH][k] (signal_dspin_false_cmd_out[x][YMAX-1][NORTH][k]);
      }

      for (size_t k = 0; k < 2; k++)
      {
         clusters[x][0]->p_rsp_in[SOUTH][k]       (signal_dspin_false_rsp_in[x][0][SOUTH][k]);
         clusters[x][0]->p_rsp_out[SOUTH][k]      (signal_dspin_false_rsp_out[x][0][SOUTH][k]);
         clusters[x][YMAX-1]->p_rsp_in[NORTH][k]  (signal_dspin_false_rsp_in[x][YMAX-1][NORTH][k]);
         clusters[x][YMAX-1]->p_rsp_out[NORTH][k] (signal_dspin_false_rsp_out[x][YMAX-1][NORTH][k]);
      }
   }
   std::cout << "North, South, West, East connections established" << std::endl;
   std::cout << std::endl;


   ////////////////////////////////////////////////////////
   //   Simulation
   ///////////////////////////////////////////////////////

   sc_start(sc_core::sc_time(0, SC_NS));
   signal_resetn = false;

   // network boundaries signals
   for (size_t x = 0; x < XMAX ; x++){
      for (size_t y = 0; y < YMAX ; y++){
         for (size_t a = 0; a < 4; a++){
            for (size_t k = 0; k < 3; k++){
               signal_dspin_false_cmd_in [x][y][a][k].write = false;
               signal_dspin_false_cmd_in [x][y][a][k].read  = true;
               signal_dspin_false_cmd_out[x][y][a][k].write = false;
               signal_dspin_false_cmd_out[x][y][a][k].read  = true;
            }

            for (size_t k = 0; k < 2; k++){
               signal_dspin_false_rsp_in [x][y][a][k].write = false;
               signal_dspin_false_rsp_in [x][y][a][k].read  = true;
               signal_dspin_false_rsp_out[x][y][a][k].write = false;
               signal_dspin_false_rsp_out[x][y][a][k].read  = true;
            }
         }
      }
   }

   sc_start(sc_core::sc_time(1, SC_NS));
   signal_resetn = true;

   if (gettimeofday(&t1, NULL) != 0) 
   {
      perror("gettimeofday");
      return EXIT_FAILURE;
   }

   for (uint64_t n = 1; n < ncycles && !stop_called; n++)
   {
      // Monitor a specific address for L1 & L2 caches
      //clusters[0][0]->proc[0]->cache_monitor(0x800002c000ULL);
      //clusters[1][0]->memc->copies_monitor(0x800002C000ULL);

      if( (n % 5000000) == 0)
      {

         if (gettimeofday(&t2, NULL) != 0) 
         {
            perror("gettimeofday");
            return EXIT_FAILURE;
         }

         ms1 = (uint64_t) t1.tv_sec * 1000ULL + (uint64_t) t1.tv_usec / 1000;
         ms2 = (uint64_t) t2.tv_sec * 1000ULL + (uint64_t) t2.tv_usec / 1000;
         std::cerr << "platform clock frequency " << (double) 5000000 / (double) (ms2 - ms1) << "Khz" << std::endl;

         if (gettimeofday(&t1, NULL) != 0) 
         {
            perror("gettimeofday");
            return EXIT_FAILURE;
         }
      }

      if (debug_ok and (n > debug_from) and (n % debug_period == 0))
      {
         std::cout << "****************** cycle " << std::dec << n ;
         std::cout << " ************************************************" << std::endl;

        // trace proc[debug_proc_id] 
        size_t l = debug_proc_id % NB_PROCS_MAX ;
        size_t y = (debug_proc_id / NB_PROCS_MAX) % YMAX ;
        size_t x = debug_proc_id / (YMAX * NB_PROCS_MAX) ;

        std::ostringstream proc_signame;
        proc_signame << "[SIG]PROC_" << x << "_" << y << "_" << l ;
        std::ostringstream p2m_signame;
        p2m_signame << "[SIG]PROC_" << x << "_" << y << "_" << l << " P2M" ;
        std::ostringstream m2p_signame;
        m2p_signame << "[SIG]PROC_" << x << "_" << y << "_" << l << " M2P" ;
        std::ostringstream p_cmd_signame;
        p_cmd_signame << "[SIG]PROC_" << x << "_" << y << "_" << l << " CMD" ;
        std::ostringstream p_rsp_signame;
        p_rsp_signame << "[SIG]PROC_" << x << "_" << y << "_" << l << " RSP" ;

        for (int _x = 0, _y = 0; _x != XMAX; (_y == YMAX - 1 ? _x++, _y = 0 : _y++)) {
           for (int _l = 0; _l < NB_PROCS_MAX; _l++) {
              clusters[_x][_y]->proc[_l]->print_trace();
           }
        }
        //clusters[x][y]->wi_proc[l]->print_trace();
        //clusters[x][y]->signal_vci_ini_proc[l].print_trace(proc_signame.str());
        //clusters[x][y]->signal_dspin_p2m_proc[l].print_trace(p2m_signame.str());
        //clusters[x][y]->signal_dspin_m2p_proc[l].print_trace(m2p_signame.str());
        //clusters[x][y]->signal_dspin_cmd_proc_i[l].print_trace(p_cmd_signame.str());
        //clusters[x][y]->signal_dspin_rsp_proc_i[l].print_trace(p_rsp_signame.str());

        //clusters[x][y]->xbar_rsp_d->print_trace();
        //clusters[x][y]->xbar_cmd_d->print_trace();
        //clusters[x][y]->signal_dspin_cmd_l2g_d.print_trace("[SIG]L2G CMD");
        //clusters[x][y]->signal_dspin_cmd_g2l_d.print_trace("[SIG]G2L CMD");
        //clusters[x][y]->signal_dspin_rsp_l2g_d.print_trace("[SIG]L2G RSP");
        //clusters[x][y]->signal_dspin_rsp_g2l_d.print_trace("[SIG]G2L RSP");

        // trace memc[debug_memc_id] 
        x = debug_memc_id / YMAX;
        y = debug_memc_id % YMAX;

        std::ostringstream smemc;
        smemc << "[SIG]MEMC_" << x << "_" << y;
        std::ostringstream sxram;
        sxram << "[SIG]XRAM_" << x << "_" << y;
        std::ostringstream sm2p;
        sm2p << "[SIG]MEMC_" << x << "_" << y << " M2P" ;
        std::ostringstream sp2m;
        sp2m << "[SIG]MEMC_" << x << "_" << y << " P2M" ;
        std::ostringstream m_cmd_signame;
        m_cmd_signame << "[SIG]MEMC_" << x << "_" << y <<  " CMD" ;
        std::ostringstream m_rsp_signame;
        m_rsp_signame << "[SIG]MEMC_" << x << "_" << y <<  " RSP" ;

        clusters[x][y]->memc->print_trace();
        //clusters[x][y]->wt_memc->print_trace();
        //clusters[x][y]->signal_vci_tgt_memc.print_trace(smemc.str());
        //clusters[x][y]->signal_vci_xram.print_trace(sxram.str());
        //clusters[x][y]->signal_dspin_p2m_memc.print_trace(sp2m.str());
        //clusters[x][y]->signal_dspin_m2p_memc.print_trace(sm2p.str());
        //clusters[x][y]->signal_dspin_cmd_memc_t.print_trace(m_cmd_signame.str());
        //clusters[x][y]->signal_dspin_rsp_memc_t.print_trace(m_rsp_signame.str());
        
        // trace replicated peripherals
        //clusters[1][1]->mdma->print_trace();
        //clusters[1][1]->signal_vci_tgt_mdma.print_trace("[SIG]MDMA_TGT_1_1");
        //clusters[1][1]->signal_vci_ini_mdma.print_trace("[SIG]MDMA_INI_1_1");
        

        // trace external peripherals
        //size_t io_x   = cluster_io_id / YMAX;
        //size_t io_y   = cluster_io_id % YMAX;
        
        //clusters[io_x][io_y]->brom->print_trace();
        //clusters[io_x][io_y]->wt_brom->print_trace();
        //clusters[io_x][io_y]->signal_vci_tgt_brom.print_trace("[SIG]BROM");
        //clusters[io_x][io_y]->signal_dspin_cmd_brom_t.print_trace("[SIG]BROM CMD");
        //clusters[io_x][io_y]->signal_dspin_rsp_brom_t.print_trace("[SIG]BROM RSP");

        //clusters[io_x][io_y]->bdev->print_trace();
        //clusters[io_x][io_y]->signal_vci_tgt_bdev.print_trace("[SIG]BDEV_TGT");
        //clusters[io_x][io_y]->signal_vci_ini_bdev.print_trace("[SIG]BDEV_INI");
      }

      sc_start(sc_core::sc_time(1, SC_NS));
   }

   
   // Free memory
   for (size_t i = 0; i  < (XMAX * YMAX); i++)
   {
      size_t x = i / YMAX;
      size_t y = i % YMAX;
      delete clusters[x][y];
   }

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_cmd_inc, XMAX - 1, YMAX, 3);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_cmd_dec, XMAX - 1, YMAX, 3);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_h_rsp_inc, XMAX - 1, YMAX, 2);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_h_rsp_dec, XMAX - 1, YMAX, 2);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_cmd_inc, XMAX, YMAX - 1, 3);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_cmd_dec, XMAX, YMAX - 1, 3);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_v_rsp_inc, XMAX, YMAX - 1, 2);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_v_rsp_dec, XMAX, YMAX - 1, 2);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_false_cmd_in, XMAX, YMAX, 4, 3);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_false_cmd_out, XMAX, YMAX, 4, 3);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_false_rsp_in, XMAX, YMAX, 4, 2);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_false_rsp_out, XMAX, YMAX, 4, 2);

   return EXIT_SUCCESS;
}


void handler(int dummy = 0) {
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
