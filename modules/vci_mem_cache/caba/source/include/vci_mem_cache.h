/* -*- c++ -*-
 * File         : vci_mem_cache.h
 * Date         : 26/10/2008
 * Copyright    : UPMC / LIP6
 * Authors      : Alain Greiner / Eric Guthmuller
 *
 * SOCLIB_LGPL_HEADER_BEGIN
 *
 * This file is part of SoCLib, GNU LGPLv2.1.
 *
 * SoCLib is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 of the License.
 *
 * SoCLib is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SoCLib; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * SOCLIB_LGPL_HEADER_END
 *
 * Maintainers: alain.greiner@lip6.fr 
 *              eric.guthmuller@polytechnique.edu
 *              cesar.fuguet-tortolero@lip6.fr
 *              alexandre.joannou@lip6.fr
 */

#ifndef SOCLIB_CABA_MEM_CACHE_H
#define SOCLIB_CABA_MEM_CACHE_H

#include <inttypes.h>
#include <systemc>
#include <list>
#include <cassert>
#include "arithmetics.h"
#include "alloc_elems.h"
#include "caba_base_module.h"
#include "vci_target.h"
#include "vci_initiator.h"
#include "generic_fifo.h"
#include "mapping_table.h"
#include "int_tab.h"
#include "generic_llsc_global_table.h"
#include "mem_cache_directory.h"
#include "xram_transaction.h"
#include "update_tab.h"
#include "dspin_interface.h"
#include "dspin_dhccp_param.h"

#define TRT_ENTRIES      4      // Number of entries in TRT
#define UPT_ENTRIES      4      // Number of entries in UPT
#define IVT_ENTRIES      4      // Number of entries in IVT
#define HEAP_ENTRIES     1024   // Number of entries in HEAP

namespace soclib {  namespace caba {

  using namespace sc_core;

  template<typename vci_param_int, 
           typename vci_param_ext,
           size_t   memc_dspin_in_width,
           size_t   memc_dspin_out_width>
    class VciMemCache
    : public soclib::caba::BaseModule
    {
      typedef typename vci_param_int::fast_addr_t  addr_t;
      typedef typename sc_dt::sc_uint<64>          wide_data_t;
      typedef uint32_t                             data_t;
      typedef uint32_t                             tag_t;
      typedef uint32_t                             be_t;
      typedef uint32_t                             copy_t;

      /* States of the TGT_CMD fsm */
      enum tgt_cmd_fsm_state_e
      {
        TGT_CMD_IDLE,
        TGT_CMD_READ,
        TGT_CMD_WRITE,
        TGT_CMD_CAS,
        TGT_CMD_CONFIG,
        TGT_CMD_ERROR
      };

      /* States of the TGT_RSP fsm */
      enum tgt_rsp_fsm_state_e
      {
        TGT_RSP_READ_IDLE,
        TGT_RSP_WRITE_IDLE,
        TGT_RSP_CAS_IDLE,
        TGT_RSP_XRAM_IDLE,
        TGT_RSP_MULTI_ACK_IDLE,
        TGT_RSP_CLEANUP_IDLE,
        TGT_RSP_TGT_CMD_IDLE,
        TGT_RSP_CONFIG_IDLE,
        TGT_RSP_READ,
        TGT_RSP_WRITE,
        TGT_RSP_CAS,
        TGT_RSP_XRAM,
        TGT_RSP_MULTI_ACK,
        TGT_RSP_CLEANUP,
        TGT_RSP_TGT_CMD,
        TGT_RSP_CONFIG
      };

      /* States of the DSPIN_TGT fsm */
      enum cc_receive_fsm_state_e
      {
        CC_RECEIVE_IDLE,
        CC_RECEIVE_CLEANUP,
        CC_RECEIVE_CLEANUP_EOP,
        CC_RECEIVE_MULTI_ACK
      };

      /* States of the CC_SEND fsm */
      enum cc_send_fsm_state_e
      {
        CC_SEND_XRAM_RSP_IDLE,
        CC_SEND_WRITE_IDLE,
        CC_SEND_CAS_IDLE,
        CC_SEND_CONFIG_IDLE,
        CC_SEND_XRAM_RSP_BRDCAST_HEADER,
        CC_SEND_XRAM_RSP_BRDCAST_NLINE,
        CC_SEND_XRAM_RSP_INVAL_HEADER,
        CC_SEND_XRAM_RSP_INVAL_NLINE,
        CC_SEND_WRITE_BRDCAST_HEADER,
        CC_SEND_WRITE_BRDCAST_NLINE,
        CC_SEND_WRITE_UPDT_HEADER,
        CC_SEND_WRITE_UPDT_NLINE,
        CC_SEND_WRITE_UPDT_DATA,
        CC_SEND_CAS_BRDCAST_HEADER,
        CC_SEND_CAS_BRDCAST_NLINE,
        CC_SEND_CAS_UPDT_HEADER,
        CC_SEND_CAS_UPDT_NLINE,
        CC_SEND_CAS_UPDT_DATA,
        CC_SEND_CAS_UPDT_DATA_HIGH,
        CC_SEND_CONFIG_INVAL_HEADER,
        CC_SEND_CONFIG_INVAL_NLINE,
        CC_SEND_CONFIG_BRDCAST_HEADER,
        CC_SEND_CONFIG_BRDCAST_NLINE
      };

      /* States of the MULTI_ACK fsm */
      enum multi_ack_fsm_state_e
      {
        MULTI_ACK_IDLE,
        MULTI_ACK_UPT_LOCK,
        MULTI_ACK_UPT_CLEAR,
        MULTI_ACK_WRITE_RSP
      };

      /* States of the CONFIG fsm */
      enum config_fsm_state_e
      {
        CONFIG_IDLE,
        CONFIG_LOOP,
        CONFIG_WAIT,
        CONFIG_RSP,
        CONFIG_DIR_REQ,
        CONFIG_DIR_ACCESS,
        CONFIG_IVT_LOCK,
        CONFIG_BC_SEND,
        CONFIG_INVAL_SEND,
        CONFIG_HEAP_REQ,
        CONFIG_HEAP_SCAN,
        CONFIG_HEAP_LAST,
        CONFIG_TRT_LOCK,
        CONFIG_TRT_SET,
        CONFIG_PUT_REQ
      };

      /* States of the READ fsm */
      enum read_fsm_state_e
      {
        READ_IDLE,
        READ_DIR_REQ,
        READ_DIR_LOCK,
        READ_DIR_HIT,
        READ_HEAP_REQ,
        READ_HEAP_LOCK,
        READ_HEAP_WRITE,
        READ_HEAP_ERASE,
        READ_HEAP_LAST,
        READ_RSP,
        READ_TRT_LOCK,
        READ_TRT_SET,
        READ_TRT_REQ
      };

      /* States of the WRITE fsm */
      enum write_fsm_state_e
      {
        WRITE_IDLE,
        WRITE_NEXT,
        WRITE_DIR_REQ,
        WRITE_DIR_LOCK,
        WRITE_DIR_HIT,
        WRITE_UPT_LOCK,
        WRITE_UPT_HEAP_LOCK,
        WRITE_UPT_REQ,
        WRITE_UPT_NEXT,
        WRITE_UPT_DEC,
        WRITE_RSP,
        WRITE_MISS_TRT_LOCK,
        WRITE_MISS_TRT_DATA,
        WRITE_MISS_TRT_SET,
        WRITE_MISS_XRAM_REQ,
        WRITE_BC_DIR_READ,
        WRITE_BC_TRT_LOCK,
        WRITE_BC_IVT_LOCK,
        WRITE_BC_DIR_INVAL,
        WRITE_BC_CC_SEND,
        WRITE_BC_XRAM_REQ,
        WRITE_WAIT
      };

      /* States of the IXR_RSP fsm */
      enum ixr_rsp_fsm_state_e
      {
        IXR_RSP_IDLE,
        IXR_RSP_TRT_ERASE,
        IXR_RSP_TRT_READ
      };

      /* States of the XRAM_RSP fsm */
      enum xram_rsp_fsm_state_e
      {
        XRAM_RSP_IDLE,
        XRAM_RSP_TRT_COPY,
        XRAM_RSP_TRT_DIRTY,
        XRAM_RSP_DIR_LOCK,
        XRAM_RSP_DIR_UPDT,
        XRAM_RSP_DIR_RSP,
        XRAM_RSP_IVT_LOCK,
        XRAM_RSP_INVAL_WAIT,
        XRAM_RSP_INVAL,
        XRAM_RSP_WRITE_DIRTY,
        XRAM_RSP_HEAP_REQ,
        XRAM_RSP_HEAP_ERASE,
        XRAM_RSP_HEAP_LAST,
        XRAM_RSP_ERROR_ERASE,
        XRAM_RSP_ERROR_RSP
      };

      /* States of the IXR_CMD fsm */
      enum ixr_cmd_fsm_state_e
      {
        IXR_CMD_READ_IDLE,
        IXR_CMD_WRITE_IDLE,
        IXR_CMD_CAS_IDLE,
        IXR_CMD_XRAM_IDLE,
        IXR_CMD_CONFIG_IDLE,
        IXR_CMD_READ_TRT,
        IXR_CMD_WRITE_TRT,
        IXR_CMD_CAS_TRT,
        IXR_CMD_XRAM_TRT,
        IXR_CMD_CONFIG_TRT,
        IXR_CMD_READ_SEND,
        IXR_CMD_WRITE_SEND,
        IXR_CMD_CAS_SEND,
        IXR_CMD_XRAM_SEND,
        IXR_CMD_CONFIG_SEND
      };

      /* States of the CAS fsm */
      enum cas_fsm_state_e
      {
        CAS_IDLE,
        CAS_DIR_REQ,
        CAS_DIR_LOCK,
        CAS_DIR_HIT_READ,
        CAS_DIR_HIT_COMPARE,
        CAS_DIR_HIT_WRITE,
        CAS_UPT_LOCK,
        CAS_UPT_HEAP_LOCK,
        CAS_UPT_REQ,
        CAS_UPT_NEXT,
        CAS_BC_TRT_LOCK,
        CAS_BC_IVT_LOCK,
        CAS_BC_DIR_INVAL,
        CAS_BC_CC_SEND,
        CAS_BC_XRAM_REQ,
        CAS_RSP_FAIL,
        CAS_RSP_SUCCESS,
        CAS_MISS_TRT_LOCK,
        CAS_MISS_TRT_SET,
        CAS_MISS_XRAM_REQ,
        CAS_WAIT
      };

      /* States of the CLEANUP fsm */
      enum cleanup_fsm_state_e
      {
        CLEANUP_IDLE,
        CLEANUP_GET_NLINE,
        CLEANUP_DIR_REQ,
        CLEANUP_DIR_LOCK,
        CLEANUP_DIR_WRITE,
        CLEANUP_HEAP_REQ,
        CLEANUP_HEAP_LOCK,
        CLEANUP_HEAP_SEARCH,
        CLEANUP_HEAP_CLEAN,
        CLEANUP_HEAP_FREE,
        CLEANUP_IVT_LOCK,
        CLEANUP_IVT_DECREMENT,
        CLEANUP_IVT_CLEAR,
        CLEANUP_WRITE_RSP,
        CLEANUP_SEND_CLACK
      };

      /* States of the ALLOC_DIR fsm */
      enum alloc_dir_fsm_state_e
      {
        ALLOC_DIR_RESET,
        ALLOC_DIR_READ,
        ALLOC_DIR_WRITE,
        ALLOC_DIR_CAS,
        ALLOC_DIR_CLEANUP,
        ALLOC_DIR_XRAM_RSP,
        ALLOC_DIR_CONFIG
      };

      /* States of the ALLOC_TRT fsm */
      enum alloc_trt_fsm_state_e
      {
        ALLOC_TRT_READ,
        ALLOC_TRT_WRITE,
        ALLOC_TRT_CAS,
        ALLOC_TRT_XRAM_RSP,
        ALLOC_TRT_IXR_RSP,
        ALLOC_TRT_IXR_CMD,
        ALLOC_TRT_CONFIG
      };

      /* States of the ALLOC_UPT fsm */
      enum alloc_upt_fsm_state_e
      {
        ALLOC_UPT_WRITE,
        ALLOC_UPT_CAS,
        ALLOC_UPT_MULTI_ACK
      };

      /* States of the ALLOC_IVT fsm */
      enum alloc_ivt_fsm_state_e
      {
        ALLOC_IVT_WRITE,
        ALLOC_IVT_XRAM_RSP,
        ALLOC_IVT_CLEANUP,
        ALLOC_IVT_CAS,
        ALLOC_IVT_CONFIG
      };

      /* States of the ALLOC_HEAP fsm */
      enum alloc_heap_fsm_state_e
      {
        ALLOC_HEAP_RESET,
        ALLOC_HEAP_READ,
        ALLOC_HEAP_WRITE,
        ALLOC_HEAP_CAS,
        ALLOC_HEAP_CLEANUP,
        ALLOC_HEAP_XRAM_RSP,
        ALLOC_HEAP_CONFIG
      };

      /* transaction type, pktid field */
      enum transaction_type_e
      {
          // b3 unused
          // b2 READ / NOT READ
          // Si READ
          //  b1 DATA / INS
          //  b0 UNC / MISS
          // Si NOT READ
          //  b1 accès table llsc type SW / other
          //  b2 WRITE/CAS/LL/SC
          TYPE_READ_DATA_UNC          = 0x0,
          TYPE_READ_DATA_MISS         = 0x1,
          TYPE_READ_INS_UNC           = 0x2,
          TYPE_READ_INS_MISS          = 0x3,
          TYPE_WRITE                  = 0x4,
          TYPE_CAS                    = 0x5,
          TYPE_LL                     = 0x6,
          TYPE_SC                     = 0x7
      };

      /* SC return values */
      enum sc_status_type_e
      {
          SC_SUCCESS  =   0x00000000,
          SC_FAIL     =   0x00000001
      };

      // debug variables 
      bool                 m_debug;
      bool                 m_debug_previous_valid;
      size_t               m_debug_previous_count;
      bool                 m_debug_previous_dirty;
      data_t *             m_debug_previous_data;
      data_t *             m_debug_data;

      // instrumentation counters
      uint64_t     m_cpt_cycles;         // Counter of cycles
      uint64_t     m_cpt_reset_count;    // Cycle at which the counters were last reset

      // Counters accessible in software (not yet but eventually)
      uint32_t     m_cpt_read_local;     // Number of local READ transactions
      uint32_t     m_cpt_read_remote;    // number of remote READ transactions
      uint32_t     m_cpt_read_cost;      // Number of (flits * distance) for READs

      uint32_t     m_cpt_write_local;    // Number of local WRITE transactions
      uint32_t     m_cpt_write_remote;   // number of remote WRITE transactions
      uint32_t     m_cpt_write_flits_local;  // number of flits for local WRITEs
      uint32_t     m_cpt_write_flits_remote; // number of flits for remote WRITEs
      uint32_t     m_cpt_write_cost;     // Number of (flits * distance) for WRITEs

      uint32_t     m_cpt_ll_local;       // Number of local LL transactions
      uint32_t     m_cpt_ll_remote;      // number of remote LL transactions
      uint32_t     m_cpt_ll_cost;        // Number of (flits * distance) for LLs

      uint32_t     m_cpt_sc_local;       // Number of local SC transactions
      uint32_t     m_cpt_sc_remote;      // number of remote SC transactions
      uint32_t     m_cpt_sc_cost;        // Number of (flits * distance) for SCs

      uint32_t     m_cpt_cas_local;      // Number of local SC transactions
      uint32_t     m_cpt_cas_remote;     // number of remote SC transactions
      uint32_t     m_cpt_cas_cost;       // Number of (flits * distance) for SCs

      uint32_t     m_cpt_update;         // Number of requests causing an UPDATE
      uint32_t     m_cpt_update_local;   // Number of local UPDATE transactions
      uint32_t     m_cpt_update_remote;  // Number of remote UPDATE transactions
      uint32_t     m_cpt_update_cost;    // Number of (flits * distance) for UPDT

      uint32_t     m_cpt_minval;         // Number of requests causing M_INV
      uint32_t     m_cpt_minval_local;   // Number of local M_INV transactions
      uint32_t     m_cpt_minval_remote;  // Number of remote M_INV transactions
      uint32_t     m_cpt_minval_cost;    // Number of (flits * distance) for M_INV

      uint32_t     m_cpt_binval;         // Number of BROADCAST INVAL

      uint32_t     m_cpt_cleanup_local;  // Number of local CLEANUP transactions
      uint32_t     m_cpt_cleanup_remote; // Number of remote CLEANUP transactions
      uint32_t     m_cpt_cleanup_cost;   // Number of (flits * distance) for CLEANUPs

      // Counters not accessible by software
      uint32_t     m_cpt_read_miss;      // Number of MISS READ
      uint32_t     m_cpt_write_miss;     // Number of MISS WRITE
      uint32_t     m_cpt_write_dirty;    // Cumulated length for WRITE transactions
      uint32_t     m_cpt_write_broadcast;// Number of BROADCAST INVAL because of writes

      uint32_t     m_cpt_trt_rb;         // Read blocked by a hit in trt
      uint32_t     m_cpt_trt_full;       // Transaction blocked due to a full trt

      uint32_t     m_cpt_get;
      uint32_t     m_cpt_put;

      size_t       m_prev_count;

      protected:

      SC_HAS_PROCESS(VciMemCache);

      public:
      sc_in<bool>                                 p_clk;
      sc_in<bool>                                 p_resetn;
      sc_out<bool>                                p_irq;
      soclib::caba::VciTarget<vci_param_int>      p_vci_tgt;
      soclib::caba::VciInitiator<vci_param_ext>   p_vci_ixr;
      soclib::caba::DspinInput<memc_dspin_in_width>    p_dspin_p2m;
      soclib::caba::DspinOutput<memc_dspin_out_width>  p_dspin_m2p;
      soclib::caba::DspinOutput<memc_dspin_out_width>  p_dspin_clack;

#if MONITOR_MEMCACHE_FSM == 1
      sc_out<int> p_read_fsm; 
      sc_out<int> p_write_fsm; 
      sc_out<int> p_xram_rsp_fsm; 
      sc_out<int> p_cas_fsm; 
      sc_out<int> p_cleanup_fsm; 
      sc_out<int> p_config_fsm; 
      sc_out<int> p_alloc_heap_fsm; 
      sc_out<int> p_alloc_dir_fsm; 
      sc_out<int> p_alloc_trt_fsm; 
      sc_out<int> p_alloc_upt_fsm; 
      sc_out<int> p_alloc_ivt_fsm; 
      sc_out<int> p_tgt_cmd_fsm; 
      sc_out<int> p_tgt_rsp_fsm; 
      sc_out<int> p_ixr_cmd_fsm; 
      sc_out<int> p_ixr_rsp_fsm; 
      sc_out<int> p_cc_send_fsm; 
      sc_out<int> p_cc_receive_fsm; 
      sc_out<int> p_multi_ack_fsm; 
#endif

      VciMemCache(
          sc_module_name name,                                // Instance Name
          const soclib::common::MappingTable &mtp,            // Mapping table INT network
          const soclib::common::MappingTable &mtx,            // Mapping table RAM network
          const soclib::common::IntTab       &srcid_x,        // global index RAM network
          const soclib::common::IntTab       &tgtid_d,        // global index INT network
          const size_t                       x_width,         // X width in platform
          const size_t                       y_width,         // Y width in platform
          const size_t                       nways,           // Number of ways per set
          const size_t                       nsets,           // Number of sets
          const size_t                       nwords,          // Number of words per line
          const size_t                       max_copies,      // max number of copies
          const size_t                       heap_size=HEAP_ENTRIES,
          const size_t                       trt_lines=TRT_ENTRIES, 
          const size_t                       upt_lines=UPT_ENTRIES,     
          const size_t                       ivt_lines=IVT_ENTRIES,     
          const size_t                       debug_start_cycle=0,
          const bool                         debug_ok=false );

      ~VciMemCache();

      void reset_counters();
      void print_stats(bool activity_counters = true, bool stats = true);
      void print_trace( size_t detailed = 0 );
      void cache_monitor(addr_t addr, bool single_word = false);
      void start_monitor(addr_t addr, addr_t length);
      void stop_monitor();

      private:

      void transition();
      void genMoore();
      void check_monitor(addr_t addr, data_t data, bool read);

      uint32_t req_distance(uint32_t req_srcid);
      bool is_local_req(uint32_t req_srcid);
      int  read_instrumentation(uint32_t regr, uint32_t & rdata);

      // Component attributes
      std::list<soclib::common::Segment> m_seglist;          // segments allocated 
      size_t                             m_nseg;             // number of segments
      soclib::common::Segment            **m_seg;            // array of segments pointers
      size_t                             m_seg_config;       // config segment index
      const size_t                       m_srcid_x;          // global index on RAM network
      const size_t                       m_initiators;       // Number of initiators
      const size_t                       m_heap_size;        // Size of the heap
      const size_t                       m_ways;             // Number of ways in a set
      const size_t                       m_sets;             // Number of cache sets
      const size_t                       m_words;            // Number of words in a line
      size_t                             m_x_self;           // X self coordinate
      size_t                             m_y_self;           // Y self coordinate
      const size_t                       m_x_width;          // number of x bits in platform
      const size_t                       m_y_width;          // number of y bits in platform
      size_t                             m_debug_start_cycle;
      bool                               m_debug_ok;
      uint32_t                           m_trt_lines;
      TransactionTab                     m_trt;              // xram transaction table
      uint32_t                           m_upt_lines;
      UpdateTab                          m_upt;              // pending update
      UpdateTab                          m_ivt;              // pending invalidate
      CacheDirectory                     m_cache_directory;  // data cache directory
      CacheData                          m_cache_data;       // data array[set][way][word]
      HeapDirectory                      m_heap;             // heap for copies
      size_t                             m_max_copies;       // max number of copies in heap
      GenericLLSCGlobalTable
      < 32  ,    // number of slots
        4096,    // number of processors in the system
        8000,    // registration life (# of LL operations)
        addr_t >                         m_llsc_table;       // ll/sc registration table

      // adress masks
      const soclib::common::AddressMaskingTable<addr_t>   m_x;
      const soclib::common::AddressMaskingTable<addr_t>   m_y;
      const soclib::common::AddressMaskingTable<addr_t>   m_z;
      const soclib::common::AddressMaskingTable<addr_t>   m_nline;

      // broadcast address
      uint32_t                           m_broadcast_boundaries;

      // configuration interface constants
      const uint32_t m_config_addr_mask;
      const uint32_t m_config_regr_width;
      const uint32_t m_config_func_width;
      const uint32_t m_config_regr_idx_mask;
      const uint32_t m_config_func_idx_mask;

      // Fifo between TGT_CMD fsm and READ fsm
      GenericFifo<addr_t>    m_cmd_read_addr_fifo;
      GenericFifo<size_t>    m_cmd_read_length_fifo;
      GenericFifo<size_t>    m_cmd_read_srcid_fifo;
      GenericFifo<size_t>    m_cmd_read_trdid_fifo;
      GenericFifo<size_t>    m_cmd_read_pktid_fifo;

      // Fifo between TGT_CMD fsm and WRITE fsm
      GenericFifo<addr_t>    m_cmd_write_addr_fifo;
      GenericFifo<bool>      m_cmd_write_eop_fifo;
      GenericFifo<size_t>    m_cmd_write_srcid_fifo;
      GenericFifo<size_t>    m_cmd_write_trdid_fifo;
      GenericFifo<size_t>    m_cmd_write_pktid_fifo;
      GenericFifo<data_t>    m_cmd_write_data_fifo;
      GenericFifo<be_t>      m_cmd_write_be_fifo;

      // Fifo between TGT_CMD fsm and CAS fsm
      GenericFifo<addr_t>    m_cmd_cas_addr_fifo;
      GenericFifo<bool>      m_cmd_cas_eop_fifo;
      GenericFifo<size_t>    m_cmd_cas_srcid_fifo;
      GenericFifo<size_t>    m_cmd_cas_trdid_fifo;
      GenericFifo<size_t>    m_cmd_cas_pktid_fifo;
      GenericFifo<data_t>    m_cmd_cas_wdata_fifo;

      // Fifo between CC_RECEIVE fsm and CLEANUP fsm
      GenericFifo<uint64_t>  m_cc_receive_to_cleanup_fifo;
      
      // Fifo between CC_RECEIVE fsm and MULTI_ACK fsm
      GenericFifo<uint64_t>  m_cc_receive_to_multi_ack_fifo;

      // Buffer between TGT_CMD fsm and TGT_RSP fsm
      // (segmentation violation response request)
      sc_signal<bool>     r_tgt_cmd_to_tgt_rsp_req;

      sc_signal<uint32_t> r_tgt_cmd_to_tgt_rsp_rdata;
      sc_signal<size_t>   r_tgt_cmd_to_tgt_rsp_error;
      sc_signal<size_t>   r_tgt_cmd_to_tgt_rsp_srcid;
      sc_signal<size_t>   r_tgt_cmd_to_tgt_rsp_trdid;
      sc_signal<size_t>   r_tgt_cmd_to_tgt_rsp_pktid;

      sc_signal<addr_t>   r_tgt_cmd_config_addr;
      sc_signal<size_t>   r_tgt_cmd_config_cmd;

      //////////////////////////////////////////////////
      // Registers controlled by the TGT_CMD fsm
      //////////////////////////////////////////////////

      sc_signal<int>         r_tgt_cmd_fsm;

      ///////////////////////////////////////////////////////
      // Registers controlled by the CONFIG fsm
      ///////////////////////////////////////////////////////

      sc_signal<int>      r_config_fsm;               // FSM state
      sc_signal<int>      r_config_cmd;               // config request type  
      sc_signal<addr_t>   r_config_address;           // target buffer physical address
      sc_signal<size_t>   r_config_srcid;             // config request srcid
      sc_signal<size_t>   r_config_trdid;             // config request trdid
      sc_signal<size_t>   r_config_pktid;             // config request pktid
      sc_signal<size_t>   r_config_cmd_lines;         // number of lines to be handled
      sc_signal<size_t>   r_config_rsp_lines;         // number of lines not completed
      sc_signal<size_t>   r_config_dir_way;           // DIR: selected way
      sc_signal<bool>     r_config_dir_lock;          // DIR: locked entry
      sc_signal<size_t>   r_config_dir_count;         // DIR: number of copies
      sc_signal<bool>     r_config_dir_is_cnt;        // DIR: counter mode (broadcast)
      sc_signal<size_t>   r_config_dir_copy_srcid;    // DIR: first copy SRCID
      sc_signal<bool>     r_config_dir_copy_inst;     // DIR: first copy L1 type
      sc_signal<size_t>   r_config_dir_ptr;           // DIR: index of next copy in HEAP
      sc_signal<size_t>   r_config_heap_next;         // current pointer to scan HEAP
      sc_signal<size_t>   r_config_trt_index;         // selected entry in TRT
      sc_signal<size_t>   r_config_ivt_index;         // selected entry in IVT 

      // Buffer between CONFIG fsm and IXR_CMD fsm
      sc_signal<bool>     r_config_to_ixr_cmd_req;    // valid request
      sc_signal<size_t>   r_config_to_ixr_cmd_index;  // TRT index

      // Buffer between CONFIG fsm and TGT_RSP fsm (send a done response to L1 cache)
      sc_signal<bool>     r_config_to_tgt_rsp_req;    // valid request
      sc_signal<bool>     r_config_to_tgt_rsp_error;  // error response
      sc_signal<size_t>   r_config_to_tgt_rsp_srcid;  // Transaction srcid
      sc_signal<size_t>   r_config_to_tgt_rsp_trdid;  // Transaction trdid
      sc_signal<size_t>   r_config_to_tgt_rsp_pktid;  // Transaction pktid

      // Buffer between CONFIG fsm and CC_SEND fsm (multi-inval / broadcast-inval)
      sc_signal<bool>     r_config_to_cc_send_multi_req;    // multi-inval request
      sc_signal<bool>     r_config_to_cc_send_brdcast_req;  // broadcast-inval request
      sc_signal<addr_t>   r_config_to_cc_send_nline;        // line index
      sc_signal<size_t>   r_config_to_cc_send_trdid;        // UPT index
      GenericFifo<bool>   m_config_to_cc_send_inst_fifo;    // fifo for the L1 type
      GenericFifo<size_t> m_config_to_cc_send_srcid_fifo;   // fifo for owners srcid

      ///////////////////////////////////////////////////////
      // Registers controlled by the READ fsm
      ///////////////////////////////////////////////////////

      sc_signal<int>      r_read_fsm;                 // FSM state
      sc_signal<size_t>   r_read_copy;                // Srcid of the first copy
      sc_signal<size_t>   r_read_copy_cache;          // Srcid of the first copy
      sc_signal<bool>     r_read_copy_inst;           // Type of the first copy
      sc_signal<tag_t>    r_read_tag;                 // cache line tag (in directory)
      sc_signal<bool>     r_read_is_cnt;              // is_cnt bit (in directory)
      sc_signal<bool>     r_read_lock;                // lock bit (in directory)
      sc_signal<bool>     r_read_dirty;               // dirty bit (in directory)
      sc_signal<size_t>   r_read_count;               // number of copies
      sc_signal<size_t>   r_read_ptr;                 // pointer to the heap
      sc_signal<data_t> * r_read_data;                // data (one cache line)
      sc_signal<size_t>   r_read_way;                 // associative way (in cache)
      sc_signal<size_t>   r_read_trt_index;           // Transaction Table index
      sc_signal<size_t>   r_read_next_ptr;            // Next entry to point to
      sc_signal<bool>     r_read_last_free;           // Last free entry
      sc_signal<addr_t>   r_read_ll_key;              // LL key from llsc_global_table

      // Buffer between READ fsm and IXR_CMD fsm 
      sc_signal<bool>     r_read_to_ixr_cmd_req;      // valid request
      sc_signal<size_t>   r_read_to_ixr_cmd_index;    // TRT index

      // Buffer between READ fsm and TGT_RSP fsm (send a hit read response to L1 cache)
      sc_signal<bool>     r_read_to_tgt_rsp_req;      // valid request
      sc_signal<size_t>   r_read_to_tgt_rsp_srcid;    // Transaction srcid
      sc_signal<size_t>   r_read_to_tgt_rsp_trdid;    // Transaction trdid
      sc_signal<size_t>   r_read_to_tgt_rsp_pktid;    // Transaction pktid
      sc_signal<data_t> * r_read_to_tgt_rsp_data;     // data (one cache line)
      sc_signal<size_t>   r_read_to_tgt_rsp_word;     // first word of the response
      sc_signal<size_t>   r_read_to_tgt_rsp_length;   // length of the response
      sc_signal<addr_t>   r_read_to_tgt_rsp_ll_key;   // LL key from llsc_global_table

      ///////////////////////////////////////////////////////////////
      // Registers controlled by the WRITE fsm
      ///////////////////////////////////////////////////////////////

      sc_signal<int>      r_write_fsm;                // FSM state
      sc_signal<addr_t>   r_write_address;            // first word address
      sc_signal<size_t>   r_write_word_index;         // first word index in line
      sc_signal<size_t>   r_write_word_count;         // number of words in line
      sc_signal<size_t>   r_write_srcid;              // transaction srcid
      sc_signal<size_t>   r_write_trdid;              // transaction trdid
      sc_signal<size_t>   r_write_pktid;              // transaction pktid
      sc_signal<data_t> * r_write_data;               // data (one cache line)
      sc_signal<be_t>   * r_write_be;                 // one byte enable per word
      sc_signal<bool>     r_write_byte;               // (BE != 0X0) and (BE != 0xF)
      sc_signal<bool>     r_write_is_cnt;             // is_cnt bit (in directory)
      sc_signal<bool>     r_write_lock;               // lock bit (in directory)
      sc_signal<tag_t>    r_write_tag;                // cache line tag (in directory)
      sc_signal<size_t>   r_write_copy;               // first owner of the line
      sc_signal<size_t>   r_write_copy_cache;         // first owner of the line
      sc_signal<bool>     r_write_copy_inst;          // is this owner a ICache ?
      sc_signal<size_t>   r_write_count;              // number of copies
      sc_signal<size_t>   r_write_ptr;                // pointer to the heap
      sc_signal<size_t>   r_write_next_ptr;           // next pointer to the heap
      sc_signal<bool>     r_write_to_dec;             // need to decrement update counter
      sc_signal<size_t>   r_write_way;                // way of the line
      sc_signal<size_t>   r_write_trt_index;          // index in Transaction Table
      sc_signal<size_t>   r_write_upt_index;          // index in Update Table
      sc_signal<bool>     r_write_sc_fail;            // sc command failed
      sc_signal<data_t>   r_write_sc_key;             // sc command key
      sc_signal<bool>     r_write_bc_data_we;         // Write enable for data buffer

      // Buffer between WRITE fsm and TGT_RSP fsm (acknowledge a write command from L1)
      sc_signal<bool>     r_write_to_tgt_rsp_req;     // valid request
      sc_signal<size_t>   r_write_to_tgt_rsp_srcid;   // transaction srcid
      sc_signal<size_t>   r_write_to_tgt_rsp_trdid;   // transaction trdid
      sc_signal<size_t>   r_write_to_tgt_rsp_pktid;   // transaction pktid
      sc_signal<bool>     r_write_to_tgt_rsp_sc_fail; // sc command failed

      // Buffer between WRITE fsm and IXR_CMD fsm 
      sc_signal<bool>     r_write_to_ixr_cmd_req;     // valid request
      sc_signal<size_t>   r_write_to_ixr_cmd_index;   // TRT index 

      // Buffer between WRITE fsm and CC_SEND fsm (Update/Invalidate L1 caches)
      sc_signal<bool>     r_write_to_cc_send_multi_req;     // valid multicast request
      sc_signal<bool>     r_write_to_cc_send_brdcast_req;   // valid brdcast request
      sc_signal<addr_t>   r_write_to_cc_send_nline;         // cache line index
      sc_signal<size_t>   r_write_to_cc_send_trdid;         // index in Update Table
      sc_signal<data_t> * r_write_to_cc_send_data;          // data (one cache line)
      sc_signal<be_t>   * r_write_to_cc_send_be;            // word enable
      sc_signal<size_t>   r_write_to_cc_send_count;         // number of words in line
      sc_signal<size_t>   r_write_to_cc_send_index;         // index of first word in line
      GenericFifo<bool>   m_write_to_cc_send_inst_fifo;     // fifo for the L1 type
      GenericFifo<size_t> m_write_to_cc_send_srcid_fifo;    // fifo for srcids

      // Buffer between WRITE fsm and MULTI_ACK fsm (Decrement UPT entry)
      sc_signal<bool>     r_write_to_multi_ack_req;       // valid request
      sc_signal<size_t>   r_write_to_multi_ack_upt_index; // index in update table

      /////////////////////////////////////////////////////////
      // Registers controlled by MULTI_ACK fsm
      //////////////////////////////////////////////////////////

      sc_signal<int>      r_multi_ack_fsm;       // FSM state
      sc_signal<size_t>   r_multi_ack_upt_index; // index in the Update Table
      sc_signal<size_t>   r_multi_ack_srcid;     // pending write srcid
      sc_signal<size_t>   r_multi_ack_trdid;     // pending write trdid
      sc_signal<size_t>   r_multi_ack_pktid;     // pending write pktid
      sc_signal<addr_t>   r_multi_ack_nline;     // pending write nline

      // Buffer between MULTI_ACK fsm and TGT_RSP fsm (complete write/update transaction)
      sc_signal<bool>     r_multi_ack_to_tgt_rsp_req;   // valid request
      sc_signal<size_t>   r_multi_ack_to_tgt_rsp_srcid; // Transaction srcid
      sc_signal<size_t>   r_multi_ack_to_tgt_rsp_trdid; // Transaction trdid
      sc_signal<size_t>   r_multi_ack_to_tgt_rsp_pktid; // Transaction pktid

      ///////////////////////////////////////////////////////
      // Registers controlled by CLEANUP fsm
      ///////////////////////////////////////////////////////

      sc_signal<int>      r_cleanup_fsm;           // FSM state
      sc_signal<size_t>   r_cleanup_srcid;         // transaction srcid
      sc_signal<bool>     r_cleanup_inst;          // Instruction or Data ?
      sc_signal<size_t>   r_cleanup_way_index;     // L1 Cache Way index
      sc_signal<addr_t>   r_cleanup_nline;         // cache line index


      sc_signal<copy_t>   r_cleanup_copy;          // first copy
      sc_signal<copy_t>   r_cleanup_copy_cache;    // first copy
      sc_signal<size_t>   r_cleanup_copy_inst;     // type of the first copy
      sc_signal<copy_t>   r_cleanup_count;         // number of copies
      sc_signal<size_t>   r_cleanup_ptr;           // pointer to the heap
      sc_signal<size_t>   r_cleanup_prev_ptr;      // previous pointer to the heap
      sc_signal<size_t>   r_cleanup_prev_srcid;    // srcid of previous heap entry
      sc_signal<size_t>   r_cleanup_prev_cache_id; // srcid of previous heap entry
      sc_signal<bool>     r_cleanup_prev_inst;     // inst bit of previous heap entry
      sc_signal<size_t>   r_cleanup_next_ptr;      // next pointer to the heap
      sc_signal<tag_t>    r_cleanup_tag;           // cache line tag (in directory)
      sc_signal<bool>     r_cleanup_is_cnt;        // inst bit (in directory)
      sc_signal<bool>     r_cleanup_lock;          // lock bit (in directory)
      sc_signal<bool>     r_cleanup_dirty;         // dirty bit (in directory)
      sc_signal<size_t>   r_cleanup_way;           // associative way (in cache)

      sc_signal<size_t>   r_cleanup_write_srcid;   // srcid of write rsp
      sc_signal<size_t>   r_cleanup_write_trdid;   // trdid of write rsp
      sc_signal<size_t>   r_cleanup_write_pktid;   // pktid of write rsp

      sc_signal<bool>     r_cleanup_need_rsp;      // write response required
      sc_signal<bool>     r_cleanup_need_ack;      // config acknowledge required

      sc_signal<size_t>   r_cleanup_index;         // index of the INVAL line (in the UPT)

      // Buffer between CLEANUP fsm and TGT_RSP fsm (acknowledge a write command from L1)
      sc_signal<bool>     r_cleanup_to_tgt_rsp_req;   // valid request
      sc_signal<size_t>   r_cleanup_to_tgt_rsp_srcid; // transaction srcid
      sc_signal<size_t>   r_cleanup_to_tgt_rsp_trdid; // transaction trdid
      sc_signal<size_t>   r_cleanup_to_tgt_rsp_pktid; // transaction pktid

      ///////////////////////////////////////////////////////
      // Registers controlled by CAS fsm
      ///////////////////////////////////////////////////////

      sc_signal<int>      r_cas_fsm;              // FSM state
      sc_signal<data_t>   r_cas_wdata;            // write data word
      sc_signal<data_t> * r_cas_rdata;            // read data word
      sc_signal<uint32_t> r_cas_lfsr;             // lfsr for random introducing
      sc_signal<size_t>   r_cas_cpt;              // size of command
      sc_signal<copy_t>   r_cas_copy;             // Srcid of the first copy
      sc_signal<copy_t>   r_cas_copy_cache;       // Srcid of the first copy
      sc_signal<bool>     r_cas_copy_inst;        // Type of the first copy
      sc_signal<size_t>   r_cas_count;            // number of copies
      sc_signal<size_t>   r_cas_ptr;              // pointer to the heap
      sc_signal<size_t>   r_cas_next_ptr;         // next pointer to the heap
      sc_signal<bool>     r_cas_is_cnt;           // is_cnt bit (in directory)
      sc_signal<bool>     r_cas_dirty;            // dirty bit (in directory)
      sc_signal<size_t>   r_cas_way;              // way in directory
      sc_signal<size_t>   r_cas_set;              // set in directory
      sc_signal<data_t>   r_cas_tag;              // cache line tag (in directory)
      sc_signal<size_t>   r_cas_trt_index;        // Transaction Table index
      sc_signal<size_t>   r_cas_upt_index;        // Update Table index
      sc_signal<data_t> * r_cas_data;             // cache line data

      // Buffer between CAS fsm and IXR_CMD fsm 
      sc_signal<bool>     r_cas_to_ixr_cmd_req;   // valid request
      sc_signal<size_t>   r_cas_to_ixr_cmd_index; // TRT index 

      // Buffer between CAS fsm and TGT_RSP fsm
      sc_signal<bool>     r_cas_to_tgt_rsp_req;   // valid request
      sc_signal<data_t>   r_cas_to_tgt_rsp_data;  // read data word
      sc_signal<size_t>   r_cas_to_tgt_rsp_srcid; // Transaction srcid
      sc_signal<size_t>   r_cas_to_tgt_rsp_trdid; // Transaction trdid
      sc_signal<size_t>   r_cas_to_tgt_rsp_pktid; // Transaction pktid

      // Buffer between CAS fsm and CC_SEND fsm (Update/Invalidate L1 caches)
      sc_signal<bool>     r_cas_to_cc_send_multi_req;     // valid request
      sc_signal<bool>     r_cas_to_cc_send_brdcast_req;   // brdcast request
      sc_signal<addr_t>   r_cas_to_cc_send_nline;         // cache line index
      sc_signal<size_t>   r_cas_to_cc_send_trdid;         // index in Update Table
      sc_signal<data_t>   r_cas_to_cc_send_wdata;         // data (one word)
      sc_signal<bool>     r_cas_to_cc_send_is_long;       // it is a 64 bits CAS
      sc_signal<data_t>   r_cas_to_cc_send_wdata_high;    // data high (one word)
      sc_signal<size_t>   r_cas_to_cc_send_index;         // index of the word in line
      GenericFifo<bool>   m_cas_to_cc_send_inst_fifo;     // fifo for the L1 type
      GenericFifo<size_t> m_cas_to_cc_send_srcid_fifo;    // fifo for srcids

      ////////////////////////////////////////////////////
      // Registers controlled by the IXR_RSP fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_ixr_rsp_fsm;                // FSM state
      sc_signal<size_t>   r_ixr_rsp_trt_index;          // TRT entry index
      sc_signal<size_t>   r_ixr_rsp_cpt;                // word counter

      // Buffer between IXR_RSP fsm and CONFIG fsm  (response from the XRAM)
      sc_signal<bool>     r_ixr_rsp_to_config_ack;      // one single bit   

      // Buffer between IXR_RSP fsm and XRAM_RSP fsm  (response from the XRAM)
      sc_signal<bool>   * r_ixr_rsp_to_xram_rsp_rok;    // one bit per TRT entry

      ////////////////////////////////////////////////////
      // Registers controlled by the XRAM_RSP fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_xram_rsp_fsm;               // FSM state
      sc_signal<size_t>   r_xram_rsp_trt_index;         // TRT entry index
      TransactionTabEntry r_xram_rsp_trt_buf;           // TRT entry local buffer
      sc_signal<bool>     r_xram_rsp_victim_inval;      // victim line invalidate
      sc_signal<bool>     r_xram_rsp_victim_is_cnt;     // victim line inst bit
      sc_signal<bool>     r_xram_rsp_victim_dirty;      // victim line dirty bit
      sc_signal<size_t>   r_xram_rsp_victim_way;        // victim line way
      sc_signal<size_t>   r_xram_rsp_victim_set;        // victim line set
      sc_signal<addr_t>   r_xram_rsp_victim_nline;      // victim line index
      sc_signal<copy_t>   r_xram_rsp_victim_copy;       // victim line first copy
      sc_signal<copy_t>   r_xram_rsp_victim_copy_cache; // victim line first copy
      sc_signal<bool>     r_xram_rsp_victim_copy_inst;  // victim line type of first copy
      sc_signal<size_t>   r_xram_rsp_victim_count;      // victim line number of copies
      sc_signal<size_t>   r_xram_rsp_victim_ptr;        // victim line pointer to the heap
      sc_signal<data_t> * r_xram_rsp_victim_data;       // victim line data
      sc_signal<size_t>   r_xram_rsp_ivt_index;         // IVT entry index
      sc_signal<size_t>   r_xram_rsp_next_ptr;          // Next pointer to the heap
      sc_signal<bool>     r_xram_rsp_rerror_irq;        // WRITE MISS rerror irq
      sc_signal<bool>     r_xram_rsp_rerror_irq_enable; // WRITE MISS rerror irq enable
      sc_signal<addr_t>   r_xram_rsp_rerror_address;    // WRITE MISS rerror address
      sc_signal<size_t>   r_xram_rsp_rerror_rsrcid;     // WRITE MISS rerror srcid

      // Buffer between XRAM_RSP fsm and TGT_RSP fsm  (response to L1 cache)
      sc_signal<bool>     r_xram_rsp_to_tgt_rsp_req;    // Valid request
      sc_signal<size_t>   r_xram_rsp_to_tgt_rsp_srcid;  // Transaction srcid
      sc_signal<size_t>   r_xram_rsp_to_tgt_rsp_trdid;  // Transaction trdid
      sc_signal<size_t>   r_xram_rsp_to_tgt_rsp_pktid;  // Transaction pktid
      sc_signal<data_t> * r_xram_rsp_to_tgt_rsp_data;   // data (one cache line)
      sc_signal<size_t>   r_xram_rsp_to_tgt_rsp_word;   // first word index
      sc_signal<size_t>   r_xram_rsp_to_tgt_rsp_length; // length of the response
      sc_signal<bool>     r_xram_rsp_to_tgt_rsp_rerror; // send error to requester
      sc_signal<addr_t>   r_xram_rsp_to_tgt_rsp_ll_key; // LL key from llsc_global_table

      // Buffer between XRAM_RSP fsm and CC_SEND fsm (Inval L1 Caches)
      sc_signal<bool>     r_xram_rsp_to_cc_send_multi_req;     // Valid request
      sc_signal<bool>     r_xram_rsp_to_cc_send_brdcast_req;   // Broadcast request
      sc_signal<addr_t>   r_xram_rsp_to_cc_send_nline;         // cache line index;
      sc_signal<size_t>   r_xram_rsp_to_cc_send_trdid;         // index of UPT entry
      GenericFifo<bool>   m_xram_rsp_to_cc_send_inst_fifo;     // fifo for the L1 type
      GenericFifo<size_t> m_xram_rsp_to_cc_send_srcid_fifo;    // fifo for srcids

      // Buffer between XRAM_RSP fsm and IXR_CMD fsm 
      sc_signal<bool>     r_xram_rsp_to_ixr_cmd_req;   // Valid request
      sc_signal<size_t>   r_xram_rsp_to_ixr_cmd_index; // TRT index 

      ////////////////////////////////////////////////////
      // Registers controlled by the IXR_CMD fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_ixr_cmd_fsm;
      sc_signal<size_t>   r_ixr_cmd_word;              // word index for a put
      sc_signal<size_t>   r_ixr_cmd_trdid;             // TRT index value     
      sc_signal<addr_t>   r_ixr_cmd_address;           // address to XRAM
      sc_signal<data_t> * r_ixr_cmd_wdata;             // cache line buffer
      sc_signal<bool>     r_ixr_cmd_get;               // transaction type (PUT/GET)

      ////////////////////////////////////////////////////
      // Registers controlled by TGT_RSP fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_tgt_rsp_fsm;
      sc_signal<size_t>   r_tgt_rsp_cpt;
      sc_signal<bool>     r_tgt_rsp_key_sent;

      ////////////////////////////////////////////////////
      // Registers controlled by CC_SEND fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_cc_send_fsm;
      sc_signal<size_t>   r_cc_send_cpt;
      sc_signal<bool>     r_cc_send_inst;

      ////////////////////////////////////////////////////
      // Registers controlled by CC_RECEIVE fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_cc_receive_fsm;

      ////////////////////////////////////////////////////
      // Registers controlled by ALLOC_DIR fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_alloc_dir_fsm;
      sc_signal<unsigned> r_alloc_dir_reset_cpt;

      ////////////////////////////////////////////////////
      // Registers controlled by ALLOC_TRT fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_alloc_trt_fsm;

      ////////////////////////////////////////////////////
      // Registers controlled by ALLOC_UPT fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_alloc_upt_fsm;

      ////////////////////////////////////////////////////
      // Registers controlled by ALLOC_IVT fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_alloc_ivt_fsm;

      ////////////////////////////////////////////////////
      // Registers controlled by ALLOC_HEAP fsm
      ////////////////////////////////////////////////////

      sc_signal<int>      r_alloc_heap_fsm;
      sc_signal<unsigned> r_alloc_heap_reset_cpt;
    }; // end class VciMemCache

}}

#endif

// Local Variables:
// tab-width: 2
// c-basic-offset: 2
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=2:tabstop=2:softtabstop=2

