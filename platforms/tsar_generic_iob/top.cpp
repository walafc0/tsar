///////////////////////////////////////////////////////////////////////////////
// File: top.cpp  (for tsar_generic_iob platform)
// Author: Alain Greiner
// Copyright: UPMC/LIP6
// Date : august 2013 / updated march 2015
// This program is released under the GNU public license
///////////////////////////////////////////////////////////////////////////////
// This file define a generic TSAR architecture with an external IO network 
// emulating a PCI or Hypertransport I/O bus to access 7 external peripherals:
//
// - BROM : boot ROM
// - FBUF : Frame Buffer
// - MTTY : multi TTY (one channel)
// - MNIC : Network controller (up to 2 channels)
// - CDMA : Chained Buffer DMA controller (up to 4 channels)
// - DISK : Block device controler (BDV / HBA / SDC)
// - IOPI : HWI to SWI translator.
//
// This I/0 bus is connected to internal address space through two IOB bridges
// located in cluster[0][0] and cluster[X_SIZE-1][Y_SIZE-1].
// 
// The internal physical address space is 40 bits, and the cluster index
// is defined by the 8 MSB bits, using a fixed format: X is encoded on 4 bits,
// Y is encoded on 4 bits, whatever the actual mesh size.
// => at most 16 * 16 clusters. Each cluster contains up to 4 processors.
//
// It contains 3 networks:
//
// 1) the "INT" network supports Read/Write transactions
//    between processors and L2 caches or peripherals.
//    (VCI ADDDRESS = 40 bits / VCI DATA width = 32 bits)
//    It supports also coherence transactions between L1 & L2 caches.
// 3) the "RAM" network emulates the 3D network between L2 caches
//    and L3 caches, and is implemented as a 2D mesh between the L2 caches,
//    the two IO bridges and the physical RAMs disributed in all clusters.
//    (VCI ADDRESS = 40 bits / VCI DATA = 64 bits)
// 4) the IOX network connects the two IO bridge components to the
//    7 external peripheral controllers.
//    (VCI ADDDRESS = 40 bits / VCI DATA width = 64 bits)
//
// The external peripherals HWI IRQs are translated to WTI IRQs by the
// external IOPIC component, that must be configured by the OS to route
// these WTI IRQS to one or several internal XICU components.
// - IOPIC HWI[1:0]     connected to IRQ_NIC_RX[1:0]
// - IOPIC HWI[3:2]     connected to IRQ_NIC_TX[1:0]
// - IOPIC HWI[7:4]     connected to IRQ_CMA_TX[3:0]]
// - IOPIC HWI[8]       connected to IRQ_DISK
// - IOPIC HWI[31:16]   connected to IRQ_TTY_RX[15:0]
//
// Each cluster contains the following component:
// - From 1 to 8 MIP32 processors
// - One L2 cache controller
// - One XICU component,
// - One - optional - single channel DMA controler,
// - One - optional - hardware coprocessor 
// The XICU component is mainly used to handle WTI IRQs, as at most 
// 2 HWI IRQs are connected to XICU in each cluster:
// - IRQ_IN[0]            : MMC
// - IRQ_IN[1]            : MWR 
//
// All clusters are identical, but cluster(0,0) and cluster(XMAX-1,YMAX-1)
// contain an extra IO bridge component. These IOB0 & IOB1 components are
// connected to the three networks (INT, RAM, IOX).
//
// - It uses two dspin_local_crossbar per cluster to implement the
//   local interconnect correponding to the INT network.
// - It uses three dspin_local_crossbar per cluster to implement the
//   local interconnect correponding to the coherence INT network.
// - It uses two virtual_dspin_router per cluster to implement
//   the INT network (routing both the direct and coherence trafic).
// - It uses two dspin_router per cluster to implement the RAM network.
// - It uses the vci_cc_vcache_wrapper.
// - It uses the vci_mem_cache.
// - It contains one vci_xicu and one vci_multi_dma per cluster.
// - It contains one vci_simple ram per cluster to model the L3 cache.
//
// The TsarIobCluster component is defined in files
// tsar_iob_cluster.* (with * = cpp, h, sd)
//
// The main hardware parameters must be defined in the hard_config.h file :
// - X_WIDTH          : number of bits for x cluster coordinate
// - Y_WIDTH          : number of bits for y cluster coordinate
// - P_WIDTH          : number of bits for local processor coordinate
// - X_SIZE           : number of clusters in a row
// - Y_SIZE           : number of clusters in a column
// - NB_PROCS_MAX     : number of processors per cluster (up to 8)
// - NB_DMA_CHANNELS  : number of DMA channels per cluster    (>= NB_PROCS_MAX)
// - NB_TTY_CHANNELS  : number of TTY channels in I/O network (up to 16)
// - NB_NIC_CHANNELS  : number of NIC channels in I/O network (up to 2)
// - NB_CMA_CHANNELS  : number of CMA channels in I/O network (up to 4)
// - FBUF_X_SIZE      : width of frame buffer (pixels)
// - FBUF_Y_SIZE      : heigth of frame buffer (lines)
// - XCU_NB_HWI       : number of XCU HWIs (>= NB_PROCS_MAX + 1)
// - XCU_NB_PTI       : number of XCU PTIs (>= NB_PROCS_MAX)
// - XCU_NB_WTI       : number of XCU WTIs (>= 4*NB_PROCS_MAX)
// - XCU_NB_OUT       : number of XCU output IRQs (>= 4*NB_PROCS_MAX)
// - USE_IOC_XYZ      : IOC type (XYZ in HBA / BDV / SDC)
//
// Some other hardware parameters must be defined in this top.cpp file:
// - XRAM_LATENCY     : external ram latency
// - MEMC_WAYS        : L2 cache number of ways
// - MEMC_SETS        : L2 cache number of sets
// - L1_IWAYS
// - L1_ISETS
// - L1_DWAYS
// - L1_DSETS
// - DISK_IMAGE_NAME  : file pathname for block device
//
// General policy for 40 bits physical address decoding:
// All physical segments base addresses are multiple of 1 Mbytes
// (=> the 24 LSB bits = 0, and the 16 MSB bits define the target)
// The (x_width + y_width) MSB bits (left aligned) define
// the cluster index, and the LADR bits define the local index:
//      |X_ID|Y_ID|  LADR  |     OFFSET          |
//      |  4 |  4 |   8    |       24            |
//
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



#include "tsar_iob_cluster.h"
#include "vci_chbuf_dma.h"
#include "vci_multi_tty.h"
#include "vci_multi_nic.h"
#include "vci_simple_rom.h"
#include "vci_multi_ahci.h"
#include "vci_block_device_tsar.h"
#include "vci_ahci_sdc.h"
#include "sd_card.h"
#include "vci_framebuffer.h"
#include "vci_iox_network.h"
#include "vci_iopic.h"

#include "alloc_elems.h"


//////////////////////////////////////////////////////////////////
//    Coprocessor type (must be replicated in tsar_iob_cluster)
//////////////////////////////////////////////////////////////////

#define MWR_COPROC_CPY  0
#define MWR_COPROC_DCT  1
#define MWR_COPROC_GCD  2

//////////////////////////////////////////////////////////////////
//      For ALMOS
//////////////////////////////////////////////////////////////////

#define USE_ALMOS 0

#define almos_bootloader_pathname "bootloader.bin"
#define almos_kernel_pathname     "kernel-soclib.bin@0xbfc10000:D"
#define almos_archinfo_pathname   "arch-info.bin@0xBFC08000:D"

//////////////////////////////////////////////////////////////////
//        Parallelisation
//////////////////////////////////////////////////////////////////

#if USE_OPENMP
#include <omp.h>
#endif

//////////////////////////////////////////////////////////////////
//          DSPIN parameters
//////////////////////////////////////////////////////////////////

#define dspin_int_cmd_width   39
#define dspin_int_rsp_width   32

#define dspin_ram_cmd_width   64
#define dspin_ram_rsp_width   64

//////////////////////////////////////////////////////////////////
//         VCI fields width  for the 3 VCI networks
//////////////////////////////////////////////////////////////////

#define vci_cell_width_int    4
#define vci_cell_width_ext    8

#define vci_plen_width        8
#define vci_address_width     40
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

#include "hard_config.h"

////////////////////////////////////////////////////////////
//    Secondary Hardware Parameters values
//////////////////////i/////////////////////////////////////

#define XMAX                  X_SIZE
#define YMAX                  Y_SIZE

#define XRAM_LATENCY          0

#define MEMC_WAYS             16
#define MEMC_SETS             256

#define L1_IWAYS              4
#define L1_ISETS              64

#define L1_DWAYS              4
#define L1_DSETS              64

#define DISK_IMAGE_NAME       "virt_hdd.dmg"

#define ROM_SOFT_NAME         "../../softs/tsar_boot/preloader.elf"

#define NORTH                 0
#define SOUTH                 1
#define EAST                  2
#define WEST                  3

#define cluster(x,y)   ((y) + ((x) << 4))

////////////////////////////////////////////////////////////
//     DEBUG Parameters default values
//////////////////////i/////////////////////////////////////

#define MAX_FROZEN_CYCLES     1000000

/////////////////////////////////////////////////////////
//    Physical segments definition
/////////////////////////////////////////////////////////

// All physical segments base addresses and sizes are defined
// in the hard_config.h file. For replicated segments, the
// base address is incremented by a cluster offset:
// offset  = cluster(x,y) << (address_width-x_width-y_width);

////////////////////////////////////////////////////////////////////////
//          SRCID definition
////////////////////////////////////////////////////////////////////////
// All initiators are in the same indexing space (14 bits).
// The SRCID is structured in two fields:
// - The 8 MSB bits define the cluster index (left aligned)
// - The 6  LSB bits define the local index.
// Two different initiators cannot have the same SRCID, but a given
// initiator can have two alias SRCIDs:
// - Internal initiators (procs, mwmr) are replicated in all clusters,
//   and each initiator has one single SRCID.
// - External initiators (disk, cdma) are not replicated, but can be
//   accessed in 2 clusters : cluster_iob0 and cluster_iob1.
//   They have the same local index, but two different cluster indexes.
//
// As cluster_iob0 and cluster_iob1 contain both internal initiators
// and external initiators, they must have different local indexes.
// Consequence: For a local interconnect, the INI_ID port index
// is NOT equal to the SRCID local index, and the local interconnect
// must make a translation: SRCID => INI_ID
////////////////////////////////////////////////////////////////////////

#define PROC_LOCAL_SRCID             0x0    // from 0 to 7
#define MWMR_LOCAL_SRCID             0x8
#define IOBX_LOCAL_SRCID             0x9
#define MEMC_LOCAL_SRCID             0xA
#define CDMA_LOCAL_SRCID             0xB
#define DISK_LOCAL_SRCID             0xC
#define IOPI_LOCAL_SRCID             0xD

///////////////////////////////////////////////////////////////////////
//     TGT_ID and INI_ID port indexing for INT local interconnect
///////////////////////////////////////////////////////////////////////

#define INT_MEMC_TGT_ID              0
#define INT_XICU_TGT_ID              1
#define INT_MWMR_TGT_ID              2
#define INT_IOBX_TGT_ID              3

#define INT_PROC_INI_ID              0   // from 0 to (NB_PROCS_MAX-1)
#define INT_MWMR_INI_ID              (NB_PROCS_MAX)
#define INT_IOBX_INI_ID              (NB_PROCS_MAX+1)

///////////////////////////////////////////////////////////////////////
//     TGT_ID and INI_ID port indexing for RAM local interconnect
///////////////////////////////////////////////////////////////////////

#define RAM_XRAM_TGT_ID              0

#define RAM_MEMC_INI_ID              0
#define RAM_IOBX_INI_ID              1

///////////////////////////////////////////////////////////////////////
//     TGT_ID and INI_ID port indexing for I0X local interconnect
///////////////////////////////////////////////////////////////////////

#define IOX_FBUF_TGT_ID              0
#define IOX_DISK_TGT_ID              1
#define IOX_MNIC_TGT_ID              2
#define IOX_CDMA_TGT_ID              3
#define IOX_BROM_TGT_ID              4
#define IOX_MTTY_TGT_ID              5
#define IOX_IOPI_TGT_ID              6
#define IOX_IOB0_TGT_ID              7
#define IOX_IOB1_TGT_ID              8

#define IOX_DISK_INI_ID              0
#define IOX_CDMA_INI_ID              1
#define IOX_IOPI_INI_ID              2
#define IOX_IOB0_INI_ID              3
#define IOX_IOB1_INI_ID              4

////////////////////////////////////////////////////////////////////////
int _main(int argc, char *argv[])
////////////////////////////////////////////////////////////////////////
{
   using namespace sc_core;
   using namespace soclib::caba;
   using namespace soclib::common;

   char     soft_name[256]   = ROM_SOFT_NAME;           // pathname: binary code
   size_t   ncycles          = 4000000000;              // simulated cycles
   char     disk_name[256]   = DISK_IMAGE_NAME;         // pathname: disk image
   ssize_t  threads          = 1;                       // simulator's threads number
   bool     debug_ok         = false;                   // trace activated
   size_t   debug_memc_id    = 0xFFFFFFFF;              // index of traced memc
   size_t   debug_proc_id    = 0xFFFFFFFF;              // index of traced proc
   size_t   debug_xram_id    = 0xFFFFFFFF;              // index of traced xram
   bool     debug_iob        = false;                   // trace iob0 & iob1 when true
   uint32_t debug_from       = 0;                       // trace start cycle
   uint32_t frozen_cycles    = MAX_FROZEN_CYCLES;       // monitoring frozen processor
   size_t   cluster_iob0     = cluster(0,0);            // cluster containing IOB0
   size_t   cluster_iob1     = cluster(XMAX-1,YMAX-1);  // cluster containing IOB1
   size_t   x_width          = X_WIDTH;                 // # of bits for x
   size_t   y_width          = Y_WIDTH;                 // # of bits for y
   size_t   p_width          = P_WIDTH;                 // # of bits for lpid

#if USE_OPENMP
   size_t   simul_period     = 1000000;
#else
   size_t   simul_period     = 1;
#endif

   assert( (X_WIDTH == 4) and (Y_WIDTH == 4) and
   "ERROR: we must have X_WIDTH == Y_WIDTH == 4");

   assert( P_WIDTH <= 4 and
   "ERROR: we must have P_WIDTH <= 4");

   ////////////// command line arguments //////////////////////
   if (argc > 1)
   {
      for (int n = 1; n < argc; n = n + 2)
      {
         if ((strcmp(argv[n],"-NCYCLES") == 0) && (n+1<argc))
         {
            ncycles = atoi(argv[n+1]);
         }
         else if ((strcmp(argv[n],"-DEBUG") == 0) && (n+1<argc) )
         {
            debug_ok = true;
            debug_from = atoi(argv[n+1]);
         }
         else if ((strcmp(argv[n],"-DISK") == 0) && (n+1<argc) )
         {
            strcpy(disk_name, argv[n+1]);
         }
         else if ((strcmp(argv[n],"-MEMCID") == 0) && (n+1<argc) )
         {
            debug_memc_id = atoi(argv[n+1]);
            size_t x = debug_memc_id >> 4;
            size_t y = debug_memc_id & 0xF;
            if( (x>=XMAX) || (y>=YMAX) )
            {
                std::cout << "MEMCID parameter doesn't fit XMAX/YMAX" << std::endl;
                std::cout << " - MEMCID = " << std::hex << debug_memc_id << std::endl;
                std::cout << " - XMAX   = " << std::hex << XMAX          << std::endl;
                std::cout << " - YMAX   = " << std::hex << YMAX          << std::endl;
                exit(0);
            }
         }
         else if ((strcmp(argv[n],"-XRAMID") == 0) && (n+1<argc) )
         {
            debug_xram_id = atoi(argv[n+1]);
            size_t x = debug_xram_id >> 4;
            size_t y = debug_xram_id & 0xF;
            if( (x>=XMAX) || (y>=YMAX) )
            {
                std::cout << "XRAMID parameter does'nt fit XMAX/YMAX" << std::endl;
                exit(0);
            }
         }
         else if ((strcmp(argv[n],"-IOB") == 0) && (n+1<argc) )
         {
            debug_iob = atoi(argv[n+1]);
         }
         else if ((strcmp(argv[n],"-PROCID") == 0) && (n+1<argc) )
         {
            debug_proc_id     = atoi(argv[n+1]);
            size_t cluster_xy = debug_proc_id >> P_WIDTH ;
            size_t x          = cluster_xy >> 4;
            size_t y          = cluster_xy & 0xF;
            if( (x>=XMAX) || (y>=YMAX) )
            {
                std::cout << "PROCID parameter does'nt fit XMAX/YMAX" << std::endl;
                std::cout << " - PROCID = " << std::hex << debug_proc_id << std::endl;
                std::cout << " - XMAX   = " << std::hex << XMAX          << std::endl;
                std::cout << " - YMAX   = " << std::hex << YMAX          << std::endl;
                exit(0);
            }
         }
         else if ((strcmp(argv[n], "-THREADS") == 0) && ((n+1) < argc))
         {
            threads = atoi(argv[n+1]);
            threads = (threads < 1) ? 1 : threads;
         }
         else if ((strcmp(argv[n], "-FROZEN") == 0) && (n+1 < argc))
         {
            frozen_cycles = atoi(argv[n+1]);
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
            std::cout << "     - IOB    non_zero_value" << std::endl;
            exit(0);
         }
      }
   }

   // checking hardware parameters
   assert( (XMAX <= 16) and
   "Error in tsar_generic_iob : XMAX parameter cannot be larger than 16" );

   assert( (YMAX <= 16) and
   "Error in tsar_generic_iob : YMAX parameter cannot be larger than 16" );

   assert( (NB_PROCS_MAX <= 8) and
   "Error in tsar_generic_iob : NB_PROCS_MAX parameter cannot be larger than 8" );

   assert( (XCU_NB_HWI > NB_PROCS_MAX) and
   "Error in tsar_generic_iob : XCU_NB_HWI must be larger than NB_PROCS_MAX" );

   assert( (XCU_NB_PTI >= NB_PROCS_MAX) and
   "Error in tsar_generic_iob : XCU_NB_PTI cannot be smaller than NB_PROCS_MAX" );

   assert( (XCU_NB_WTI >= 4*NB_PROCS_MAX) and
   "Error in tsar_generic_iob : XCU_NB_WTI cannot be smaller than 4*NB_PROCS_MAX" );

   assert( (XCU_NB_OUT >= 4*NB_PROCS_MAX) and
   "Error in tsar_generic_iob : XCU_NB_OUT cannot be smaller than 4*NB_PROCS_MAX" );
   
   assert( (NB_TTY_CHANNELS >= 1) and (NB_TTY_CHANNELS <= 16) and
   "Error in tsar_generic_iob : NB_TTY_CHANNELS parameter cannot be larger than 16" );

   assert( (NB_NIC_CHANNELS <= 2) and
   "Error in tsar_generic_iob :  NB_NIC_CHANNELS parameter cannot be larger than 2" );

   assert( (NB_CMA_CHANNELS <= 4) and
   "Error in tsar_generic_iob :  NB_CMA_CHANNELS parameter cannot be larger than 4" );

   assert( (X_WIDTH == 4) and (Y_WIDTH == 4) and
   "Error in tsar_generic_iob : You must have X_WIDTH == Y_WIDTH == 4");

   assert(  ((USE_MWR_CPY + USE_MWR_GCD + USE_MWR_DCT) == 1) and
   "Error in tsar_generic_iob : No MWR coprocessor found in hard_config.h");

   assert(  ((USE_IOC_HBA + USE_IOC_BDV + USE_IOC_SDC) == 1) and
   "Error in tsar_generic_iob : NoIOC controller found in hard_config.h");

   std::cout << std::endl << std::dec
             << " - XMAX            = " << XMAX << std::endl
             << " - YMAX            = " << YMAX << std::endl
             << " - NB_PROCS_MAX    = " << NB_PROCS_MAX << std::endl
             << " - NB_TTY_CHANNELS = " << NB_TTY_CHANNELS <<  std::endl
             << " - NB_NIC_CHANNELS = " << NB_NIC_CHANNELS <<  std::endl
             << " - NB_CMA_CHANNELS = " << NB_CMA_CHANNELS <<  std::endl
             << " - MEMC_WAYS       = " << MEMC_WAYS << std::endl
             << " - MEMC_SETS       = " << MEMC_SETS << std::endl
             << " - RAM_LATENCY     = " << XRAM_LATENCY << std::endl
             << " - MAX_FROZEN      = " << frozen_cycles << std::endl
             << " - NCYCLES         = " << ncycles << std::endl
             << " - SOFT_FILENAME   = " << soft_name << std::endl
             << " - DISK_IMAGENAME  = " << disk_name << std::endl
             << " - OPENMP THREADS  = " << threads << std::endl
             << " - DEBUG_PROCID    = " << debug_proc_id << std::endl
             << " - DEBUG_MEMCID    = " << debug_memc_id << std::endl
             << " - DEBUG_XRAMID    = " << debug_xram_id << std::endl
             << " - DEBUG_XRAMID    = " << debug_xram_id << std::endl;

   std::cout << std::endl;

#if USE_OPENMP
   omp_set_dynamic(false);
   omp_set_num_threads(threads);
   std::cerr << "Built with openmp version " << _OPENMP << std::endl;
#endif

   // Define VciParams objects
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

   /////////////////////////////////////////////////////////////////////
   // INT network mapping table
   // - two levels address decoding for commands
   // - two levels srcid decoding for responses
   // - NB_PROCS_MAX + 2 (MWMR, IOBX) local initiators per cluster
   // - 4 local targets (MEMC, XICU, MWMR, IOBX) per cluster
   /////////////////////////////////////////////////////////////////////
   MappingTable maptab_int( vci_address_width,
                            IntTab(x_width + y_width, 16 - x_width - y_width),
                            IntTab(x_width + y_width, vci_srcid_width - x_width - y_width),
                            0x00FF000000);

   for (size_t x = 0; x < XMAX; x++)
   {
      for (size_t y = 0; y < YMAX; y++)
      {
         uint64_t offset = ((uint64_t)cluster(x,y))
                              << (vci_address_width-x_width-y_width);
         bool config    = true;
         bool cacheable = true;

         // the four following segments are defined in all clusters

         std::ostringstream    smemc_conf;
         smemc_conf << "int_seg_memc_conf_" << x << "_" << y;
         maptab_int.add(Segment(smemc_conf.str(), SEG_MMC_BASE+offset, SEG_MMC_SIZE,
                     IntTab(cluster(x,y), INT_MEMC_TGT_ID), not cacheable, config ));

         std::ostringstream    smemc_xram;
         smemc_xram << "int_seg_memc_xram_" << x << "_" << y;
         maptab_int.add(Segment(smemc_xram.str(), SEG_RAM_BASE+offset, SEG_RAM_SIZE,
                     IntTab(cluster(x,y), INT_MEMC_TGT_ID), cacheable));

         std::ostringstream    sxicu;
         sxicu << "int_seg_xicu_" << x << "_" << y;
         maptab_int.add(Segment(sxicu.str(), SEG_XCU_BASE+offset, SEG_XCU_SIZE,
                     IntTab(cluster(x,y), INT_XICU_TGT_ID), not cacheable));

         std::ostringstream    smwmr;
         smwmr << "int_seg_mwmr_" << x << "_" << y;
         maptab_int.add(Segment(smwmr.str(), SEG_MWR_BASE+offset, SEG_MWR_SIZE,
                     IntTab(cluster(x,y), INT_MWMR_TGT_ID), not cacheable));

         // the following segments are only defined in cluster_iob0 or in cluster_iob1

         if ( (cluster(x,y) == cluster_iob0) or (cluster(x,y) == cluster_iob1) )
         {
            std::ostringstream    siobx;
            siobx << "int_seg_iobx_" << x << "_" << y;
            maptab_int.add(Segment(siobx.str(), SEG_IOB_BASE+offset, SEG_IOB_SIZE,
                        IntTab(cluster(x,y), INT_IOBX_TGT_ID), not cacheable, config ));

            std::ostringstream    stty;
            stty << "int_seg_mtty_" << x << "_" << y;
            maptab_int.add(Segment(stty.str(), SEG_TTY_BASE+offset, SEG_TTY_SIZE,
                        IntTab(cluster(x,y), INT_IOBX_TGT_ID), not cacheable));

            std::ostringstream    sfbf;
            sfbf << "int_seg_fbuf_" << x << "_" << y;
            maptab_int.add(Segment(sfbf.str(), SEG_FBF_BASE+offset, SEG_FBF_SIZE,
                        IntTab(cluster(x,y), INT_IOBX_TGT_ID), not cacheable));

            std::ostringstream    sdsk;
            sdsk << "int_seg_disk_" << x << "_" << y;
            maptab_int.add(Segment(sdsk.str(), SEG_IOC_BASE+offset, SEG_IOC_SIZE,
                        IntTab(cluster(x,y), INT_IOBX_TGT_ID), not cacheable));

            std::ostringstream    snic;
            snic << "int_seg_mnic_" << x << "_" << y;
            maptab_int.add(Segment(snic.str(), SEG_NIC_BASE+offset, SEG_NIC_SIZE,
                        IntTab(cluster(x,y), INT_IOBX_TGT_ID), not cacheable));

            std::ostringstream    srom;
            srom << "int_seg_brom_" << x << "_" << y;
            maptab_int.add(Segment(srom.str(), SEG_ROM_BASE+offset, SEG_ROM_SIZE,
                        IntTab(cluster(x,y), INT_IOBX_TGT_ID), cacheable ));

            std::ostringstream    sdma;
            sdma << "int_seg_cdma_" << x << "_" << y;
            maptab_int.add(Segment(sdma.str(), SEG_CMA_BASE+offset, SEG_CMA_SIZE,
                        IntTab(cluster(x,y), INT_IOBX_TGT_ID), not cacheable));

            std::ostringstream    spic;
            spic << "int_seg_iopi_" << x << "_" << y;
            maptab_int.add(Segment(spic.str(), SEG_PIC_BASE+offset, SEG_PIC_SIZE,
                        IntTab(cluster(x,y), INT_IOBX_TGT_ID), not cacheable));
         }

         // This define the mapping between the SRCIDs
         // and the port index on the local interconnect.

         maptab_int.srcid_map( IntTab( cluster(x,y), MWMR_LOCAL_SRCID ),
                               IntTab( cluster(x,y), INT_MWMR_INI_ID ) );

         maptab_int.srcid_map( IntTab( cluster(x,y), IOBX_LOCAL_SRCID ),
                               IntTab( cluster(x,y), INT_IOBX_INI_ID ) );

         maptab_int.srcid_map( IntTab( cluster(x,y), IOPI_LOCAL_SRCID ),
                               IntTab( cluster(x,y), INT_IOBX_INI_ID ) );

         for ( size_t p = 0 ; p < NB_PROCS_MAX; p++ )
         maptab_int.srcid_map( IntTab( cluster(x,y), PROC_LOCAL_SRCID+p ),
                               IntTab( cluster(x,y), INT_PROC_INI_ID+p ) );
      }
   }
   std::cout << "INT network " << maptab_int << std::endl;

    /////////////////////////////////////////////////////////////////////////
    // RAM network mapping table
    // - two levels address decoding for commands
    // - two levels srcid decoding for responses
    // - 2 local initiators (MEMC, IOBX) per cluster
    //   (IOBX component only in cluster_iob0 and cluster_iob1)
    // - 1 local target (XRAM) per cluster
    ////////////////////////////////////////////////////////////////////////
    MappingTable maptab_ram( vci_address_width,
                             IntTab(x_width+y_width, 0),
                             IntTab(x_width+y_width, vci_srcid_width - x_width - y_width),
                             0x00FF000000);

    for (size_t x = 0; x < XMAX; x++)
    {
        for (size_t y = 0; y < YMAX ; y++)
        {
            uint64_t offset = ((uint64_t)cluster(x,y))
                                << (vci_address_width-x_width-y_width);

            std::ostringstream sxram;
            sxram << "ext_seg_xram_" << x << "_" << y;
            maptab_ram.add(Segment(sxram.str(), SEG_RAM_BASE+offset,
                           SEG_RAM_SIZE, IntTab(cluster(x,y), RAM_XRAM_TGT_ID), false));
        }
    }

    // This define the mapping between the initiators SRCID
    // and the port index on the RAM local interconnect.
    // External initiator have two alias SRCID (iob0 / iob1)

    maptab_ram.srcid_map( IntTab( cluster_iob0, CDMA_LOCAL_SRCID ),
                          IntTab( cluster_iob0, RAM_IOBX_INI_ID ) );

    maptab_ram.srcid_map( IntTab( cluster_iob1, CDMA_LOCAL_SRCID ),
                          IntTab( cluster_iob1, RAM_IOBX_INI_ID ) );

    maptab_ram.srcid_map( IntTab( cluster_iob0, DISK_LOCAL_SRCID ),
                          IntTab( cluster_iob0, RAM_IOBX_INI_ID ) );

    maptab_ram.srcid_map( IntTab( cluster_iob1, DISK_LOCAL_SRCID ),
                          IntTab( cluster_iob1, RAM_IOBX_INI_ID ) );

    maptab_ram.srcid_map( IntTab( cluster_iob0, IOPI_LOCAL_SRCID ),
                          IntTab( cluster_iob0, RAM_IOBX_INI_ID ) );

    maptab_ram.srcid_map( IntTab( cluster_iob1, IOPI_LOCAL_SRCID ),
                          IntTab( cluster_iob1, RAM_IOBX_INI_ID ) );

    maptab_ram.srcid_map( IntTab( cluster_iob0, MEMC_LOCAL_SRCID ),
                          IntTab( cluster_iob0, RAM_MEMC_INI_ID ) );

    maptab_ram.srcid_map( IntTab( cluster_iob1, MEMC_LOCAL_SRCID ),
                          IntTab( cluster_iob1, RAM_MEMC_INI_ID ) );

    std::cout << "RAM network " << maptab_ram << std::endl;

    ///////////////////////////////////////////////////////////////////////
    // IOX network mapping table
    // - two levels address decoding for commands (9, 7) bits
    // - two levels srcid decoding for responses
    // - 5 initiators (IOB0, IOB1, DISK, CDMA, IOPI)
    // - 9 targets (IOB0, IOB1, DISK, CDMA, MTTY, FBUF, BROM, MNIC, IOPI)
    //
    // Address bit 32 is used to determine if a command must be routed to
    // IOB0 or IOB1.
    ///////////////////////////////////////////////////////////////////////
    MappingTable maptab_iox(
          vci_address_width,
          IntTab(x_width + y_width - 1, 16 - x_width - y_width + 1),
          IntTab(x_width + y_width    , vci_param_ext::S - x_width - y_width),
          0x00FF000000);

    // External peripherals segments
    // When there is more than one cluster, external peripherals can be accessed
    // through two segments, depending on the used IOB (IOB0 or IOB1).

    const uint64_t iob0_base = ((uint64_t)cluster_iob0)
       << (vci_address_width - x_width - y_width);

    maptab_iox.add(Segment("iox_seg_mtty_0", SEG_TTY_BASE + iob0_base, SEG_TTY_SIZE,
                   IntTab(0, IOX_MTTY_TGT_ID), false));
    maptab_iox.add(Segment("iox_seg_fbuf_0", SEG_FBF_BASE + iob0_base, SEG_FBF_SIZE,
                   IntTab(0, IOX_FBUF_TGT_ID), false));
    maptab_iox.add(Segment("iox_seg_disk_0", SEG_IOC_BASE + iob0_base, SEG_IOC_SIZE,
                   IntTab(0, IOX_DISK_TGT_ID), false));
    maptab_iox.add(Segment("iox_seg_mnic_0", SEG_NIC_BASE + iob0_base, SEG_NIC_SIZE,
                   IntTab(0, IOX_MNIC_TGT_ID), false));
    maptab_iox.add(Segment("iox_seg_cdma_0", SEG_CMA_BASE + iob0_base, SEG_CMA_SIZE,
                   IntTab(0, IOX_CDMA_TGT_ID), false));
    maptab_iox.add(Segment("iox_seg_brom_0", SEG_ROM_BASE + iob0_base, SEG_ROM_SIZE,
                   IntTab(0, IOX_BROM_TGT_ID), false));
    maptab_iox.add(Segment("iox_seg_iopi_0", SEG_PIC_BASE + iob0_base, SEG_PIC_SIZE,
                   IntTab(0, IOX_IOPI_TGT_ID), false));

    if ( cluster_iob0 != cluster_iob1 )
    {
       const uint64_t iob1_base = ((uint64_t)cluster_iob1)
          << (vci_address_width - x_width - y_width);

        maptab_iox.add(Segment("iox_seg_mtty_1", SEG_TTY_BASE + iob1_base, SEG_TTY_SIZE,
                   IntTab(0, IOX_MTTY_TGT_ID), false));
        maptab_iox.add(Segment("iox_seg_fbuf_1", SEG_FBF_BASE + iob1_base, SEG_FBF_SIZE,
                   IntTab(0, IOX_FBUF_TGT_ID), false));
        maptab_iox.add(Segment("iox_seg_disk_1", SEG_IOC_BASE + iob1_base, SEG_IOC_SIZE,
                   IntTab(0, IOX_DISK_TGT_ID), false));
        maptab_iox.add(Segment("iox_seg_mnic_1", SEG_NIC_BASE + iob1_base, SEG_NIC_SIZE,
                   IntTab(0, IOX_MNIC_TGT_ID), false));
        maptab_iox.add(Segment("iox_seg_cdma_1", SEG_CMA_BASE + iob1_base, SEG_CMA_SIZE,
                   IntTab(0, IOX_CDMA_TGT_ID), false));
        maptab_iox.add(Segment("iox_seg_brom_1", SEG_ROM_BASE + iob1_base, SEG_ROM_SIZE,
                   IntTab(0, IOX_BROM_TGT_ID), false));
        maptab_iox.add(Segment("iox_seg_iopi_1", SEG_PIC_BASE + iob1_base, SEG_PIC_SIZE,
                   IntTab(0, IOX_IOPI_TGT_ID), false));
    }

    // If there is more than one cluster, external peripherals
    // can access RAM through two segments (IOB0 / IOB1).
    // As IOMMU is not activated, addresses are 40 bits (physical addresses),
    // and the choice depends on address bit A[32].
    for (size_t x = 0; x < XMAX; x++)
    {
        for (size_t y = 0; y < YMAX ; y++)
        {
            const bool wti       = true;
            const bool cacheable = true;

            const uint64_t offset = ((uint64_t)cluster(x,y))
                << (vci_address_width-x_width-y_width);

            const uint64_t xicu_base = SEG_XCU_BASE + offset;

            if ( (y & 0x1) == 0 ) // use IOB0
            {
                std::ostringstream sxcu0;
                sxcu0 << "iox_seg_xcu0_" << x << "_" << y;
                maptab_iox.add(Segment(sxcu0.str(), xicu_base, SEG_XCU_SIZE,
                            IntTab(0, IOX_IOB0_TGT_ID), not cacheable, wti));

                std::ostringstream siob0;
                siob0 << "iox_seg_ram0_" << x << "_" << y;
                maptab_iox.add(Segment(siob0.str(), offset, SEG_XCU_BASE,
                            IntTab(0, IOX_IOB0_TGT_ID), not cacheable, not wti));
            }
            else                  // USE IOB1
            {
                std::ostringstream sxcu1;
                sxcu1 << "iox_seg_xcu1_" << x << "_" << y;
                maptab_iox.add(Segment(sxcu1.str(), xicu_base, SEG_XCU_SIZE,
                            IntTab(0, IOX_IOB1_TGT_ID), not cacheable, wti));

                std::ostringstream siob1;
                siob1 << "iox_seg_ram1_" << x << "_" << y;
                maptab_iox.add(Segment(siob1.str(), offset, SEG_XCU_BASE,
                            IntTab(0, IOX_IOB1_TGT_ID), not cacheable, not wti));
            }
        }
    }

    // This define the mapping between the external initiators (SRCID)
    // and the port index on the IOX local interconnect.

    maptab_iox.srcid_map( IntTab( 0, CDMA_LOCAL_SRCID ) ,
                          IntTab( 0, IOX_CDMA_INI_ID  ) );
    maptab_iox.srcid_map( IntTab( 0, DISK_LOCAL_SRCID ) ,
                          IntTab( 0, IOX_DISK_INI_ID  ) );
    maptab_iox.srcid_map( IntTab( 0, IOPI_LOCAL_SRCID ) ,
                          IntTab( 0, IOX_IOPI_INI_ID  ) );
    maptab_iox.srcid_map( IntTab( 0, IOX_IOB0_INI_ID  ) ,
                          IntTab( 0, IOX_IOB0_INI_ID  ) );

    if ( cluster_iob0 != cluster_iob1 )
    {
        maptab_iox.srcid_map( IntTab( 0, IOX_IOB1_INI_ID ) ,
                              IntTab( 0, IOX_IOB1_INI_ID ) );
    }

    std::cout << "IOX network " << maptab_iox << std::endl;

    ////////////////////
    // Signals
    ///////////////////

    sc_clock                          signal_clk("clk");
    sc_signal<bool>                   signal_resetn("resetn");

    sc_signal<bool>                   signal_irq_false;
    sc_signal<bool>                   signal_irq_disk;
    sc_signal<bool>                   signal_irq_mtty_rx[NB_TTY_CHANNELS];
    sc_signal<bool>                   signal_irq_mnic_rx[NB_NIC_CHANNELS];
    sc_signal<bool>                   signal_irq_mnic_tx[NB_NIC_CHANNELS];
    sc_signal<bool>                   signal_irq_cdma[NB_CMA_CHANNELS];

    // VCI signals for IOX network
    VciSignals<vci_param_ext>         signal_vci_ini_iob0("signal_vci_ini_iob0");
    VciSignals<vci_param_ext>         signal_vci_ini_iob1("signal_vci_ini_iob1");
    VciSignals<vci_param_ext>         signal_vci_ini_disk("signal_vci_ini_disk");
    VciSignals<vci_param_ext>         signal_vci_ini_cdma("signal_vci_ini_cdma");
    VciSignals<vci_param_ext>         signal_vci_ini_iopi("signal_vci_ini_iopi");

    VciSignals<vci_param_ext>         signal_vci_tgt_iob0("signal_vci_tgt_iob0");
    VciSignals<vci_param_ext>         signal_vci_tgt_iob1("signal_vci_tgt_iob1");
    VciSignals<vci_param_ext>         signal_vci_tgt_mtty("signal_vci_tgt_mtty");
    VciSignals<vci_param_ext>         signal_vci_tgt_fbuf("signal_vci_tgt_fbuf");
    VciSignals<vci_param_ext>         signal_vci_tgt_mnic("signal_vci_tgt_mnic");
    VciSignals<vci_param_ext>         signal_vci_tgt_brom("signal_vci_tgt_brom");
    VciSignals<vci_param_ext>         signal_vci_tgt_disk("signal_vci_tgt_disk");
    VciSignals<vci_param_ext>         signal_vci_tgt_cdma("signal_vci_tgt_cdma");
    VciSignals<vci_param_ext>         signal_vci_tgt_iopi("signal_vci_tgt_iopi");

   // Horizontal inter-clusters INT_CMD DSPIN
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_cmd_h_inc =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_cmd_h_inc", XMAX-1, YMAX);
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_cmd_h_dec =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_cmd_h_dec", XMAX-1, YMAX);

   // Horizontal inter-clusters INT_RSP DSPIN
   DspinSignals<dspin_int_rsp_width>** signal_dspin_int_rsp_h_inc =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_int_rsp_h_inc", XMAX-1, YMAX);
   DspinSignals<dspin_int_rsp_width>** signal_dspin_int_rsp_h_dec =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_int_rsp_h_dec", XMAX-1, YMAX);

   // Horizontal inter-clusters INT_M2P DSPIN
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_m2p_h_inc =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_m2p_h_inc", XMAX-1, YMAX);
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_m2p_h_dec =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_m2p_h_dec", XMAX-1, YMAX);

   // Horizontal inter-clusters INT_P2M DSPIN
   DspinSignals<dspin_int_rsp_width>** signal_dspin_int_p2m_h_inc =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_int_p2m_h_inc", XMAX-1, YMAX);
   DspinSignals<dspin_int_rsp_width>** signal_dspin_int_p2m_h_dec =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_int_p2m_h_dec", XMAX-1, YMAX);

   // Horizontal inter-clusters INT_CLA DSPIN
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_cla_h_inc =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_cla_h_inc", XMAX-1, YMAX);
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_cla_h_dec =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_cla_h_dec", XMAX-1, YMAX);


   // Vertical inter-clusters INT_CMD DSPIN
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_cmd_v_inc =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_cmd_v_inc", XMAX, YMAX-1);
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_cmd_v_dec =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_cmd_v_dec", XMAX, YMAX-1);

   // Vertical inter-clusters INT_RSP DSPIN
   DspinSignals<dspin_int_rsp_width>** signal_dspin_int_rsp_v_inc =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_int_rsp_v_inc", XMAX, YMAX-1);
   DspinSignals<dspin_int_rsp_width>** signal_dspin_int_rsp_v_dec =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_int_rsp_v_dec", XMAX, YMAX-1);

   // Vertical inter-clusters INT_M2P DSPIN
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_m2p_v_inc =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_m2p_v_inc", XMAX, YMAX-1);
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_m2p_v_dec =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_m2p_v_dec", XMAX, YMAX-1);

   // Vertical inter-clusters INT_P2M DSPIN
   DspinSignals<dspin_int_rsp_width>** signal_dspin_int_p2m_v_inc =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_int_p2m_v_inc", XMAX, YMAX-1);
   DspinSignals<dspin_int_rsp_width>** signal_dspin_int_p2m_v_dec =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_int_p2m_v_dec", XMAX, YMAX-1);

   // Vertical inter-clusters INT_CLA DSPIN
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_cla_v_inc =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_cla_v_inc", XMAX, YMAX-1);
   DspinSignals<dspin_int_cmd_width>** signal_dspin_int_cla_v_dec =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_int_cla_v_dec", XMAX, YMAX-1);


   // Mesh boundaries INT_CMD DSPIN
   DspinSignals<dspin_int_cmd_width>*** signal_dspin_false_int_cmd_in =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_false_int_cmd_in", XMAX, YMAX, 4);
   DspinSignals<dspin_int_cmd_width>*** signal_dspin_false_int_cmd_out =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_false_int_cmd_out", XMAX, YMAX, 4);

   // Mesh boundaries INT_RSP DSPIN
   DspinSignals<dspin_int_rsp_width>*** signal_dspin_false_int_rsp_in =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_false_int_rsp_in", XMAX, YMAX, 4);
   DspinSignals<dspin_int_rsp_width>*** signal_dspin_false_int_rsp_out =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_false_int_rsp_out", XMAX, YMAX, 4);

   // Mesh boundaries INT_M2P DSPIN
   DspinSignals<dspin_int_cmd_width>*** signal_dspin_false_int_m2p_in =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_false_int_m2p_in", XMAX, YMAX, 4);
   DspinSignals<dspin_int_cmd_width>*** signal_dspin_false_int_m2p_out =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_false_int_m2P_out", XMAX, YMAX, 4);

   // Mesh boundaries INT_P2M DSPIN
   DspinSignals<dspin_int_rsp_width>*** signal_dspin_false_int_p2m_in =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_false_int_p2m_in", XMAX, YMAX, 4);
   DspinSignals<dspin_int_rsp_width>*** signal_dspin_false_int_p2m_out =
      alloc_elems<DspinSignals<dspin_int_rsp_width> >("signal_dspin_false_int_p2m_out", XMAX, YMAX, 4);

   // Mesh boundaries INT_CLA DSPIN
   DspinSignals<dspin_int_cmd_width>*** signal_dspin_false_int_cla_in =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_false_int_cla_in", XMAX, YMAX, 4);
   DspinSignals<dspin_int_cmd_width>*** signal_dspin_false_int_cla_out =
      alloc_elems<DspinSignals<dspin_int_cmd_width> >("signal_dspin_false_int_cla_out", XMAX, YMAX, 4);


   // Horizontal inter-clusters RAM_CMD DSPIN
   DspinSignals<dspin_ram_cmd_width>** signal_dspin_ram_cmd_h_inc =
      alloc_elems<DspinSignals<dspin_ram_cmd_width> >("signal_dspin_ram_cmd_h_inc", XMAX-1, YMAX);
   DspinSignals<dspin_ram_cmd_width>** signal_dspin_ram_cmd_h_dec =
      alloc_elems<DspinSignals<dspin_ram_cmd_width> >("signal_dspin_ram_cmd_h_dec", XMAX-1, YMAX);

   // Horizontal inter-clusters RAM_RSP DSPIN
   DspinSignals<dspin_ram_rsp_width>** signal_dspin_ram_rsp_h_inc =
      alloc_elems<DspinSignals<dspin_ram_rsp_width> >("signal_dspin_ram_rsp_h_inc", XMAX-1, YMAX);
   DspinSignals<dspin_ram_rsp_width>** signal_dspin_ram_rsp_h_dec =
      alloc_elems<DspinSignals<dspin_ram_rsp_width> >("signal_dspin_ram_rsp_h_dec", XMAX-1, YMAX);

   // Vertical inter-clusters RAM_CMD DSPIN
   DspinSignals<dspin_ram_cmd_width>** signal_dspin_ram_cmd_v_inc =
      alloc_elems<DspinSignals<dspin_ram_cmd_width> >("signal_dspin_ram_cmd_v_inc", XMAX, YMAX-1);
   DspinSignals<dspin_ram_cmd_width>** signal_dspin_ram_cmd_v_dec =
      alloc_elems<DspinSignals<dspin_ram_cmd_width> >("signal_dspin_ram_cmd_v_dec", XMAX, YMAX-1);

   // Vertical inter-clusters RAM_RSP DSPIN
   DspinSignals<dspin_ram_rsp_width>** signal_dspin_ram_rsp_v_inc =
      alloc_elems<DspinSignals<dspin_ram_rsp_width> >("signal_dspin_ram_rsp_v_inc", XMAX, YMAX-1);
   DspinSignals<dspin_ram_rsp_width>** signal_dspin_ram_rsp_v_dec =
      alloc_elems<DspinSignals<dspin_ram_rsp_width> >("signal_dspin_ram_rsp_v_dec", XMAX, YMAX-1);

   // Mesh boundaries RAM_CMD DSPIN
   DspinSignals<dspin_ram_cmd_width>*** signal_dspin_false_ram_cmd_in =
      alloc_elems<DspinSignals<dspin_ram_cmd_width> >("signal_dspin_false_ram_cmd_in", XMAX, YMAX, 4);
   DspinSignals<dspin_ram_cmd_width>*** signal_dspin_false_ram_cmd_out =
      alloc_elems<DspinSignals<dspin_ram_cmd_width> >("signal_dspin_false_ram_cmd_out", XMAX, YMAX, 4);

   // Mesh boundaries RAM_RSP DSPIN
   DspinSignals<dspin_ram_rsp_width>*** signal_dspin_false_ram_rsp_in =
      alloc_elems<DspinSignals<dspin_ram_rsp_width> >("signal_dspin_false_ram_rsp_in", XMAX, YMAX, 4);
   DspinSignals<dspin_ram_rsp_width>*** signal_dspin_false_ram_rsp_out =
      alloc_elems<DspinSignals<dspin_ram_rsp_width> >("signal_dspin_false_ram_rsp_out", XMAX, YMAX, 4);

   // SD card signals
   sc_signal<bool>   signal_sdc_clk;
   sc_signal<bool>   signal_sdc_cmd_enable_to_card;
   sc_signal<bool>   signal_sdc_cmd_value_to_card;
   sc_signal<bool>   signal_sdc_dat_enable_to_card;
   sc_signal<bool>   signal_sdc_dat_value_to_card[4];
   sc_signal<bool>   signal_sdc_cmd_enable_from_card;
   sc_signal<bool>   signal_sdc_cmd_value_from_card;
   sc_signal<bool>   signal_sdc_dat_enable_from_card;
   sc_signal<bool>   signal_sdc_dat_value_from_card[4];

    ////////////////////////////
    //      Loader
    ////////////////////////////

#if USE_ALMOS
    soclib::common::Loader loader(almos_bootloader_pathname,
                                 almos_archinfo_pathname,
                                 almos_kernel_pathname);
#else
    soclib::common::Loader loader(soft_name);
#endif

    typedef soclib::common::GdbServer<soclib::common::Mips32ElIss> proc_iss;
    proc_iss::set_loader(loader);

    ////////////////////////////////////////
    //  Instanciated Hardware Components
    ////////////////////////////////////////

    std::cout << std::endl << "External Bus and Peripherals" << std::endl << std::endl;

    const size_t nb_iox_initiators = (cluster_iob0 != cluster_iob1) ? 5 : 4;
    const size_t nb_iox_targets = (cluster_iob0 != cluster_iob1) ? 9 : 8;

    // IOX network
    VciIoxNetwork<vci_param_ext>* iox_network;
    iox_network = new VciIoxNetwork<vci_param_ext>( "iox_network",
                                                    maptab_iox,
                                                    nb_iox_targets,
                                                    nb_iox_initiators );
    // boot ROM
    VciSimpleRom<vci_param_ext>*  brom;
    brom = new VciSimpleRom<vci_param_ext>( "brom",
                                            IntTab(0, IOX_BROM_TGT_ID),
                                            maptab_iox,
                                            loader );
    // Network Controller
    VciMultiNic<vci_param_ext>*  mnic;
    mnic = new VciMultiNic<vci_param_ext>( "mnic",
                                           IntTab(0, IOX_MNIC_TGT_ID),
                                           maptab_iox,
                                           NB_NIC_CHANNELS,
                                           0,                // mac_4 address
                                           0,                // mac_2 address
                                           1,                // NIC_MODE_SYNTHESIS
                                           12);              // INTER_FRAME_GAP

    // Frame Buffer
    VciFrameBuffer<vci_param_ext>*  fbuf;
    fbuf = new VciFrameBuffer<vci_param_ext>( "fbuf",
                                              IntTab(0, IOX_FBUF_TGT_ID),
                                              maptab_iox,
                                              FBUF_X_SIZE, FBUF_Y_SIZE );

    // Disk
    std::vector<std::string> filenames;
    filenames.push_back(disk_name);            // one single disk

#if ( USE_IOC_HBA )

    VciMultiAhci<vci_param_ext>*  disk;
    disk = new VciMultiAhci<vci_param_ext>( "disk",
                                            maptab_iox,
                                            IntTab(0, DISK_LOCAL_SRCID),
                                            IntTab(0, IOX_DISK_TGT_ID),
                                            filenames,
                                            512,        // block size
                                            64,         // burst size (bytes)
                                            0 );        // disk latency
#elif ( USE_IOC_BDV )

    VciBlockDeviceTsar<vci_param_ext>*  disk;
    disk = new VciBlockDeviceTsar<vci_param_ext>( "disk",
                                                  maptab_iox,
                                                  IntTab(0, DISK_LOCAL_SRCID),
                                                  IntTab(0, IOX_DISK_TGT_ID),
                                                  disk_name,
                                                  512,        // block size
                                                  64,         // burst size (bytes)
                                                  0 );        // disk latency
#elif ( USE_IOC_SDC )

    VciAhciSdc<vci_param_ext>*  disk;
    disk = new VciAhciSdc<vci_param_ext>( "disk",
                                          maptab_iox,
                                          IntTab(0, DISK_LOCAL_SRCID),
                                          IntTab(0, IOX_DISK_TGT_ID),
                                          64 );       // burst size (bytes)
    SdCard* card;
    card = new SdCard( "card",
                       disk_name,
                       10,         // RX one block latency
                       10 );       // TX one block latency
#endif

    // Chained Buffer DMA controller
    VciChbufDma<vci_param_ext>*  cdma;
    cdma = new VciChbufDma<vci_param_ext>( "cdma",
                                           maptab_iox,
                                           IntTab(0, CDMA_LOCAL_SRCID),
                                           IntTab(0, IOX_CDMA_TGT_ID),
                                           64,          // burst size (bytes)
                                           NB_CMA_CHANNELS,
                                           4 );         // number of pipelined bursts

    // Multi-TTY controller
    std::vector<std::string> vect_names;
    for( size_t tid = 0 ; tid < NB_TTY_CHANNELS ; tid++ )
    {
        std::ostringstream term_name;
          term_name <<  "term" << tid;
 
         vect_names.push_back(term_name.str().c_str());
      }
      VciMultiTty<vci_param_ext>*  mtty;
      mtty = new VciMultiTty<vci_param_ext>( "mtty",
                                             IntTab(0, IOX_MTTY_TGT_ID),
                                             maptab_iox,
                                             vect_names);

    // IOPIC
    VciIopic<vci_param_ext>* iopi;
    iopi = new VciIopic<vci_param_ext>( "iopi",
                                        maptab_iox,
                                        IntTab(0, IOPI_LOCAL_SRCID),
                                        IntTab(0, IOX_IOPI_TGT_ID),
                                        32 );        // number of input HWI
    // Clusters
    TsarIobCluster<vci_param_int,
                   vci_param_ext,
                   dspin_int_cmd_width,
                   dspin_int_rsp_width,
                   dspin_ram_cmd_width,
                   dspin_ram_rsp_width>* clusters[XMAX][YMAX];

    unsigned int coproc_type;
    if ( USE_MWR_CPY ) coproc_type = MWR_COPROC_CPY;
    if ( USE_MWR_DCT ) coproc_type = MWR_COPROC_DCT;
    if ( USE_MWR_GCD ) coproc_type = MWR_COPROC_GCD;

#if USE_OPENMP
#pragma omp parallel
    {
#pragma omp for
#endif
        for(size_t i = 0; i  < (XMAX * YMAX); i++)
        {
            size_t x = i / YMAX;
            size_t y = i % YMAX;

#if USE_OPENMP
#pragma omp critical
            {
#endif
            std::cout << std::endl;
            std::cout << "Cluster_" << std::dec << x << "_" << y << std::endl;
            std::cout << std::endl;

            const bool is_iob0 = (cluster(x,y) == cluster_iob0);
            const bool is_iob1 = (cluster(x,y) == cluster_iob1);
            const bool is_io_cluster = is_iob0 || is_iob1;

            const int iox_iob_ini_id = is_iob0 ?
                IOX_IOB0_INI_ID :
                IOX_IOB1_INI_ID ;
            const int iox_iob_tgt_id = is_iob0 ?
                IOX_IOB0_TGT_ID :
                IOX_IOB1_TGT_ID ;


            std::ostringstream sc;
            sc << "cluster_" << x << "_" << y;
            clusters[x][y] = new TsarIobCluster<vci_param_int,
                                                vci_param_ext,
                                                dspin_int_cmd_width,
                                                dspin_int_rsp_width,
                                                dspin_ram_cmd_width,
                                                dspin_ram_rsp_width>
            (
                sc.str().c_str(),
                NB_PROCS_MAX,
                x,
                y,
                XMAX,
                YMAX,

                maptab_int,
                maptab_ram,
                maptab_iox,

                x_width,
                y_width,
                vci_srcid_width - x_width - y_width,            // l_id width,
                p_width,

                INT_MEMC_TGT_ID,
                INT_XICU_TGT_ID,
                INT_MWMR_TGT_ID,
                INT_IOBX_TGT_ID,

                INT_PROC_INI_ID,
                INT_MWMR_INI_ID,
                INT_IOBX_INI_ID,

                RAM_XRAM_TGT_ID,

                RAM_MEMC_INI_ID,
                RAM_IOBX_INI_ID,

                is_io_cluster,
                iox_iob_tgt_id,
                iox_iob_ini_id,

                MEMC_WAYS,
                MEMC_SETS,
                L1_IWAYS,
                L1_ISETS,
                L1_DWAYS,
                L1_DSETS,
                XRAM_LATENCY,
                XCU_NB_HWI,
                XCU_NB_PTI,
                XCU_NB_WTI,
                XCU_NB_OUT,

                coproc_type,

                loader,

                frozen_cycles,
                debug_from,
                debug_ok and (cluster(x,y) == debug_memc_id),
                debug_ok and (cluster(x,y) == debug_proc_id),
                debug_ok and debug_iob
            );

#if USE_OPENMP
            } // end critical
#endif
        } // end for
#if USE_OPENMP
    }
#endif

    std::cout << std::endl;

    ///////////////////////////////////////////////////////////////////////////////
    //     Net-list
    ///////////////////////////////////////////////////////////////////////////////

    // IOX network connexion
    iox_network->p_clk                                   (signal_clk);
    iox_network->p_resetn                                (signal_resetn);
    iox_network->p_to_ini[IOX_IOB0_INI_ID]               (signal_vci_ini_iob0);
    iox_network->p_to_ini[IOX_DISK_INI_ID]               (signal_vci_ini_disk);
    iox_network->p_to_ini[IOX_CDMA_INI_ID]               (signal_vci_ini_cdma);
    iox_network->p_to_ini[IOX_IOPI_INI_ID]               (signal_vci_ini_iopi);

    iox_network->p_to_tgt[IOX_IOB0_TGT_ID]               (signal_vci_tgt_iob0);
    iox_network->p_to_tgt[IOX_MTTY_TGT_ID]               (signal_vci_tgt_mtty);
    iox_network->p_to_tgt[IOX_FBUF_TGT_ID]               (signal_vci_tgt_fbuf);
    iox_network->p_to_tgt[IOX_MNIC_TGT_ID]               (signal_vci_tgt_mnic);
    iox_network->p_to_tgt[IOX_BROM_TGT_ID]               (signal_vci_tgt_brom);
    iox_network->p_to_tgt[IOX_DISK_TGT_ID]               (signal_vci_tgt_disk);
    iox_network->p_to_tgt[IOX_CDMA_TGT_ID]               (signal_vci_tgt_cdma);
    iox_network->p_to_tgt[IOX_IOPI_TGT_ID]               (signal_vci_tgt_iopi);

    if (cluster_iob0 != cluster_iob1)
    {
        iox_network->p_to_ini[IOX_IOB1_INI_ID]           (signal_vci_ini_iob1);
        iox_network->p_to_tgt[IOX_IOB1_TGT_ID]           (signal_vci_tgt_iob1);
    }

    // DISK connexion

#if ( USE_IOC_HBA )

    disk->p_clk                                          (signal_clk);
    disk->p_resetn                                       (signal_resetn);
    disk->p_vci_target                                   (signal_vci_tgt_disk);
    disk->p_vci_initiator                                (signal_vci_ini_disk);
    disk->p_channel_irq[0]                               (signal_irq_disk);

#elif ( USE_IOC_BDV )

    disk->p_clk                                          (signal_clk);
    disk->p_resetn                                       (signal_resetn);
    disk->p_vci_target                                   (signal_vci_tgt_disk);
    disk->p_vci_initiator                                (signal_vci_ini_disk);
    disk->p_irq                                          (signal_irq_disk);

#elif ( USE_IOC_SDC )

    disk->p_clk                                          (signal_clk);
    disk->p_resetn                                       (signal_resetn);
    disk->p_vci_target                                   (signal_vci_tgt_disk);
    disk->p_vci_initiator                                (signal_vci_ini_disk);
    disk->p_irq                                          (signal_irq_disk);

    disk->p_sdc_clk                                      (signal_sdc_clk);
    disk->p_sdc_cmd_enable_out                           (signal_sdc_cmd_enable_to_card);
    disk->p_sdc_cmd_value_out                            (signal_sdc_cmd_value_to_card);
    disk->p_sdc_cmd_enable_in                            (signal_sdc_cmd_enable_from_card);
    disk->p_sdc_cmd_value_in                             (signal_sdc_cmd_value_from_card);
    disk->p_sdc_dat_enable_out                           (signal_sdc_dat_enable_to_card);
    disk->p_sdc_dat_value_out[0]                         (signal_sdc_dat_value_to_card[0]);
    disk->p_sdc_dat_value_out[1]                         (signal_sdc_dat_value_to_card[1]);
    disk->p_sdc_dat_value_out[2]                         (signal_sdc_dat_value_to_card[2]);
    disk->p_sdc_dat_value_out[3]                         (signal_sdc_dat_value_to_card[3]);
    disk->p_sdc_dat_enable_in                            (signal_sdc_dat_enable_from_card);
    disk->p_sdc_dat_value_in[0]                          (signal_sdc_dat_value_from_card[0]);
    disk->p_sdc_dat_value_in[1]                          (signal_sdc_dat_value_from_card[1]);
    disk->p_sdc_dat_value_in[2]                          (signal_sdc_dat_value_from_card[2]);
    disk->p_sdc_dat_value_in[3]                          (signal_sdc_dat_value_from_card[3]);
    
    card->p_clk                                          (signal_clk);
    card->p_resetn                                       (signal_resetn);

    card->p_sdc_clk                                      (signal_sdc_clk);
    card->p_sdc_cmd_enable_out                           (signal_sdc_cmd_enable_from_card);
    card->p_sdc_cmd_value_out                            (signal_sdc_cmd_value_from_card);
    card->p_sdc_cmd_enable_in                            (signal_sdc_cmd_enable_to_card);
    card->p_sdc_cmd_value_in                             (signal_sdc_cmd_value_to_card);
    card->p_sdc_dat_enable_out                           (signal_sdc_dat_enable_from_card);
    card->p_sdc_dat_value_out[0]                         (signal_sdc_dat_value_from_card[0]);
    card->p_sdc_dat_value_out[1]                         (signal_sdc_dat_value_from_card[1]);
    card->p_sdc_dat_value_out[2]                         (signal_sdc_dat_value_from_card[2]);
    card->p_sdc_dat_value_out[3]                         (signal_sdc_dat_value_from_card[3]);
    card->p_sdc_dat_enable_in                            (signal_sdc_dat_enable_to_card);
    card->p_sdc_dat_value_in[0]                          (signal_sdc_dat_value_to_card[0]);
    card->p_sdc_dat_value_in[1]                          (signal_sdc_dat_value_to_card[1]);
    card->p_sdc_dat_value_in[2]                          (signal_sdc_dat_value_to_card[2]);
    card->p_sdc_dat_value_in[3]                          (signal_sdc_dat_value_to_card[3]);
    
#endif

    std::cout << "  - DISK connected" << std::endl;

    // FBUF connexion
    fbuf->p_clk                                          (signal_clk);
    fbuf->p_resetn                                       (signal_resetn);
    fbuf->p_vci                                          (signal_vci_tgt_fbuf);

    std::cout << "  - FBUF connected" << std::endl;

    // MNIC connexion
    mnic->p_clk                                          (signal_clk);
    mnic->p_resetn                                       (signal_resetn);
    mnic->p_vci                                          (signal_vci_tgt_mnic);
    for ( size_t i=0 ; i<NB_NIC_CHANNELS ; i++ )
    {
         mnic->p_rx_irq[i]                               (signal_irq_mnic_rx[i]);
         mnic->p_tx_irq[i]                               (signal_irq_mnic_tx[i]);
    }

    std::cout << "  - MNIC connected" << std::endl;

    // BROM connexion
    brom->p_clk                                          (signal_clk);
    brom->p_resetn                                       (signal_resetn);
    brom->p_vci                                          (signal_vci_tgt_brom);

    std::cout << "  - BROM connected" << std::endl;

    // MTTY connexion
    mtty->p_clk                                          (signal_clk);
    mtty->p_resetn                                       (signal_resetn);
    mtty->p_vci                                          (signal_vci_tgt_mtty);
    for ( size_t i=0 ; i<NB_TTY_CHANNELS ; i++ )
    {
        mtty->p_irq[i]                                   (signal_irq_mtty_rx[i]);
    }
    std::cout << "  - MTTY connected" << std::endl;

    // CDMA connexion
    cdma->p_clk                                          (signal_clk);
    cdma->p_resetn                                       (signal_resetn);
    cdma->p_vci_target                                   (signal_vci_tgt_cdma);
    cdma->p_vci_initiator                                (signal_vci_ini_cdma);
    for ( size_t i=0 ; i<(NB_CMA_CHANNELS) ; i++)
    {
        cdma->p_irq[i]                                   (signal_irq_cdma[i]);
    }

    std::cout << "  - CDMA connected" << std::endl;

    // IOPI connexion
    iopi->p_clk                                          (signal_clk);
    iopi->p_resetn                                       (signal_resetn);
    iopi->p_vci_target                                   (signal_vci_tgt_iopi);
    iopi->p_vci_initiator                                (signal_vci_ini_iopi);
    for ( size_t i=0 ; i<32 ; i++)
    {
       if     (i < NB_NIC_CHANNELS)    iopi->p_hwi[i] (signal_irq_mnic_rx[i]);
       else if(i < 2 )                 iopi->p_hwi[i] (signal_irq_false);
       else if(i < 2+NB_NIC_CHANNELS)  iopi->p_hwi[i] (signal_irq_mnic_tx[i-2]);
       else if(i < 4 )                 iopi->p_hwi[i] (signal_irq_false);
       else if(i < 4+NB_CMA_CHANNELS)  iopi->p_hwi[i] (signal_irq_cdma[i-4]);
       else if(i < 8)                  iopi->p_hwi[i] (signal_irq_false);
       else if(i < 9)                  iopi->p_hwi[i] (signal_irq_disk);
       else if(i < 16)                 iopi->p_hwi[i] (signal_irq_false);
       else if(i < 16+NB_TTY_CHANNELS) iopi->p_hwi[i] (signal_irq_mtty_rx[i-16]);
       else                            iopi->p_hwi[i] (signal_irq_false);
    }

    std::cout << "  - IOPIC connected" << std::endl;


    // IOB0 cluster connexion to IOX network
    (*clusters[0][0]->p_vci_iob_iox_ini) (signal_vci_ini_iob0);
    (*clusters[0][0]->p_vci_iob_iox_tgt) (signal_vci_tgt_iob0);

    // IOB1 cluster connexion to IOX network
    // (only when there is more than 1 cluster)
    if ( cluster_iob0 != cluster_iob1 )
    {
        (*clusters[XMAX-1][YMAX-1]->p_vci_iob_iox_ini) (signal_vci_ini_iob1);
        (*clusters[XMAX-1][YMAX-1]->p_vci_iob_iox_tgt) (signal_vci_tgt_iob1);
    }

    // All clusters Clock & RESET connexions
    for ( size_t x = 0; x < (XMAX); x++ )
    {
        for (size_t y = 0; y < YMAX; y++)
        {
            clusters[x][y]->p_clk     (signal_clk);
            clusters[x][y]->p_resetn  (signal_resetn);
        }
    }

   // Inter Clusters horizontal connections
   if (XMAX > 1)
   {
      for (size_t x = 0; x < (XMAX-1); x++)
      {
         for (size_t y = 0; y < YMAX; y++)
         {
            clusters[x][y]->p_dspin_int_cmd_out[EAST]      (signal_dspin_int_cmd_h_inc[x][y]);
            clusters[x+1][y]->p_dspin_int_cmd_in[WEST]     (signal_dspin_int_cmd_h_inc[x][y]);
            clusters[x][y]->p_dspin_int_cmd_in[EAST]       (signal_dspin_int_cmd_h_dec[x][y]);
            clusters[x+1][y]->p_dspin_int_cmd_out[WEST]    (signal_dspin_int_cmd_h_dec[x][y]);

            clusters[x][y]->p_dspin_int_rsp_out[EAST]      (signal_dspin_int_rsp_h_inc[x][y]);
            clusters[x+1][y]->p_dspin_int_rsp_in[WEST]     (signal_dspin_int_rsp_h_inc[x][y]);
            clusters[x][y]->p_dspin_int_rsp_in[EAST]       (signal_dspin_int_rsp_h_dec[x][y]);
            clusters[x+1][y]->p_dspin_int_rsp_out[WEST]    (signal_dspin_int_rsp_h_dec[x][y]);

            clusters[x][y]->p_dspin_int_m2p_out[EAST]      (signal_dspin_int_m2p_h_inc[x][y]);
            clusters[x+1][y]->p_dspin_int_m2p_in[WEST]     (signal_dspin_int_m2p_h_inc[x][y]);
            clusters[x][y]->p_dspin_int_m2p_in[EAST]       (signal_dspin_int_m2p_h_dec[x][y]);
            clusters[x+1][y]->p_dspin_int_m2p_out[WEST]    (signal_dspin_int_m2p_h_dec[x][y]);

            clusters[x][y]->p_dspin_int_p2m_out[EAST]      (signal_dspin_int_p2m_h_inc[x][y]);
            clusters[x+1][y]->p_dspin_int_p2m_in[WEST]     (signal_dspin_int_p2m_h_inc[x][y]);
            clusters[x][y]->p_dspin_int_p2m_in[EAST]       (signal_dspin_int_p2m_h_dec[x][y]);
            clusters[x+1][y]->p_dspin_int_p2m_out[WEST]    (signal_dspin_int_p2m_h_dec[x][y]);

            clusters[x][y]->p_dspin_int_cla_out[EAST]      (signal_dspin_int_cla_h_inc[x][y]);
            clusters[x+1][y]->p_dspin_int_cla_in[WEST]     (signal_dspin_int_cla_h_inc[x][y]);
            clusters[x][y]->p_dspin_int_cla_in[EAST]       (signal_dspin_int_cla_h_dec[x][y]);
            clusters[x+1][y]->p_dspin_int_cla_out[WEST]    (signal_dspin_int_cla_h_dec[x][y]);

            clusters[x][y]->p_dspin_ram_cmd_out[EAST]      (signal_dspin_ram_cmd_h_inc[x][y]);
            clusters[x+1][y]->p_dspin_ram_cmd_in[WEST]     (signal_dspin_ram_cmd_h_inc[x][y]);
            clusters[x][y]->p_dspin_ram_cmd_in[EAST]       (signal_dspin_ram_cmd_h_dec[x][y]);
            clusters[x+1][y]->p_dspin_ram_cmd_out[WEST]    (signal_dspin_ram_cmd_h_dec[x][y]);

            clusters[x][y]->p_dspin_ram_rsp_out[EAST]      (signal_dspin_ram_rsp_h_inc[x][y]);
            clusters[x+1][y]->p_dspin_ram_rsp_in[WEST]     (signal_dspin_ram_rsp_h_inc[x][y]);
            clusters[x][y]->p_dspin_ram_rsp_in[EAST]       (signal_dspin_ram_rsp_h_dec[x][y]);
            clusters[x+1][y]->p_dspin_ram_rsp_out[WEST]    (signal_dspin_ram_rsp_h_dec[x][y]);
         }
      }
   }

   std::cout << std::endl << "Horizontal connections established" << std::endl;

   // Inter Clusters vertical connections
   if (YMAX > 1)
   {
      for (size_t y = 0; y < (YMAX-1); y++)
      {
         for (size_t x = 0; x < XMAX; x++)
         {
            clusters[x][y]->p_dspin_int_cmd_out[NORTH]     (signal_dspin_int_cmd_v_inc[x][y]);
            clusters[x][y+1]->p_dspin_int_cmd_in[SOUTH]    (signal_dspin_int_cmd_v_inc[x][y]);
            clusters[x][y]->p_dspin_int_cmd_in[NORTH]      (signal_dspin_int_cmd_v_dec[x][y]);
            clusters[x][y+1]->p_dspin_int_cmd_out[SOUTH]   (signal_dspin_int_cmd_v_dec[x][y]);

            clusters[x][y]->p_dspin_int_rsp_out[NORTH]     (signal_dspin_int_rsp_v_inc[x][y]);
            clusters[x][y+1]->p_dspin_int_rsp_in[SOUTH]    (signal_dspin_int_rsp_v_inc[x][y]);
            clusters[x][y]->p_dspin_int_rsp_in[NORTH]      (signal_dspin_int_rsp_v_dec[x][y]);
            clusters[x][y+1]->p_dspin_int_rsp_out[SOUTH]   (signal_dspin_int_rsp_v_dec[x][y]);

            clusters[x][y]->p_dspin_int_m2p_out[NORTH]     (signal_dspin_int_m2p_v_inc[x][y]);
            clusters[x][y+1]->p_dspin_int_m2p_in[SOUTH]    (signal_dspin_int_m2p_v_inc[x][y]);
            clusters[x][y]->p_dspin_int_m2p_in[NORTH]      (signal_dspin_int_m2p_v_dec[x][y]);
            clusters[x][y+1]->p_dspin_int_m2p_out[SOUTH]   (signal_dspin_int_m2p_v_dec[x][y]);

            clusters[x][y]->p_dspin_int_p2m_out[NORTH]     (signal_dspin_int_p2m_v_inc[x][y]);
            clusters[x][y+1]->p_dspin_int_p2m_in[SOUTH]    (signal_dspin_int_p2m_v_inc[x][y]);
            clusters[x][y]->p_dspin_int_p2m_in[NORTH]      (signal_dspin_int_p2m_v_dec[x][y]);
            clusters[x][y+1]->p_dspin_int_p2m_out[SOUTH]   (signal_dspin_int_p2m_v_dec[x][y]);

            clusters[x][y]->p_dspin_int_cla_out[NORTH]     (signal_dspin_int_cla_v_inc[x][y]);
            clusters[x][y+1]->p_dspin_int_cla_in[SOUTH]    (signal_dspin_int_cla_v_inc[x][y]);
            clusters[x][y]->p_dspin_int_cla_in[NORTH]      (signal_dspin_int_cla_v_dec[x][y]);
            clusters[x][y+1]->p_dspin_int_cla_out[SOUTH]   (signal_dspin_int_cla_v_dec[x][y]);

            clusters[x][y]->p_dspin_ram_cmd_out[NORTH]     (signal_dspin_ram_cmd_v_inc[x][y]);
            clusters[x][y+1]->p_dspin_ram_cmd_in[SOUTH]    (signal_dspin_ram_cmd_v_inc[x][y]);
            clusters[x][y]->p_dspin_ram_cmd_in[NORTH]      (signal_dspin_ram_cmd_v_dec[x][y]);
            clusters[x][y+1]->p_dspin_ram_cmd_out[SOUTH]   (signal_dspin_ram_cmd_v_dec[x][y]);

            clusters[x][y]->p_dspin_ram_rsp_out[NORTH]     (signal_dspin_ram_rsp_v_inc[x][y]);
            clusters[x][y+1]->p_dspin_ram_rsp_in[SOUTH]    (signal_dspin_ram_rsp_v_inc[x][y]);
            clusters[x][y]->p_dspin_ram_rsp_in[NORTH]      (signal_dspin_ram_rsp_v_dec[x][y]);
            clusters[x][y+1]->p_dspin_ram_rsp_out[SOUTH]   (signal_dspin_ram_rsp_v_dec[x][y]);
         }
      }
   }

   std::cout << "Vertical connections established" << std::endl;

   // East & West boundary cluster connections
   for (size_t y = 0; y < YMAX; y++)
   {
      clusters[0][y]->p_dspin_int_cmd_in[WEST]         (signal_dspin_false_int_cmd_in[0][y][WEST]);
      clusters[0][y]->p_dspin_int_cmd_out[WEST]        (signal_dspin_false_int_cmd_out[0][y][WEST]);
      clusters[XMAX-1][y]->p_dspin_int_cmd_in[EAST]    (signal_dspin_false_int_cmd_in[XMAX-1][y][EAST]);
      clusters[XMAX-1][y]->p_dspin_int_cmd_out[EAST]   (signal_dspin_false_int_cmd_out[XMAX-1][y][EAST]);

      clusters[0][y]->p_dspin_int_rsp_in[WEST]         (signal_dspin_false_int_rsp_in[0][y][WEST]);
      clusters[0][y]->p_dspin_int_rsp_out[WEST]        (signal_dspin_false_int_rsp_out[0][y][WEST]);
      clusters[XMAX-1][y]->p_dspin_int_rsp_in[EAST]    (signal_dspin_false_int_rsp_in[XMAX-1][y][EAST]);
      clusters[XMAX-1][y]->p_dspin_int_rsp_out[EAST]   (signal_dspin_false_int_rsp_out[XMAX-1][y][EAST]);

      clusters[0][y]->p_dspin_int_m2p_in[WEST]         (signal_dspin_false_int_m2p_in[0][y][WEST]);
      clusters[0][y]->p_dspin_int_m2p_out[WEST]        (signal_dspin_false_int_m2p_out[0][y][WEST]);
      clusters[XMAX-1][y]->p_dspin_int_m2p_in[EAST]    (signal_dspin_false_int_m2p_in[XMAX-1][y][EAST]);
      clusters[XMAX-1][y]->p_dspin_int_m2p_out[EAST]   (signal_dspin_false_int_m2p_out[XMAX-1][y][EAST]);

      clusters[0][y]->p_dspin_int_p2m_in[WEST]         (signal_dspin_false_int_p2m_in[0][y][WEST]);
      clusters[0][y]->p_dspin_int_p2m_out[WEST]        (signal_dspin_false_int_p2m_out[0][y][WEST]);
      clusters[XMAX-1][y]->p_dspin_int_p2m_in[EAST]    (signal_dspin_false_int_p2m_in[XMAX-1][y][EAST]);
      clusters[XMAX-1][y]->p_dspin_int_p2m_out[EAST]   (signal_dspin_false_int_p2m_out[XMAX-1][y][EAST]);

      clusters[0][y]->p_dspin_int_cla_in[WEST]         (signal_dspin_false_int_cla_in[0][y][WEST]);
      clusters[0][y]->p_dspin_int_cla_out[WEST]        (signal_dspin_false_int_cla_out[0][y][WEST]);
      clusters[XMAX-1][y]->p_dspin_int_cla_in[EAST]    (signal_dspin_false_int_cla_in[XMAX-1][y][EAST]);
      clusters[XMAX-1][y]->p_dspin_int_cla_out[EAST]   (signal_dspin_false_int_cla_out[XMAX-1][y][EAST]);

      clusters[0][y]->p_dspin_ram_cmd_in[WEST]         (signal_dspin_false_ram_cmd_in[0][y][WEST]);
      clusters[0][y]->p_dspin_ram_cmd_out[WEST]        (signal_dspin_false_ram_cmd_out[0][y][WEST]);
      clusters[XMAX-1][y]->p_dspin_ram_cmd_in[EAST]    (signal_dspin_false_ram_cmd_in[XMAX-1][y][EAST]);
      clusters[XMAX-1][y]->p_dspin_ram_cmd_out[EAST]   (signal_dspin_false_ram_cmd_out[XMAX-1][y][EAST]);

      clusters[0][y]->p_dspin_ram_rsp_in[WEST]         (signal_dspin_false_ram_rsp_in[0][y][WEST]);
      clusters[0][y]->p_dspin_ram_rsp_out[WEST]        (signal_dspin_false_ram_rsp_out[0][y][WEST]);
      clusters[XMAX-1][y]->p_dspin_ram_rsp_in[EAST]    (signal_dspin_false_ram_rsp_in[XMAX-1][y][EAST]);
      clusters[XMAX-1][y]->p_dspin_ram_rsp_out[EAST]   (signal_dspin_false_ram_rsp_out[XMAX-1][y][EAST]);
   }

   std::cout << "East & West boundaries established" << std::endl;

   // North & South boundary clusters connections
   for (size_t x = 0; x < XMAX; x++)
   {
      clusters[x][0]->p_dspin_int_cmd_in[SOUTH]        (signal_dspin_false_int_cmd_in[x][0][SOUTH]);
      clusters[x][0]->p_dspin_int_cmd_out[SOUTH]       (signal_dspin_false_int_cmd_out[x][0][SOUTH]);
      clusters[x][YMAX-1]->p_dspin_int_cmd_in[NORTH]   (signal_dspin_false_int_cmd_in[x][YMAX-1][NORTH]);
      clusters[x][YMAX-1]->p_dspin_int_cmd_out[NORTH]  (signal_dspin_false_int_cmd_out[x][YMAX-1][NORTH]);

      clusters[x][0]->p_dspin_int_rsp_in[SOUTH]        (signal_dspin_false_int_rsp_in[x][0][SOUTH]);
      clusters[x][0]->p_dspin_int_rsp_out[SOUTH]       (signal_dspin_false_int_rsp_out[x][0][SOUTH]);
      clusters[x][YMAX-1]->p_dspin_int_rsp_in[NORTH]   (signal_dspin_false_int_rsp_in[x][YMAX-1][NORTH]);
      clusters[x][YMAX-1]->p_dspin_int_rsp_out[NORTH]  (signal_dspin_false_int_rsp_out[x][YMAX-1][NORTH]);

      clusters[x][0]->p_dspin_int_m2p_in[SOUTH]        (signal_dspin_false_int_m2p_in[x][0][SOUTH]);
      clusters[x][0]->p_dspin_int_m2p_out[SOUTH]       (signal_dspin_false_int_m2p_out[x][0][SOUTH]);
      clusters[x][YMAX-1]->p_dspin_int_m2p_in[NORTH]   (signal_dspin_false_int_m2p_in[x][YMAX-1][NORTH]);
      clusters[x][YMAX-1]->p_dspin_int_m2p_out[NORTH]  (signal_dspin_false_int_m2p_out[x][YMAX-1][NORTH]);

      clusters[x][0]->p_dspin_int_p2m_in[SOUTH]        (signal_dspin_false_int_p2m_in[x][0][SOUTH]);
      clusters[x][0]->p_dspin_int_p2m_out[SOUTH]       (signal_dspin_false_int_p2m_out[x][0][SOUTH]);
      clusters[x][YMAX-1]->p_dspin_int_p2m_in[NORTH]   (signal_dspin_false_int_p2m_in[x][YMAX-1][NORTH]);
      clusters[x][YMAX-1]->p_dspin_int_p2m_out[NORTH]  (signal_dspin_false_int_p2m_out[x][YMAX-1][NORTH]);

      clusters[x][0]->p_dspin_int_cla_in[SOUTH]        (signal_dspin_false_int_cla_in[x][0][SOUTH]);
      clusters[x][0]->p_dspin_int_cla_out[SOUTH]       (signal_dspin_false_int_cla_out[x][0][SOUTH]);
      clusters[x][YMAX-1]->p_dspin_int_cla_in[NORTH]   (signal_dspin_false_int_cla_in[x][YMAX-1][NORTH]);
      clusters[x][YMAX-1]->p_dspin_int_cla_out[NORTH]  (signal_dspin_false_int_cla_out[x][YMAX-1][NORTH]);

      clusters[x][0]->p_dspin_ram_cmd_in[SOUTH]        (signal_dspin_false_ram_cmd_in[x][0][SOUTH]);
      clusters[x][0]->p_dspin_ram_cmd_out[SOUTH]       (signal_dspin_false_ram_cmd_out[x][0][SOUTH]);
      clusters[x][YMAX-1]->p_dspin_ram_cmd_in[NORTH]   (signal_dspin_false_ram_cmd_in[x][YMAX-1][NORTH]);
      clusters[x][YMAX-1]->p_dspin_ram_cmd_out[NORTH]  (signal_dspin_false_ram_cmd_out[x][YMAX-1][NORTH]);

      clusters[x][0]->p_dspin_ram_rsp_in[SOUTH]        (signal_dspin_false_ram_rsp_in[x][0][SOUTH]);
      clusters[x][0]->p_dspin_ram_rsp_out[SOUTH]       (signal_dspin_false_ram_rsp_out[x][0][SOUTH]);
      clusters[x][YMAX-1]->p_dspin_ram_rsp_in[NORTH]   (signal_dspin_false_ram_rsp_in[x][YMAX-1][NORTH]);
      clusters[x][YMAX-1]->p_dspin_ram_rsp_out[NORTH]  (signal_dspin_false_ram_rsp_out[x][YMAX-1][NORTH]);
   }

   std::cout << "North & South boundaries established" << std::endl << std::endl;

   ////////////////////////////////////////////////////////
   //   Simulation
   ///////////////////////////////////////////////////////

   sc_start(sc_core::sc_time(0, SC_NS));

   signal_resetn = false;
   signal_irq_false = false;

   // network boundaries signals
   for (size_t x = 0; x < XMAX ; x++)
   {
      for (size_t y = 0; y < YMAX ; y++)
      {
         for (size_t a = 0; a < 4; a++)
         {
            signal_dspin_false_int_cmd_in[x][y][a].write = false;
            signal_dspin_false_int_cmd_in[x][y][a].read = true;
            signal_dspin_false_int_cmd_out[x][y][a].write = false;
            signal_dspin_false_int_cmd_out[x][y][a].read = true;

            signal_dspin_false_int_rsp_in[x][y][a].write = false;
            signal_dspin_false_int_rsp_in[x][y][a].read = true;
            signal_dspin_false_int_rsp_out[x][y][a].write = false;
            signal_dspin_false_int_rsp_out[x][y][a].read = true;

            signal_dspin_false_int_m2p_in[x][y][a].write = false;
            signal_dspin_false_int_m2p_in[x][y][a].read = true;
            signal_dspin_false_int_m2p_out[x][y][a].write = false;
            signal_dspin_false_int_m2p_out[x][y][a].read = true;

            signal_dspin_false_int_p2m_in[x][y][a].write = false;
            signal_dspin_false_int_p2m_in[x][y][a].read = true;
            signal_dspin_false_int_p2m_out[x][y][a].write = false;
            signal_dspin_false_int_p2m_out[x][y][a].read = true;

            signal_dspin_false_int_cla_in[x][y][a].write = false;
            signal_dspin_false_int_cla_in[x][y][a].read = true;
            signal_dspin_false_int_cla_out[x][y][a].write = false;
            signal_dspin_false_int_cla_out[x][y][a].read = true;

            signal_dspin_false_ram_cmd_in[x][y][a].write = false;
            signal_dspin_false_ram_cmd_in[x][y][a].read = true;
            signal_dspin_false_ram_cmd_out[x][y][a].write = false;
            signal_dspin_false_ram_cmd_out[x][y][a].read = true;

            signal_dspin_false_ram_rsp_in[x][y][a].write = false;
            signal_dspin_false_ram_rsp_in[x][y][a].read = true;
            signal_dspin_false_ram_rsp_out[x][y][a].write = false;
            signal_dspin_false_ram_rsp_out[x][y][a].read = true;
         }
      }
   }

    sc_start(sc_core::sc_time(1, SC_NS));
    signal_resetn = true;


    // simulation loop
    struct timeval t1,t2;
    gettimeofday(&t1, NULL);


    for ( size_t n = 0; n < ncycles ; n += simul_period )
    {
        // stats display
        if( (n % 1000000) == 0)
        {
            gettimeofday(&t2, NULL);

            uint64_t ms1 = (uint64_t) t1.tv_sec  * 1000ULL +
                           (uint64_t) t1.tv_usec / 1000;
            uint64_t ms2 = (uint64_t) t2.tv_sec  * 1000ULL +
                           (uint64_t) t2.tv_usec / 1000;
            std::cerr << "### cycle = " << std::dec << n
                      << " / frequency = "
                      << (double) 1000000 / (double) (ms2 - ms1) << "Khz"
                      << std::endl;

            gettimeofday(&t1, NULL);
        }

        // Monitor a specific address for one L1 cache
        // clusters[0][0]->proc[0]->cache_monitor(0x800080ULL);

        // Monitor a specific address for one L2 cache (single word if second argument true)
        // clusters[0][0]->memc->cache_monitor( 0x00FF8000ULL, false );

        // Monitor a specific address for one XRAM
        // clusters[0][0]->xram->start_monitor( 0x600800ULL , 64);

        if ( debug_ok and (n > debug_from) )
        {
            std::cout << "****************** cycle " << std::dec << n ;
            std::cout << " ************************************************" << std::endl;

            // trace proc[debug_proc_id]
            if ( debug_proc_id != 0xFFFFFFFF )
            {
                size_t l          = debug_proc_id & ((1<<P_WIDTH)-1) ;
                size_t cluster_xy = debug_proc_id >> P_WIDTH ;
                size_t x          = cluster_xy >> 4;
                size_t y          = cluster_xy & 0xF;

                clusters[x][y]->proc[l]->print_trace(0x1);
                std::ostringstream proc_signame;
                proc_signame << "[SIG]PROC_" << x << "_" << y << "_" << l ;
                clusters[x][y]->signal_int_vci_ini_proc[l].print_trace(proc_signame.str());

                clusters[x][y]->xicu->print_trace(1);
                std::ostringstream xicu_signame;
                xicu_signame << "[SIG]XICU_" << x << "_" << y;
                clusters[x][y]->signal_int_vci_tgt_xicu.print_trace(xicu_signame.str());

                // coprocessor in cluster(x,y)
//              clusters[x][y]->mwmr->print_trace();
//              std::ostringstream mwmr_tgt_signame;
//              mwmr_tgt_signame << "[SIG]MWMR_TGT_" << x << "_" << y;
//              clusters[x][y]->signal_int_vci_tgt_mwmr.print_trace(mwmr_tgt_signame.str());
//              std::ostringstream mwmr_ini_signame;
//              mwmr_ini_signame << "[SIG]MWMR_INI_" << x << "_" << y;
//              clusters[x][y]->signal_int_vci_ini_mwmr.print_trace(mwmr_ini_signame.str());
//              if ( USE_MWR_CPY ) clusters[x][y]->cpy->print_trace();
//              if ( USE_MWR_DCT ) clusters[x][y]->dct->print_trace();
//              if ( USE_MWR_GCD ) clusters[x][y]->gcd->print_trace();

                // local interrupts in cluster(x,y)
                if( clusters[x][y]->signal_irq_memc.read() )
                std::cout << "### IRQ_MMC_" << std::dec << x << "_" << y
                          << " ACTIVE" << std::endl;

                if( clusters[x][y]->signal_irq_mwmr.read() )
                std::cout << "### IRQ_MWR_" << std::dec << x << "_" << y 
                          << " ACTIVE" << std::endl;

                for ( size_t c = 0 ; c < NB_PROCS_MAX ; c++ )
                {
                    if( clusters[x][y]->signal_proc_it[c<<2].read() )
                    std::cout << "### IRQ_PROC_" << std::dec << x << "_" << y << "_" << c
                              << " ACTIVE" << std::endl;
                }
            }

            // trace memc[debug_memc_id]
            if ( debug_memc_id != 0xFFFFFFFF )
            {
                size_t x = debug_memc_id >> 4;
                size_t y = debug_memc_id & 0xF;

                clusters[x][y]->memc->print_trace(0);
                std::ostringstream smemc_tgt;
                smemc_tgt << "[SIG]MEMC_TGT_" << x << "_" << y;
                clusters[x][y]->signal_int_vci_tgt_memc.print_trace(smemc_tgt.str());
                std::ostringstream smemc_ini;
                smemc_ini << "[SIG]MEMC_INI_" << x << "_" << y;
                clusters[x][y]->signal_ram_vci_ini_memc.print_trace(smemc_ini.str());

                clusters[x][y]->xram->print_trace();
                std::ostringstream sxram_tgt;
                sxram_tgt << "[SIG]XRAM_TGT_" << x << "_" << y;
                clusters[x][y]->signal_ram_vci_tgt_xram.print_trace(sxram_tgt.str());
            }


            // trace XRAM and XRAM network routers in cluster[debug_xram_id]
            if ( debug_xram_id != 0xFFFFFFFF )
            {
                size_t x = debug_xram_id >> 4;
                size_t y = debug_xram_id & 0xF;

                clusters[x][y]->xram->print_trace();
                std::ostringstream sxram_tgt;
                sxram_tgt << "[SIG]XRAM_TGT_" << x << "_" << y;
                clusters[x][y]->signal_ram_vci_tgt_xram.print_trace(sxram_tgt.str());

                clusters[x][y]->ram_router_cmd->print_trace();
                clusters[x][y]->ram_router_rsp->print_trace();
            }

            // trace iob, iox and external peripherals
            if ( debug_iob )
            {
//              clusters[0][0]->iob->print_trace();
//              clusters[0][0]->signal_int_vci_tgt_iobx.print_trace( "[SIG]IOB0_INT_TGT");
//              clusters[0][0]->signal_int_vci_ini_iobx.print_trace( "[SIG]IOB0_INT_INI");
//              clusters[0][0]->signal_ram_vci_ini_iobx.print_trace( "[SIG]IOB0_RAM_INI");
//              signal_vci_ini_iob0.print_trace("[SIG]IOB0_IOX_INI");
//              signal_vci_tgt_iob0.print_trace("[SIG]IOB0_IOX_TGT");

//              cdma->print_trace();
//              signal_vci_tgt_cdma.print_trace("[SIG]CDMA_TGT");
//              signal_vci_ini_cdma.print_trace("[SIG]CDMA_INI");

//              brom->print_trace();
//              signal_vci_tgt_brom.print_trace("[SIG]BROM_TGT");

//              mtty->print_trace();
//              signal_vci_tgt_mtty.print_trace("[SIG]MTTY_TGT");

                disk->print_trace();
                signal_vci_tgt_disk.print_trace("[SIG]DISK_TGT");
                signal_vci_ini_disk.print_trace("[SIG]DISK_INI");

#if ( USE_IOC_SDC )
                card->print_trace();
#endif

//              mnic->print_trace( 0x000 );
//              signal_vci_tgt_mnic.print_trace("[SIG]MNIC_TGT");

//              fbuf->print_trace();
//              signal_vci_tgt_fbuf.print_trace("[SIG]FBUF_TGT");

//              iopi->print_trace();
//              signal_vci_ini_iopi.print_trace("[SIG]IOPI_INI");
//              signal_vci_tgt_iopi.print_trace("[SIG]IOPI_TGT");

//              iox_network->print_trace();

                // interrupts
                if ( signal_irq_disk.read() )      
                std::cout << "### IRQ_DISK ACTIVE" << std::endl;

                if ( signal_irq_mtty_rx[0].read() ) 
                std::cout << "### IRQ_MTTY_RX[0] ACTIVE" << std::endl;

                if ( signal_irq_mnic_rx[0].read() ) 
                std::cout << "### IRQ_MNIC_RX[0] ACTIVE" << std::endl;

                if ( signal_irq_mnic_tx[0].read() ) 
                std::cout << "### IRQ_MNIC_TX[0] ACTIVE" << std::endl;
            }
        }

        sc_start(sc_core::sc_time(simul_period, SC_NS));
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
   return 1;
}


// Local Variables:
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3



