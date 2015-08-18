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
//               Parallelisation
///////////////////////////////////////////////////
#if USE_OPENMP
#include <omp.h>
#endif

//  cluster index (computed from x,y coordinates)
#define cluster(x,y)   (y + ymax * x)

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
#define vci_address_width     32
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
////////////////////////////////////////////////////////////
#define CLUSTER_X             1
#define CLUSTER_Y             1
#define NB_CLUSTERS           1
#define NB_PROCS_MAX          4
#define NB_DMA_CHANNELS       1
#define NB_TTY_CHANNELS       4
#define NB_IOC_CHANNELS       1
#define NB_NIC_CHANNELS       1
#define NB_CMA_CHANNELS       0
#define USE_XICU              1
#define IOMMU_ACTIVE          0

////////////////////////////////////////////////////////////
//    Secondary Hardware Parameters         
////////////////////////////////////////////////////////////
#define XRAM_LATENCY          0

#define MEMC_WAYS             16
#define MEMC_SETS             256

#define L1_IWAYS              4
#define L1_ISETS              64
#define L1_DWAYS              4
#define L1_DSETS              64

#define FBUF_X_SIZE           512
#define FBUF_Y_SIZE           512

#define BDEV_SECTOR_SIZE      4096
#define BDEV_IMAGE_NAME       "hdd-img.bin"

#define NIC_RX_NAME           "rx_packets.txt"
#define NIC_TX_NAME           "tx_packets.txt"
#define NIC_TIMEOUT           10000

#define NORTH                 0
#define SOUTH                 1
#define EAST                  2
#define WEST                  3

////////////////////////////////////////////////////////////
//    Software to be loaded in ROM/RAM         
////////////////////////////////////////////////////////////
#define soft_name       "bootloader.bin",\
                        "kernel-soclib.bin@0xbfc10000:D",\
                        "arch-info.bin@0xBFC08000:D"

////////////////////////////////////////////////////////////
//     DEBUG Parameters default values         
////////////////////////////////////////////////////////////
#define MAX_FROZEN_CYCLES     100000000

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

/////////////////////////////////////////////////////////
//    Physical segments definition
/////////////////////////////////////////////////////////
// There is 3 segments replicated in all clusters
// and 5 specific segments in the "IO" cluster
// (containing address 0xBF000000)
/////////////////////////////////////////////////////////
// Physical Address Decoding: 8 GID + 8 LID + 16 offset.
/////////////////////////////////////////////////////////
#define RAM_BASE        0x00000000
#define RAM_SIZE        0x00C00000

#define BROM_BASE       0xBFC00000
#define BROM_SIZE       0x00100000

#define FBUF_BASE       0xBFD00000
#define FBUF_SIZE       0x00200000

#define XICU_BASE       0x00F00000
#define XICU_SIZE       0x00002000

#define BDEV_BASE       0xBFF10000
#define BDEV_SIZE       0x00000100

#define MTTY_BASE       0xBFF20000
#define MTTY_SIZE       0x00000100

#define MDMA_BASE       0x00F30000
#define MDMA_SIZE       0x00001000 * NB_DMA_CHANNELS  // 4 Kbytes per channel

#define MEMC_BASE       0x00F40000
#define MEMC_SIZE       0x00001000

#define SIMH_BASE       0xBFF50000
#define SIMH_SIZE       0x00001000

#define CDMA_BASE       0xBFF60000
#define CDMA_SIZE       0x00000100

#define MNIC_BASE       0xB0F80000
#define MNIC_SIZE       0x00080000   // 512 Kbytes (for 8 channels)

bool stop_called = false;

/////////////////////////////////
int _main(int argc, char *argv[])
{
   using namespace sc_core;
   using namespace soclib::caba;
   using namespace soclib::common;

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
   size_t   xmax             = CLUSTER_X;          // number of clusters in a row
   size_t   ymax             = CLUSTER_Y;          // number of clusters in a column
   size_t   nprocs           = NB_PROCS_MAX;		   // number of processors per cluster
   size_t   xfb              = FBUF_X_SIZE;	      // frameBuffer column number
   size_t   yfb              = FBUF_Y_SIZE;        // frameBuffer lines number
   size_t   fb_mode          = 420;
   size_t   ram_size         = RAM_SIZE;
   size_t   blk_size         = BDEV_SECTOR_SIZE;
   size_t   l1_i_ways        = L1_IWAYS;
   size_t   l1_d_ways        = L1_DWAYS;
   size_t   l1_i_sets        = L1_ISETS;
   size_t   l1_d_sets        = L1_DSETS;
   size_t   memc_sets        = MEMC_SETS;
   size_t   memc_ways        = MEMC_WAYS;
   size_t   xram_latency     = XRAM_LATENCY;
   size_t   xicu_base        = XICU_BASE;
   size_t   mdma_base        = MDMA_BASE;
   size_t   memc_base        = MEMC_BASE;
   bool     isRamSizeSet     = false;
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
         else if( (strcmp(argv[n],"-NPROCS") == 0) && (n+1<argc) )
         {
            nprocs = atoi(argv[n+1]);
            assert( ((nprocs == 1) || (nprocs == 2) || (nprocs == 4)) &&
                    "NPROCS must be equal to 1, 2, or 4");
         }
         else if ((strcmp(argv[n], "-THREADS") == 0) && ((n + 1) < argc))
         {
            threads_nr = atoi(argv[n + 1]);
            threads_nr = (threads_nr < 1) ? 1 : threads_nr;
         }
         else if( (strcmp(argv[n],"-XMAX") == 0) && (n+1<argc) )
         {
            xmax = atoi(argv[n+1]);
            assert( ((xmax == 1) || (xmax == 2) || (xmax == 4) || (xmax == 8) || (xmax == 16)) 
                         && "The XMAX parameter must be 2, 4, 8, or 16" );
         }
         else if( (strcmp(argv[n],"-YMAX") == 0) && (n+1<argc) )
         {
            ymax = atoi(argv[n+1]);
            assert( ((ymax == 1) || (ymax == 2) || (ymax == 4) || (ymax == 8) || (ymax == 16)) 
                         && "The YMAX parameter must be 2, 4, 8, or 16" );
         }
         else if((strcmp(argv[n], "-MEMSZ") == 0) && (n+1 < argc))
         {
            ram_size = atoi(argv[n+1]);
            isRamSizeSet = true;
         }
         else if((strcmp(argv[n], "-MCWAYS") == 0) && (n+1 < argc))
         {
            memc_ways = atoi(argv[n+1]);
         }
         else if((strcmp(argv[n], "-MCSETS") == 0) && (n+1 < argc))
         {
            memc_sets = atoi(argv[n+1]);
         }
         else if((strcmp(argv[n], "-L1_IWAYS") == 0) && (n+1 < argc))
         {
            l1_i_ways = atoi(argv[n+1]);
         }
         else if((strcmp(argv[n], "-L1_ISETS") == 0) && (n+1 < argc))
	      {
            l1_i_sets = atoi(argv[n+1]);
         }
         else if((strcmp(argv[n], "-L1_DWAYS") == 0) && (n+1 < argc))
	      {
            l1_d_ways = atoi(argv[n+1]);
         }
         else if((strcmp(argv[n], "-L1_DSETS") == 0) && (n+1 < argc))
	      {
            l1_d_sets = atoi(argv[n+1]);
         }
         else if((strcmp(argv[n], "-XLATENCY") == 0) && (n+1 < argc))
	      {
            xram_latency = atoi(argv[n+1]);
         }
         else if( (strcmp(argv[n],"-XFB") == 0) && (n+1<argc) )
         {
            xfb = atoi(argv[n+1]);
         }
         else if( (strcmp(argv[n],"-YFB") == 0) && (n+1<argc) )
         {
            yfb = atoi(argv[n+1]);
         }
         else if( (strcmp(argv[n], "-FBMODE") == 0) && (n+1 < argc))
         {
            fb_mode = atoi(argv[n+1]);
         }
         else if ((strcmp(argv[n], "-SOFT") == 0) && (n + 1 < argc))
         {
            std::cerr << "Warning: -SOFT is useless when using Almos, ignored" << std::endl;
         }
         else if ((strcmp(argv[n],"-DISK") == 0) && (n + 1 < argc))
         {
            strcpy(disk_name, argv[n + 1]);
         }
         else if( (strcmp(argv[n],"-BLKSZ") == 0) && (n+1<argc) )
         {
            blk_size = atoi(argv[n+1]);
            assert(((blk_size % 512) == 0) && "BDEV: Block size must be multiple of 512 bytes");
         }
         else if ((strcmp(argv[n],"-DEBUG") == 0) && (n + 1 < argc))
         {
            debug_ok = true;
            debug_from = atoi(argv[n + 1]);
         }
         else if ((strcmp(argv[n], "-MEMCID") == 0) && (n + 1 < argc))
         {
            debug_memc_id = atoi(argv[n + 1]);
            assert((debug_memc_id < (xmax * ymax)) && 
                   "debug_memc_id larger than xmax * ymax" );
         }
         else if ((strcmp(argv[n], "-PROCID") == 0) && (n + 1 < argc))
         {
            debug_proc_id = atoi(argv[n + 1]);
            assert((debug_proc_id < (xmax * ymax * nprocs)) && 
                   "debug_proc_id larger than XMAX * ymax * NB_PROCS");
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
            std::cout << "     -NCYCLES number_of_simulated_cycles" << std::endl;
            std::cout << "     -NPROCS number_of_processors_per_cluster" << std::endl;
            std::cout << "     -THREADS simulator's openmp threads number" << std::endl;
            std::cout << "     -XMAX number_of_clusters_in_a_row" << std::endl;
            std::cout << "     -YMAX number_of_clusters_in_a_column" << std::endl;
            std::cout << "     -MCWAYS memory_cache_number_of_ways" << std::endl;
            std::cout << "     -MCSETS memory_cache_number_of_sets" << std::endl;
            std::cout << "     -L1_IWAYS L1_instruction_cache_number_of_ways" << std::endl;
            std::cout << "     -L1_ISETS L1_instruction_cache_number_of_sets" << std::endl;
            std::cout << "     -L1_DWAYS L1_data_cache_number_of_ways" << std::endl;
            std::cout << "     -XLATENCY external_ram_latency_value" << std::endl;
            std::cout << "     -XFB fram_buffer_number_of_pixels" << std::endl;
            std::cout << "     -YFB fram_buffer_number_of_lines" << std::endl;
            std::cout << "     -FBMODE fram buffer subsampling integer value "
               "(YUV:420,YUV:422,RGB:0,RGB:16,RGB:32,RGBPAL:256)" << std::endl;
            std::cout << "     -MEMSZ per-cluster memory size ( <= 12 MB when using Almos)" << std::endl;
            std::cout << "     -L1_DSETS L1_data_cache_number_of_sets" << std::endl;
            std::cout << "     -SOFT pathname_for_embedded_soft (GIET only)" << std::endl;
            std::cout << "     -DISK pathname_for_disk_image" << std::endl;
            std::cout << "     -BLKSZ sector size in bytes ( must be multiple of 512 bytes )" << std::endl;
            std::cout << "     -DEBUG debug_start_cycle" << std::endl;
            std::cout << "     -FROZEN max_number_of_lines" << std::endl;
            std::cout << "     -PERIOD number_of_cycles between trace" << std::endl;
            std::cout << "     -MEMCID index_memc_to_be_traced" << std::endl;
            std::cout << "     -PROCID index_proc_to_be_traced" << std::endl;
            exit(0);
         }
      }
   }

    // checking hardware parameters
    assert( ( (xmax == 1) or (xmax == 2) or (xmax == 4) or
              (xmax == 8) or (xmax == 16) ) and
              "The XMAX parameter must be 1, 2, 4, 8 or 16" );

    assert( ( (ymax == 1) or (ymax == 2) or (ymax == 4) or
              (ymax == 8) or (ymax == 16) ) and
              "The YMAX parameter must be 1, 2, 4, 8 or 16" );

    assert( ( (nprocs == 1) or (nprocs == 2) or
              (nprocs == 4) or (nprocs == 8) ) and
             "The nprocs parameter must be 1, 2, 4 or 8" );

    assert( (NB_DMA_CHANNELS < 9) and
            "The NB_DMA_CHANNELS parameter must be smaller than 9" );

    assert( (NB_TTY_CHANNELS < 15) and
            "The NB_TTY_CHANNELS parameter must be smaller than 15" );

    assert( (NB_NIC_CHANNELS < 9) and
            "The NB_NIC_CHANNELS parameter must be smaller than 9" );

    assert( (vci_address_width == 32) and
            "VCI address width with ALMOS must be 32 bits" );

    std::cout << std::endl;
    std::cout << " - XMAX             = " << xmax << std::endl;
    std::cout << " - YMAX             = " << ymax << std::endl;
    std::cout << " - NPROCS           = " << nprocs <<  std::endl;
    std::cout << " - NB_DMA_CHANNELS  = " << NB_DMA_CHANNELS <<  std::endl;
    std::cout << " - NB_TTY_CHANNELS  = " << NB_TTY_CHANNELS <<  std::endl;
    std::cout << " - NB_NIC_CHANNELS  = " << NB_NIC_CHANNELS <<  std::endl;
    std::cout << " - MEMC_WAYS        = " << memc_ways << std::endl;
    std::cout << " - MEMC_SETS        = " << memc_sets << std::endl;
    std::cout << " - RAM_SIZE         = " << ram_size << std::endl;
    std::cout << " - RAM_LATENCY      = " << xram_latency << std::endl;
    std::cout << " - MAX_FROZEN       = " << frozen_cycles << std::endl;
    std::cout << "[PROCS] " << nprocs * xmax * ymax << std::endl;

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

   if      (xmax == 1) x_width = 0;
   else if (xmax == 2) x_width = 1;
   else if (xmax <= 4) x_width = 2;
   else if (xmax <= 8) x_width = 3;
   else                x_width = 4;

   if      (ymax == 1) y_width = 0;
   else if (ymax == 2) y_width = 1;
   else if (ymax <= 4) y_width = 2;
   else if (ymax <= 8) y_width = 3;
   else                y_width = 4;

   if((xmax == 1) && (ymax == 1))
   {
      cluster_io_id = 0;
      ram_size      = (isRamSizeSet == true) ? ram_size : 0x8000000;
      xicu_base     = 0x80f00000;
      memc_base     = 0x80f40000;
      mdma_base     = 0x80f30000;
   }
   else
      cluster_io_id = 0xbfc00000 >> (vci_address_width - x_width - y_width); // index of cluster containing IOs

   /////////////////////
   //  Mapping Tables
   /////////////////////

   // internal network
   MappingTable maptabd(vci_address_width,
                        IntTab(x_width + y_width, 16 - x_width - y_width), 
                        IntTab(x_width + y_width, vci_srcid_width - x_width - y_width), 
                        0x00FFFF0000);

   for (size_t x = 0; x < xmax; x++)
   {
      for (size_t y = 0; y < ymax; y++)
      {
         sc_uint<vci_address_width> offset;
         offset = (sc_uint<vci_address_width>)cluster(x,y) 
                   << (vci_address_width-x_width-y_width);

         std::ostringstream    si;
         si << "seg_xicu_" << x << "_" << y;
         maptabd.add(Segment(si.str(), xicu_base + offset, XICU_SIZE, 
                  IntTab(cluster(x,y),XICU_TGTID), false));

         std::ostringstream    sd;
         sd << "seg_mdma_" << x << "_" << y;
         maptabd.add(Segment(sd.str(), mdma_base + offset, MDMA_SIZE, 
                  IntTab(cluster(x,y),MDMA_TGTID), false));

         std::ostringstream    sh;
         sh << "seg_ram_" << x << "_" << y;
         maptabd.add(Segment(sh.str(), RAM_BASE + offset, ram_size, 
                  IntTab(cluster(x,y),MEMC_TGTID), true));

         std::ostringstream    sconf;
         sconf << "seg_memc_config_" << x << "_" << y;
         maptabd.add(Segment(sconf.str(), memc_base + offset, MEMC_SIZE, 
                             IntTab(cluster(x,y),MEMC_TGTID), true, true));

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
                        0x00FFFF0000ULL);

   for (size_t x = 0; x < xmax; x++)
   {
      for (size_t y = 0; y < ymax ; y++)
      {

         sc_uint<vci_address_width> offset;
         offset = (sc_uint<vci_address_width>)cluster(x,y) 
                   << (vci_address_width-x_width-y_width);

         std::ostringstream sh;
         sh << "x_seg_memc_" << x << "_" << y;

         maptabx.add(Segment(sh.str(), RAM_BASE + offset, 
                     ram_size, IntTab(cluster(x,y)), false));
      }
   }
   std::cout << maptabx << std::endl;

   ////////////////////
   // Signals
   ///////////////////

   std::cout << "Clock  .. ";
   sc_clock           signal_clk("clk");
   std::cout << ". [OK]" << std::endl;
   sc_signal<bool>    signal_resetn("resetn");

   // Horizontal inter-clusters DSPIN signals
   DspinSignals<dspin_cmd_width>*** signal_dspin_h_cmd_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cmd_inc", xmax-1, ymax, 3);

   DspinSignals<dspin_cmd_width>*** signal_dspin_h_cmd_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_h_cmd_dec", xmax-1, ymax, 3);

   DspinSignals<dspin_rsp_width>*** signal_dspin_h_rsp_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_rsp_inc", xmax-1, ymax, 2);
   DspinSignals<dspin_rsp_width>*** signal_dspin_h_rsp_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_h_rsp_dec", xmax-1, ymax, 2);

   // Vertical inter-clusters DSPIN signals
   DspinSignals<dspin_cmd_width>*** signal_dspin_v_cmd_inc =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cmd_inc", xmax, ymax-1, 3);
   DspinSignals<dspin_cmd_width>*** signal_dspin_v_cmd_dec =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_v_cmd_dec", xmax, ymax-1, 3);
   DspinSignals<dspin_rsp_width>*** signal_dspin_v_rsp_inc =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_rsp_inc", xmax, ymax-1, 2);
   DspinSignals<dspin_rsp_width>*** signal_dspin_v_rsp_dec =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_v_rsp_dec", xmax, ymax-1, 2);

   // Mesh boundaries DSPIN signals
   DspinSignals<dspin_cmd_width>**** signal_dspin_false_cmd_in =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_false_cmd_in" , xmax, ymax, 4, 3);
   DspinSignals<dspin_cmd_width>**** signal_dspin_false_cmd_out =
      alloc_elems<DspinSignals<dspin_cmd_width> >("signal_dspin_false_cmd_out", xmax, ymax, 4, 3);
   DspinSignals<dspin_rsp_width>**** signal_dspin_false_rsp_in =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_false_rsp_in" , xmax, ymax, 4, 2);
   DspinSignals<dspin_rsp_width>**** signal_dspin_false_rsp_out =
      alloc_elems<DspinSignals<dspin_rsp_width> >("signal_dspin_false_rsp_out", xmax, ymax, 4, 2);

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
                   vci_param_ext>*          clusters[xmax][ymax];

#if USE_OPENMP
#pragma omp parallel
    {
#pragma omp for
#endif
        for (size_t i = 0; i  < (xmax * ymax); i++)
        {
            size_t x = i / ymax;
            size_t y = i % ymax;

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
                nprocs,
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
                memc_ways,
                memc_sets,
                l1_i_ways,
                l1_i_sets,
                l1_d_ways,
                l1_d_sets,
                xram_latency,
                (cluster(x,y) == cluster_io_id),
                xfb,
                yfb,
                disk_name,
                blk_size,
                NB_NIC_CHANNELS,
                nic_rx_name,
                nic_tx_name,
                NIC_TIMEOUT,
                NB_CMA_CHANNELS,
                loader,
                frozen_cycles,
                debug_from   ,
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
   for (size_t x = 0; x < (xmax); x++){
      for (size_t y = 0; y < ymax; y++){
         clusters[x][y]->p_clk                         (signal_clk);
         clusters[x][y]->p_resetn                      (signal_resetn);
      }
   }

   // Inter Clusters horizontal connections
   if (xmax > 1){
      for (size_t x = 0; x < (xmax-1); x++){
         for (size_t y = 0; y < ymax; y++){
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
   if (ymax > 1) {
      for (size_t y = 0; y < (ymax-1); y++){
         for (size_t x = 0; x < xmax; x++){
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
   for (size_t y = 0; y < ymax; y++)
   {
      for (size_t k = 0; k < 3; k++)
      {
         clusters[0][y]->p_cmd_in[WEST][k]        (signal_dspin_false_cmd_in[0][y][WEST][k]);
         clusters[0][y]->p_cmd_out[WEST][k]       (signal_dspin_false_cmd_out[0][y][WEST][k]);
         clusters[xmax-1][y]->p_cmd_in[EAST][k]   (signal_dspin_false_cmd_in[xmax-1][y][EAST][k]);
         clusters[xmax-1][y]->p_cmd_out[EAST][k]  (signal_dspin_false_cmd_out[xmax-1][y][EAST][k]);
      }

      for (size_t k = 0; k < 2; k++)
      {
         clusters[0][y]->p_rsp_in[WEST][k]        (signal_dspin_false_rsp_in[0][y][WEST][k]);
         clusters[0][y]->p_rsp_out[WEST][k]       (signal_dspin_false_rsp_out[0][y][WEST][k]);
         clusters[xmax-1][y]->p_rsp_in[EAST][k]   (signal_dspin_false_rsp_in[xmax-1][y][EAST][k]);
         clusters[xmax-1][y]->p_rsp_out[EAST][k]  (signal_dspin_false_rsp_out[xmax-1][y][EAST][k]);
      }
   }

   // North & South boundary clusters connections
   for (size_t x = 0; x < xmax; x++)
   {
      for (size_t k = 0; k < 3; k++)
      {
         clusters[x][0]->p_cmd_in[SOUTH][k]       (signal_dspin_false_cmd_in[x][0][SOUTH][k]);
         clusters[x][0]->p_cmd_out[SOUTH][k]      (signal_dspin_false_cmd_out[x][0][SOUTH][k]);
         clusters[x][ymax-1]->p_cmd_in[NORTH][k]  (signal_dspin_false_cmd_in[x][ymax-1][NORTH][k]);
         clusters[x][ymax-1]->p_cmd_out[NORTH][k] (signal_dspin_false_cmd_out[x][ymax-1][NORTH][k]);
      }

      for (size_t k = 0; k < 2; k++)
      {
         clusters[x][0]->p_rsp_in[SOUTH][k]       (signal_dspin_false_rsp_in[x][0][SOUTH][k]);
         clusters[x][0]->p_rsp_out[SOUTH][k]      (signal_dspin_false_rsp_out[x][0][SOUTH][k]);
         clusters[x][ymax-1]->p_rsp_in[NORTH][k]  (signal_dspin_false_rsp_in[x][ymax-1][NORTH][k]);
         clusters[x][ymax-1]->p_rsp_out[NORTH][k] (signal_dspin_false_rsp_out[x][ymax-1][NORTH][k]);
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
   for (size_t x = 0; x < xmax ; x++){
      for (size_t y = 0; y < ymax ; y++){
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

#define STATS_CYCLES 10000000

   sc_start(sc_core::sc_time(1, SC_NS));
   signal_resetn = true;

   uint64_t n = 0;
   
   while (!stop_called) {
   	if (gettimeofday(&t1, NULL) != 0) {
   		perror("gettimeofday");
   		return EXIT_FAILURE;
   	}
   	sc_start(STATS_CYCLES);
   	n += STATS_CYCLES;
   	if (gettimeofday(&t2, NULL) != 0) {
   		perror("gettimeofday");
   		return EXIT_FAILURE;
   	}
   	ms1 = (uint64_t)t1.tv_sec * 1000ULL + (uint64_t)t1.tv_usec / 1000;
   	ms2 = (uint64_t)t2.tv_sec * 1000ULL + (uint64_t)t2.tv_usec / 1000;
   	std::cerr << "cycle " << n 
                << " platform clock frequency " 
                << (double)STATS_CYCLES / (double)(ms2 - ms1) 
                << "Khz" << std::endl;
   }
   
   // Free memory
   for (size_t i = 0; i  < (xmax * ymax); i++)
   {
      size_t x = i / ymax;
      size_t y = i % ymax;
      delete clusters[x][y];
   }

   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_cmd_inc, xmax - 1, ymax, 3);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_h_cmd_dec, xmax - 1, ymax, 3);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_h_rsp_inc, xmax - 1, ymax, 2);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_h_rsp_dec, xmax - 1, ymax, 2);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_cmd_inc, xmax, ymax - 1, 3);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_v_cmd_dec, xmax, ymax - 1, 3);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_v_rsp_inc, xmax, ymax - 1, 2);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_v_rsp_dec, xmax, ymax - 1, 2);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_false_cmd_in, xmax, ymax, 4, 3);
   dealloc_elems<DspinSignals<dspin_cmd_width> >(signal_dspin_false_cmd_out, xmax, ymax, 4, 3);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_false_rsp_in, xmax, ymax, 4, 2);
   dealloc_elems<DspinSignals<dspin_rsp_width> >(signal_dspin_false_rsp_out, xmax, ymax, 4, 2);

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
