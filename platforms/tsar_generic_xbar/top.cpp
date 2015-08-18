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
#include "alloc_elems.h"
#include "tsar_xbar_cluster.h"

#define USE_ALMOS 1
//#define USE_GIET 

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

#ifdef USE_ALMOS
   #define PREFIX_OS "almos/"
   #include "almos/hard_config.h"
#endif
#ifdef USE_GIET
   #define PREFIX_OS "giet_vm/"
#endif

///////////////////////////////////////////////////
//               Parallelisation
///////////////////////////////////////////////////


#if USE_OPENMP
#include <omp.h>
#endif

//  cluster index (computed from x,y coordinates)
#ifdef USE_ALMOS
   #define cluster(x,y)   (y + x * Y_SIZE)
#else
   #define cluster(x,y)   (y + (x << Y_WIDTH))
#endif


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
//    Secondary Hardware Parameters         
//////////////////////i/////////////////////////////////////


#define XRAM_LATENCY          0

#define MEMC_WAYS             16
#define MEMC_SETS             256

#define L1_IWAYS              4
#define L1_ISETS              64

#define L1_DWAYS              4
#define L1_DSETS              64

#ifdef USE_ALMOS
#define FBUF_X_SIZE           1024
#define FBUF_Y_SIZE           1024
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
#define soft_name       PREFIX_OS"bootloader-tsar-mipsel.bin",\
                        PREFIX_OS"kernel-soclib.bin@0xbfc10000:D",\
                        PREFIX_OS"arch-info.bib@0xBFC08000:D"
#endif
#ifdef USE_GIET
#define soft_pathname   PREFIX_OS"soft.elf"
#endif

////////////////////////////////////////////////////////////
//     DEBUG Parameters default values         
//////////////////////i/////////////////////////////////////

#define MAX_FROZEN_CYCLES     100000000


////////////////////////////////////////////////////////////////////
//     TGTID definition in direct space
// For all components:  global TGTID = global SRCID = cluster_index
////////////////////////////////////////////////////////////////////

#define MEMC_TGTID      0
#define XICU_TGTID      1
#define MDMA_TGTID      2
#define MTTY_TGTID      3
#define BDEV_TGTID      4
#define MNIC_TGTID      5
#define BROM_TGTID      6
#define CDMA_TGTID      7
#define SIMH_TGTID      8
#define FBUF_TGTID      9


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
   // 2^19 is the offset for the local id (8 bits for global ID :
   // 1 bit for Memcache or Peripheral, 4 for local peripheral id)
   // (Almos supports 32 bits physical addresses)

   #define CLUSTER_INC (0x80000000ULL / (X_SIZE * Y_SIZE) * 2)

   #define CLUSTER_IO_INC (cluster_io_id * CLUSTER_INC)
   #define MEMC_MAX_SIZE (0x40000000 / (X_SIZE * Y_SIZE)) // 0x40000000 : valeur totale souhaitée (ici : 1Go)

   #define BROM_BASE    0x00BFC00000
   #define BROM_SIZE    0x0000100000 // 1 Mbytes

   #define MEMC_BASE    0x0000000000
   #define MEMC_SIZE    min(0x04000000, MEMC_MAX_SIZE)

   #define XICU_BASE    (CLUSTER_INC >> 1) + (XICU_TGTID << 19)
   #define XICU_SIZE    0x0000001000 // 4 Kbytes
   
   #define MDMA_BASE    (CLUSTER_INC >> 1) + (MDMA_TGTID << 19)
   #define MDMA_SIZE    (0x0000001000 * NB_DMA_CHANNELS) // 4 Kbytes per channel  

   #define BDEV_BASE    (CLUSTER_INC >> 1) + (BDEV_TGTID << 19) + (CLUSTER_IO_INC)
   #define BDEV_SIZE    0x0000001000 // 4 Kbytes

   #define MTTY_BASE    (CLUSTER_INC >> 1) + (MTTY_TGTID << 19) + (CLUSTER_IO_INC)
   #define MTTY_SIZE    0x0000001000 // 4 Kbytes

   #define FBUF_BASE    (CLUSTER_INC >> 1) + (FBUF_TGTID << 19) + (CLUSTER_IO_INC)
   #define FBUF_SIZE    (FBUF_X_SIZE * FBUF_Y_SIZE * 2) // Should be 0x80000

   #define MNIC_BASE    (CLUSTER_INC >> 1) + (MNIC_TGTID << 19) + (CLUSTER_IO_INC)
   #define MNIC_SIZE    0x0000080000

   #define CDMA_BASE    (CLUSTER_INC >> 1) + (CDMA_TGTID << 19) + (CLUSTER_IO_INC)
   #define CDMA_SIZE    (0x0000004000 * NB_CMA_CHANNELS)

   #define SIMH_BASE    (CLUSTER_INC >> 1) + (SIMH_TGTID << 19) + (CLUSTER_IO_INC)
   #define SIMH_SIZE    0x0000001000
#endif

bool stop_called = false;

/////////////////////////////////
int _main(int argc, char *argv[])
{
   using namespace sc_core;
   using namespace soclib::caba;
   using namespace soclib::common;

#ifdef USE_GIET
   char     soft_name[256]    = soft_pathname;      // pathname to binary code
#endif
   const int64_t max_cycles   = 5000000;             // Maximum number of cycles simulated in one sc_start call
   int64_t ncycles            = 0x7FFFFFFFFFFFFFFF;  // simulated cycles
   char     disk_name[256]    = BDEV_IMAGE_NAME;    // pathname to the disk image
   char     nic_rx_name[256]  = NIC_RX_NAME;        // pathname to the rx packets file
   char     nic_tx_name[256]  = NIC_TX_NAME;        // pathname to the tx packets file
   ssize_t  threads_nr        = 1;                  // simulator's threads number
   bool     debug_ok          = false;              // trace activated
   size_t   debug_period      = 1;                  // trace period
   size_t   debug_memc_id     = 0;                  // index of memc to be traced 
   size_t   debug_proc_id     = 0;                  // index of proc to be traced
   int64_t  debug_from        = 0;                  // trace start cycle
   int64_t  frozen_cycles     = MAX_FROZEN_CYCLES;  // monitoring frozen processor
   size_t   cluster_io_id;                         // index of cluster containing IOs
   int64_t  reset_counters    = -1;
   int64_t  dump_counters     = -1;
   bool     do_reset_counters = false;
   bool     do_dump_counters  = false;
   struct   timeval t1, t2;
   uint64_t ms1, ms2;

   ////////////// command line arguments //////////////////////
   if (argc > 1)
   {
      for (int n = 1; n < argc; n = n + 2)
      {
         if ((strcmp(argv[n], "-NCYCLES") == 0) && (n + 1 < argc))
         {
            ncycles = (int64_t) strtol(argv[n + 1], NULL, 0);
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
            debug_from = (int64_t) strtol(argv[n + 1], NULL, 0);
         }
         else if ((strcmp(argv[n], "-MEMCID") == 0) && (n + 1 < argc))
         {
            debug_memc_id = (size_t) strtol(argv[n + 1], NULL, 0);
#ifdef USE_ALMOS
            assert((debug_memc_id < (X_SIZE * Y_SIZE)) &&
                   "debug_memc_id larger than X_SIZE * Y_SIZE" );
#else
            size_t x = debug_memc_id >> Y_WIDTH;
            size_t y = debug_memc_id & ((1 << Y_WIDTH) - 1);

            assert( (x <= X_SIZE) and (y <= Y_SIZE) &&
                  "MEMCID parameter refers a not valid memory cache");
#endif
         }
         else if ((strcmp(argv[n], "-PROCID") == 0) && (n + 1 < argc))
         {
            debug_proc_id = (size_t) strtol(argv[n + 1], NULL, 0);
#ifdef USE_ALMOS
            assert((debug_proc_id < (X_SIZE * Y_SIZE * NB_PROCS_MAX)) && 
                   "debug_proc_id larger than X_SIZE * Y_SIZE * NB_PROCS");
#else
            size_t cluster_xy = debug_proc_id / NB_PROCS_MAX ;
            size_t x = cluster_xy >> Y_WIDTH;
            size_t y = cluster_xy & ((1 << Y_WIDTH) - 1);

            assert( (x <= X_SIZE) and (y <= Y_SIZE) &&
                  "PROCID parameter refers a not valid processor");
#endif
         }
         else if ((strcmp(argv[n], "-THREADS") == 0) && ((n + 1) < argc))
         {
            threads_nr = (ssize_t) strtol(argv[n + 1], NULL, 0);
            threads_nr = (threads_nr < 1) ? 1 : threads_nr;
         }
         else if ((strcmp(argv[n], "-FROZEN") == 0) && (n + 1 < argc))
         {
            frozen_cycles = (int64_t) strtol(argv[n + 1], NULL, 0);
         }
         else if ((strcmp(argv[n], "-PERIOD") == 0) && (n + 1 < argc))
         {
            debug_period = (size_t) strtol(argv[n + 1], NULL, 0);
         }
         else if ((strcmp(argv[n], "--reset-counters") == 0) && (n + 1 < argc))
         {
            reset_counters = (int64_t) strtol(argv[n + 1], NULL, 0);
            do_reset_counters = true;
         }
         else if ((strcmp(argv[n], "--dump-counters") == 0) && (n + 1 < argc))
         {
            dump_counters = (int64_t) strtol(argv[n + 1], NULL, 0);
            do_dump_counters = true;
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
    assert( ( (X_SIZE == 1) or (X_SIZE == 2) or (X_SIZE == 4) or
              (X_SIZE == 8) or (X_SIZE == 16) ) and
              "The X_SIZE parameter must be 1, 2, 4, 8 or 16" );

    assert( ( (Y_SIZE == 1) or (Y_SIZE == 2) or (Y_SIZE == 4) or
              (Y_SIZE == 8) or (Y_SIZE == 16) ) and
              "The Y_SIZE parameter must be 1, 2, 4, 8 or 16" );

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
    std::cout << " - X_SIZE             = " << X_SIZE << std::endl;
    std::cout << " - Y_SIZE             = " << Y_SIZE << std::endl;
    std::cout << " - NB_PROCS_MAX     = " << NB_PROCS_MAX <<  std::endl;
    std::cout << " - NB_DMA_CHANNELS  = " << NB_DMA_CHANNELS <<  std::endl;
    std::cout << " - NB_TTY_CHANNELS  = " << NB_TTY_CHANNELS <<  std::endl;
    std::cout << " - NB_NIC_CHANNELS  = " << NB_NIC_CHANNELS <<  std::endl;
    std::cout << " - MEMC_WAYS        = " << MEMC_WAYS << std::endl;
    std::cout << " - MEMC_SETS        = " << MEMC_SETS << std::endl;
    std::cout << " - RAM_LATENCY      = " << XRAM_LATENCY << std::endl;
    std::cout << " - MAX_FROZEN       = " << frozen_cycles << std::endl;

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

#ifdef USE_ALMOS
   if      (X_SIZE == 1) x_width = 0;
   else if (X_SIZE == 2) x_width = 1;
   else if (X_SIZE <= 4) x_width = 2;
   else if (X_SIZE <= 8) x_width = 3;
   else                x_width = 4;

   if      (Y_SIZE == 1) y_width = 0;
   else if (Y_SIZE == 2) y_width = 1;
   else if (Y_SIZE <= 4) y_width = 2;
   else if (Y_SIZE <= 8) y_width = 3;
   else                y_width = 4;

#else
   size_t x_width = X_WIDTH;
   size_t y_width = Y_WIDTH;

   assert( (X_WIDTH <= 4) and (Y_WIDTH <= 4) and
           "Up to 256 clusters");

   assert( (X_SIZE <= (1 << X_WIDTH)) and (Y_SIZE <= (1 << Y_WIDTH)) and
           "The X_WIDTH and Y_WIDTH parameter are insufficient");

#endif

   // index of cluster containing IOs
   cluster_io_id = 0x00bfc00000ULL >> (vci_address_width - x_width - y_width);


   /////////////////////
   //  Mapping Tables
   /////////////////////

   // internal network
   MappingTable maptabd(vci_address_width, 
                        IntTab(x_width + y_width, 16 - x_width - y_width), 
                        IntTab(x_width + y_width, vci_srcid_width - x_width - y_width), 
                        0x00FF800000);

   for (size_t x = 0; x < X_SIZE; x++)
   {
      for (size_t y = 0; y < Y_SIZE; y++)
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

   for (size_t x = 0; x < X_SIZE; x++)
   {
      for (size_t y = 0; y < Y_SIZE ; y++)
      {

         sc_uint<vci_address_width> offset;
         offset = (sc_uint<vci_address_width>)cluster(x,y) 
                   << (vci_address_width - x_width - y_width);

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
   DspinSignals<dspin_cmd_width>** signal_dspin_h_cmd_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cmd_inc", X_SIZE-1, Y_SIZE);
   DspinSignals<dspin_cmd_width>** signal_dspin_h_cmd_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cmd_dec", X_SIZE-1, Y_SIZE);

   DspinSignals<dspin_rsp_width>** signal_dspin_h_rsp_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_rsp_inc", X_SIZE-1, Y_SIZE);
   DspinSignals<dspin_rsp_width>** signal_dspin_h_rsp_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_rsp_dec", X_SIZE-1, Y_SIZE);

   DspinSignals<dspin_cmd_width>** signal_dspin_h_m2p_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_m2p_inc", X_SIZE-1, Y_SIZE);
   DspinSignals<dspin_cmd_width>** signal_dspin_h_m2p_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_m2p_dec", X_SIZE-1, Y_SIZE);

   DspinSignals<dspin_rsp_width>** signal_dspin_h_p2m_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_p2m_inc", X_SIZE-1, Y_SIZE);
   DspinSignals<dspin_rsp_width>** signal_dspin_h_p2m_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_p2m_dec", X_SIZE-1, Y_SIZE);

   DspinSignals<dspin_cmd_width>** signal_dspin_h_cla_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cla_inc", X_SIZE-1, Y_SIZE);
   DspinSignals<dspin_cmd_width>** signal_dspin_h_cla_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cla_dec", X_SIZE-1, Y_SIZE);

   // Vertical inter-clusters DSPIN signals
   DspinSignals<dspin_cmd_width>** signal_dspin_v_cmd_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cmd_inc", X_SIZE, Y_SIZE-1);
   DspinSignals<dspin_cmd_width>** signal_dspin_v_cmd_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cmd_dec", X_SIZE, Y_SIZE-1);

   DspinSignals<dspin_rsp_width>** signal_dspin_v_rsp_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_rsp_inc", X_SIZE, Y_SIZE-1);
   DspinSignals<dspin_rsp_width>** signal_dspin_v_rsp_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_rsp_dec", X_SIZE, Y_SIZE-1);

   DspinSignals<dspin_cmd_width>** signal_dspin_v_m2p_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_m2p_inc", X_SIZE, Y_SIZE-1);
   DspinSignals<dspin_cmd_width>** signal_dspin_v_m2p_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_m2p_dec", X_SIZE, Y_SIZE-1);

   DspinSignals<dspin_rsp_width>** signal_dspin_v_p2m_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_p2m_inc", X_SIZE, Y_SIZE-1);
   DspinSignals<dspin_rsp_width>** signal_dspin_v_p2m_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_p2m_dec", X_SIZE, Y_SIZE-1);

   DspinSignals<dspin_cmd_width>** signal_dspin_v_cla_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cla_inc", X_SIZE, Y_SIZE-1);
   DspinSignals<dspin_cmd_width>** signal_dspin_v_cla_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cla_dec", X_SIZE, Y_SIZE-1);

   // Mesh boundaries DSPIN signals (Most of those signals are not used...)
   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_cmd_in =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_cmd_in" , X_SIZE, Y_SIZE, 4);
   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_cmd_out =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_cmd_out", X_SIZE, Y_SIZE, 4);

   DspinSignals<dspin_rsp_width>*** signal_dspin_bound_rsp_in =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_bound_rsp_in" , X_SIZE, Y_SIZE, 4);
   DspinSignals<dspin_rsp_width>*** signal_dspin_bound_rsp_out =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_bound_rsp_out", X_SIZE, Y_SIZE, 4);

   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_m2p_in =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_m2p_in" , X_SIZE, Y_SIZE, 4);
   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_m2p_out =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_m2p_out", X_SIZE, Y_SIZE, 4);

   DspinSignals<dspin_rsp_width>*** signal_dspin_bound_p2m_in =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_bound_p2m_in" , X_SIZE, Y_SIZE, 4);
   DspinSignals<dspin_rsp_width>*** signal_dspin_bound_p2m_out =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_bound_p2m_out", X_SIZE, Y_SIZE, 4);

   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_cla_in =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_cla_in" , X_SIZE, Y_SIZE, 4);
   DspinSignals<dspin_cmd_width>*** signal_dspin_bound_cla_out =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_bound_cla_out", X_SIZE, Y_SIZE, 4);


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
                   vci_param_ext> * clusters[X_SIZE][Y_SIZE];

#if USE_OPENMP
#pragma omp parallel
    {
#pragma omp for
#endif
        for (size_t i = 0; i  < (X_SIZE * Y_SIZE); i++)
        {
            size_t x = i / Y_SIZE;
            size_t y = i % Y_SIZE;

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
                IRQ_PER_PROCESSOR,
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
                debug_ok,
                debug_ok
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
   for (size_t x = 0; x < (X_SIZE); x++){
      for (size_t y = 0; y < Y_SIZE; y++){
         clusters[x][y]->p_clk                         (signal_clk);
         clusters[x][y]->p_resetn                      (signal_resetn);
      }
   }

   // Inter Clusters horizontal connections
   if (X_SIZE > 1) {
       for (size_t x = 0; x < (X_SIZE-1); x++) {
           for (size_t y = 0; y < (Y_SIZE); y++) {
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
   if (Y_SIZE > 1) {
       for (size_t y = 0; y < (Y_SIZE-1); y++) {
           for (size_t x = 0; x < X_SIZE; x++) {
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
   for (size_t y = 0; y < (Y_SIZE); y++) {
       clusters[0][y]->p_cmd_in[WEST]           (signal_dspin_bound_cmd_in[0][y][WEST]);
       clusters[0][y]->p_cmd_out[WEST]          (signal_dspin_bound_cmd_out[0][y][WEST]);
       clusters[X_SIZE-1][y]->p_cmd_in[EAST]    (signal_dspin_bound_cmd_in[X_SIZE-1][y][EAST]);
       clusters[X_SIZE-1][y]->p_cmd_out[EAST]   (signal_dspin_bound_cmd_out[X_SIZE-1][y][EAST]);

       clusters[0][y]->p_rsp_in[WEST]           (signal_dspin_bound_rsp_in[0][y][WEST]);
       clusters[0][y]->p_rsp_out[WEST]          (signal_dspin_bound_rsp_out[0][y][WEST]);
       clusters[X_SIZE-1][y]->p_rsp_in[EAST]    (signal_dspin_bound_rsp_in[X_SIZE-1][y][EAST]);
       clusters[X_SIZE-1][y]->p_rsp_out[EAST]   (signal_dspin_bound_rsp_out[X_SIZE-1][y][EAST]);

       clusters[0][y]->p_m2p_in[WEST]           (signal_dspin_bound_m2p_in[0][y][WEST]);
       clusters[0][y]->p_m2p_out[WEST]          (signal_dspin_bound_m2p_out[0][y][WEST]);
       clusters[X_SIZE-1][y]->p_m2p_in[EAST]    (signal_dspin_bound_m2p_in[X_SIZE-1][y][EAST]);
       clusters[X_SIZE-1][y]->p_m2p_out[EAST]   (signal_dspin_bound_m2p_out[X_SIZE-1][y][EAST]);

       clusters[0][y]->p_p2m_in[WEST]           (signal_dspin_bound_p2m_in[0][y][WEST]);
       clusters[0][y]->p_p2m_out[WEST]          (signal_dspin_bound_p2m_out[0][y][WEST]);
       clusters[X_SIZE-1][y]->p_p2m_in[EAST]    (signal_dspin_bound_p2m_in[X_SIZE-1][y][EAST]);
       clusters[X_SIZE-1][y]->p_p2m_out[EAST]   (signal_dspin_bound_p2m_out[X_SIZE-1][y][EAST]);

       clusters[0][y]->p_cla_in[WEST]           (signal_dspin_bound_cla_in[0][y][WEST]);
       clusters[0][y]->p_cla_out[WEST]          (signal_dspin_bound_cla_out[0][y][WEST]);
       clusters[X_SIZE-1][y]->p_cla_in[EAST]    (signal_dspin_bound_cla_in[X_SIZE-1][y][EAST]);
       clusters[X_SIZE-1][y]->p_cla_out[EAST]   (signal_dspin_bound_cla_out[X_SIZE-1][y][EAST]);
   }

   std::cout << std::endl << "West & East boundaries connections done" << std::endl;

   // North & South boundary clusters connections
   for (size_t x = 0; x < X_SIZE; x++) {
       clusters[x][0]->p_cmd_in[SOUTH]          (signal_dspin_bound_cmd_in[x][0][SOUTH]);
       clusters[x][0]->p_cmd_out[SOUTH]         (signal_dspin_bound_cmd_out[x][0][SOUTH]);
       clusters[x][Y_SIZE-1]->p_cmd_in[NORTH]   (signal_dspin_bound_cmd_in[x][Y_SIZE-1][NORTH]);
       clusters[x][Y_SIZE-1]->p_cmd_out[NORTH]  (signal_dspin_bound_cmd_out[x][Y_SIZE-1][NORTH]);

       clusters[x][0]->p_rsp_in[SOUTH]          (signal_dspin_bound_rsp_in[x][0][SOUTH]);
       clusters[x][0]->p_rsp_out[SOUTH]         (signal_dspin_bound_rsp_out[x][0][SOUTH]);
       clusters[x][Y_SIZE-1]->p_rsp_in[NORTH]   (signal_dspin_bound_rsp_in[x][Y_SIZE-1][NORTH]);
       clusters[x][Y_SIZE-1]->p_rsp_out[NORTH]  (signal_dspin_bound_rsp_out[x][Y_SIZE-1][NORTH]);

       clusters[x][0]->p_m2p_in[SOUTH]          (signal_dspin_bound_m2p_in[x][0][SOUTH]);
       clusters[x][0]->p_m2p_out[SOUTH]         (signal_dspin_bound_m2p_out[x][0][SOUTH]);
       clusters[x][Y_SIZE-1]->p_m2p_in[NORTH]   (signal_dspin_bound_m2p_in[x][Y_SIZE-1][NORTH]);
       clusters[x][Y_SIZE-1]->p_m2p_out[NORTH]  (signal_dspin_bound_m2p_out[x][Y_SIZE-1][NORTH]);

       clusters[x][0]->p_p2m_in[SOUTH]          (signal_dspin_bound_p2m_in[x][0][SOUTH]);
       clusters[x][0]->p_p2m_out[SOUTH]         (signal_dspin_bound_p2m_out[x][0][SOUTH]);
       clusters[x][Y_SIZE-1]->p_p2m_in[NORTH]   (signal_dspin_bound_p2m_in[x][Y_SIZE-1][NORTH]);
       clusters[x][Y_SIZE-1]->p_p2m_out[NORTH]  (signal_dspin_bound_p2m_out[x][Y_SIZE-1][NORTH]);

       clusters[x][0]->p_cla_in[SOUTH]          (signal_dspin_bound_cla_in[x][0][SOUTH]);
       clusters[x][0]->p_cla_out[SOUTH]         (signal_dspin_bound_cla_out[x][0][SOUTH]);
       clusters[x][Y_SIZE-1]->p_cla_in[NORTH]   (signal_dspin_bound_cla_in[x][Y_SIZE-1][NORTH]);
       clusters[x][Y_SIZE-1]->p_cla_out[NORTH]  (signal_dspin_bound_cla_out[x][Y_SIZE-1][NORTH]);
   }

   std::cout << std::endl << "North & South boundaries connections done" << std::endl;
   std::cout << std::endl;


#ifdef WT_IDL
    std::list<VciCcVCacheWrapper<vci_param_int,
        dspin_cmd_width,
        dspin_rsp_width,
        GdbServer<Mips32ElIss> > * > l1_caches;

   for (size_t x = 0; x < X_SIZE; x++) {
      for (size_t y = 0; y < Y_SIZE; y++) {
         for (int proc = 0; proc < NB_PROCS_MAX; proc++) {
            l1_caches.push_back(clusters[x][y]->proc[proc]);
         }
      }
   }

   for (size_t x = 0; x < X_SIZE; x++) {
      for (size_t y = 0; y < Y_SIZE; y++) {
         clusters[x][y]->memc->set_vcache_list(l1_caches);
      }
   }
#endif


//#define SC_TRACE
#ifdef SC_TRACE
   sc_trace_file * tf = sc_create_vcd_trace_file("my_trace_file");

   if (X_SIZE > 1){
      for (size_t x = 0; x < (X_SIZE-1); x++){
         for (size_t y = 0; y < Y_SIZE; y++){
            for (size_t k = 0; k < 3; k++){
               signal_dspin_h_cmd_inc[x][y][k].trace(tf, "dspin_h_cmd_inc");
               signal_dspin_h_cmd_dec[x][y][k].trace(tf, "dspin_h_cmd_dec");
            }

            for (size_t k = 0; k < 2; k++){
               signal_dspin_h_rsp_inc[x][y][k].trace(tf, "dspin_h_rsp_inc");
               signal_dspin_h_rsp_dec[x][y][k].trace(tf, "dspin_h_rsp_dec");
            }
         }
      }
   }

   if (Y_SIZE > 1) {
      for (size_t y = 0; y < (Y_SIZE-1); y++){
         for (size_t x = 0; x < X_SIZE; x++){
            for (size_t k = 0; k < 3; k++){
               signal_dspin_v_cmd_inc[x][y][k].trace(tf, "dspin_v_cmd_inc");
               signal_dspin_v_cmd_dec[x][y][k].trace(tf, "dspin_v_cmd_dec");
            }

            for (size_t k = 0; k < 2; k++){
               signal_dspin_v_rsp_inc[x][y][k].trace(tf, "dspin_v_rsp_inc");
               signal_dspin_v_rsp_dec[x][y][k].trace(tf, "dspin_v_rsp_dec");
            }
         }
      }
   }

   for (size_t x = 0; x < (X_SIZE); x++){
      for (size_t y = 0; y < Y_SIZE; y++){
         std::ostringstream signame;
         signame << "cluster" << x << "_" << y;
         clusters[x][y]->trace(tf, signame.str());
      }
   }
#endif


   ////////////////////////////////////////////////////////
   //   Simulation
   ///////////////////////////////////////////////////////

   sc_start(sc_core::sc_time(0, SC_NS));
   signal_resetn = false;

   // set network boundaries signals default values
   // for all boundary clusters
   for (size_t x = 0; x < X_SIZE ; x++) {
       for (size_t y = 0; y < Y_SIZE ; y++) {
           for (size_t face = 0; face < 4; face++) {
               signal_dspin_bound_cmd_in [x][y][face].write = false;
               signal_dspin_bound_cmd_in [x][y][face].read  = true;
               signal_dspin_bound_cmd_out[x][y][face].write = false;
               signal_dspin_bound_cmd_out[x][y][face].read  = true;

               signal_dspin_bound_rsp_in [x][y][face].write = false;
               signal_dspin_bound_rsp_in [x][y][face].read  = true;
               signal_dspin_bound_rsp_out[x][y][face].write = false;
               signal_dspin_bound_rsp_out[x][y][face].read  = true;

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

   sc_start(sc_core::sc_time(1, SC_NS));
   signal_resetn = true;

   if (debug_ok) {
      #if USE_OPENMP
         assert(false && "OPEN MP should not be used with debug because of its traces");
      #endif

      if (gettimeofday(&t1, NULL) != 0) {
         perror("gettimeofday");
         return EXIT_FAILURE;
      }

      for (int64_t n = 1; n < ncycles && !stop_called; n++)
      {
         if ((n % max_cycles) == 0)
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


         if (n == reset_counters) {
            for (size_t x = 0; x < (X_SIZE); x++) {
               for (size_t y = 0; y < Y_SIZE; y++) {
                  clusters[x][y]->memc->reset_counters();
               }
            }
         }

         if (n == dump_counters) {
            for (size_t x = 0; x < (X_SIZE); x++) {
               for (size_t y = 0; y < Y_SIZE; y++) {
                  clusters[x][y]->memc->print_stats(true, false);
               }
            }
         }

         if ((n > debug_from) and (n % debug_period == 0))
         {
            std::cout << "****************** cycle " << std::dec << n ;
            std::cout << "************************************************" << std::endl;

            for (size_t x = 0; x < X_SIZE ; x++){
               for (size_t y = 0; y < Y_SIZE ; y++){
                  for (int proc = 0; proc < NB_PROCS_MAX; proc++) {

                     clusters[x][y]->proc[proc]->print_trace();
                     std::ostringstream proc_signame;
                     proc_signame << "[SIG]PROC_" << x << "_" << y << "_" << proc ;
                     std::ostringstream p2m_signame;
                     p2m_signame << "[SIG]PROC_" << x << "_" << y << "_" << proc << " P2M";
                     std::ostringstream m2p_signame;
                     m2p_signame << "[SIG]PROC_" << x << "_" << y << "_" << proc << " M2P";

                     clusters[x][y]->signal_vci_ini_proc[proc].print_trace(proc_signame.str());
                     clusters[x][y]->signal_dspin_p2m_proc[proc].print_trace(p2m_signame.str());
                     clusters[x][y]->signal_dspin_m2p_proc[proc].print_trace(m2p_signame.str());
                  }

                  clusters[x][y]->memc->print_trace();

                  std::ostringstream smemc;
                  smemc << "[SIG]MEMC_" << x << "_" << y;
                  std::ostringstream sxram;
                  sxram << "[SIG]XRAM_" << x << "_" << y;
                  std::ostringstream sm2p;
                  sm2p << "[SIG]MEMC_" << x << "_" << y << " M2P";
                  std::ostringstream sp2m;
                  sp2m << "[SIG]MEMC_" << x << "_" << y << " P2M";

                  clusters[x][y]->signal_vci_tgt_memc.print_trace(smemc.str());
                  clusters[x][y]->signal_vci_xram.print_trace(sxram.str());
                  clusters[x][y]->signal_dspin_p2m_memc.print_trace(sp2m.str());
                  clusters[x][y]->signal_dspin_m2p_memc.print_trace(sm2p.str());
               }
            }
         }

         sc_start(sc_core::sc_time(1, SC_NS));
      }
   }
   else {
      int64_t n = 0;
      while (!stop_called && n != ncycles) {
         if (gettimeofday(&t1, NULL) != 0) {
            perror("gettimeofday");
            return EXIT_FAILURE;
         }
         int64_t nb_cycles = min(max_cycles, ncycles - n);
         if (do_reset_counters) {
            nb_cycles = min(nb_cycles, reset_counters - n);
         }
         if (do_dump_counters) {
            nb_cycles = min(nb_cycles, dump_counters - n);
         }

         sc_start(sc_core::sc_time(nb_cycles, SC_NS));
         n += nb_cycles;

         if (do_reset_counters && n == reset_counters) {
            // Reseting counters
            for (size_t x = 0; x < (X_SIZE); x++) {
               for (size_t y = 0; y < Y_SIZE; y++) {
                  clusters[x][y]->memc->reset_counters();
               }
            }
            do_reset_counters = false;
         }

         if (do_dump_counters && n == dump_counters) {
            // Dumping counters
            for (size_t x = 0; x < (X_SIZE); x++) {
               for (size_t y = 0; y < Y_SIZE; y++) {
                  clusters[x][y]->memc->print_stats(true, false);
               }
            }
            do_dump_counters = false;
         }


         if (gettimeofday(&t2, NULL) != 0) {
            perror("gettimeofday");
            return EXIT_FAILURE;
         }
         ms1 = (uint64_t) t1.tv_sec * 1000ULL + (uint64_t) t1.tv_usec / 1000;
         ms2 = (uint64_t) t2.tv_sec * 1000ULL + (uint64_t) t2.tv_usec / 1000;
         std::cerr << std::dec << "cycle " << n << " platform clock frequency " << (double) nb_cycles / (double) (ms2 - ms1) << "Khz" << std::endl;
      }
   }


   // Free memory
   for (size_t i = 0; i  < (X_SIZE * Y_SIZE); i++)
   {
      size_t x = i / Y_SIZE;
      size_t y = i % Y_SIZE;
      delete clusters[x][y];
   }

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_cmd_inc, X_SIZE-1, Y_SIZE);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_cmd_dec, X_SIZE-1, Y_SIZE);

   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_h_rsp_inc, X_SIZE-1, Y_SIZE);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_h_rsp_dec, X_SIZE-1, Y_SIZE);

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_m2p_inc, X_SIZE-1, Y_SIZE);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_m2p_dec, X_SIZE-1, Y_SIZE);

   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_h_p2m_inc, X_SIZE-1, Y_SIZE);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_h_p2m_dec, X_SIZE-1, Y_SIZE);

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_cla_inc, X_SIZE-1, Y_SIZE);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_cla_dec, X_SIZE-1, Y_SIZE);

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_cmd_inc, X_SIZE, Y_SIZE-1);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_cmd_dec, X_SIZE, Y_SIZE-1);

   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_v_rsp_inc, X_SIZE, Y_SIZE-1);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_v_rsp_dec, X_SIZE, Y_SIZE-1);

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_m2p_inc, X_SIZE, Y_SIZE-1);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_m2p_dec, X_SIZE, Y_SIZE-1);

   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_v_p2m_inc, X_SIZE, Y_SIZE-1);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_v_p2m_dec, X_SIZE, Y_SIZE-1);

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_cla_inc, X_SIZE, Y_SIZE-1);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_cla_dec, X_SIZE, Y_SIZE-1);

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_bound_cmd_in, X_SIZE, Y_SIZE, 4);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_bound_cmd_out, X_SIZE, Y_SIZE, 4);

   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_bound_rsp_in, X_SIZE, Y_SIZE, 4);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_bound_rsp_out, X_SIZE, Y_SIZE, 4);

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_bound_m2p_in, X_SIZE, Y_SIZE, 4);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_bound_m2p_out, X_SIZE, Y_SIZE, 4);

   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_bound_p2m_in, X_SIZE, Y_SIZE, 4);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_bound_p2m_out, X_SIZE, Y_SIZE, 4);

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_bound_cla_in, X_SIZE, Y_SIZE, 4);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_bound_cla_out, X_SIZE, Y_SIZE, 4);

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
