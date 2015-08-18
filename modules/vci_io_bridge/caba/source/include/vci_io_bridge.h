/* -*- c++ -*-
 * File : vci_io_bridge.h
 * Copyright (c) UPMC, Lip6, SoC
 * Date : 16/04/2012
 * Authors: Cassio Fraga, Alain Greiner
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
 * Maintainers: Cesar Fuguet Tortolero <cesar.fuguet-tortolero@lip6.fr>
 */
/////////////////////////////////////////////////////////////////////////////////
// This TSAR component is a bridge to access external peripherals
// connected to an external I/O bus (such as Hypertransport or PCIe).
// It connects three VCI networks:
//
// - INT network : to receive both configuration requests from processors
//                 or software driven data access to peripherals.
// - RAM network : to send DMA transactions initiated by peripherals
//                 directly to the RAM (or L3 caches).
// - IOX network : to receive DMA transactions from peripherals, or to send
//                 configuration or data transactions to peripherals.
//
// It supports two types of transactions from peripherals:
//   - DMA transactions to the RAM network,
//   - WTI transactions to the INT network.
// Regarding transactions initiated by external peripherals, it provides
// an - optional - IOMMU service : the 32 bits virtual address is translated
// to a (up to) 40 bits physical address by a standard SoCLib generic TLB.
// In case of TLB MISS, the DMA transaction is stalled until the TLB is updated.
// In case of page fault or read_only violation (illegal access), a VCI error
// is returned to the faulty peripheral, and a IOMMU WTI is sent.
/////////////////////////////////////////////////////////////////////////////////
//   General Constraints:
//
// - All VCI fields have the same widths on the RAM and IOX networks,
//   and the VCI DATA field is 64 bits.
// - Only the VCI DATA field differ between INT and IOX/RAM networks,
//   as the VCI DATA field is 32 bits.
// - The common VCI ADDRESS width cannot be larger than 64 bits.
// - All VCI transactions must be included in a single cache line.
// - Page Tables must have the format required by the SoCLib generic_tlb.
// - IO's segments must be the same in INT and IOX networks
// - Write operations on IOMMU configuration registers (PTPR, ACTIVE) are
//   delayed until DMA_TLB FSM is IDLE. It should, preferably, be done before
//   starting any transfers. Pseudo register INVAL may be modified any time.
////////////////////////////////////////////////////////////////////////////////


///////TODO List///////////////////////////////////////////////////////////////
// - Ne pas garder tous les champs WRITE CMD dans les FIFO a chaque flit
//   (seulement 'data' et 'be')
///////////////////////////////////////////////////////////////////////////////

#ifndef SOCLIB_CABA_VCI_IO_BRIDGE_H
#define SOCLIB_CABA_VCI_IO_BRIDGE_H

#include <inttypes.h>
#include <systemc>
#include "caba_base_module.h"
#include "generic_fifo.h"
#include "generic_tlb.h"
#include "mapping_table.h"
#include "address_decoding_table.h"
#include "address_masking_table.h"
#include "static_assert.h"
#include "vci_initiator.h"
#include "vci_target.h"
#include "transaction_tab_io.h"
#include "../../../include/soclib/io_bridge.h"

namespace soclib {
namespace caba {

using namespace soclib::common;

///////////////////////////////////////////////////////////////////////////////////
template<typename vci_param_int,
         typename vci_param_ext>
class VciIoBridge
///////////////////////////////////////////////////////////////////////////////////
    : public soclib::caba::BaseModule
{
    // Data and be fields have different widths on INT and EXT/IOC networks
    typedef typename vci_param_ext::data_t          ext_data_t;
    typedef typename vci_param_int::data_t          int_data_t;
    typedef typename vci_param_ext::be_t            ext_be_t;
    typedef typename vci_param_int::be_t            int_be_t;

    // Other fields must be equal
    typedef typename vci_param_int::fast_addr_t     vci_addr_t;
    typedef typename vci_param_int::srcid_t         vci_srcid_t;
    typedef typename vci_param_int::trdid_t         vci_trdid_t;
    typedef typename vci_param_int::pktid_t         vci_pktid_t;
    typedef typename vci_param_int::plen_t          vci_plen_t;
    typedef typename vci_param_int::cmd_t           vci_cmd_t;
    typedef typename vci_param_int::contig_t        vci_contig_t;
    typedef typename vci_param_int::eop_t           vci_eop_t;
    typedef typename vci_param_int::const_t         vci_cons_t;
    typedef typename vci_param_int::wrap_t          vci_wrap_t;
    typedef typename vci_param_int::clen_t          vci_clen_t;
    typedef typename vci_param_int::cfixed_t        vci_cfixed_t;
    typedef typename vci_param_int::rerror_t        vci_rerror_t;

    enum
    {
        CACHE_LINE_MASK    = 0xFFFFFFFFC0ULL,
        PPN1_MASK          = 0x0007FFFF,
        PPN2_MASK          = 0x0FFFFFFF,
        K_PAGE_OFFSET_MASK = 0x00000FFF,
        M_PAGE_OFFSET_MASK = 0x001FFFFF,
        PTE2_LINE_OFFSET   = 0x00007000, // bits 12,13,14.
        PTE1_LINE_OFFSET   = 0x01E00000, // bits 21,22,23,24
    };

    // States for DMA_CMD FSM (from IOX to RAM)
    enum dma_cmd_fsm_state
    {
        DMA_CMD_IDLE,
        DMA_CMD_DMA_REQ,
        DMA_CMD_WTI_IOX_REQ,
        DMA_CMD_ERR_WAIT_EOP,
        DMA_CMD_ERR_WTI_REQ,
        DMA_CMD_ERR_RSP_REQ,
        DMA_CMD_TLB_MISS_WAIT,
    };

    // States for DMA_RSP FSM
    enum dma_rsp_fsm_state
    {
        DMA_RSP_IDLE_DMA,
        DMA_RSP_IDLE_WTI,
        DMA_RSP_IDLE_ERR,
        DMA_RSP_PUT_DMA,
        DMA_RSP_PUT_WTI,
        DMA_RSP_PUT_ERR,
    };

    // States for TLB_MISS FSM
    enum dma_tlb_fsm_state
    {
        TLB_IDLE,
        TLB_MISS,
        TLB_PTE1_GET,
        TLB_PTE1_SELECT,
        TLB_PTE1_UPDT,
        TLB_PTE2_GET,
        TLB_PTE2_SELECT,
        TLB_PTE2_UPDT,
        TLB_WAIT,
        TLB_RETURN,
        TLB_INVAL_CHECK,
    };

    // States for CONFIG_CMD FSM
    enum config_cmd_fsm_state
    {
        CONFIG_CMD_IDLE,
        CONFIG_CMD_WAIT,
        CONFIG_CMD_HI,
        CONFIG_CMD_LO,
        CONFIG_CMD_PUT,
        CONFIG_CMD_RSP,
    };

    // states for CONFIG_RSP FSM
    enum config_rsp_fsm_state
    {
        CONFIG_RSP_IDLE_IOX,
        CONFIG_RSP_IDLE_LOC,
        CONFIG_RSP_PUT_LO,
        CONFIG_RSP_PUT_HI,
        CONFIG_RSP_PUT_UNC,
        CONFIG_RSP_PUT_LOC,

    };

    // States for MISS_WTI_RSP FSM
    enum miss_wti_rsp_state
    {
        MISS_WTI_RSP_IDLE,
        MISS_WTI_RSP_WTI_IOX,
        MISS_WTI_RSP_WTI_MMU,
        MISS_WTI_RSP_MISS,
    };

    // PKTID values for TLB MISS and WTI transactions
    enum pktid_values_e
    {
        PKTID_MISS    = 0x0,  // TSAR code for read data uncached
        PKTID_WTI_IOX = 0x4,  // TSAR code for write
        PKTID_WTI_MMU = 0xC,  // TSAR code for write
    };

    // Miss types for iotlb
    enum tlb_miss_type_e
    {
        PTE1_MISS,
        PTE2_MISS,
    };

public:
    sc_in<bool>                               p_clk;
    sc_in<bool>                               p_resetn;

    soclib::caba::VciInitiator<vci_param_ext> p_vci_ini_ram;

    soclib::caba::VciTarget<vci_param_ext>    p_vci_tgt_iox;
    soclib::caba::VciInitiator<vci_param_ext> p_vci_ini_iox;

    soclib::caba::VciTarget<vci_param_int>    p_vci_tgt_int;
    soclib::caba::VciInitiator<vci_param_int> p_vci_ini_int;

private:
    const size_t                              m_words;

    // INT & IOX Networks
    std::list<soclib::common::Segment>        m_int_seglist;
    const vci_srcid_t                         m_int_srcid;      // SRCID on INT network
    std::list<soclib::common::Segment>        m_iox_seglist;
    const vci_srcid_t                         m_iox_srcid;      // SRCID on IOX network

    // INT & RAM srcid masking table
    const AddressMaskingTable<uint32_t>       m_srcid_gid_mask;
    const AddressMaskingTable<uint32_t>       m_srcid_lid_mask;

    // TLB parameters
    const size_t                              m_iotlb_ways;
    const size_t                              m_iotlb_sets;

    // debug variables
    uint32_t                                  m_debug_start_cycle;
    bool                                      m_debug_ok;
    bool                                      m_debug_activated;

    ///////////////////////////////
    // MEMORY MAPPED REGISTERS
    ///////////////////////////////
    sc_signal<uint32_t>         r_iommu_ptpr;           // page table pointer
    sc_signal<bool>             r_iommu_active;         // iotlb mode
    sc_signal<uint32_t>         r_iommu_bvar;           // bad vaddr
    sc_signal<uint32_t>         r_iommu_etr;            // error type
    sc_signal<uint32_t>         r_iommu_bad_id;         // faulty srcid
    sc_signal<bool>             r_iommu_wti_enable;     // enable IOB WTI
    sc_signal<uint32_t>         r_iommu_wti_addr_lo;    // IOMMU WTI paddr (32 lsb)
    sc_signal<uint32_t>         r_iommu_wti_addr_hi;    // IOMMU WTI paddr (32 msb)

    ///////////////////////////////////
    // DMA_CMD FSM REGISTERS
    ///////////////////////////////////
    sc_signal<int>              r_dma_cmd_fsm;
    sc_signal<vci_addr_t>       r_dma_cmd_paddr;                // output paddr

    sc_signal<bool>             r_dma_cmd_to_miss_wti_cmd_req;
    sc_signal<vci_addr_t>       r_dma_cmd_to_miss_wti_cmd_addr;
    sc_signal<vci_cmd_t>        r_dma_cmd_to_miss_wti_cmd_cmd;
    sc_signal<vci_srcid_t>      r_dma_cmd_to_miss_wti_cmd_srcid;
    sc_signal<vci_trdid_t>      r_dma_cmd_to_miss_wti_cmd_trdid;
    sc_signal<vci_trdid_t>      r_dma_cmd_to_miss_wti_cmd_pktid;
    sc_signal<int_data_t>       r_dma_cmd_to_miss_wti_cmd_wdata;

    sc_signal<bool>             r_dma_cmd_to_dma_rsp_req;
    sc_signal<vci_srcid_t>      r_dma_cmd_to_dma_rsp_rsrcid;
    sc_signal<vci_trdid_t>      r_dma_cmd_to_dma_rsp_rtrdid;
    sc_signal<vci_pktid_t>      r_dma_cmd_to_dma_rsp_rpktid;
    sc_signal<vci_rerror_t>     r_dma_cmd_to_dma_rsp_rerror;
    sc_signal<ext_data_t>       r_dma_cmd_to_dma_rsp_rdata;

    sc_signal<bool>             r_dma_cmd_to_tlb_req;
    sc_signal<uint32_t>         r_dma_cmd_to_tlb_vaddr;         // input vaddr

    ///////////////////////////////////
    // DMA_RSP FSM REGISTERS
    ///////////////////////////////////
    sc_signal<int>              r_dma_rsp_fsm;

    ///////////////////////////////////
    // CONFIG_CMD FSM REGISTERS
    ///////////////////////////////////
    sc_signal<int>              r_config_cmd_fsm;

    sc_signal<bool>             r_config_cmd_to_tlb_req;
    sc_signal<uint32_t>         r_config_cmd_to_tlb_vaddr;

    sc_signal<bool>             r_config_cmd_to_config_rsp_req;
    sc_signal<bool>             r_config_cmd_to_config_rsp_rerror;
    sc_signal<int_data_t>       r_config_cmd_to_config_rsp_rdata;
    sc_signal<vci_srcid_t>      r_config_cmd_to_config_rsp_rsrcid;
    sc_signal<vci_trdid_t>      r_config_cmd_to_config_rsp_rtrdid;
    sc_signal<vci_pktid_t>      r_config_cmd_to_config_rsp_rpktid;

    sc_signal<ext_data_t>       r_config_cmd_wdata;
    sc_signal<ext_be_t>         r_config_cmd_be;
    sc_signal<vci_plen_t>       r_config_cmd_cmd;
    sc_signal<vci_addr_t>       r_config_cmd_address;
    sc_signal<vci_srcid_t>      r_config_cmd_srcid;
    sc_signal<vci_trdid_t>      r_config_cmd_trdid;
    sc_signal<vci_pktid_t>      r_config_cmd_pktid;
    sc_signal<vci_plen_t>       r_config_cmd_plen;
    sc_signal<vci_clen_t>       r_config_cmd_clen;
    sc_signal<vci_cons_t>       r_config_cmd_cons;
    sc_signal<vci_contig_t>     r_config_cmd_contig;
    sc_signal<vci_cfixed_t>     r_config_cmd_cfixed;
    sc_signal<vci_wrap_t>       r_config_cmd_wrap;
    sc_signal<vci_eop_t>        r_config_cmd_eop;

    TransactionTabIO            m_iox_transaction_tab;

    ///////////////////////////////////
    // CONFIG_RSP FSM REGISTERS
    ///////////////////////////////////
    sc_signal<int>              r_config_rsp_fsm;
    sc_signal<vci_srcid_t>      r_config_rsp_rsrcid;
    sc_signal<vci_trdid_t>      r_config_rsp_rtrdid;

    ///////////////////////////////////
    // TLB FSM REGISTERS
    ///////////////////////////////////
    sc_signal<int>              r_tlb_fsm;                  // state register
    sc_signal<bool>             r_waiting_transaction;      // Flag for returning from
    sc_signal<int>              r_tlb_miss_type;
    sc_signal<bool>             r_tlb_miss_error;

    sc_signal<vci_addr_t>       r_tlb_paddr;                // physical address of pte
    sc_signal<uint32_t>         r_tlb_pte_flags;            // pte1 or first word of pte2
    sc_signal<uint32_t>         r_tlb_pte_ppn;              // second word of pte2
    sc_signal<size_t>           r_tlb_way;                  // selected way in tlb
    sc_signal<size_t>           r_tlb_set;                  // selected set in tlb

    uint32_t*                   r_tlb_buf_data;             // prefetch buffer for PTEs
    sc_signal<bool>             r_tlb_buf_valid;            // one valit flag for all PTEs
    sc_signal<vci_addr_t>       r_tlb_buf_tag;              // cache line number
    sc_signal<vci_addr_t>       r_tlb_buf_vaddr;            // vaddr for first PTE
    sc_signal<bool>             r_tlb_buf_big_page;         // ???

    sc_signal<bool>             r_tlb_to_miss_wti_cmd_req;

    ///////////////////////////////////
    // MISS_WTI_RSP FSM REGISTERS
    ///////////////////////////////////
    sc_signal<int>              r_miss_wti_rsp_fsm;
    sc_signal<bool>             r_miss_wti_rsp_error_wti;   // VCI error on WTI
    sc_signal<bool>             r_miss_wti_rsp_error_miss;  // VCI error on MISS
    sc_signal<size_t>           r_miss_wti_rsp_count;       // flits counter

    sc_signal<bool>             r_miss_wti_rsp_to_dma_rsp_req;
    sc_signal<vci_rerror_t>     r_miss_wti_rsp_to_dma_rsp_rerror;
    sc_signal<vci_srcid_t>      r_miss_wti_rsp_to_dma_rsp_rsrcid;
    sc_signal<vci_trdid_t>      r_miss_wti_rsp_to_dma_rsp_rtrdid;
    sc_signal<vci_pktid_t>      r_miss_wti_rsp_to_dma_rsp_rpktid;

    sc_signal<bool>             r_miss_wti_rsp_to_tlb_done;

    /////////////////////////////////////////////////////
    //  ALLOCATORS for CONFIG_RSP fifo & DMA_RSP fifo
    /////////////////////////////////////////////////////
    sc_signal<bool>             r_alloc_fifo_config_rsp_local;


    //////////////////////////////////////////////////////////////////
    // IOTLB
    //////////////////////////////////////////////////////////////////
    GenericTlb<vci_addr_t>      r_iotlb;


    /////////////////////////
    // FIFOs
    /////////////////////////

    // ouput FIFO to VCI INI port on RAM network (VCI command)
    GenericFifo<vci_addr_t>     m_dma_cmd_addr_fifo;
    GenericFifo<vci_srcid_t>    m_dma_cmd_srcid_fifo;
    GenericFifo<vci_trdid_t>    m_dma_cmd_trdid_fifo;
    GenericFifo<vci_pktid_t>    m_dma_cmd_pktid_fifo;
    GenericFifo<ext_be_t>       m_dma_cmd_be_fifo;
    GenericFifo<vci_cmd_t>      m_dma_cmd_cmd_fifo;
    GenericFifo<vci_contig_t>   m_dma_cmd_contig_fifo;
    GenericFifo<ext_data_t>     m_dma_cmd_data_fifo;
    GenericFifo<vci_eop_t>      m_dma_cmd_eop_fifo;
    GenericFifo<vci_cons_t>     m_dma_cmd_cons_fifo;
    GenericFifo<vci_plen_t>     m_dma_cmd_plen_fifo;
    GenericFifo<vci_wrap_t>     m_dma_cmd_wrap_fifo;
    GenericFifo<vci_cfixed_t>   m_dma_cmd_cfixed_fifo;
    GenericFifo<vci_clen_t>     m_dma_cmd_clen_fifo;

    // output FIFO to VCI TGT port on IOX network (VCI response)
    GenericFifo<ext_data_t>     m_dma_rsp_data_fifo;
    GenericFifo<vci_srcid_t>    m_dma_rsp_rsrcid_fifo;
    GenericFifo<vci_trdid_t>    m_dma_rsp_rtrdid_fifo;
    GenericFifo<vci_pktid_t>    m_dma_rsp_rpktid_fifo;
    GenericFifo<vci_eop_t>      m_dma_rsp_reop_fifo;
    GenericFifo<vci_rerror_t>   m_dma_rsp_rerror_fifo;

    // output FIFO to VCI INI port on IOX network (VCI command)
    GenericFifo<vci_addr_t>     m_config_cmd_addr_fifo;
    GenericFifo<vci_srcid_t>    m_config_cmd_srcid_fifo;
    GenericFifo<vci_trdid_t>    m_config_cmd_trdid_fifo;
    GenericFifo<vci_pktid_t>    m_config_cmd_pktid_fifo;
    GenericFifo<ext_be_t>       m_config_cmd_be_fifo;
    GenericFifo<vci_cmd_t>      m_config_cmd_cmd_fifo;
    GenericFifo<vci_contig_t>   m_config_cmd_contig_fifo;
    GenericFifo<ext_data_t>     m_config_cmd_data_fifo;
    GenericFifo<vci_eop_t>      m_config_cmd_eop_fifo;
    GenericFifo<vci_cons_t>     m_config_cmd_cons_fifo;
    GenericFifo<vci_plen_t>     m_config_cmd_plen_fifo;
    GenericFifo<vci_wrap_t>     m_config_cmd_wrap_fifo;
    GenericFifo<vci_cfixed_t>   m_config_cmd_cfixed_fifo;
    GenericFifo<vci_clen_t>     m_config_cmd_clen_fifo;

    // output FIFO to VCI TGT port on INT network (VCI response)
    GenericFifo<int_data_t>     m_config_rsp_data_fifo;
    GenericFifo<vci_srcid_t>    m_config_rsp_rsrcid_fifo;
    GenericFifo<vci_trdid_t>    m_config_rsp_rtrdid_fifo;
    GenericFifo<vci_pktid_t>    m_config_rsp_rpktid_fifo;
    GenericFifo<vci_eop_t>      m_config_rsp_reop_fifo;
    GenericFifo<vci_rerror_t>   m_config_rsp_rerror_fifo;

    // output FIFO to VCI_INI port on INT network (VCI command)
    GenericFifo<vci_addr_t>     m_miss_wti_cmd_addr_fifo;
    GenericFifo<vci_srcid_t>    m_miss_wti_cmd_srcid_fifo;
    GenericFifo<vci_trdid_t>    m_miss_wti_cmd_trdid_fifo;
    GenericFifo<vci_pktid_t>    m_miss_wti_cmd_pktid_fifo;
    GenericFifo<int_be_t>       m_miss_wti_cmd_be_fifo;
    GenericFifo<vci_cmd_t>      m_miss_wti_cmd_cmd_fifo;
    GenericFifo<vci_contig_t>   m_miss_wti_cmd_contig_fifo;
    GenericFifo<int_data_t>     m_miss_wti_cmd_data_fifo;
    GenericFifo<vci_eop_t>      m_miss_wti_cmd_eop_fifo;
    GenericFifo<vci_cons_t>     m_miss_wti_cmd_cons_fifo;
    GenericFifo<vci_plen_t>     m_miss_wti_cmd_plen_fifo;
    GenericFifo<vci_wrap_t>     m_miss_wti_cmd_wrap_fifo;
    GenericFifo<vci_cfixed_t>   m_miss_wti_cmd_cfixed_fifo;
    GenericFifo<vci_clen_t>     m_miss_wti_cmd_clen_fifo;

    ////////////////////////////////
    // Activity counters
    ////////////////////////////////

    uint32_t m_cpt_total_cycles;            // total number of cycles

    // TLB activity counters
    uint32_t m_cpt_iotlb_read;              // number of iotlb read
    uint32_t m_cpt_iotlb_miss;              // number of iotlb miss
    uint32_t m_cost_iotlb_miss;             // number of wait cycles (not treatment itself)
    uint32_t m_cpt_iotlbmiss_transaction;   // number of tlb miss transactions
    uint32_t m_cost_iotlbmiss_transaction;  // cumulated duration tlb miss transactions

    //Transaction Tabs (TRTs) activity counters
    uint32_t m_cpt_trt_dma_full;            // DMA TRT full when a new command arrives
    uint32_t m_cpt_trt_dma_full_cost;       // total number of cycles blocked
    uint32_t m_cpt_trt_config_full;         // Config TRT full when a new command arrives
    uint32_t m_cpt_trt_config_full_cost;    // total number of cycles blocked

    // FSM activity counters
    // unused on print_stats
    uint32_t m_cpt_fsm_dma_cmd          [32];
    uint32_t m_cpt_fsm_dma_rsp          [32];
    uint32_t m_cpt_fsm_tlb              [32];
    uint32_t m_cpt_fsm_config_cmd       [32];
    uint32_t m_cpt_fsm_config_rsp       [32];
    uint32_t m_cpt_fsm_miss_wti_rsp     [32];

protected:

    SC_HAS_PROCESS(VciIoBridge);

public:

    VciIoBridge(
        sc_module_name                      insname,
        const soclib::common::MappingTable  &mt_ext,      // external network
        const soclib::common::MappingTable  &mt_int,      // internal network
        const soclib::common::MappingTable  &mt_iox,      // iox network
        const soclib::common::IntTab        &int_tgtid,   // INT network TGTID
        const soclib::common::IntTab        &int_srcid,   // INT network SRCID
        const soclib::common::IntTab        &iox_tgtid,   // IOX network TGTID
        const soclib::common::IntTab        &iox_srcid,   // IOX network SRCID
        const size_t                        dcache_words,
        const size_t                        iotlb_ways,
        const size_t                        iotlb_sets,
        const uint32_t                      debug_start_cycle,
        const bool                          debug_ok );

    ~VciIoBridge();

    void print_stats();
    void clear_stats();
    void print_trace(size_t mode = 0);


private:

    bool is_wti( vci_addr_t paddr );
    void transition();
    void genMoore();
};

}}

#endif /* SOCLIB_CABA_VCI_CC_VCACHE_WRAPPER_V4_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4




