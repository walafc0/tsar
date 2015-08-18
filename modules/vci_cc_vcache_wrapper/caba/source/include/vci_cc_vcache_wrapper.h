/* -*- c++ -*-
 *
 * File : vci_cc_vcache_wrapper.h
 * Copyright (c) UPMC, Lip6, SoC
 * Authors : Alain GREINER, Yang GAO
 * Date : 27/11/2011
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
 * Maintainers: cesar.fuguet-tortolero@lip6.fr
 *              alexandre.joannou@lip6.fr
 */

#ifndef SOCLIB_CABA_VCI_CC_VCACHE_WRAPPER_H
#define SOCLIB_CABA_VCI_CC_VCACHE_WRAPPER_H

#include <inttypes.h>
#include <systemc>
#include "caba_base_module.h"
#include "multi_write_buffer.h"
#include "generic_fifo.h"
#include "generic_tlb.h"
#include "generic_cache.h"
#include "vci_initiator.h"
#include "dspin_interface.h"
#include "dspin_dhccp_param.h"
#include "mapping_table.h"
#include "static_assert.h"
#include "iss2.h"

#define LLSC_TIMEOUT    10000

namespace soclib {
namespace caba {

using namespace sc_core;

////////////////////////////////////////////
template<typename vci_param, 
         size_t   dspin_in_width,
         size_t   dspin_out_width,
         typename iss_t>
class VciCcVCacheWrapper
////////////////////////////////////////////
    : public soclib::caba::BaseModule
{

    typedef typename vci_param::fast_addr_t  paddr_t;

    enum icache_fsm_state_e 
    {
        ICACHE_IDLE,
        // handling XTN processor requests
        ICACHE_XTN_TLB_FLUSH,
        ICACHE_XTN_CACHE_FLUSH,
        ICACHE_XTN_CACHE_FLUSH_GO,
        ICACHE_XTN_TLB_INVAL,
        ICACHE_XTN_CACHE_INVAL_VA,
        ICACHE_XTN_CACHE_INVAL_PA,
        ICACHE_XTN_CACHE_INVAL_GO,
        // handling tlb miss
        ICACHE_TLB_WAIT,
        // handling cache miss
        ICACHE_MISS_SELECT,
        ICACHE_MISS_CLEAN,
        ICACHE_MISS_WAIT,
        ICACHE_MISS_DATA_UPDT,
        ICACHE_MISS_DIR_UPDT,
        // handling unc read
        ICACHE_UNC_WAIT,
        // handling coherence requests
        ICACHE_CC_CHECK,
        ICACHE_CC_UPDT,
        ICACHE_CC_INVAL,
    };

    enum dcache_fsm_state_e 
    {
        DCACHE_IDLE,
        // handling itlb & dtlb miss
        DCACHE_TLB_MISS,
        DCACHE_TLB_PTE1_GET,
        DCACHE_TLB_PTE1_SELECT,
        DCACHE_TLB_PTE1_UPDT,
        DCACHE_TLB_PTE2_GET,
        DCACHE_TLB_PTE2_SELECT,
        DCACHE_TLB_PTE2_UPDT,
        DCACHE_TLB_LR_UPDT,
        DCACHE_TLB_LR_WAIT,
        DCACHE_TLB_RETURN,
	    // handling processor XTN requests
        DCACHE_XTN_SWITCH,
        DCACHE_XTN_SYNC,
        DCACHE_XTN_IC_INVAL_VA,
        DCACHE_XTN_IC_FLUSH,
        DCACHE_XTN_IC_INVAL_PA,
        DCACHE_XTN_IC_PADDR_EXT,
        DCACHE_XTN_IT_INVAL,
        DCACHE_XTN_DC_FLUSH,
        DCACHE_XTN_DC_FLUSH_GO,
        DCACHE_XTN_DC_INVAL_VA,
        DCACHE_XTN_DC_INVAL_PA,
        DCACHE_XTN_DC_INVAL_END,
        DCACHE_XTN_DC_INVAL_GO,
        DCACHE_XTN_DT_INVAL,
        //handling dirty bit update
        DCACHE_DIRTY_GET_PTE,
        DCACHE_DIRTY_WAIT,
	    // handling processor miss requests
        DCACHE_MISS_SELECT,
        DCACHE_MISS_CLEAN,
        DCACHE_MISS_WAIT,
        DCACHE_MISS_DATA_UPDT,
        DCACHE_MISS_DIR_UPDT,
        // handling processor unc, ll and sc requests
        DCACHE_UNC_WAIT,
        DCACHE_LL_WAIT,
        DCACHE_SC_WAIT,
        // handling coherence requests
        DCACHE_CC_CHECK,
        DCACHE_CC_UPDT,
        DCACHE_CC_INVAL,
        // handling TLB inval (after a coherence or XTN request)
        DCACHE_INVAL_TLB_SCAN,
    };

    enum cmd_fsm_state_e 
    {
        CMD_IDLE,
        CMD_INS_MISS,
        CMD_INS_UNC,
        CMD_DATA_MISS,
        CMD_DATA_UNC_READ,
        CMD_DATA_UNC_WRITE,
        CMD_DATA_WRITE,
        CMD_DATA_LL,
        CMD_DATA_SC,
        CMD_DATA_CAS,
    };

    enum rsp_fsm_state_e 
    {
        RSP_IDLE,
        RSP_INS_MISS,
        RSP_INS_UNC,
        RSP_DATA_MISS,
        RSP_DATA_UNC,
        RSP_DATA_LL,
        RSP_DATA_WRITE,
    };

    enum cc_receive_fsm_state_e 
    {
        CC_RECEIVE_IDLE,
        CC_RECEIVE_BRDCAST_HEADER,
        CC_RECEIVE_BRDCAST_NLINE,
        CC_RECEIVE_INS_INVAL_HEADER,
        CC_RECEIVE_INS_INVAL_NLINE,
        CC_RECEIVE_INS_UPDT_HEADER,
        CC_RECEIVE_INS_UPDT_NLINE,
        CC_RECEIVE_DATA_INVAL_HEADER,
        CC_RECEIVE_DATA_INVAL_NLINE,
        CC_RECEIVE_DATA_UPDT_HEADER,
        CC_RECEIVE_DATA_UPDT_NLINE,
        CC_RECEIVE_INS_UPDT_DATA,
        CC_RECEIVE_DATA_UPDT_DATA,
    };

    enum cc_send_fsm_state_e 
    {
        CC_SEND_IDLE,
        CC_SEND_CLEANUP_1,
        CC_SEND_CLEANUP_2,
        CC_SEND_MULTI_ACK,
    };

    /* transaction type, pktid field */
    enum transaction_type_e
    {
        // b3 unused
        // b2 READ / NOT READ
        // if READ
        //  b1 DATA / INS
        //  b0 UNC / MISS
        // else
        //  b1 accès table llsc type SW / other
        //  b2 WRITE/CAS/LL/SC
        TYPE_DATA_UNC               = 0x0,
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

    // cc_send_type
    typedef enum 
    {
        CC_TYPE_CLEANUP,
        CC_TYPE_MULTI_ACK,
    } cc_send_t;

    // cc_receive_type
    typedef enum 
    {
        CC_TYPE_CLACK,
        CC_TYPE_BRDCAST,
        CC_TYPE_INVAL,
        CC_TYPE_UPDT,
    } cc_receive_t;

    // TLB Mode : ITLB / DTLB / ICACHE / DCACHE
    enum 
    {
        INS_TLB_MASK    = 0x8,
        DATA_TLB_MASK   = 0x4,
        INS_CACHE_MASK  = 0x2,
        DATA_CACHE_MASK = 0x1,
    };

    // Error Type
    enum mmu_error_type_e
    {
        MMU_NONE                      = 0x0000, // None
        MMU_WRITE_PT1_UNMAPPED 	      = 0x0001, // Write & Page fault on PT1
        MMU_WRITE_PT2_UNMAPPED 	      = 0x0002, // Write & Page fault on PT2
        MMU_WRITE_PRIVILEGE_VIOLATION = 0x0004, // Write & Protected access in user mode
        MMU_WRITE_ACCES_VIOLATION     = 0x0008, // Write to non writable page
        MMU_WRITE_UNDEFINED_XTN       = 0x0020, // Write & undefined external access
        MMU_WRITE_PT1_ILLEGAL_ACCESS  = 0x0040, // Write & Bus Error accessing PT1
        MMU_WRITE_PT2_ILLEGAL_ACCESS  = 0x0080, // Write & Bus Error accessing PT2
        MMU_WRITE_DATA_ILLEGAL_ACCESS = 0x0100, // Write & Bus Error in cache access
        MMU_READ_PT1_UNMAPPED 	      = 0x1001, // Read & Page fault on PT1
        MMU_READ_PT2_UNMAPPED 	      = 0x1002, // Read & Page fault on PT2
        MMU_READ_PRIVILEGE_VIOLATION  = 0x1004, // Read & Protected access in user mode
        MMU_READ_EXEC_VIOLATION       = 0x1010, // Read & Exec access to a non exec page
        MMU_READ_UNDEFINED_XTN 	      = 0x1020, // Read & Undefined external access
        MMU_READ_PT1_ILLEGAL_ACCESS   = 0x1040, // Read & Bus Error accessing PT1
        MMU_READ_PT2_ILLEGAL_ACCESS   = 0x1080, // Read & Bus Error accessing PT2
        MMU_READ_DATA_ILLEGAL_ACCESS  = 0x1100, // Read & Bus Error in cache access
    };

    // miss types for data cache
    enum dcache_miss_type_e
    {
        PTE1_MISS,
        PTE2_MISS,
        PROC_MISS,
    };

//    enum transaction_type_d_e
//    {
//        // b0 : 1 if cached
//        // b1 : 1 if instruction
//        TYPE_DATA_UNC     = 0x0,
//        TYPE_DATA_MISS    = 0x1,
//        TYPE_INS_UNC      = 0x2,
//        TYPE_INS_MISS     = 0x3,
//    };

public:
    sc_in<bool>                                p_clk;
    sc_in<bool>                                p_resetn;
    sc_in<bool>                                p_irq[iss_t::n_irq];
    soclib::caba::VciInitiator<vci_param>      p_vci;
    soclib::caba::DspinInput<dspin_in_width>   p_dspin_m2p;
    soclib::caba::DspinOutput<dspin_out_width> p_dspin_p2m;
    soclib::caba::DspinInput<dspin_in_width>   p_dspin_clack;

private:

    // STRUCTURAL PARAMETERS
    soclib::common::AddressDecodingTable<uint64_t, bool> m_cacheability_table;

    const size_t                        m_srcid;
    const size_t                        m_cc_global_id;
    const size_t                        m_nline_width;
    const size_t  						m_itlb_ways;
    const size_t  						m_itlb_sets;
    const size_t  						m_dtlb_ways;
    const size_t  						m_dtlb_sets;
    const size_t  						m_icache_ways;
    const size_t  						m_icache_sets;
    const paddr_t 						m_icache_yzmask;
    const size_t  						m_icache_words;
    const size_t  						m_dcache_ways;
    const size_t  						m_dcache_sets;
    const paddr_t 						m_dcache_yzmask;
    const size_t  						m_dcache_words;
    const size_t                        m_x_width;
    const size_t                        m_y_width;
    const size_t                        m_proc_id;
    const uint32_t						m_max_frozen_cycles;
    const size_t  						m_paddr_nbits;
    uint32_t                            m_debug_start_cycle;
    bool                                m_debug_ok;

    uint32_t                            m_dcache_paddr_ext_reset;
    uint32_t                            m_icache_paddr_ext_reset;

    ////////////////////////////////////////
    // Communication with processor ISS
    ////////////////////////////////////////
    typename iss_t::InstructionRequest  m_ireq;
    typename iss_t::InstructionResponse m_irsp;
    typename iss_t::DataRequest         m_dreq;
    typename iss_t::DataResponse        m_drsp;

    /////////////////////////////////////////////
    // debug variables 
    /////////////////////////////////////////////
    bool                                m_debug_previous_i_hit;
    bool                                m_debug_previous_d_hit;
    bool                                m_debug_icache_fsm;
    bool                                m_debug_dcache_fsm;
    bool                                m_debug_cmd_fsm;
    uint32_t                            m_previous_status;


    ///////////////////////////////
    // Software visible REGISTERS
    ///////////////////////////////
    sc_signal<uint32_t>     r_mmu_ptpr;             	// page table pointer register
    sc_signal<uint32_t>     r_mmu_mode;             	// mmu mode register
    sc_signal<uint32_t>     r_mmu_word_lo;          	// mmu misc data low
    sc_signal<uint32_t>     r_mmu_word_hi;          	// mmu misc data hight
    sc_signal<uint32_t>     r_mmu_ibvar;      	    	// mmu bad instruction address
    sc_signal<uint32_t>     r_mmu_dbvar;              	// mmu bad data address
    sc_signal<uint32_t>     r_mmu_ietr;                 // mmu instruction error type
    sc_signal<uint32_t>     r_mmu_detr;                 // mmu data error type
    uint32_t	            r_mmu_params;		        // read-only
    uint32_t	            r_mmu_release;		        // read_only


    //////////////////////////////
    // ICACHE FSM REGISTERS
    //////////////////////////////
    sc_signal<int>          r_icache_fsm;               // state register
    sc_signal<int>          r_icache_fsm_save;          // return state for coherence op
    sc_signal<paddr_t>      r_icache_vci_paddr;      	// physical address
    sc_signal<uint32_t>     r_icache_vaddr_save;        // virtual address from processor

    // icache miss handling
    sc_signal<size_t>       r_icache_miss_way;		    // selected way for cache update
    sc_signal<size_t>       r_icache_miss_set;		    // selected set for cache update
    sc_signal<size_t>       r_icache_miss_word;		    // word index ( cache update)
    sc_signal<bool>         r_icache_miss_inval;        // coherence request matching a miss
    sc_signal<bool>         r_icache_miss_clack;        // waiting for a cleanup acknowledge

    // coherence request handling
    sc_signal<size_t>       r_icache_cc_way;		    // selected way for cc update/inval
    sc_signal<size_t>       r_icache_cc_set;		    // selected set for cc update/inval
    sc_signal<size_t>       r_icache_cc_word;		    // word counter for cc update
    sc_signal<bool>         r_icache_cc_need_write;     // activate the cache for writing

    // coherence clack handling
    sc_signal<bool>         r_icache_clack_req;         // clack request
    sc_signal<size_t>       r_icache_clack_way;		    // clack way
    sc_signal<size_t>       r_icache_clack_set;		    // clack set

    // icache flush handling
    sc_signal<size_t>       r_icache_flush_count;	    // slot counter used for cache flush

    // communication between ICACHE FSM and VCI_CMD FSM
    sc_signal<bool>         r_icache_miss_req;           // cached read miss
    sc_signal<bool>         r_icache_unc_req;            // uncached read miss

    // communication between ICACHE FSM and DCACHE FSM
    sc_signal<bool>	        r_icache_tlb_miss_req;       // (set icache/reset dcache)
    sc_signal<bool>         r_icache_tlb_rsp_error;      // tlb miss response error

    // Filp-Flop in ICACHE FSM for saving the cleanup victim request
    sc_signal<bool>         r_icache_cleanup_victim_req; 
    sc_signal<paddr_t>      r_icache_cleanup_victim_nline;

    // communication between ICACHE FSM and CC_SEND FSM
    sc_signal<bool>         r_icache_cc_send_req;           // ICACHE cc_send request
    sc_signal<int>          r_icache_cc_send_type;          // ICACHE cc_send request type
    sc_signal<paddr_t>      r_icache_cc_send_nline;         // ICACHE cc_send nline
    sc_signal<size_t>       r_icache_cc_send_way;           // ICACHE cc_send way
    sc_signal<size_t>       r_icache_cc_send_updt_tab_idx;  // ICACHE cc_send update table index

    // Physical address extension for data access
    sc_signal<uint32_t>     r_icache_paddr_ext;             // CP2 register (if vci_address > 32)

    ///////////////////////////////
    // DCACHE FSM REGISTERS
    ///////////////////////////////
    sc_signal<int>          r_dcache_fsm;               // state register
    sc_signal<int>          r_dcache_fsm_cc_save;       // return state for coherence op
    sc_signal<int>          r_dcache_fsm_scan_save;     // return state for tlb scan op
    // registers written in P0 stage (used in P1 stage)
    sc_signal<bool>         r_dcache_wbuf_req;          // WBUF must be written in P1 stage
    sc_signal<bool>         r_dcache_updt_req;          // DCACHE must be updated in P1 stage
    sc_signal<uint32_t>     r_dcache_save_vaddr;        // virtual address (from proc)
    sc_signal<uint32_t>     r_dcache_save_wdata;        // write data (from proc)
    sc_signal<uint32_t>     r_dcache_save_be;           // byte enable (from proc)
    sc_signal<paddr_t>      r_dcache_save_paddr;        // physical address
    sc_signal<size_t>       r_dcache_save_cache_way;	// selected way (from dcache)
    sc_signal<size_t>       r_dcache_save_cache_set;	// selected set (from dcache)
    sc_signal<size_t>       r_dcache_save_cache_word;	// selected word (from dcache)
    // registers used by the Dirty bit sub-fsm
    sc_signal<paddr_t>      r_dcache_dirty_paddr;       // PTE physical address
    sc_signal<size_t>       r_dcache_dirty_way;	        // way to invalidate in dcache
    sc_signal<size_t>       r_dcache_dirty_set;	        // set to invalidate in dcache

    // communication between DCACHE FSM and VCI_CMD FSM
    sc_signal<paddr_t>      r_dcache_vci_paddr;		    // physical address for VCI command
    sc_signal<uint32_t>     r_dcache_vci_wdata;		    // write unc data for VCI command
    sc_signal<bool>         r_dcache_vci_miss_req;      // read miss request
    sc_signal<bool>         r_dcache_vci_unc_req;       // uncacheable request (read/write)
    sc_signal<uint32_t>     r_dcache_vci_unc_be;        // uncacheable byte enable
    sc_signal<uint32_t>     r_dcache_vci_unc_write;     // uncacheable data write request
    sc_signal<bool>         r_dcache_vci_cas_req;       // atomic write request CAS
    sc_signal<uint32_t>     r_dcache_vci_cas_old;       // previous data value for a CAS
    sc_signal<uint32_t>     r_dcache_vci_cas_new;       // new data value for a CAS
    sc_signal<bool>         r_dcache_vci_ll_req;        // atomic read request LL
    sc_signal<bool>         r_dcache_vci_sc_req;        // atomic write request SC
    sc_signal<uint32_t>     r_dcache_vci_sc_data;       // SC data (command)

    // register used for XTN inval
    sc_signal<size_t>       r_dcache_xtn_way;		    // selected way (from dcache)
    sc_signal<size_t>       r_dcache_xtn_set;		    // selected set (from dcache)

    // handling dcache miss
    sc_signal<int>	        r_dcache_miss_type;		    // depending on the requester
    sc_signal<size_t>       r_dcache_miss_word;		    // word index for cache update
    sc_signal<size_t>       r_dcache_miss_way;		    // selected way for cache update
    sc_signal<size_t>       r_dcache_miss_set;		    // selected set for cache update
    sc_signal<bool>         r_dcache_miss_inval;        // coherence request matching a miss
    sc_signal<bool>         r_dcache_miss_clack;        // waiting for a cleanup acknowledge

    // handling coherence requests
    sc_signal<size_t>       r_dcache_cc_way;		    // selected way for cc update/inval
    sc_signal<size_t>       r_dcache_cc_set;		    // selected set for cc update/inval
    sc_signal<size_t>       r_dcache_cc_word;		    // word counter for cc update
    sc_signal<bool>         r_dcache_cc_need_write;     // activate the cache for writing

    // coherence clack handling
    sc_signal<bool>         r_dcache_clack_req;         // clack request
    sc_signal<size_t>       r_dcache_clack_way;		    // clack way
    sc_signal<size_t>       r_dcache_clack_set;		    // clack set

    // dcache flush handling
    sc_signal<size_t>       r_dcache_flush_count;	    // slot counter used for cache flush

    // ll response handling
    sc_signal<size_t>       r_dcache_ll_rsp_count;	    // flit counter used for ll rsp

    // used by the TLB miss sub-fsm
    sc_signal<uint32_t>     r_dcache_tlb_vaddr;		    // virtual address for a tlb miss
    sc_signal<bool>         r_dcache_tlb_ins;		    // target tlb (itlb if true)
    sc_signal<paddr_t>      r_dcache_tlb_paddr;		    // physical address of pte
    sc_signal<uint32_t>     r_dcache_tlb_pte_flags;	    // pte1 or first word of pte2
    sc_signal<uint32_t>     r_dcache_tlb_pte_ppn;	    // second word of pte2
    sc_signal<size_t>       r_dcache_tlb_cache_way;	    // selected way in dcache
    sc_signal<size_t>       r_dcache_tlb_cache_set;	    // selected set in dcache
    sc_signal<size_t>       r_dcache_tlb_cache_word;	// selected word in dcache
    sc_signal<size_t>       r_dcache_tlb_way;		    // selected way in tlb
    sc_signal<size_t>       r_dcache_tlb_set;		    // selected set in tlb

    // ITLB and DTLB invalidation
    sc_signal<paddr_t>      r_dcache_tlb_inval_line;	// line index
    sc_signal<size_t>       r_dcache_tlb_inval_set;     // tlb set counter

    // communication between DCACHE FSM and ICACHE FSM
    sc_signal<bool>         r_dcache_xtn_req;           // xtn request (caused by processor)
    sc_signal<int>          r_dcache_xtn_opcode;        // xtn request type

    // Filp-Flop in DCACHE FSM for saving the cleanup victim request
    sc_signal<bool>         r_dcache_cleanup_victim_req; 
    sc_signal<paddr_t>      r_dcache_cleanup_victim_nline;

    // communication between DCACHE FSM and CC_SEND FSM
    sc_signal<bool>         r_dcache_cc_send_req;           // DCACHE cc_send request
    sc_signal<int>          r_dcache_cc_send_type;          // DCACHE cc_send request type
    sc_signal<paddr_t>      r_dcache_cc_send_nline;         // DCACHE cc_send nline
    sc_signal<size_t>       r_dcache_cc_send_way;           // DCACHE cc_send way
    sc_signal<size_t>       r_dcache_cc_send_updt_tab_idx;  // DCACHE cc_send update table index

    // dcache directory extension
    bool                    *r_dcache_in_tlb;               // copy exist in dtlb or itlb
    bool                    *r_dcache_contains_ptd;         // cache line contains a PTD

    // Physical address extension for data access
    sc_signal<uint32_t>     r_dcache_paddr_ext;             // CP2 register (if vci_address > 32)

    ///////////////////////////////////
    // VCI_CMD FSM REGISTERS
    ///////////////////////////////////
    sc_signal<int>          r_vci_cmd_fsm;
    sc_signal<size_t>       r_vci_cmd_min;      	        // used for write bursts
    sc_signal<size_t>       r_vci_cmd_max;      	        // used for write bursts
    sc_signal<size_t>       r_vci_cmd_cpt;    		        // used for write bursts
    sc_signal<bool>         r_vci_cmd_imiss_prio;	        // round-robin between imiss & dmiss

    ///////////////////////////////////
    // VCI_RSP FSM REGISTERS
    ///////////////////////////////////
    sc_signal<int>          r_vci_rsp_fsm;
    sc_signal<size_t>       r_vci_rsp_cpt;
    sc_signal<bool>         r_vci_rsp_ins_error;
    sc_signal<bool>         r_vci_rsp_data_error;
    GenericFifo<uint32_t>   r_vci_rsp_fifo_icache;	        // response FIFO to ICACHE FSM
    GenericFifo<uint32_t>   r_vci_rsp_fifo_dcache;	        // response FIFO to DCACHE FSM

    ///////////////////////////////////
    //  CC_SEND FSM REGISTER
    ///////////////////////////////////
    sc_signal<int>          r_cc_send_fsm;                  // state register
    sc_signal<bool>         r_cc_send_last_client;          // 0 dcache / 1 icache

    ///////////////////////////////////
    //  CC_RECEIVE FSM REGISTER
    ///////////////////////////////////
    sc_signal<int>          r_cc_receive_fsm;               // state register
    sc_signal<bool>         r_cc_receive_data_ins;          // request to : 0 dcache / 1 icache

    // communication between CC_RECEIVE FSM and ICACHE/DCACHE FSM
    sc_signal<size_t>       r_cc_receive_word_idx;          // word index
    GenericFifo<uint32_t>   r_cc_receive_updt_fifo_be;
    GenericFifo<uint32_t>   r_cc_receive_updt_fifo_data;
    GenericFifo<bool>       r_cc_receive_updt_fifo_eop;

    // communication between CC_RECEIVE FSM and ICACHE FSM
    sc_signal<bool>         r_cc_receive_icache_req;        // cc_receive to icache request
    sc_signal<int>          r_cc_receive_icache_type;       // cc_receive type of request
    sc_signal<size_t>       r_cc_receive_icache_way;        // cc_receive to icache way
    sc_signal<size_t>       r_cc_receive_icache_set;        // cc_receive to icache set
    sc_signal<size_t>       r_cc_receive_icache_updt_tab_idx;  // cc_receive update table index
    sc_signal<paddr_t>      r_cc_receive_icache_nline;	    // cache line physical address

    // communication between CC_RECEIVE FSM and DCACHE FSM
    sc_signal<bool>         r_cc_receive_dcache_req;        // cc_receive to dcache request
    sc_signal<int>          r_cc_receive_dcache_type;       // cc_receive type of request
    sc_signal<size_t>       r_cc_receive_dcache_way;        // cc_receive to dcache way
    sc_signal<size_t>       r_cc_receive_dcache_set;        // cc_receive to dcache set
    sc_signal<size_t>       r_cc_receive_dcache_updt_tab_idx;  // cc_receive update table index
    sc_signal<paddr_t>      r_cc_receive_dcache_nline;	    // cache line physical address

    ///////////////////////////////////
    //  DSPIN CLACK INTERFACE REGISTER
    ///////////////////////////////////
    sc_signal<bool>         r_dspin_clack_req;
    sc_signal<uint64_t>     r_dspin_clack_flit;
    
    //////////////////////////////////////////////////////////////////
    // processor, write buffer, caches , TLBs
    //////////////////////////////////////////////////////////////////

    iss_t                       r_iss;
    MultiWriteBuffer<paddr_t>	r_wbuf;
    GenericCache<paddr_t>   	r_icache;
    GenericCache<paddr_t>    	r_dcache;
    GenericTlb<paddr_t>       	r_itlb;
    GenericTlb<paddr_t>     	r_dtlb;

    //////////////////////////////////////////////////////////////////
    // llsc registration buffer
    //////////////////////////////////////////////////////////////////

    sc_signal<paddr_t>                     r_dcache_llsc_paddr;
    sc_signal<uint32_t>                    r_dcache_llsc_key;
    sc_signal<uint32_t>                    r_dcache_llsc_count;
    sc_signal<bool>                        r_dcache_llsc_valid;

    ////////////////////////////////
    // Activity counters
    ////////////////////////////////
    uint32_t m_cpt_dcache_data_read;        // DCACHE DATA READ
    uint32_t m_cpt_dcache_data_write;       // DCACHE DATA WRITE
    uint32_t m_cpt_dcache_dir_read;         // DCACHE DIR READ
    uint32_t m_cpt_dcache_dir_write;        // DCACHE DIR WRITE

    uint32_t m_cpt_icache_data_read;        // ICACHE DATA READ
    uint32_t m_cpt_icache_data_write;       // ICACHE DATA WRITE
    uint32_t m_cpt_icache_dir_read;         // ICACHE DIR READ
    uint32_t m_cpt_icache_dir_write;        // ICACHE DIR WRITE

    uint32_t m_cpt_frz_cycles;	            // number of cycles where the cpu is frozen
    uint32_t m_cpt_total_cycles;	        // total number of cycles

    // Cache activity counters
    uint32_t m_cpt_data_read;               // total number of read data
    uint32_t m_cpt_data_write;              // total number of write data
    uint32_t m_cpt_data_miss;               // number of read miss
    uint32_t m_cpt_ins_miss;                // number of instruction miss
    uint32_t m_cpt_unc_read;                // number of read uncached
    uint32_t m_cpt_write_cached;            // number of cached write
    uint32_t m_cpt_ins_read;                // number of instruction read
    uint32_t m_cpt_ins_spc_miss;            // number of speculative instruction miss

    uint32_t m_cost_write_frz;              // number of frozen cycles related to write buffer
    uint32_t m_cost_data_miss_frz;          // number of frozen cycles related to data miss
    uint32_t m_cost_unc_read_frz;           // number of frozen cycles related to uncached read
    uint32_t m_cost_ins_miss_frz;           // number of frozen cycles related to ins miss

    uint32_t m_cpt_imiss_transaction;       // number of VCI instruction miss transactions
    uint32_t m_cpt_dmiss_transaction;       // number of VCI data miss transactions
    uint32_t m_cpt_unc_transaction;         // number of VCI uncached read transactions
    uint32_t m_cpt_write_transaction;       // number of VCI write transactions
    uint32_t m_cpt_icache_unc_transaction;

    uint32_t m_cost_imiss_transaction;      // cumulated duration for VCI IMISS transactions
    uint32_t m_cost_dmiss_transaction;      // cumulated duration for VCI DMISS transactions
    uint32_t m_cost_unc_transaction;        // cumulated duration for VCI UNC transactions
    uint32_t m_cost_write_transaction;      // cumulated duration for VCI WRITE transactions
    uint32_t m_cost_icache_unc_transaction; // cumulated duration for VCI IUNC transactions
    uint32_t m_length_write_transaction;    // cumulated length for VCI WRITE transactions

    // TLB activity counters
    uint32_t m_cpt_ins_tlb_read;            // number of instruction tlb read
    uint32_t m_cpt_ins_tlb_miss;            // number of instruction tlb miss
    uint32_t m_cpt_ins_tlb_update_acc;      // number of instruction tlb update
    uint32_t m_cpt_ins_tlb_occup_cache;     // number of instruction tlb occupy data cache line
    uint32_t m_cpt_ins_tlb_hit_dcache;      // number of instruction tlb hit in data cache

    uint32_t m_cpt_data_tlb_read;           // number of data tlb read
    uint32_t m_cpt_data_tlb_miss;           // number of data tlb miss
    uint32_t m_cpt_data_tlb_update_acc;     // number of data tlb update
    uint32_t m_cpt_data_tlb_update_dirty;   // number of data tlb update dirty
    uint32_t m_cpt_data_tlb_hit_dcache;     // number of data tlb hit in data cache
    uint32_t m_cpt_data_tlb_occup_cache;    // number of data tlb occupy data cache line
    uint32_t m_cpt_tlb_occup_dcache;

    uint32_t m_cost_ins_tlb_miss_frz;       // number of frozen cycles related to instruction tlb miss
    uint32_t m_cost_data_tlb_miss_frz;      // number of frozen cycles related to data tlb miss
    uint32_t m_cost_ins_tlb_update_acc_frz;    // number of frozen cycles related to instruction tlb update acc
    uint32_t m_cost_data_tlb_update_acc_frz;   // number of frozen cycles related to data tlb update acc
    uint32_t m_cost_data_tlb_update_dirty_frz; // number of frozen cycles related to data tlb update dirty
    uint32_t m_cost_ins_tlb_occup_cache_frz;   // number of frozen cycles related to instruction tlb miss operate in dcache
    uint32_t m_cost_data_tlb_occup_cache_frz;  // number of frozen cycles related to data tlb miss operate in dcache

    uint32_t m_cpt_itlbmiss_transaction;       // number of itlb miss transactions
    uint32_t m_cpt_itlb_ll_transaction;        // number of itlb ll acc transactions
    uint32_t m_cpt_itlb_sc_transaction;        // number of itlb sc acc transactions
    uint32_t m_cpt_dtlbmiss_transaction;       // number of dtlb miss transactions
    uint32_t m_cpt_dtlb_ll_transaction;        // number of dtlb ll acc transactions
    uint32_t m_cpt_dtlb_sc_transaction;        // number of dtlb sc acc transactions
    uint32_t m_cpt_dtlb_ll_dirty_transaction;  // number of dtlb ll dirty transactions
    uint32_t m_cpt_dtlb_sc_dirty_transaction;  // number of dtlb sc dirty transactions

    uint32_t m_cost_itlbmiss_transaction;       // cumulated duration for VCI instruction TLB miss transactions
    uint32_t m_cost_itlb_ll_transaction;        // cumulated duration for VCI instruction TLB ll acc transactions
    uint32_t m_cost_itlb_sc_transaction;        // cumulated duration for VCI instruction TLB sc acc transactions
    uint32_t m_cost_dtlbmiss_transaction;       // cumulated duration for VCI data TLB miss transactions
    uint32_t m_cost_dtlb_ll_transaction;        // cumulated duration for VCI data TLB ll acc transactions
    uint32_t m_cost_dtlb_sc_transaction;        // cumulated duration for VCI data TLB sc acc transactions
    uint32_t m_cost_dtlb_ll_dirty_transaction;  // cumulated duration for VCI data TLB ll dirty transactions
    uint32_t m_cost_dtlb_sc_dirty_transaction;  // cumulated duration for VCI data TLB sc dirty transactions

    // coherence activity counters
    uint32_t m_cpt_cc_update_icache;            // number of coherence update instruction commands
    uint32_t m_cpt_cc_update_dcache;            // number of coherence update data commands
    uint32_t m_cpt_cc_inval_icache;             // number of coherence inval instruction commands
    uint32_t m_cpt_cc_inval_dcache;             // number of coherence inval data commands
    uint32_t m_cpt_cc_broadcast;                // number of coherence broadcast commands

    uint32_t m_cost_updt_data_frz;              // number of frozen cycles related to coherence update data packets
    uint32_t m_cost_inval_ins_frz;              // number of frozen cycles related to coherence inval instruction packets
    uint32_t m_cost_inval_data_frz;             // number of frozen cycles related to coherence inval data packets
    uint32_t m_cost_broadcast_frz;              // number of frozen cycles related to coherence broadcast packets

    uint32_t m_cpt_cc_cleanup_ins;              // number of coherence cleanup packets
    uint32_t m_cpt_cc_cleanup_data;             // number of coherence cleanup packets

    uint32_t m_cpt_icleanup_transaction;        // number of instruction cleanup transactions
    uint32_t m_cpt_dcleanup_transaction;        // number of instructinumber of data cleanup transactions
    uint32_t m_cost_icleanup_transaction;       // cumulated duration for VCI instruction cleanup transactions
    uint32_t m_cost_dcleanup_transaction;       // cumulated duration for VCI data cleanup transactions

    uint32_t m_cost_ins_tlb_inval_frz;      // number of frozen cycles related to checking ins tlb invalidate
    uint32_t m_cpt_ins_tlb_inval;           // number of ins tlb invalidate

    uint32_t m_cost_data_tlb_inval_frz;     // number of frozen cycles related to checking data tlb invalidate
    uint32_t m_cpt_data_tlb_inval;          // number of data tlb invalidate

    // FSM activity counters
    uint32_t m_cpt_fsm_icache     [64];
    uint32_t m_cpt_fsm_dcache     [64];
    uint32_t m_cpt_fsm_cmd        [64];
    uint32_t m_cpt_fsm_rsp        [64];
    uint32_t m_cpt_fsm_cc_receive [64];
    uint32_t m_cpt_fsm_cc_send    [64];

    uint32_t m_cpt_stop_simulation;		// used to stop simulation if frozen
    bool     m_monitor_ok;		        // used to debug cache output  
    uint32_t m_monitor_base;		    
    uint32_t m_monitor_length;		    

protected:
    SC_HAS_PROCESS(VciCcVCacheWrapper);

public:
    VciCcVCacheWrapper(
        sc_module_name                      name,
        const int                           proc_id,
        const soclib::common::MappingTable  &mtd,
        const soclib::common::IntTab        &srcid,
        const size_t                        cc_global_id,
        const size_t                        itlb_ways,
        const size_t                        itlb_sets,
        const size_t                        dtlb_ways,
        const size_t                        dtlb_sets,
        const size_t                        icache_ways,
        const size_t                        icache_sets,
        const size_t                        icache_words,
        const size_t                        dcache_ways,
        const size_t                        dcache_sets,
        const size_t                        dcache_words,
        const size_t                        wbuf_nlines,
        const size_t                        wbuf_nwords,
        const size_t                        x_width,
        const size_t                        y_width,
        const uint32_t                      max_frozen_cycles,
        const uint32_t                      debug_start_cycle,
        const bool                          debug_ok );

    ~VciCcVCacheWrapper();

    void print_cpi();
    void print_stats();
    void clear_stats();
    void print_trace(size_t mode = 0);
    void cache_monitor(paddr_t addr);
    void start_monitor(paddr_t,paddr_t);
    void stop_monitor();
    inline void iss_set_debug_mask(uint v) 
    {
	    r_iss.set_debug_mask(v);
    }

    /////////////////////////////////////////////////////////////
    // Set the m_dcache_paddr_ext_reset attribute
    //
    // The r_dcache_paddr_ext register will be initialized after
    // reset with the m_dcache_paddr_ext_reset value
    /////////////////////////////////////////////////////////////
    inline void set_dcache_paddr_ext_reset(uint32_t v)
    {
        m_dcache_paddr_ext_reset = v;
    }

    /////////////////////////////////////////////////////////////
    // Set the m_icache_paddr_ext_reset attribute
    //
    // The r_icache_paddr_ext register will be initialized after
    // reset with the m_icache_paddr_ext_reset value
    /////////////////////////////////////////////////////////////
    inline void set_icache_paddr_ext_reset(uint32_t v)
    {
        m_icache_paddr_ext_reset = v;
    }

private:
    void transition();
    void genMoore();

    soclib_static_assert((int)iss_t::SC_ATOMIC == (int)vci_param::STORE_COND_ATOMIC);
    soclib_static_assert((int)iss_t::SC_NOT_ATOMIC == (int)vci_param::STORE_COND_NOT_ATOMIC);
};

}}

#endif /* SOCLIB_CABA_VCI_CC_VCACHE_WRAPPER_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
