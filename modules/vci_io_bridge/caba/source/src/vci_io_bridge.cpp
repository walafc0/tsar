/* -*- c++ -*-
 * File : vci_io_bridge.cpp
 * Copyright (c) UPMC, Lip6, SoC
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

#include <cassert>
#include "arithmetics.h"
#include "alloc_elems.h"
#include "../include/vci_io_bridge.h"


//////   debug services   ///////////////////////////////////////////////////////
// All debug messages are conditionned by two variables:
// - compile time : DEBUG_*** : defined below
// - execution time : m_debug_***  : defined by constructor arguments
//    m_debug_activated = (m_debug_ok) and (m_cpt_cycle > m_debug_start_cycle)
/////////////////////////////////////////////////////////////////////////////////

#define DEBUG_DMA_CMD           1
#define DEBUG_DMA_RSP           1
#define DEBUG_TLB_MISS          1
#define DEBUG_CONFIG_CMD        1
#define DEBUG_CONFIG_RSP        1
#define DEBUG_MISS_WTI_CMD      1

namespace soclib {
namespace caba {

namespace {

const char *dma_cmd_fsm_state_str[] =
    {
        "DMA_CMD_IDLE",
        "DMA_CMD_DMA_REQ",
        "DMA_CMD_WTI_IOX_REQ",
        "DMA_CMD_ERR_WAIT_EOP",
        "DMA_CMD_ERR_WTI_REQ",
        "DMA_CMD_ERR_RSP_REQ",
        "DMA_CMD_TLB_MISS_WAIT",
    };

const char *dma_rsp_fsm_state_str[] =
    {
        "DMA_RSP_IDLE_DMA",
        "DMA_RSP_IDLE_WTI",
        "DMA_RSP_IDLE_ERR",
        "DMA_RSP_PUT_DMA",
        "DMA_RSP_PUT_WTI",
        "DMA_RSP_PUT_ERR",
    };

const char *tlb_fsm_state_str[] =
    {
        "TLB_IDLE",
        "TLB_MISS",
        "TLB_PTE1_GET",
        "TLB_PTE1_SELECT",
        "TLB_PTE1_UPDT",
        "TLB_PTE2_GET",
        "TLB_PTE2_SELECT",
        "TLB_PTE2_UPDT",
        "TLB_WAIT",
        "TLB_RETURN",
        "TLB_INVAL_CHECK",
    };

const char *config_cmd_fsm_state_str[] =
    {
        "CONFIG_CMD_IDLE",
        "CONFIG_CMD_WAIT",
        "CONFIG_CMD_HI",
        "CONFIG_CMD_LO",
        "CONFIG_CMD_PUT",
        "CONFIG_CMD_RSP",
    };

const char *config_rsp_fsm_state_str[] =
    {
        "CONFIG_RSP_IDLE_IOX",
        "CONFIG_RSP_IDLE_LOC",
        "CONFIG_RSP_PUT_LO",
        "CONFIG_RSP_PUT_HI",
        "CONFIG_RSP_PUT_UNC",
        "CONFIG_RSP_PUT_LOC",
    };

const char *miss_wti_rsp_state_str[] =
    {
        "MISS_WTI_RSP_IDLE",
        "MISS_WTI_RSP_WTI_IOX",
        "MISS_WTI_RSP_WTI_MMU",
        "MISS_WTI_RSP_MISS",
    };
}

#define tmpl(...)  template<typename vci_param_int,typename vci_param_ext> \
    __VA_ARGS__ VciIoBridge<vci_param_int,vci_param_ext>

////////////////////////
tmpl(/**/)::VciIoBridge(
    sc_module_name                      name,
    const soclib::common::MappingTable  &mt_ext,
    const soclib::common::MappingTable  &mt_int,
    const soclib::common::MappingTable  &mt_iox,
    const soclib::common::IntTab        &int_tgtid,     // INT network TGTID
    const soclib::common::IntTab        &int_srcid,     // INT network SRCID
    const soclib::common::IntTab        &iox_tgtid,     // IOX network TGTID
    const soclib::common::IntTab        &iox_srcid,     // IOX network SRCID
    const size_t                        dcache_words,
    const size_t                        iotlb_ways,
    const size_t                        iotlb_sets,
    const uint32_t                      debug_start_cycle,
    const bool                          debug_ok)
    : soclib::caba::BaseModule(name),

      p_clk("p_clk"),
      p_resetn("p_resetn"),
      p_vci_ini_ram("p_vci_ini_ram"),
      p_vci_tgt_iox("p_vci_tgt_iox"),
      p_vci_ini_iox("p_vci_ini_iox"),
      p_vci_tgt_int("p_vci_tgt_int"),
      p_vci_ini_int("p_vci_ini_int"),

      m_words( dcache_words ),

      // INT & IOX Network
      m_int_seglist( mt_int.getSegmentList( int_tgtid )),
      m_int_srcid( mt_int.indexForId( int_srcid )),
      m_iox_seglist( mt_iox.getSegmentList( iox_tgtid )),
      m_iox_srcid( mt_iox.indexForId( iox_srcid )),

      m_srcid_gid_mask( mt_int.getSrcidLevelBits()[0],  // use global bits
                        mt_int.getSrcidLevelBits()[1]), // drop local bits
      m_srcid_lid_mask( mt_int.getSrcidLevelBits()[1],  // use local bits
                        0),

      m_iotlb_ways(iotlb_ways),
      m_iotlb_sets(iotlb_sets),

      m_debug_start_cycle(debug_start_cycle),
      m_debug_ok(debug_ok),

      // addressable registers
      r_iommu_ptpr("r_iommu_ptpr"),
      r_iommu_active("r_iommu_active"),
      r_iommu_bvar("r_iommu_bvar"),
      r_iommu_etr("r_iommu_etr"),
      r_iommu_bad_id("r_iommu_bad_id"),
      r_iommu_wti_enable("r_iommu_wti_enable"),
      r_iommu_wti_addr_lo("r_iommu_wti_addr_lo"),

      // DMA_CMD FSM registers
      r_dma_cmd_fsm("r_dma_cmd_fsm"),
      r_dma_cmd_paddr("r_dma_cmd_paddr"),

      r_dma_cmd_to_miss_wti_cmd_req("r_dma_cmd_to_miss_wti_cmd_req"),
      r_dma_cmd_to_miss_wti_cmd_addr("r_dma_cmd_to_miss_wti_cmd_addr"),
      r_dma_cmd_to_miss_wti_cmd_srcid("r_dma_cmd_to_miss_wti_cmd_srcid"),
      r_dma_cmd_to_miss_wti_cmd_trdid("r_dma_cmd_to_miss_wti_cmd_trdid"),
      r_dma_cmd_to_miss_wti_cmd_pktid("r_dma_cmd_to_miss_wti_cmd_pktid"),
      r_dma_cmd_to_miss_wti_cmd_wdata("r_dma_cmd_to_miss_wti_cmd_wdata"),

      r_dma_cmd_to_dma_rsp_req("r_dma_cmd_to_dma_rsp_req"),
      r_dma_cmd_to_dma_rsp_rsrcid("r_dma_cmd_to_dma_rsp_rsrcid"),
      r_dma_cmd_to_dma_rsp_rtrdid("r_dma_cmd_to_dma_rsp_rtrdid"),
      r_dma_cmd_to_dma_rsp_rpktid("r_dma_cmd_to_dma_rsp_rpktid"),

      r_dma_cmd_to_tlb_req("r_dma_cmd_to_tlb_req"),
      r_dma_cmd_to_tlb_vaddr("r_dma_cmd_to_tlb_vaddr"),

      //DMA_RSP FSM registers
      r_dma_rsp_fsm("r_dma_rsp_fsm"),

      // CONFIG_CMD FSM registers
      r_config_cmd_fsm("r_config_cmd_fsm"),

      r_config_cmd_to_tlb_req("r_config_cmd_to_tlb_req"),
      r_config_cmd_to_tlb_vaddr("r_config_cmd_to_tlb_vaddr"),

      r_config_cmd_to_config_rsp_req("r_config_cmd_to_config_rsp_req"),
      r_config_cmd_to_config_rsp_rerror("r_config_cmd_to_config_rsp_rerror"),
      r_config_cmd_to_config_rsp_rdata("r_config_cmd_to_config_rsp_rdata"),
      r_config_cmd_to_config_rsp_rsrcid("r_config_cmd_to_config_rsp_rsrcid"),
      r_config_cmd_to_config_rsp_rtrdid("r_config_cmd_to_config_rsp_rtrdid"),
      r_config_cmd_to_config_rsp_rpktid("r_config_cmd_to_config_rsp_rpktid"),

      r_config_cmd_wdata("r_config_cmd_wdata"),
      r_config_cmd_be("r_config_cmd_be"),
      r_config_cmd_cmd("r_config_cmd_cmd"),
      r_config_cmd_address("r_config_cmd_address"),
      r_config_cmd_srcid("r_config_cmd_srcid"),
      r_config_cmd_trdid("r_config_cmd_trdid"),
      r_config_cmd_pktid("r_config_cmd_pktid"),
      r_config_cmd_plen("r_config_cmd_plen"),
      r_config_cmd_clen("r_config_cmd_clen"),
      r_config_cmd_cons("r_config_cmd_cons"),
      r_config_cmd_contig("r_config_cmd_contig"),
      r_config_cmd_cfixed("r_config_cmd_cfixed"),
      r_config_cmd_wrap("r_config_cmd_wrap"),
      r_config_cmd_eop("r_config_cmd_eop"),

      // ID translation table used by CONFIG_CMD and CONFIG_RSP FSMs
      m_iox_transaction_tab(1),

      // CONFIG_RSP FSM registers
      r_config_rsp_fsm("r_config_rsp_fsm"),
      r_config_rsp_rsrcid("r_config_rsp_rsrcid"),
      r_config_rsp_rtrdid("r_config_rsp_rtrdid"),

      // TLB FSM registers
      r_tlb_fsm("r_tlb_fsm"),
      r_waiting_transaction("r_waiting_transaction"),
      r_tlb_miss_type("r_tlb_miss_type"),
      r_tlb_miss_error("r_tlb_miss_error"),

      r_tlb_paddr("r_tlb_paddr"),
      r_tlb_pte_flags("r_tlb_pte_flags"),
      r_tlb_pte_ppn("r_tlb_pte_ppn"),
      r_tlb_way("r_tlb_way"),
      r_tlb_set("r_tlb_set"),

      r_tlb_buf_valid("r_tlb_buf_valid"),
      r_tlb_buf_tag("r_tlb_buf_tag"),
      r_tlb_buf_vaddr("r_tlb_buf_vaddr"),
      r_tlb_buf_big_page("r_tlb_buf_big_page"),

      r_tlb_to_miss_wti_cmd_req("r_tlb_to_miss_wti_cmd_req"),

      // MISS_WTI_RSP FSM registers
      r_miss_wti_rsp_fsm("r_miss_wti_rsp_fsm"),
      r_miss_wti_rsp_error_wti("r_miss_wti_rsp_error_wti"),
      r_miss_wti_rsp_error_miss("r_miss_wti_rsp_error_miss"),
      r_miss_wti_rsp_count("r_miss_wti_rsp_count"),

      r_miss_wti_rsp_to_dma_rsp_req("r_miss_wti_rsp_to_dma_rsp_req"),
      r_miss_wti_rsp_to_dma_rsp_rerror("r_miss_wti_rsp_to_dma_rsp_rerror"),
      r_miss_wti_rsp_to_dma_rsp_rsrcid("r_miss_wti_rsp_to_dma_rsp_rsrcid"),
      r_miss_wti_rsp_to_dma_rsp_rtrdid("r_miss_wti_rsp_to_dma_rsp_rtrdid"),
      r_miss_wti_rsp_to_dma_rsp_rpktid("r_miss_wti_rsp_to_dma_rsp_rpktid"),

      r_miss_wti_rsp_to_tlb_done("r_miss_wti_rsp_to_tlb_done"),

      // TLB for IOMMU
      r_iotlb("iotlb", 0, iotlb_ways, iotlb_sets, vci_param_int::N),

      // DMA_CMD FIFOs
      m_dma_cmd_addr_fifo("m_dma_cmd_addr_fifo",2),
      m_dma_cmd_srcid_fifo("m_dma_cmd_srcid_fifo",2),
      m_dma_cmd_trdid_fifo("m_dma_cmd_trdid_fifo",2),
      m_dma_cmd_pktid_fifo("m_dma_cmd_pktid_fifo",2),
      m_dma_cmd_be_fifo("m_dma_cmd_be_fifo",2),
      m_dma_cmd_cmd_fifo("m_dma_cmd_cmd_fifo",2),
      m_dma_cmd_contig_fifo("m_dma_cmd_contig_fifo",2),
      m_dma_cmd_data_fifo("m_dma_cmd_data_fifo",2),
      m_dma_cmd_eop_fifo("m_dma_cmd_eop_fifo",2),
      m_dma_cmd_cons_fifo("m_dma_cmd_cons_fifo",2),
      m_dma_cmd_plen_fifo("m_dma_cmd_plen_fifo",2),
      m_dma_cmd_wrap_fifo("m_dma_cmd_wrap_fifo",2),
      m_dma_cmd_cfixed_fifo("m_dma_cmd_cfixed_fifo",2),
      m_dma_cmd_clen_fifo("m_dma_cmd_clen_fifo",2),

      // DMA_RSP FIFOs
      m_dma_rsp_data_fifo("m_dma_rsp_data_fifo",2),
      m_dma_rsp_rsrcid_fifo("m_dma_rsp_rsrcid_fifo",2),
      m_dma_rsp_rtrdid_fifo("m_dma_rsp_rtrdid_fifo",2),
      m_dma_rsp_rpktid_fifo("m_dma_rsp_rpktid_fifo",2),
      m_dma_rsp_reop_fifo("m_dma_rsp_reop_fifo",2),
      m_dma_rsp_rerror_fifo("m_dma_rsp_rerror_fifo",2),

      // CONFIG_CMD FIFOs
      m_config_cmd_addr_fifo("m_config_cmd_addr_fifo",2),
      m_config_cmd_srcid_fifo("m_config_cmd_srcid_fifo",2),
      m_config_cmd_trdid_fifo("m_config_cmd_trdid_fifo",2),
      m_config_cmd_pktid_fifo("m_config_cmd_pktid_fifo",2),
      m_config_cmd_be_fifo("m_config_cmd_be_fifo",2),
      m_config_cmd_cmd_fifo("m_config_cmd_cmd_fifo",2),
      m_config_cmd_contig_fifo("m_config_cmd_contig_fifo",2),
      m_config_cmd_data_fifo("m_config_cmd_data_fifo",2),
      m_config_cmd_eop_fifo("m_config_cmd_eop_fifo",2),
      m_config_cmd_cons_fifo("m_config_cmd_cons_fifo",2),
      m_config_cmd_plen_fifo("m_config_cmd_plen_fifo",2),
      m_config_cmd_wrap_fifo("m_config_cmd_wrap_fifo",2),
      m_config_cmd_cfixed_fifo("m_config_cmd_cfixed_fifo",2),
      m_config_cmd_clen_fifo("m_config_cmd_clen_fifo",2),

      // CONFIG_RSP FIFOs
      m_config_rsp_data_fifo("m_config_rsp_data_fifo",2),
      m_config_rsp_rsrcid_fifo("m_config_rsp_rsrcid_fifo",2),
      m_config_rsp_rtrdid_fifo("m_config_rsp_rtrdid_fifo",2),
      m_config_rsp_rpktid_fifo("m_config_rsp_rpktid_fifo",2),
      m_config_rsp_reop_fifo("m_config_rsp_reop_fifo",2),
      m_config_rsp_rerror_fifo("m_config_rsp_rerror_fifo",2),

      // MISS_WTI_CMD FIFOs
      m_miss_wti_cmd_addr_fifo("m_miss_wti_cmd_addr_fifo",2),
      m_miss_wti_cmd_srcid_fifo("m_miss_wti_cmd_srcid_fifo",2),
      m_miss_wti_cmd_trdid_fifo("m_miss_wti_cmd_trdid_fifo",2),
      m_miss_wti_cmd_pktid_fifo("m_miss_wti_cmd_pktid_fifo",2),
      m_miss_wti_cmd_be_fifo("m_miss_wti_cmd_be_fifo",2),
      m_miss_wti_cmd_cmd_fifo("m_miss_wti_cmd_cmd_fifo",2),
      m_miss_wti_cmd_contig_fifo("m_miss_wti_cmd_contig_fifo",2),
      m_miss_wti_cmd_data_fifo("m_miss_wti_cmd_data_fifo",2),
      m_miss_wti_cmd_eop_fifo("m_miss_wti_cmd_eop_fifo",2),
      m_miss_wti_cmd_cons_fifo("m_miss_wti_cmd_cons_fifo",2),
      m_miss_wti_cmd_plen_fifo("m_miss_wti_cmd_plen_fifo",2),
      m_miss_wti_cmd_wrap_fifo("m_miss_wti_cmd_wrap_fifo",2),
      m_miss_wti_cmd_cfixed_fifo("m_miss_wti_cmd_cfixed_fifo",2),
      m_miss_wti_cmd_clen_fifo("m_miss_wti_cmd_clen_fifo",2)
{
    std::cout << "  - Building VciIoBridge : " << name << std::endl;

    // checking segments on INT network
    assert ( ( not m_int_seglist.empty() ) and
    "VCI_IO_BRIDGE ERROR : no segment allocated on INT network");

    std::list<soclib::common::Segment>::iterator int_seg;
    for ( int_seg = m_int_seglist.begin() ; int_seg != m_int_seglist.end() ; int_seg++ )
    {
        std::cout << "    => segment " << int_seg->name()
                  << " / base = " << std::hex << int_seg->baseAddress()
                  << " / size = " << int_seg->size()
                  << " / special = " << int_seg->special() << std::endl;
    }

    // checking segments on IOX network
    assert ( ( not m_iox_seglist.empty() ) and
    "VCI_IO_BRIDGE ERROR : no segment allocated on IOX network");

    std::list<soclib::common::Segment>::iterator iox_seg;
    for ( iox_seg = m_iox_seglist.begin() ; iox_seg != m_iox_seglist.end() ; iox_seg++ )
    {
        std::cout << "    => segment " << iox_seg->name()
                  << " / base = " << std::hex << iox_seg->baseAddress()
                  << " / size = " << iox_seg->size() << std::endl;
    }

    assert( (vci_param_int::N == vci_param_ext::N) and
    "VCI_IO_BRIDGE ERROR: VCI ADDRESS widths must be equal on the 3 networks");

    assert( (vci_param_int::N <=  64) and
    "VCI_IO_BRIDGE ERROR: VCI ADDRESS width cannot be bigger than 64 bits");

    assert( (vci_param_int::B == 4) and
    "VCI_IO_BRIDGE ERROR: VCI DATA width must be 32 bits on internal network");

    assert( (vci_param_ext::B == 8) and
    "VCI_IO_BRIDGE ERROR: VCI DATA width must be 64 bits on external network");

    assert( (vci_param_int::S == vci_param_ext::S) and
            "VCI_IO_BRIDGE ERROR: SRCID widths must be equal on the 3 networks");

    // Cache line buffer
    r_tlb_buf_data = new uint32_t[dcache_words];

    SC_METHOD(transition);
    dont_initialize();
    sensitive << p_clk.pos();

    SC_METHOD(genMoore);
    dont_initialize();
    sensitive << p_clk.neg();

 }

/////////////////////////////////////
tmpl(/**/)::~VciIoBridge()
/////////////////////////////////////
{
    delete [] r_tlb_buf_data;
}

////////////////////////////////////
tmpl(void)::print_trace(size_t mode)
////////////////////////////////////
{
    // b0 : IOTLB trace

    std::cout << std::dec << "IO_BRIDGE " << name() << std::endl;

    std::cout << "  "  << dma_cmd_fsm_state_str[r_dma_cmd_fsm.read()]
              << " | " << dma_rsp_fsm_state_str[r_dma_rsp_fsm.read()]
              << " | " << tlb_fsm_state_str[r_tlb_fsm.read()]
              << " | " << config_cmd_fsm_state_str[r_config_cmd_fsm.read()]
              << " | " << config_rsp_fsm_state_str[r_config_rsp_fsm.read()]
              << " | " << miss_wti_rsp_state_str[r_miss_wti_rsp_fsm.read()]
              << std::endl;

    if(mode & 0x01)
    {
        std::cout << "  IOTLB" << std::endl;
        r_iotlb.printTrace();
    }

    if(mode & 0x02)
    {
        std::cout << "  IOX TRANSACTION TAB" << std::endl;
        m_iox_transaction_tab.printTrace();
    }
}

////////////////////////
tmpl(void)::print_stats()
////////////////////////
{
    std::cout << name()
        << "\n- IOTLB MISS RATE                                = "
        << (float)m_cpt_iotlb_miss/m_cpt_iotlb_read
        << "\n- IOTLB MISS COST                                = "
        << (float)m_cost_iotlb_miss/m_cpt_iotlb_miss
        << "\n- IOTLB MISS TRANSACTION COST                    = "
        << (float)m_cost_iotlbmiss_transaction/m_cpt_iotlbmiss_transaction
        << "\n- IOTLB MISS TRANSACTION RATE (OVER ALL MISSES)  = "
        << (float)m_cpt_iotlbmiss_transaction/m_cpt_iotlb_miss
        << std::endl;
}

////////////////////////
tmpl(void)::clear_stats()
////////////////////////
{
    m_cpt_iotlb_read                = 0;
    m_cpt_iotlb_miss                = 0;
    m_cost_iotlb_miss               = 0;
    m_cpt_iotlbmiss_transaction     = 0;
    m_cost_iotlbmiss_transaction    = 0;
}

////////////////////////////////////
tmpl(bool)::is_wti(vci_addr_t paddr)
////////////////////////////////////
{
    std::list<soclib::common::Segment>::iterator seg;
    for ( seg  = m_iox_seglist.begin() ;
          seg != m_iox_seglist.end()   ;
          seg++ )
    {
        if ( seg->contains(paddr) ) return seg->special();
    }
    return false;
}

/////////////////////////
tmpl(void)::transition()
/////////////////////////
{
    if ( not p_resetn.read() )
    {
        r_dma_cmd_fsm      = DMA_CMD_IDLE;
        r_dma_rsp_fsm      = DMA_RSP_IDLE_DMA;
        r_tlb_fsm          = TLB_IDLE;
        r_config_cmd_fsm   = CONFIG_CMD_IDLE;
        r_config_rsp_fsm   = CONFIG_RSP_IDLE_IOX;
        r_miss_wti_rsp_fsm = MISS_WTI_RSP_IDLE;

        r_tlb_buf_valid    = false;
        r_iommu_active     = false;
        r_iommu_wti_enable = false;

        // initializing translation table
        m_iox_transaction_tab.init();

        // initializing FIFOs
        m_dma_cmd_addr_fifo.init();
        m_dma_cmd_srcid_fifo.init();
        m_dma_cmd_trdid_fifo.init();
        m_dma_cmd_pktid_fifo.init();
        m_dma_cmd_be_fifo.init();
        m_dma_cmd_cmd_fifo.init();
        m_dma_cmd_contig_fifo.init();
        m_dma_cmd_data_fifo.init();
        m_dma_cmd_eop_fifo.init();
        m_dma_cmd_cons_fifo.init();
        m_dma_cmd_plen_fifo.init();
        m_dma_cmd_wrap_fifo.init();
        m_dma_cmd_cfixed_fifo.init();
        m_dma_cmd_clen_fifo.init();

        m_dma_rsp_rsrcid_fifo.init();
        m_dma_rsp_rtrdid_fifo.init();
        m_dma_rsp_rpktid_fifo.init();
        m_dma_rsp_data_fifo.init();
        m_dma_rsp_rerror_fifo.init();
        m_dma_rsp_reop_fifo.init();

        m_config_cmd_addr_fifo.init();
        m_config_cmd_srcid_fifo.init();
        m_config_cmd_trdid_fifo.init();
        m_config_cmd_pktid_fifo.init();
        m_config_cmd_be_fifo.init();
        m_config_cmd_cmd_fifo.init();
        m_config_cmd_contig_fifo.init();
        m_config_cmd_data_fifo.init();
        m_config_cmd_eop_fifo.init();
        m_config_cmd_cons_fifo.init();
        m_config_cmd_plen_fifo.init();
        m_config_cmd_wrap_fifo.init();
        m_config_cmd_cfixed_fifo.init();
        m_config_cmd_clen_fifo.init();

        m_miss_wti_cmd_addr_fifo.init();
        m_miss_wti_cmd_srcid_fifo.init();
        m_miss_wti_cmd_trdid_fifo.init();
        m_miss_wti_cmd_pktid_fifo.init();
        m_miss_wti_cmd_be_fifo.init();
        m_miss_wti_cmd_cmd_fifo.init();
        m_miss_wti_cmd_contig_fifo.init();
        m_miss_wti_cmd_data_fifo.init();
        m_miss_wti_cmd_eop_fifo.init();
        m_miss_wti_cmd_cons_fifo.init();
        m_miss_wti_cmd_plen_fifo.init();
        m_miss_wti_cmd_wrap_fifo.init();
        m_miss_wti_cmd_cfixed_fifo.init();
        m_miss_wti_cmd_clen_fifo.init();

        m_config_rsp_rsrcid_fifo.init();
        m_config_rsp_rtrdid_fifo.init();
        m_config_rsp_rpktid_fifo.init();
        m_config_rsp_data_fifo.init();
        m_config_rsp_rerror_fifo.init();
        m_config_rsp_reop_fifo.init();

        // SET/RESET Communication flip-flops
        r_dma_cmd_to_miss_wti_cmd_req  = false;
        r_dma_cmd_to_dma_rsp_req       = false;
        r_dma_cmd_to_tlb_req           = false;
        r_config_cmd_to_tlb_req        = false;
        r_config_cmd_to_config_rsp_req = false;
        r_tlb_to_miss_wti_cmd_req      = false;
        r_miss_wti_rsp_to_dma_rsp_req  = false;
        r_miss_wti_rsp_to_tlb_done     = false;

        // error flip_flops
        r_miss_wti_rsp_error_miss      = false;
        r_miss_wti_rsp_error_wti       = false;

        // Debug variable
        m_debug_activated              = false;

        // activity counters
        m_cpt_total_cycles             = 0;
        m_cpt_iotlb_read               = 0;
        m_cpt_iotlb_miss               = 0;
        m_cpt_iotlbmiss_transaction    = 0;
        m_cost_iotlbmiss_transaction   = 0;

        m_cpt_trt_dma_full             = 0;
        m_cpt_trt_dma_full_cost        = 0;
        m_cpt_trt_config_full          = 0;
        m_cpt_trt_config_full_cost     = 0;

        for (uint32_t i=0; i<32 ; ++i) m_cpt_fsm_dma_cmd            [i]   = 0;
        for (uint32_t i=0; i<32 ; ++i) m_cpt_fsm_dma_rsp            [i]   = 0;
        for (uint32_t i=0; i<32 ; ++i) m_cpt_fsm_tlb                [i]   = 0;
        for (uint32_t i=0; i<32 ; ++i) m_cpt_fsm_config_cmd         [i]   = 0;
        for (uint32_t i=0; i<32 ; ++i) m_cpt_fsm_config_rsp         [i]   = 0;
        for (uint32_t i=0; i<32 ; ++i) m_cpt_fsm_miss_wti_rsp       [i]   = 0;

        return;
    }

    // default values for the 5 FIFOs
    bool            dma_cmd_fifo_put          = false;
    bool            dma_cmd_fifo_get          = p_vci_ini_ram.cmdack.read();
    vci_srcid_t     dma_cmd_fifo_srcid        = 0;

    bool            dma_rsp_fifo_put          = false;
    bool            dma_rsp_fifo_get          = p_vci_tgt_iox.rspack.read();
    vci_rerror_t    dma_rsp_fifo_rerror       = 0;
    vci_srcid_t     dma_rsp_fifo_rsrcid       = 0;
    vci_trdid_t     dma_rsp_fifo_rtrdid       = 0;
    vci_pktid_t     dma_rsp_fifo_rpktid       = 0;
    ext_data_t      dma_rsp_fifo_rdata        = 0;
    bool            dma_rsp_fifo_reop         = false;

    bool            config_cmd_fifo_put       = false;
    bool            config_cmd_fifo_get       = p_vci_ini_iox.cmdack.read();

    bool            config_rsp_fifo_put       = false;
    bool            config_rsp_fifo_get       = p_vci_tgt_int.rspack.read();
    vci_rerror_t    config_rsp_fifo_rerror    = 0;
    vci_srcid_t     config_rsp_fifo_rsrcid    = 0;
    vci_trdid_t     config_rsp_fifo_rtrdid    = 0;
    vci_pktid_t     config_rsp_fifo_rpktid    = 0;
    ext_data_t      config_rsp_fifo_rdata     = 0;
    bool            config_rsp_fifo_reop      = false;

    bool            miss_wti_cmd_fifo_put     = false;
    bool            miss_wti_cmd_fifo_get     = p_vci_ini_int.cmdack.read();
    vci_addr_t      miss_wti_cmd_fifo_address = 0;
    vci_cmd_t       miss_wti_cmd_fifo_cmd     = 0;
    vci_srcid_t     miss_wti_cmd_fifo_srcid   = 0;
    vci_trdid_t     miss_wti_cmd_fifo_trdid   = 0;
    vci_pktid_t     miss_wti_cmd_fifo_pktid   = 0;
    int_data_t      miss_wti_cmd_fifo_wdata   = 0;
    vci_plen_t      miss_wti_cmd_fifo_plen    = 0;

#ifdef INSTRUMENTATION
    m_cpt_fsm_dma_cmd           [r_dma_cmd_fsm.read()] ++;
    m_cpt_fsm_dma_rsp           [r_dma_rsp_fsm.read() ] ++;
    m_cpt_fsm_tlb               [r_tlb_fsm.read() ] ++;
    m_cpt_fsm_config_cmd        [r_config_cmd_fsm.read() ] ++;
    m_cpt_fsm_config_rsp        [r_config_rsp_fsm.read() ] ++;
    m_cpt_fsm_miss_wti_rsp      [r_miss_wti_rsp_fsm.read() ] ++;
#endif

    m_cpt_total_cycles++;

    m_debug_activated  = (m_cpt_total_cycles > m_debug_start_cycle) and m_debug_ok;

    //////////////////////////////////////////////////////////////////////////////
    // The DMA_CMD_FSM handles transactions requested by peripherals.
    // - it can be DMA transactions to RAM network (DMA_REQ state).
    // - it can be WTI transactions to INT network (EXT_WTI_REQ state).
    // It makes the address translation if IOMMU is activated, requesting
    // the TLB_MISS FSM in case of TLB miss (TLB_MISS_WAIT state).
    // When the IOMMU is activated, a DMA request can fail in two cases:
    // - write to a read-only address : detected in IDLE state
    // - virtual address unmapped     : detected in MISS_WAIT state
    // In both cases of violation, the DMA_CMD FSM makes the following actions :
    // 1. register the error in r_iommu_*** registers
    // 2. wait the faulty command EOP (ERR_WAIT_EOP state)
    // 3. request a IOMMU WTI to MISS_WTI FSM, (ERR_WTI_REQ state)
    // 4. request a response error to DMA_RSP FSM (ERR_RSP_REQ state)
    ///////////////////////////////////////////////////////////////////////////////

    switch( r_dma_cmd_fsm.read() )
    {
    //////////////////
    case DMA_CMD_IDLE:  // wait a DMA or WTI VCI transaction and route it
                        // after an IOMMU translation if IOMMU activated.
                        // no VCI flit is consumed in this state
    {
        if ( p_vci_tgt_iox.cmdval.read() )
        {

#if DEBUG_DMA_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_CMD_IDLE> DMA command"
          << " : address = " << std::hex << p_vci_tgt_iox.address.read()
          << " / srcid = " << p_vci_tgt_iox.srcid.read()
          << " / wdata = " << std::hex << p_vci_tgt_iox.wdata.read()
          << " / plen = " << std::dec << p_vci_tgt_iox.plen.read()
          << " / eop = " << p_vci_tgt_iox.eop.read() << std::endl;
#endif

            if ( not r_iommu_active.read() )    // tlb not activated
            {
                // save paddr address
                r_dma_cmd_paddr = p_vci_tgt_iox.address.read();

                // analyse paddr for WTI/DMA routing
                // WTI requests must be single flit (READ or WRITE)
                if ( is_wti( p_vci_tgt_iox.address.read() ) )
                {
                    assert( p_vci_tgt_iox.eop.read() and
                    "ERROR in VCI_IOB illegal VCI WTI command from IOX network");

                    r_dma_cmd_fsm = DMA_CMD_WTI_IOX_REQ;
                }
                else
                {
                    r_dma_cmd_fsm = DMA_CMD_DMA_REQ;
                }

            }
            else if (r_tlb_fsm.read() == TLB_IDLE ||
                     r_tlb_fsm.read() == TLB_WAIT )   // tlb access possible
            {
                vci_addr_t  iotlb_paddr;
                pte_info_t  iotlb_flags;
                size_t      iotlb_way;
                size_t      iotlb_set;
                vci_addr_t  iotlb_nline;
                bool        iotlb_hit;

#ifdef INSTRUMENTATION
m_cpt_iotlb_read++;
#endif
                iotlb_hit = r_iotlb.translate(p_vci_tgt_iox.address.read(),
                                              &iotlb_paddr,
                                              &iotlb_flags,
                                              &iotlb_nline,  // unused
                                              &iotlb_way,    // unused
                                              &iotlb_set );  // unused

                if ( iotlb_hit )                                 // tlb hit
                {
                    if ( not iotlb_flags.w and    // access right violation
                        (p_vci_tgt_iox.cmd.read() == vci_param_ext::CMD_WRITE) )
                    {
                        // register error
                        r_iommu_etr      = MMU_WRITE_ACCES_VIOLATION;
                        r_iommu_bvar     = p_vci_tgt_iox.address.read();
                        r_iommu_bad_id   = p_vci_tgt_iox.srcid.read();

                        // prepare response error request to DMA_RSP FSM
                        r_dma_cmd_to_dma_rsp_rsrcid = p_vci_tgt_iox.srcid.read();
                        r_dma_cmd_to_dma_rsp_rtrdid = p_vci_tgt_iox.trdid.read();
                        r_dma_cmd_to_dma_rsp_rpktid = p_vci_tgt_iox.pktid.read();

                        // jumps IOMMU error sequence
                        r_dma_cmd_fsm = DMA_CMD_ERR_WAIT_EOP;
#if DEBUG_DMA_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_CMD_IDLE> TLB HIT but writable violation" << std::endl;
#endif
                    }
                    else                         // no access rights violation
                    {
#if DEBUG_DMA_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_CMD_IDLE> TLB HIT" << std::endl;
#endif
                        // save paddr address
                        r_dma_cmd_paddr = iotlb_paddr;

                        // analyse address for WTI/DMA routing
                        if ( is_wti( iotlb_paddr ) )
                        {
                            assert( p_vci_tgt_iox.eop.read() &&
                            "ERROR in VCI_IOB illegal VCI WTI command from IOX network");

                            r_dma_cmd_fsm = DMA_CMD_WTI_IOX_REQ;
                        }
                        else
                        {
                            r_dma_cmd_fsm = DMA_CMD_DMA_REQ;
                        }
                    }
                }
                else                                             // TLB miss
                {

#ifdef INSTRUMENTATION
m_cpt_iotlb_miss++;
#endif
                    // register virtual address, and send request to TLB FSM
                    r_dma_cmd_to_tlb_vaddr = p_vci_tgt_iox.address.read();
                    r_dma_cmd_to_tlb_req   = true;
                    r_dma_cmd_fsm          = DMA_CMD_TLB_MISS_WAIT;
#if DEBUG_DMA_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_CMD_IDLE> TLB MISS" << std::endl;
#endif
                } // end tlb miss
            } // end if tlb_activated
        } // end if cmdval
        break;
    }
    /////////////////////
    case DMA_CMD_DMA_REQ:    // put a flit in DMA_CMD FIFO
                             // if contig, VCI address must be incremented
                             // after initial translation by IOMMU.
                             // flit is consumed if DMA_CMD FIFO not full
    {
        if ( p_vci_tgt_iox.cmdval && m_dma_cmd_addr_fifo.wok() )
        {
            // SRCID in RAM network is the concatenation of the IO bridge
            // cluster id with the DMA peripheral local id
            assert((m_srcid_gid_mask[p_vci_tgt_iox.srcid.read()] == 0) &&
                    "error: external DMA peripherals global id must be 0");

            dma_cmd_fifo_srcid = (m_srcid_gid_mask.mask() & m_int_srcid) |
                                 p_vci_tgt_iox.srcid.read();
            dma_cmd_fifo_put   = true;

            if ( p_vci_tgt_iox.contig.read() )
            {
                r_dma_cmd_paddr = r_dma_cmd_paddr.read() + vci_param_ext::B;
            }

            if ( p_vci_tgt_iox.eop.read() )
            {
                r_dma_cmd_fsm = DMA_CMD_IDLE;
            }

#if DEBUG_DMA_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_CMD_FIFO_PUT_CMD> Push into DMA_CMD fifo:"
          << " address = " << std::hex << r_dma_cmd_paddr.read()
          << " srcid = " << dma_cmd_fifo_srcid
          << " wdata = " << p_vci_tgt_iox.wdata.read()
          << " plen = " << std::dec << p_vci_tgt_iox.plen.read()
          << " eop = " << std::dec << p_vci_tgt_iox.eop.read() << std::endl;
#endif
        }
        break;
    }
    /////////////////////////
    case DMA_CMD_WTI_IOX_REQ:    // post a WTI_IOX request to MISS_WTI FSM
                                 // if no prending previous request
                                 // command arguments are stored in dedicated registers
                                 // VCI flit is consumed if no previous request
    {
        if ( not r_dma_cmd_to_miss_wti_cmd_req.read() )  // no previous pending request
        {
            // SRCID in INT network for WTI transactions is the concatenation
            // of the IO bridge cluster id with the DMA peripheral local id
            assert((m_srcid_gid_mask[p_vci_tgt_iox.srcid.read()] == 0) &&
                    "error: external DMA peripherals global id must be 0");

            vci_srcid_t wti_srcid = (m_srcid_gid_mask.mask() & m_int_srcid) | 
                                    p_vci_tgt_iox.srcid.read();

            r_dma_cmd_to_miss_wti_cmd_req   = true;
            r_dma_cmd_to_miss_wti_cmd_addr  = p_vci_tgt_iox.address.read();
            r_dma_cmd_to_miss_wti_cmd_cmd   = p_vci_tgt_iox.cmd.read();
            r_dma_cmd_to_miss_wti_cmd_wdata = (uint32_t)p_vci_tgt_iox.wdata.read();
            r_dma_cmd_to_miss_wti_cmd_srcid = wti_srcid;
            r_dma_cmd_to_miss_wti_cmd_trdid = p_vci_tgt_iox.trdid.read();
            r_dma_cmd_to_miss_wti_cmd_pktid = PKTID_WTI_IOX;

            r_dma_cmd_fsm = DMA_CMD_IDLE;

#if DEBUG_DMA_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_CMD_WTI_IOX_REQ> request WTI transaction from ext peripheral"
          << " : address = " << std::hex << r_dma_cmd_paddr.read()
          << " / srcid = " << wti_srcid
          << " / wdata = " << p_vci_tgt_iox.wdata.read() << std::endl;
#endif
        }
        break;
    }
    //////////////////////////
    case DMA_CMD_ERR_WAIT_EOP:   // wait EOP before requesting WTI & error response
                                 // VCI flit is always consumed
    {
        if ( p_vci_tgt_iox.eop.read() ) r_dma_cmd_fsm = DMA_CMD_ERR_WTI_REQ;

#if DEBUG_DMA_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_CMD_WAIT_EOP> wait EOP for faulty DMA command" << std::endl;
#endif
        break;
    }

    /////////////////////////
    case DMA_CMD_ERR_WTI_REQ:    // post a WTI_MMU request to MISS_WTI_CMD FSM
                                 // if no prending previous request
                                 // response arguments are stored in dedicated registers
                                 // no VCI flit is consumed
    {
        if ( not r_dma_cmd_to_miss_wti_cmd_req.read() )  // no pending previous request
        {
            r_dma_cmd_to_miss_wti_cmd_req   = true;
            r_dma_cmd_to_miss_wti_cmd_addr  = (vci_addr_t)r_iommu_wti_addr_lo.read() |
                                              (((vci_addr_t)r_iommu_wti_addr_hi.read())<<32);
            r_dma_cmd_to_miss_wti_cmd_wdata = 0;
            r_dma_cmd_to_miss_wti_cmd_srcid = m_int_srcid;
            r_dma_cmd_to_miss_wti_cmd_trdid = 0;
            r_dma_cmd_to_miss_wti_cmd_pktid = PKTID_WTI_MMU;

            r_dma_cmd_fsm            = DMA_CMD_ERR_RSP_REQ;

#if DEBUG_DMA_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_CMD_ERR_WTI_REQ> request an IOMMU WTI" << std::endl;
#endif
        }
        break;
    }
    /////////////////////////
    case DMA_CMD_ERR_RSP_REQ:    // post an error response request to DMA_RSP FSM
                                 // if no prending previous request
                                 // response arguments are stored in dedicated registers
                                 // no VCI flit is consumed
    {
        if ( not r_dma_cmd_to_dma_rsp_req.read() )  // no pending previous request
        {
            r_dma_cmd_to_dma_rsp_req    = true;
            r_dma_cmd_to_dma_rsp_rerror = 0x1;
            r_dma_cmd_to_dma_rsp_rdata  = 0;
        }
        break;
    }
    ///////////////////////////
    case DMA_CMD_TLB_MISS_WAIT:  // waiting completion of a TLB miss
                                 // we must test a possible page fault error...
    {
        if ( not r_dma_cmd_to_tlb_req.read() ) // TLB miss completed
        {
            if ( r_tlb_miss_error.read() )   // Error reported by TLB FSM
            {
                r_iommu_etr     = MMU_READ_PT2_UNMAPPED;
                r_iommu_bvar    = r_dma_cmd_to_tlb_vaddr.read();
                r_iommu_bad_id  = p_vci_tgt_iox.srcid.read();
                r_dma_cmd_fsm   = DMA_CMD_ERR_WAIT_EOP;
            }
            else                            // No error
            {
                r_dma_cmd_fsm   = DMA_CMD_IDLE;
            }
        }
        break;
    }
    } // end switch DMA_CMD FSM

    ////////////////////////////////////////////////////////////////////////////////
    // The DMA_RSP_FSM controls access to the DMA_RSP FIFO to the IOX network.
    // There exist 3 "clients" to send VCI responses on the IOX network:
    // - request from p_vci_ini_ram    : normal DMA response from RAM network,
    // - request from MISS_WTI_RSP FSM : normal WTI response from INT network,
    // - request from DMA_CMD FSM      : bad address error response
    // This FSM implements a round robin priority, with a "dead cycle" between
    // two transactions. It could be optimized if throughput is critical...
    ////////////////////////////////////////////////////////////////////////////////

    // does nothing if FIFO is full
    if ( m_dma_rsp_rerror_fifo.wok() )
    {
        switch( r_dma_rsp_fsm.read() )
        {
            //////////////////////
            case DMA_RSP_IDLE_DMA:  // normal DMA response has highest priority
            {
                if     (p_vci_ini_ram.rspval.read())          r_dma_rsp_fsm = DMA_RSP_PUT_DMA;
                else if(r_miss_wti_rsp_to_dma_rsp_req.read()) r_dma_rsp_fsm = DMA_RSP_PUT_WTI;
                else if(r_dma_cmd_to_dma_rsp_req.read())      r_dma_rsp_fsm = DMA_RSP_PUT_ERR;
                break;
            }
            //////////////////////
            case DMA_RSP_IDLE_WTI:  // normal WTI response has highest priority
            {
                if     (r_miss_wti_rsp_to_dma_rsp_req.read()) r_dma_rsp_fsm = DMA_RSP_PUT_WTI;
                else if(r_dma_cmd_to_dma_rsp_req.read())      r_dma_rsp_fsm = DMA_RSP_PUT_ERR;
                else if(p_vci_ini_ram.rspval.read())          r_dma_rsp_fsm = DMA_RSP_PUT_DMA;
                break;
            }
            //////////////////////
            case DMA_RSP_IDLE_ERR:  // error  response has highest priority
            {
                if     (r_dma_cmd_to_dma_rsp_req.read())      r_dma_rsp_fsm = DMA_RSP_PUT_ERR;
                else if(p_vci_ini_ram.rspval.read())          r_dma_rsp_fsm = DMA_RSP_PUT_DMA;
                else if(r_miss_wti_rsp_to_dma_rsp_req.read()) r_dma_rsp_fsm = DMA_RSP_PUT_WTI;
                break;
            }
            ///////////////////////
            case DMA_RSP_PUT_DMA:  // put one flit of the DMA response into FIFO
            {
                dma_rsp_fifo_put    = true;
                dma_rsp_fifo_rerror = p_vci_ini_ram.rerror.read();
                dma_rsp_fifo_rdata  = p_vci_ini_ram.rdata.read();
                dma_rsp_fifo_rsrcid = m_srcid_lid_mask[p_vci_ini_ram.rsrcid.read()];
                dma_rsp_fifo_rtrdid = p_vci_ini_ram.rtrdid.read();
                dma_rsp_fifo_rpktid = p_vci_ini_ram.rpktid.read();
                dma_rsp_fifo_reop   = p_vci_ini_ram.reop.read();

                // update priority
                if ( p_vci_ini_ram.reop.read() ) r_dma_rsp_fsm = DMA_RSP_IDLE_WTI;

#if DEBUG_DMA_RSP
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_RSP_PUT_DMA> Push DMA response into DMA_RSP FIFO"
          << std::hex
          << " : rsrcid = " << m_srcid_lid_mask[p_vci_ini_ram.rsrcid.read()]
          << " / rtrdid = " << p_vci_ini_ram.rtrdid.read()
          << " / rpktid = " << p_vci_ini_ram.rpktid.read()
          << " / rdata = "  << p_vci_ini_ram.rdata.read()
          << " / rerror = " << p_vci_ini_ram.rerror.read()
          << " / reop = "   << p_vci_ini_ram.reop.read() << std::endl;
#endif
                break;
            }
            ///////////////////////
            case DMA_RSP_PUT_WTI:  // put single flit WTI response into FIFO
            {
                dma_rsp_fifo_put    = true;
                dma_rsp_fifo_rerror = r_miss_wti_rsp_to_dma_rsp_rerror.read();
                dma_rsp_fifo_rdata  = 0;
                dma_rsp_fifo_rsrcid = 
                    m_srcid_lid_mask[r_miss_wti_rsp_to_dma_rsp_rsrcid.read()];
                dma_rsp_fifo_rtrdid = r_miss_wti_rsp_to_dma_rsp_rtrdid.read();
                dma_rsp_fifo_rpktid = r_miss_wti_rsp_to_dma_rsp_rpktid.read();
                dma_rsp_fifo_reop   = true;

                // acknowledge request
                r_miss_wti_rsp_to_dma_rsp_req = false;

                // update priority
                r_dma_rsp_fsm = DMA_RSP_IDLE_ERR;

#if DEBUG_DMA_RSP
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_RSP_PUT_WTI> Push WTI response into DMA_RSP FIFO"
          << std::hex
          << " : rsrcid = " << m_srcid_lid_mask[r_miss_wti_rsp_to_dma_rsp_rsrcid.read()]
          << " / rtrdid = " << r_miss_wti_rsp_to_dma_rsp_rtrdid.read()
          << " / rpktid = " << r_miss_wti_rsp_to_dma_rsp_rpktid.read()
          << " / rdata = "  << 0
          << " / rerror = " << r_miss_wti_rsp_to_dma_rsp_rerror.read()
          << " / reop = "   << true << std::endl;
#endif
                break;
            }
            ///////////////////////
            case DMA_RSP_PUT_ERR:  // put sinfle flit error response into FIFO
            {
                dma_rsp_fifo_put    = true;
                dma_rsp_fifo_rerror = 0x1;
                dma_rsp_fifo_rdata  = 0;
                dma_rsp_fifo_rsrcid = r_dma_cmd_to_dma_rsp_rsrcid.read();
                dma_rsp_fifo_rtrdid = r_dma_cmd_to_dma_rsp_rtrdid.read();
                dma_rsp_fifo_rpktid = r_dma_cmd_to_dma_rsp_rpktid.read();
                dma_rsp_fifo_reop   = true;

                // acknowledge request
                r_dma_cmd_to_dma_rsp_req = false;

                // update priority
                r_dma_rsp_fsm = DMA_RSP_PUT_DMA;

#if DEBUG_DMA_RSP
if( m_debug_activated )
std::cout << name()
          << "  <IOB DMA_RSP_PUT_DMA> Push IOMMU ERROR response into DMA_RSP FIFO"
          << " : rsrcid = " << std::hex << r_dma_cmd_to_dma_rsp_rsrcid.read()
          << " / rtrdid = " << r_dma_cmd_to_dma_rsp_rtrdid.read()
          << " / rpktid = " << r_dma_cmd_to_dma_rsp_rpktid.read()
          << " / rdata = "  << 0
          << " / rerror = " << r_dma_cmd_to_dma_rsp_rerror.read()
          << " / reop = "   << true << std::endl;
#endif
                break;
            }
        } // end switch DMA_RSP FSM
    }  // end if FIFO full


    //////////////////////////////////////////////////////////////////////////////////
    // The TLB FSM handles the TLB miss requests from DMA_CMD FSM,
    // and the PTE inval request (from CONFIG_CMD FSM).
    // PTE inval request have highest priority. In case of TLB miss,
    // this fsm searchs the requested PTE on the prefetch buffer.
    // In case of buffer miss,  it request the MISS_WTI FSM to access the memory.
    // It bypass the first level page table access if possible.
    // It reset the r_dma_cmd_to_tlb_req flip-flop to signal TLB miss completion.
    // An unexpected, but possible page fault is signaled in r_tlb_miss_error flip_flop.
    ////////////////////////////////////////////////////////////////////////////////////

    switch (r_tlb_fsm.read())
    {
    //////////////
    case TLB_IDLE:   // In case of TLB miss request, chek the prefetch buffer first
                     // PTE inval request are handled as unmaskable interrupts
    {
        if ( r_config_cmd_to_tlb_req.read() ) // Request for a PTE invalidation
        {
            r_config_cmd_to_tlb_req  = false;
            r_waiting_transaction    = false;
            r_tlb_fsm                = TLB_INVAL_CHECK;
        }

        else if ( r_dma_cmd_to_tlb_req.read() )   // request for a TLB Miss
        {
            // Checking prefetch buffer
            if( r_tlb_buf_valid.read() )
            {
                if ( !r_tlb_buf_big_page.read() &&  // Hit on prefetch buffer and small page => PTE2
                    (r_tlb_buf_vaddr.read() ==
                    (r_dma_cmd_to_tlb_vaddr.read() & ~PTE2_LINE_OFFSET & ~K_PAGE_OFFSET_MASK)))
                {
                    size_t   pte_offset = (r_dma_cmd_to_tlb_vaddr.read() & PTE2_LINE_OFFSET) >> 12;
                    uint32_t pte_flags  = r_tlb_buf_data[2*pte_offset];
                    uint32_t pte_ppn    = r_tlb_buf_data[2*pte_offset+1];

                    // Bit valid checking
                    if ( not ( pte_flags & PTE_V_MASK) )    // unmapped
                    {
                        std::cout << "VCI_IO_BRIDGE ERROR : " << name()
                                  << " Page Table entry unmapped" << std::endl;

                        r_tlb_miss_error = true;
                        r_dma_cmd_to_tlb_req    = false;
#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_IDLE> PTE2 Unmapped" << std::hex
          << " / paddr = " << r_tlb_paddr.read()
          << " / PTE_FLAGS = " << pte_flags
          << " / PTE_PPN = " << pte_ppn << std::endl;
#endif
                        break;
                    }

                    // valid PTE2 : we must update the TLB
                    r_tlb_pte_flags = pte_flags;
                    r_tlb_pte_ppn   = pte_ppn;
                    r_tlb_fsm       = TLB_PTE2_SELECT;
#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_IDLE> Hit on prefetch buffer: PTE2" << std::hex
          << " / PTE_FLAGS = " << pte_flags
          << " / PTE_PPN = " << pte_ppn << std::endl;
#endif
                    break;
                }

                if( r_tlb_buf_big_page.read() &&         // Hit on prefetch buffer and big page
                    (r_tlb_buf_vaddr.read() ==
                    (r_dma_cmd_to_tlb_vaddr.read() & ~PTE1_LINE_OFFSET & ~M_PAGE_OFFSET_MASK )))
                {
                    size_t   pte_offset = (r_dma_cmd_to_tlb_vaddr.read() & PTE1_LINE_OFFSET) >> 21;
                    uint32_t pte_flags  = r_tlb_buf_data[pte_offset];

                    // Bit valid checking
                    if ( not ( pte_flags & PTE_V_MASK) )    // unmapped
                    {
                        std::cout << "VCI_IO_BRIDGE ERROR : " << name()
                                  << " Page Table entry unmapped" << std::endl;

                        r_tlb_miss_error = true;
                        r_dma_cmd_to_tlb_req    = false;
#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_IDLE> PTE1 Unmapped" << std::hex
          << " / paddr = " << r_tlb_paddr.read()
          << " / PTE = " << pte_flags << std::endl;
#endif
                        break;
                    }

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_PTE1_GET> Hit on prefetch buffer: PTE1" << std::hex
          << " / paddr = " << r_tlb_paddr.read()
          << std::hex << " / PTE1 = " << pte_flags << std::endl;
#endif
                    // valid PTE1 : we must update the TLB
                    r_tlb_pte_flags = pte_flags;
                    r_tlb_fsm       = TLB_PTE1_SELECT;

                    break;
                }
            }

            // prefetch buffer miss
            r_tlb_fsm = TLB_MISS;

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_IDLE> Miss on prefetch buffer"
          << std::hex << " / vaddr = " << r_dma_cmd_to_tlb_vaddr.read() << std::endl;
#endif
        }

        break;
    }
    //////////////
    case TLB_MISS: // handling tlb miss
    {
        uint32_t    ptba = 0;
        bool        bypass;
        vci_addr_t  pte_paddr;

#ifdef INSTRUMENTATION
m_cpt_iotlbmiss_transaction++;
#endif
        // evaluate bypass in order to skip first level page table access
        bypass = r_iotlb.get_bypass(r_dma_cmd_to_tlb_vaddr.read(), &ptba);

        // Request MISS_WTI_FSM a transaction on INT Network
        if ( not bypass )     // Read PTE1/PTD1 in XRAM
        {
            pte_paddr =
                ((vci_addr_t)r_iommu_ptpr.read() << (INDEX1_NBITS+2)) |
                ((vci_addr_t)(r_dma_cmd_to_tlb_vaddr.read() >> PAGE_M_NBITS) << 2);
            r_tlb_paddr               = pte_paddr;
            r_tlb_to_miss_wti_cmd_req = true;
            r_tlb_miss_type           = PTE1_MISS;
            r_tlb_fsm                 = TLB_WAIT;

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_MISS> Read PTE1/PTD1 in memory: PADDR = " << std::hex
          << pte_paddr << std::dec << std::endl;
#endif
        }
        else                  // Read PTE2 in XRAM
        {

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_MISS> Read PTE2 in memory" << std::endl;
#endif
            //&PTE2 = PTBA + IX2 * 8
            pte_paddr =
                ((vci_addr_t)ptba << PAGE_K_NBITS) |
                ((vci_addr_t)(r_dma_cmd_to_tlb_vaddr.read()&PTD_ID2_MASK)>>(PAGE_K_NBITS-3));
            r_tlb_paddr               = pte_paddr;
            r_tlb_to_miss_wti_cmd_req = true;
            r_tlb_miss_type           = PTE2_MISS;
            r_tlb_fsm                 = TLB_WAIT;
        }

        break;
    }
    //////////////////
    case TLB_PTE1_GET:  // Try to read a PT1 entry in the miss buffer
    {

        uint32_t  entry;

        vci_addr_t line_number  = (vci_addr_t)((r_tlb_paddr.read())&(CACHE_LINE_MASK));
        size_t word_position = (size_t)( ((r_tlb_paddr.read())&(~CACHE_LINE_MASK))>>2 );

        // Hit test. Just to verify.
        // Hit must happen, since we've just finished its' miss transaction
        bool hit = (r_tlb_buf_valid && (r_tlb_buf_tag.read() == line_number) );
        assert(hit && "Error: No hit on prefetch buffer after Miss Transaction");

        entry = r_tlb_buf_data[word_position];

        // Bit valid checking
        if ( not ( entry & PTE_V_MASK) )    // unmapped
        {
            //must not occur!
            std::cout << "IOMMU ERROR " << name() << "TLB_IDLE state" << std::endl
                      << "The Page Table entry ins't valid (unmapped)" << std::endl;

            r_tlb_miss_error       = true;
            r_dma_cmd_to_tlb_req   = false;
            r_tlb_fsm              = TLB_IDLE;

#if DEBUG_TLB_MISS
if ( m_debug_activated )
{
std::cout << name()
          << "  <IOB DMA_PTE1_GET> First level entry Unmapped"
          << std::hex << " / paddr = " << r_tlb_paddr.read()
          << std::hex << " / PTE = " << entry << std::endl;
}
#endif
            break;
        }

        if( entry & PTE_T_MASK )    //  PTD : me must access PT2
        {
            // register bypass
            r_iotlb.set_bypass( r_dma_cmd_to_tlb_vaddr.read(),
                                entry & ((1 << (vci_param_int::N-PAGE_K_NBITS)) - 1),
                                0); //nline, unused

            // &PTE2 = PTBA + IX2 * 8
            // ps: PAGE_K_NBITS corresponds also to the size of a second level page table
            r_tlb_paddr =
                (((vci_addr_t)entry & ((1ULL<<(vci_param_int::N-PAGE_K_NBITS))-1)) << PAGE_K_NBITS) |
                (((vci_addr_t)(r_dma_cmd_to_tlb_vaddr.read() & PTD_ID2_MASK) >> PAGE_K_NBITS) << 3);

            r_tlb_to_miss_wti_cmd_req = true;
            r_tlb_miss_type           = PTE2_MISS;
            r_tlb_fsm                 = TLB_WAIT;

#ifdef INSTRUMENTATION
m_cpt_iotlbmiss_transaction++;
#endif

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_PTE1_GET> Success. Search PTE2" << std::hex
          << " / PADDR = " << r_tlb_paddr.read()
          << " / PTD = " << entry << std::endl;
#endif
        }
        else            //  PTE1 :  we must update the IOTLB
                        //  Should not occur if working only with small pages
        {
            r_tlb_pte_flags = entry;
            r_tlb_fsm       = TLB_PTE1_SELECT;

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_PTE1_GET> Success. Big page"
          << std::hex << " / paddr = " << r_tlb_paddr.read()
          << std::hex << " / PTE1 = " << entry << std::endl;
#endif
        }
        break;
    }
    /////////////////////
    case TLB_PTE1_SELECT:   // select a slot for PTE1
    {
        size_t  way;
        size_t  set;

        r_iotlb.select( r_dma_cmd_to_tlb_vaddr.read(),
                        true,  // PTE1
                        &way,
                        &set );
#ifdef INSTRUMENTATION
m_cpt_iotlb_read++;
#endif

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_PTE1_SELECT> Select a slot in TLB"
          << " / way = " << std::dec << way
          << " / set = " << set << std::endl;
#endif
        r_tlb_way = way;
        r_tlb_set = set;
        r_tlb_fsm = TLB_PTE1_UPDT;
        break;
    }
    ///////////////////
    case TLB_PTE1_UPDT:     // write a new PTE1 in tlb
                            // not necessary to treat the L/R bit
    {
        uint32_t pte = r_tlb_pte_flags.read();

        // update TLB
        r_iotlb.write( true,        // 2M page
                       pte,
                       0,        // argument unused for a PTE1
                       r_dma_cmd_to_tlb_vaddr.read(),
                       r_tlb_way.read(),
                       r_tlb_set.read(),
                       0 );      //we set nline = 0

#ifdef INSTRUMENTATION
m_cpt_iotlb_write++;
#endif

#if DEBUG_TLB_MISS
if ( m_debug_activated )
{
std::cout << name()
          << "  <IOB TLB_PTE1_UPDT> write PTE1 in TLB"
          << " / set = " << std::dec << r_tlb_set.read()
          << " / way = " << r_tlb_way.read() << std::endl;
r_iotlb.printTrace();
}
#endif
        // next state
        r_tlb_fsm = TLB_RETURN; // exit sub-fsm
        break;
    }
    //////////////////
    case TLB_PTE2_GET:  // Try to read a PTE2 (64 bits) in the miss buffer
    {
        uint32_t    pte_flags;
        uint32_t    pte_ppn;

        vci_addr_t line_number  = (vci_addr_t)((r_tlb_paddr.read())&(CACHE_LINE_MASK));
        size_t word_position = (size_t)( ((r_tlb_paddr.read())&(~CACHE_LINE_MASK))>>2 );

        // Hit test. Just to verify.
        bool hit = (r_tlb_buf_valid && (r_tlb_buf_tag.read() == line_number) );
        assert(hit and "Error: No hit on prefetch buffer after Miss Transaction");
        pte_flags= r_tlb_buf_data[word_position];
        pte_ppn= r_tlb_buf_data[word_position+1]; //because PTE2 is 2 words long
        // Bit valid checking
        if ( not ( pte_flags & PTE_V_MASK) )    // unmapped
        {
            //must not occur!
            std::cout << "IOMMU ERROR " << name() << "TLB_IDLE state" << std::endl
                      << "The Page Table entry ins't valid (unmapped)" << std::endl;

            r_tlb_miss_error      = true;
            r_dma_cmd_to_tlb_req  = false;
            r_tlb_fsm             = TLB_IDLE;

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_PTE2_GET> PTE2 Unmapped" << std::hex
          << " / PADDR = " << r_tlb_paddr.read()
          << " / PTE = " << pte_flags << std::endl;
#endif
            break;
        }

        r_tlb_pte_flags       = pte_flags;
        r_tlb_pte_ppn         = pte_ppn;
        r_tlb_fsm           = TLB_PTE2_SELECT;

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_PTE2_GET> Mapped" << std::hex
          << " / PTE_FLAGS = " << pte_flags
          << " / PTE_PPN = " << pte_ppn << std::endl;
#endif
        break;
    }
    ////////////////////////////
    case TLB_PTE2_SELECT:    // select a slot for PTE2
    {
        size_t way;
        size_t set;

        r_iotlb.select( r_dma_cmd_to_tlb_vaddr.read(),
                        false,  // PTE2
                        &way,
                        &set );
#ifdef INSTRUMENTATION
m_cpt_iotlb_read++;
#endif

#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_PTE2_SELECT> Select a slot in IOTLB:"
          << " way = " << std::dec << way
          << " / set = " << set << std::endl;
#endif
        r_tlb_way = way;
        r_tlb_set = set;
        r_tlb_fsm     = TLB_PTE2_UPDT;
        break;
    }
    ///////////////////
    case TLB_PTE2_UPDT:         // write a new PTE2 in tlb
                                // not necessary to treat the L/R bit
    {
        uint32_t pte_flags = r_tlb_pte_flags.read();
        uint32_t pte_ppn   = r_tlb_pte_ppn.read();

        // update TLB for a PTE2
        r_iotlb.write( false,   // 4K page
                       pte_flags,
                       pte_ppn,
                       r_dma_cmd_to_tlb_vaddr.read(),
                       r_tlb_way.read(),
                       r_tlb_set.read(),
                       0 );     // nline = 0
#ifdef INSTRUMENTATION
m_cpt_iotlb_write++;
#endif

#if DEBUG_TLB_MISS
if ( m_debug_activated )
{
std::cout << name()
          << "  <IOB TLB_PTE2_UPDT> write PTE2 in IOTLB"
          << " / set = " << std::dec << r_tlb_set.read()
          << " / way = " << r_tlb_way.read() << std::endl;
r_iotlb.printTrace();
}
#endif
        // next state
        r_tlb_fsm = TLB_RETURN;
        break;
    }
    //////////////
    case TLB_WAIT:   // waiting completion of a miss transaction from MISS_WTI FSM
                     // PTE inval request are handled as unmaskable interrupts
    {
        if ( r_config_cmd_to_tlb_req.read() ) // Request for a PTE invalidation
        {
            r_config_cmd_to_tlb_req = false;
            r_waiting_transaction   = true;
            r_tlb_fsm               = TLB_INVAL_CHECK;
        }

#ifdef INSTRUMENTATION
m_cost_iotlbmiss_transaction++;
#endif
        if ( r_miss_wti_rsp_to_tlb_done.read() ) // Miss transaction completed
        {
            r_miss_wti_rsp_to_tlb_done = false;

            r_tlb_buf_valid = true;
            r_tlb_buf_vaddr = r_dma_cmd_to_tlb_vaddr.read();
            r_tlb_buf_tag   = r_tlb_paddr.read() & CACHE_LINE_MASK;

            if ( r_miss_wti_rsp_error_miss.read() ) // bus error reported
            {
                r_miss_wti_rsp_error_miss = false;
                r_tlb_miss_error          = true;
                r_dma_cmd_to_tlb_req      = false;
                r_tlb_fsm                 = TLB_IDLE;
            }
            else if(r_tlb_miss_type == PTE1_MISS)
            {
                r_tlb_buf_big_page = true;
                r_tlb_fsm = TLB_PTE1_GET;
            }
            else
            {
                r_tlb_buf_big_page = false;
                r_tlb_fsm = TLB_PTE2_GET;
            }
        }
        break;
    }
    ////////////////
    case TLB_RETURN:   // reset r_dma_cmd_to_tlb_req to signal TLB miss completion
                       // possible errors are signaled through r_tlb_miss_error
    {
#if DEBUG_TLB_MISS
if ( m_debug_activated )
std::cout << name()
          << "  <IOB TLB_RETURN> IOTLB MISS completed" << std::endl;
#endif
        r_dma_cmd_to_tlb_req  = false;
        r_tlb_fsm = TLB_IDLE;
        break;
    }
    /////////////////////
    case TLB_INVAL_CHECK:   // request from CONFIG_FSM to invalidate all PTE in a given line
                            // checks the necessity to invalidate prefetch buffer
    {
        // If a transaction is pending, no need to invalidate the prefetch
        // We can ignore it, since we'll replace the line.
        // The new line is necessarily up-to-date
        if(!r_waiting_transaction.read() && r_tlb_buf_valid)
        {
            if(!r_tlb_buf_big_page)
            {
               if( r_tlb_buf_vaddr.read() ==
                   (r_config_cmd_to_tlb_vaddr.read()& ~PTE2_LINE_OFFSET) )
                // The virtual address corresponds to one entry on the buffer line
                {
                    r_tlb_buf_valid = false;   //change here for individual invalidation
                }
            }
            else    // First level entries on buffer. Unused if only small pages
            {
               if( r_tlb_buf_vaddr.read() ==
                   (r_config_cmd_to_tlb_vaddr.read()& ~PTE1_LINE_OFFSET) )
                // The virtual address corresponds to one entry on the buffer line
                {
                    r_tlb_buf_valid = false;   //change here for individual invalidation
                }
            }
        }

        // Invalidation on IOTLB
        r_iotlb.inval(r_config_cmd_to_tlb_vaddr.read());

        if(r_waiting_transaction.read()) r_tlb_fsm =TLB_WAIT;
        else r_tlb_fsm = TLB_IDLE;
        break;
    }
    } //end switch r_tlb_fsm

    ////////////////////////////////////////////////////////////////////////////////
    // The CONFIG_CMD_FSM handles the VCI commands from the INT network.
    // - it can be single flit config transactions
    // - it can be multi-flits data transactions to ROM (read) or FBF (write).
    // The write burst transactions must be serialised from 32 to 64 bits width.
    // The configuration requests can be local (IO_BRIDGE config registers)
    // or remote (config registers of peripherals on IOX network).
    // - The local configuration segment is identified by the "special" atribute
    //   in the mapping table.
    // - All configuration requests are checked against segmentation violation.
    // - In case of local config request, or in case of segmentation violation,
    //   the FSM send a response request to CONFIG_RSP FSM.
    // - In case of remote transaction, it put the VCI command in CONFIG_CMD fifo,
    //   and this require two cycles per IOX flit in case of write burst.
    ///////////////////////////////////////////////////////////////////////////////

    switch( r_config_cmd_fsm.read() )
    {
    /////////////////////
    case CONFIG_CMD_IDLE:   // A VCI INT command is always consumed in this state
    {
        if ( p_vci_tgt_int.cmdval.read() )
        {

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_CMD_IDLE> Config Command received" << std::endl
          << "  address = " << std::hex << p_vci_tgt_int.address.read()
          << " / srcid = " << p_vci_tgt_int.srcid.read()
          << " / trdid = " << p_vci_tgt_int.trdid.read()
          << " / pktid = " << p_vci_tgt_int.pktid.read()
          << " / wdata = " << std::hex << p_vci_tgt_int.wdata.read()
          << " / be = " << p_vci_tgt_int.be.read()
          << " / plen = " << std::dec << p_vci_tgt_int.plen.read()
          << " / eop = " << p_vci_tgt_int.eop.read() << std::endl;
#endif
            vci_addr_t paddr = p_vci_tgt_int.address.read();
            bool       read  = (p_vci_tgt_int.cmd.read() == vci_param_int::CMD_READ);
            uint32_t   cell  = (uint32_t)((paddr & 0x1FF)>>2);
            bool       eop   = p_vci_tgt_int.eop.read();
            bool       high  = (paddr & 0x4);
            ext_data_t wdata = (ext_data_t)p_vci_tgt_int.wdata.read();
            ext_be_t   be    = (ext_be_t)p_vci_tgt_int.be.read();

            // chek segments
            std::list<soclib::common::Segment>::iterator seg;
            bool found   = false;
            bool special = false;
            for ( seg = m_int_seglist.begin() ;
                  seg != m_int_seglist.end() and not found ; seg++ )
            {
                if ( seg->contains(paddr) )
                {
                   found   = true;
                   special = seg->special();
                }
            }

            if ( found and special )  // IO_BRIDGE itself
            {
                bool rerror = false;
                int_data_t rdata;

                assert( (be == 0xF) and
                "ERROR in vci_io_bridge : BE must be 0xF for a config access");

                assert( ( eop ) and
                "ERROR in vci_io_bridge : local config access must be one flit");

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_CMD_IDLE> Command on IOB configuration registers" << std::endl;
#endif

                if ( not read && (cell == IOB_IOMMU_PTPR) )       // WRITE PTPR
                {
                    r_iommu_ptpr = (uint32_t)wdata;

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_CMD_IDLE> Write IOB_IOMMU_PTPR: / wdata = " << std::hex
          << wdata << std::dec << std::endl;
#endif
                }
                else if ( read && (cell == IOB_IOMMU_PTPR) )      // READ PTPR
                {
                    rdata = r_iommu_ptpr.read();

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_CMD_IDLE> Read IOB_IOMMU_PTPR: / rdata = " << std::hex
          << rdata << std::dec << std::endl;
#endif
                }
                else if ( not read && (cell == IOB_IOMMU_ACTIVE) )     // WRITE ACTIVE
                {
                    r_iommu_active = wdata ? true : false;

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_CMD_IDLE> Write IOB_IOMMU_ACTIVE: / wdata = " << std::hex
          << wdata << std::dec << std::endl;
#endif
                }
                else if ( read && (cell == IOB_IOMMU_ACTIVE) )    // READ ACTIVE
                {
                    rdata = r_iommu_active.read();

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_CMD_IDLE> Read IOB_IOMMU_ACTIVE: / rdata = " << std::hex
          << rdata << std::dec << std::endl;
#endif
                }
                else if( not read && (cell == IOB_WTI_ENABLE))    // WRITE WTI_ENABLE
                {
                    r_iommu_wti_enable = wdata;
                }
                else if( read && (cell == IOB_WTI_ENABLE))        // READ WTI ENABLE
                {
                    rdata = r_iommu_wti_enable.read();
                }
                else if( read && (cell == IOB_IOMMU_BVAR))        // READ BVAR
                {
                    rdata = r_iommu_bvar.read();
                }
                else if( read && (cell == IOB_IOMMU_ETR))         // READ ETR
                {
                    rdata = r_iommu_etr.read();
                }
                else if( read && (cell == IOB_IOMMU_BAD_ID))      // READ BAD_ID
                {
                    rdata = r_iommu_bad_id.read();
                }
                else if( not read && (cell == IOB_INVAL_PTE))     // WRITE INVAL_PTE
                {
                    r_config_cmd_to_tlb_req   = true;
                    r_config_cmd_to_tlb_vaddr = (uint32_t)wdata;
                }
                else if( not read && (cell == IOB_WTI_ADDR_LO))   // WRITE WTI_PADDR_LO
                {
                    r_iommu_wti_addr_lo = (vci_addr_t)wdata;
                }
                else if( read && (cell == IOB_WTI_ADDR_LO))       // READ WTI_PADDR_LO
                {
                    rdata = r_iommu_wti_addr_lo.read();
                }
                else if( not read && (cell == IOB_WTI_ADDR_HI))   // WRITE WTI_PADDR_HI
                {
                    r_iommu_wti_addr_hi = (vci_addr_t)wdata;
                }
                else if( read && (cell == IOB_WTI_ADDR_HI))       // READ WTI_PADDR_HI
                {
                    rdata = r_iommu_wti_addr_hi.read();
                }
                else   // Error: Wrong address, or invalid operation.
                {
                    rerror = true;
                }
                r_config_cmd_to_config_rsp_rerror = rerror;
                r_config_cmd_to_config_rsp_rdata  = rdata;
                r_config_cmd_to_config_rsp_rsrcid = p_vci_tgt_int.srcid.read();
                r_config_cmd_to_config_rsp_rtrdid = p_vci_tgt_int.trdid.read();
                r_config_cmd_to_config_rsp_rpktid = p_vci_tgt_int.pktid.read();
                r_config_cmd_fsm                  = CONFIG_CMD_RSP;
            }
            else if ( found )                            // remote peripheral
            {
                // buffer VCI command
                r_config_cmd_address = p_vci_tgt_int.address.read();
                r_config_cmd_pktid   = p_vci_tgt_int.pktid.read();
                r_config_cmd_plen    = p_vci_tgt_int.plen.read();
                r_config_cmd_cmd     = p_vci_tgt_int.cmd.read();
                r_config_cmd_cons    = p_vci_tgt_int.cons.read();
                r_config_cmd_clen    = p_vci_tgt_int.clen.read();
                r_config_cmd_wrap    = p_vci_tgt_int.wrap.read();
                r_config_cmd_contig  = p_vci_tgt_int.contig.read();
                r_config_cmd_cfixed  = p_vci_tgt_int.cfixed.read();
                r_config_cmd_eop     = eop;
                r_config_cmd_wdata   = (wdata << (high ? 32 : 0));
                r_config_cmd_be      = (be << (high ? 4 : 0));

                size_t tab_index;
                if (m_iox_transaction_tab.full(tab_index))
                {
#ifdef INSTRUMENTATION
m_cpt_trt_config_full++;
#endif

                    // wait for an empty slot in the IOX transaction table.
                    // buffer SRCID and TRDID of VCI command to store them
                    // later in IOX transaction table.
                    r_config_cmd_srcid = p_vci_tgt_int.srcid.read();
                    r_config_cmd_trdid = p_vci_tgt_int.trdid.read();
                    r_config_cmd_fsm   = CONFIG_CMD_WAIT;
                    break;
                }

                // TRDID in IOX interconnect is the translation table index
                r_config_cmd_trdid = tab_index;

                // create new entry in IOX transaction table
                m_iox_transaction_tab.set( tab_index,
                        p_vci_tgt_int.srcid.read(),
                        p_vci_tgt_int.trdid.read());

                if (eop) r_config_cmd_fsm = CONFIG_CMD_PUT;
                else     r_config_cmd_fsm = CONFIG_CMD_HI;

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
{
std::cout << name()
          << "  <IOB CONFIG_CMD_IDLE> New entry in IOX transaction table"
          << std::endl;
m_iox_transaction_tab.printTrace();
}
#endif

            }
            else if ( eop )                                   // out of segment address
            {
                r_config_cmd_to_config_rsp_rerror = true;
                r_config_cmd_to_config_rsp_rdata  = 0;
                r_config_cmd_to_config_rsp_rsrcid = p_vci_tgt_int.srcid.read();
                r_config_cmd_to_config_rsp_rtrdid = p_vci_tgt_int.trdid.read();
                r_config_cmd_to_config_rsp_rpktid = p_vci_tgt_int.pktid.read();
                r_config_cmd_fsm                  = CONFIG_CMD_RSP;
            }
        } // end if cmdval
        break;
    }
    /////////////////////
    case CONFIG_CMD_WAIT:
    {
        // wait for an empty slot in the translation table.
        size_t tab_index;
        if (m_iox_transaction_tab.full(tab_index))
        {
#ifdef INSTRUMENTATION
m_cpt_trt_config_full_cost++;
#endif
            break;
        }

        // create new entry in IOX transaction table
        m_iox_transaction_tab.set( tab_index,
                r_config_cmd_srcid.read(),
                r_config_cmd_trdid.read());

        // TRDID in IOX interconnect is the translation table index
        r_config_cmd_trdid = tab_index;
        if (r_config_cmd_eop.read()) r_config_cmd_fsm = CONFIG_CMD_PUT;
        else                         r_config_cmd_fsm = CONFIG_CMD_HI;

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
{
std::cout << name()
          << "  <IOB CONFIG_CMD_WAIT> New entry in IOX transaction table"
          << std::endl;
m_iox_transaction_tab.printTrace();
}
#endif
        break;
    }
    /////////////////////
    case CONFIG_CMD_HI:  // consume the second flit for a multi-flits write
    {
        if ( p_vci_tgt_int.cmdval.read() )
        {
            vci_addr_t paddr = p_vci_tgt_int.address.read();
            bool       high  = ((paddr & 0x4) == 0x4);
            bool       eop   = p_vci_tgt_int.eop.read();

            assert( (paddr == r_config_cmd_address.read() + 4) and high and
            "ERROR in vci_io_bridge : addresses must be contiguous in write burst" );

            r_config_cmd_wdata = r_config_cmd_wdata.read() |
                                 ((ext_data_t)p_vci_tgt_int.wdata.read()<<32);
            r_config_cmd_be    = r_config_cmd_be.read() |
                                 ((ext_be_t)p_vci_tgt_int.be.read()<<4);
            r_config_cmd_eop   = eop;
            r_config_cmd_fsm   = CONFIG_CMD_PUT;
        }
        break;
    }
    /////////////////////
    case CONFIG_CMD_LO:  // consume the first flit for a multi-flits write
    {
        if ( p_vci_tgt_int.cmdval.read() )
        {
            vci_addr_t paddr = p_vci_tgt_int.address.read();
            bool       high  = ((paddr & 0x4) == 0x4);
            bool       eop   = p_vci_tgt_int.eop.read();

            assert( (paddr == r_config_cmd_address.read() + 4) and !high and
            "ERROR in vci_io_bridge : addresses must be contiguous in write burst" );

            r_config_cmd_address = p_vci_tgt_int.address.read();
            r_config_cmd_wdata   = (ext_data_t)p_vci_tgt_int.wdata.read();
            r_config_cmd_be      = (ext_be_t)p_vci_tgt_int.be.read();
            r_config_cmd_eop     = eop;

            if (eop) r_config_cmd_fsm = CONFIG_CMD_PUT;
            else     r_config_cmd_fsm = CONFIG_CMD_HI;
        }
        break;
    }
    ////////////////////
    case CONFIG_CMD_PUT:   // post a command to CONFIG_CMD fifo (to IOX network)
    {
        config_cmd_fifo_put = true;

        if ( m_config_cmd_addr_fifo.wok() )
        {

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_CMD_PUT> Transmit VCI command to IOX network"
          << " : address = " << std::hex << r_config_cmd_address.read()
          << " / srcid = " << m_iox_srcid
          << " / trdid = " << r_config_cmd_trdid.read()
          << " / eop = " << r_config_cmd_eop.read()
          << std::endl;
#endif
            if (r_config_cmd_eop.read()) r_config_cmd_fsm = CONFIG_CMD_IDLE;
            else                         r_config_cmd_fsm = CONFIG_CMD_LO;
        }
        break;
    }
    ////////////////////
    case CONFIG_CMD_RSP:   // Post a request to CONFIG_RSP FSM,
                           // if no previous pending request.
                           // r_config_cmd_to_config_rsp_rerror
                           // has been set in IDLE state.
    {
        if ( not r_config_cmd_to_config_rsp_req.read() )
        {
            r_config_cmd_to_config_rsp_req = true;

#if DEBUG_CONFIG_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_CMD_RSP> Request a response to CONFIG_RSP FSM"
          << " / error = " << r_config_cmd_to_config_rsp_rerror.read() << std::endl;
#endif
            r_config_cmd_fsm = CONFIG_CMD_IDLE;
        }
        break;
    }
    } // end switch CONFIG_CMD FSM

    //////////////////////////////////////////////////////////////////////////////
    // The CONFIG_RSP_FSM controls access to the CONFIG_RSP FIFO to INT network.
    // It implements a round robin priority between 2 clients FSMs :
    // - CONFIG_CMD : response to a local config command.
    // - CONFIG_RSP : responses from peripherals on IOX network
    // Regarding the responses from IOX network it handles both single flit
    // config responses, and multi-flits read responses (ROM), where data must
    // be serialised (64 bits -> 32 bits).
    // Note: We use the VCI RPKTID field to distinguish between read cached
    // (multi-flits response) and others (single flit response).
    // The VCI response flit is only consumed in the PUT_UNC or PUT_HI states.
    //////////////////////////////////////////////////////////////////////////////

    // does nothing if FIFO full
    if ( m_config_rsp_rerror_fifo.wok() )
    {
        switch( r_config_rsp_fsm.read() )
        {
            /////////////////////////
            case CONFIG_RSP_IDLE_IOX:  // IOX requests have highest priority
                                       // no flit on IOX network is consumed
            {
                if ( p_vci_ini_iox.rspval.read() )  // IOX request
                {
                    // recover srcid and trdid from the IOX transaction table
                    size_t tab_index = (size_t)p_vci_ini_iox.rtrdid.read();
                    r_config_rsp_rsrcid = m_iox_transaction_tab.readSrcid(tab_index);
                    r_config_rsp_rtrdid = m_iox_transaction_tab.readTrdid(tab_index);

                    // erase entry
                    m_iox_transaction_tab.erase(tab_index);

                    if ( (p_vci_ini_iox.rpktid.read() & 0x5) == 0x1 )   // multi-flits
                    {
                        r_config_rsp_fsm   = CONFIG_RSP_PUT_LO;
                    }
                    else                                                // single flit
                    {
                        r_config_rsp_fsm   = CONFIG_RSP_PUT_UNC;
                    }
#if DEBUG_CONFIG_RSP
if( m_debug_activated )
{
std::cout << name()
          << "  <IOB CONFIG_RSP_IDLE_IOX> Remove entry in transaction table"
          << std::endl;
m_iox_transaction_tab.printTrace();
}
#endif
                }
                else if ( r_config_cmd_to_config_rsp_req.read() ) // LOCAL request
                {
                    r_config_rsp_fsm = CONFIG_RSP_PUT_LOC;
                }
                break;
            }
            /////////////////////////
            case CONFIG_RSP_IDLE_LOC:  // LOCAL requests have highest priority
                                       // no flit on IOX network is consumed
            {
                if ( r_config_cmd_to_config_rsp_req.read() ) // LOCAL request
                {
                    r_config_rsp_fsm = CONFIG_RSP_PUT_LOC;
                }
                else if ( p_vci_ini_iox.rspval.read() ) // IOX request
                {
                    // recover srcid and trdid from the IOX transaction table
                    size_t tab_index = (size_t)p_vci_ini_iox.rtrdid.read();
                    r_config_rsp_rsrcid = m_iox_transaction_tab.readSrcid(tab_index);
                    r_config_rsp_rtrdid = m_iox_transaction_tab.readTrdid(tab_index);

                    // erase entry
                    m_iox_transaction_tab.erase(tab_index);

                    if ( (p_vci_ini_iox.rpktid.read() & 0x5) == 0x1 )   // multi-flits
                    {
                        r_config_rsp_fsm   = CONFIG_RSP_PUT_LO;
                    }
                    else                                                // single flit
                    {
                        r_config_rsp_fsm   = CONFIG_RSP_PUT_UNC;
                    }
#if DEBUG_CONFIG_RSP
if( m_debug_activated )
{
std::cout << name()
          << "  <IOB CONFIG_RSP_IDLE_IOX> Remove entry in transaction table"
          << std::endl;
m_iox_transaction_tab.printTrace();
}
#endif
                }
                break;
            }
            ////////////////////////
            case CONFIG_RSP_PUT_LO:   // put 32 low bits into CONFIG_RSP fifo
                                       // no flit on IOX network is consumed
            {
                config_rsp_fifo_put    = true;
                config_rsp_fifo_rerror = p_vci_ini_iox.rerror.read();
                config_rsp_fifo_rdata  = (uint32_t)p_vci_ini_iox.rdata.read();
                config_rsp_fifo_rsrcid = r_config_rsp_rsrcid.read();
                config_rsp_fifo_rtrdid = r_config_rsp_rtrdid.read();
                config_rsp_fifo_rpktid = p_vci_ini_iox.rpktid.read();
                config_rsp_fifo_reop   = false;

                r_config_rsp_fsm   = CONFIG_RSP_PUT_HI;

#if DEBUG_CONFIG_RSP
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_RSP_PUT_LO> Push multi-flit response into CONFIG_RSP FIFO"
          << " / rsrcid = " << std::hex << r_config_rsp_rsrcid.read()
          << " / rtrdid = " << r_config_rsp_rtrdid.read()
          << " / rpktid = " << p_vci_ini_iox.rpktid.read()
          << " / rdata = " << (uint32_t)p_vci_ini_iox.rdata.read()
          << " / reop  = " << false
          << " / rerror = " << p_vci_ini_iox.rerror.read() << std::endl;
#endif
                break;
            }
            ///////////////////////
            case CONFIG_RSP_PUT_HI:    // put 32 high bits into CONFIG_RSP fifo
                                   // flit on IOX network is consumed
            {
                config_rsp_fifo_put    = true;
                config_rsp_fifo_rerror = p_vci_ini_iox.rerror.read();
                config_rsp_fifo_rdata  = (uint32_t)(p_vci_ini_iox.rdata.read() >> 32);
                config_rsp_fifo_rsrcid = r_config_rsp_rsrcid.read();
                config_rsp_fifo_rtrdid = r_config_rsp_rtrdid.read();
                config_rsp_fifo_rpktid = p_vci_ini_iox.rpktid.read();
                config_rsp_fifo_reop   = p_vci_ini_iox.reop.read();

                if( p_vci_ini_iox.reop.read() ) r_config_rsp_fsm = CONFIG_RSP_IDLE_LOC;
                else                            r_config_rsp_fsm = CONFIG_RSP_PUT_LO;

#if DEBUG_CONFIG_RSP
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_RSP_PUT_HI> Push multi-flit response into CONFIG_RSP FIFO"
          << " / rsrcid = " << std::hex << r_config_rsp_rsrcid.read()
          << " / rtrdid = " << r_config_rsp_rtrdid.read()
          << " / rpktid = " << p_vci_ini_iox.rpktid.read()
          << " / rdata = " << (uint32_t)(p_vci_ini_iox.rdata.read() >> 32)
          << " / reop  = " << p_vci_ini_iox.reop.read()
          << " / rerror = " << p_vci_ini_iox.rerror.read() << std::endl;
#endif
                break;
            }
            ////////////////////////
            case CONFIG_RSP_PUT_UNC:   // put single flit into CONFIG_RSP fifo
                                       // flit on IOX network is consumed
            {
                assert(  p_vci_ini_iox.reop.read() and
                "ERROR in vci_io_bridge : a remote config response should be one flit");

                config_rsp_fifo_put    = true;
                config_rsp_fifo_rerror = p_vci_ini_iox.rerror.read();
                config_rsp_fifo_rdata  = (uint32_t)p_vci_ini_iox.rdata.read();
                config_rsp_fifo_rsrcid = r_config_rsp_rsrcid.read();
                config_rsp_fifo_rtrdid = r_config_rsp_rtrdid.read();
                config_rsp_fifo_rpktid = p_vci_ini_iox.rpktid.read();
                config_rsp_fifo_reop   = true;

                // update priority
                r_config_rsp_fsm   = CONFIG_RSP_IDLE_LOC;

#if DEBUG_CONFIG_RSP
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_RSP_PUT_UNC> Push single flit response into CONFIG_RSP FIFO"
          << " / rsrcid = " << std::hex << r_config_rsp_rsrcid.read()
          << " / rtrdid = " << r_config_rsp_rtrdid.read()
          << " / rpktid = " << p_vci_ini_iox.rpktid.read()
          << " / rdata = " << (uint32_t)p_vci_ini_iox.rdata.read()
          << " / reop  = " << true
          << " / rerror = " << p_vci_ini_iox.rerror.read() << std::endl;
#endif
                break;
            }
            ////////////////////////
            case CONFIG_RSP_PUT_LOC:   // put single flit into CONFIG_RSP fifo
                                       // no flit on IOX network is consumed
            {
                config_rsp_fifo_put    = true;
                config_rsp_fifo_rerror = r_config_cmd_to_config_rsp_rerror.read();
                config_rsp_fifo_rdata  = r_config_cmd_to_config_rsp_rdata.read();
                config_rsp_fifo_rsrcid = r_config_cmd_to_config_rsp_rsrcid.read();
                config_rsp_fifo_rtrdid = r_config_cmd_to_config_rsp_rtrdid.read();
                config_rsp_fifo_rpktid = r_config_cmd_to_config_rsp_rpktid.read();
                config_rsp_fifo_reop   = true;

                // acknowledge request
                r_config_cmd_to_config_rsp_req = false;

                // update priority
                r_config_rsp_fsm   = CONFIG_RSP_IDLE_IOX;

#if DEBUG_CONFIG_RSP
if( m_debug_activated )
std::cout << name()
          << "  <IOB CONFIG_RSP_PUT_UNC> Push single flit response into CONFIG_RSP FIFO"
          << " / rsrcid = " << std::hex << r_config_cmd_to_config_rsp_rsrcid.read()
          << " / rtrdid = " << r_config_cmd_to_config_rsp_rtrdid.read()
          << " / rpktid = " << r_config_cmd_to_config_rsp_rpktid.read()
          << " / rdata = "  << r_config_cmd_to_config_rsp_rdata.read()
          << " / reop  = "  << true
          << " / rerror = " << r_config_cmd_to_config_rsp_rerror.read() << std::endl;
#endif
                break;
            }
        } // end switch CONFIG_RSP FSM
    } // end if FIFO full

    ///////////////////////////////////////////////////////////////////////////////////
    // The MISS_WTI_CMD component is a combinational switch that push one single flit
    // VCI command in the MISS_WTI FIFO to INT Network, depending on two clients :
    // 1. MISS requests from TLB_MISS FSM :
    //    These requests have highest priority because a TLB MISS is a blocking event
    //    for the DMA FSM. The r_tlb_to_miss_wti_cmd_req flip-flop is reset by the
    //    MISS_WTI_RSP FSM only when the response is received.
    // 2. WTI requests from DMA_CMD FSM :
    //    These requestsare non blocking events, and the r_dma_cmd_to_miss_wti_cmd_req
    //    flip-flop is reset as soon as the WTI command has been sent.
    //    There is two types of WTI requests:
    //    - external WTI from peripherals on IOX network.
    //    - internal WTI caused by illegal DMA requests.
    ////////////////////////////////////////////////////////////////////////////////////

    if ( r_tlb_to_miss_wti_cmd_req.read() and
         m_miss_wti_cmd_addr_fifo.wok() )                   // put MISS READ
    {
        r_tlb_to_miss_wti_cmd_req = false;

        miss_wti_cmd_fifo_put     = true;
        miss_wti_cmd_fifo_address = r_tlb_paddr.read() & CACHE_LINE_MASK;
        miss_wti_cmd_fifo_wdata   = 0;
        miss_wti_cmd_fifo_cmd     = vci_param_int::CMD_READ;
        miss_wti_cmd_fifo_pktid   = PKTID_MISS;
        miss_wti_cmd_fifo_srcid   = m_int_srcid;
        miss_wti_cmd_fifo_trdid   = 0;
        miss_wti_cmd_fifo_plen    = m_words * vci_param_int::B;

#if DEBUG_MISS_WTI_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB MISS_WTI_CMD_WTI> push MISS TLB command into MISS_WTI FIFO"
          << " / PADDR = " << std::hex << miss_wti_cmd_fifo_address << std::dec
          << std::endl;
#endif

    }
    else if ( r_dma_cmd_to_miss_wti_cmd_req.read() and
              m_miss_wti_cmd_addr_fifo.wok() )               // put WTI READ / WRITE
    {
        r_dma_cmd_to_miss_wti_cmd_req = false;

        miss_wti_cmd_fifo_put     = true;
        miss_wti_cmd_fifo_cmd     = r_dma_cmd_to_miss_wti_cmd_cmd.read();
        miss_wti_cmd_fifo_address = r_dma_cmd_to_miss_wti_cmd_addr.read();
        miss_wti_cmd_fifo_wdata   = r_dma_cmd_to_miss_wti_cmd_wdata.read();
        miss_wti_cmd_fifo_srcid   = r_dma_cmd_to_miss_wti_cmd_srcid.read();
        miss_wti_cmd_fifo_trdid   = r_dma_cmd_to_miss_wti_cmd_trdid.read();
        miss_wti_cmd_fifo_pktid   = r_dma_cmd_to_miss_wti_cmd_pktid.read();
        miss_wti_cmd_fifo_plen    = vci_param_int::B;

#if DEBUG_MISS_WTI_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB MISS_WTI_CMD_WTI> push WTI command into MISS_WTI FIFO"
          << " / CMD = " << miss_wti_cmd_fifo_cmd
          << " / PADDR = " << std::hex << miss_wti_cmd_fifo_address << std::dec
          << std::endl;
#endif

    }

    ///////////////////////////////////////////////////////////////////////////////////
    // The MISS_WTI_RSP FSM handles VCI responses from the INT network:
    // - for a TLB MISS (multi-flits read transaction), the cache line
    //   is written in r_tlb_buf_data[], and r_tlb_to_miss_wti_cmd_req flip-flop is reset.
    // - for a WTI_IOX (single flit write transaction), the response must be
    //   forwarded to the source peripheral on the INT network
    // - for a WTI_MMU (single flit write transaction), there is nothing to do.
    //
    // TODO VCI addressing errors for TLB MISS or for WTI_MMU (i.e. kernel errors...)
    // are registered in the r_miss_wti_rsp_error_miss & r_miss_wti_rsp_error_wti
    // flip-flops, and simulation stops... They could be signaled to OS by a WTI.
    ////////////////////////////////////////////////////////////////////////////////////

    switch ( r_miss_wti_rsp_fsm.read() )
    {
        ///////////////////////
        case MISS_WTI_RSP_IDLE:   // waiting a VCI response
                                  // no VCI flit is consumed
        {
            if ( p_vci_ini_int.rspval.read() )
            {
                if ( p_vci_ini_int.rpktid.read() == PKTID_WTI_IOX )
                {
                    r_miss_wti_rsp_fsm   = MISS_WTI_RSP_WTI_IOX;
                }
                else if ( p_vci_ini_int.rpktid.read() == PKTID_WTI_MMU )
                {
                    r_miss_wti_rsp_fsm   = MISS_WTI_RSP_WTI_MMU;
                }
                else if ( p_vci_ini_int.rpktid.read() == PKTID_MISS )
                {
                    r_miss_wti_rsp_fsm   = MISS_WTI_RSP_MISS;
                    r_miss_wti_rsp_count = 0;
                }
                else
                {
                    assert ( false and
                    "VCI_IO_BRIDGE ERROR : illegal response type on INT network");
                }
            }
            break;
        }
        //////////////////////////
        case MISS_WTI_RSP_WTI_IOX:   // Handling response to a peripheral WTI
                                     // consume VCI flit and transfer response
                                     // to DMA_RSP FSM in dedicated registers
                                     // if no pending previous request.
        {
            assert( p_vci_ini_int.reop.read() and
            "VCI_IO_BRIDGE ERROR: WTI_IOX response should have one single flit" );

            if ( not r_miss_wti_rsp_to_dma_rsp_req.read() ) // no previous pending request
            {
                r_miss_wti_rsp_to_dma_rsp_req    = true;
                r_miss_wti_rsp_to_dma_rsp_rerror = p_vci_ini_int.rerror.read();
                r_miss_wti_rsp_to_dma_rsp_rsrcid = p_vci_ini_int.rsrcid.read();
                r_miss_wti_rsp_to_dma_rsp_rtrdid = p_vci_ini_int.rtrdid.read();
                r_miss_wti_rsp_to_dma_rsp_rpktid = p_vci_ini_int.rpktid.read();

                r_miss_wti_rsp_fsm = MISS_WTI_RSP_IDLE;

#if DEBUG_MISS_WTI_RSP
if( m_debug_activated )
std::cout << name()
          << "  <IOB MISS_WTI_RSP_WTI_IOX> Transfer response to a WTI_IOX" << std::endl;
#endif
            }
            break;
        }
        //////////////////////////
        case MISS_WTI_RSP_WTI_MMU:   // Handling response to an iommu WTI
                                     // consume VCI flit and test VCI error.
        {
            assert( p_vci_ini_int.reop.read() and
            "VCI_IO_BRIDGE ERROR: WTI_MMU response should have one single flit" );

            if ( (p_vci_ini_int.rerror.read()&0x1) != 0 )  // error reported
            {
                // set the specific error flip-flop
                r_miss_wti_rsp_error_wti = true;
                assert( false and
                "VCI_IO_BRIDGE ERROR: VCI error response for a WTI_MMU transaction");
            }

#if DEBUG_MISS_WTI_RSP
if( m_debug_activated )
std::cout << name()
          << " <IOB MISS_WTI_RSP_WTI_MMU> Receive response to a WTI_MMU" << std::endl;
#endif
            break;
        }
        ///////////////////////
        case MISS_WTI_RSP_MISS:   // Handling response to a TLB MISS
                                  // write cache line in r_tlb_buf buffer
                                  // and analyse possible VCI error
                                  // VCI flit is consumed.
        {
            if ( p_vci_ini_int.rspval.read() )
            {
                if ( (p_vci_ini_int.rerror.read()&0x1) != 0 )  // error reported
                {
                    // set the specific error flip-flop
                    r_miss_wti_rsp_error_miss = true;
                    assert( false and
                    "VCI_IO_BRIDGE ERROR: VCI error response for a TLB MISS transaction");

                }
                else                                           // no error
                {

#if DEBUG_MISS_WTI_CMD
if( m_debug_activated )
std::cout << name()
          << "  <IOB MISS_WTI_RSP_MISS> Receive response to a TLB MISS"
          << " / Count = " << r_miss_wti_rsp_count.read()
          << " / Data = " << std::hex << p_vci_ini_int.rdata.read() << std::endl;
#endif
                    r_tlb_buf_data[r_miss_wti_rsp_count.read()] = p_vci_ini_int.rdata.read();
                }

                if ( p_vci_ini_int.reop.read() )               // last flit
                {
                    assert((r_miss_wti_rsp_count.read() == (m_words-1)) and
                    "VCI_IO_BRIDGE ERROR: invalid length for a TLB MISS response");

                    r_miss_wti_rsp_count       = 0;
                    r_miss_wti_rsp_fsm         = MISS_WTI_RSP_IDLE;
                    r_miss_wti_rsp_to_tlb_done = true;
                }
                else                                           // not the last flit
                {
                    r_miss_wti_rsp_count = r_miss_wti_rsp_count.read() + 1;
                }
            }
            break;
        }
    } // end  switch r_miss_wti_rsp_fsm


    ///////////////////////////////////////////////////////////
    // DMA_CMD fifo update
    // writer : DMA_CMD FSM
    ///////////////////////////////////////////////////////////

    m_dma_cmd_addr_fifo.update(   dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  r_dma_cmd_paddr.read() );   // address translation
    m_dma_cmd_cmd_fifo.update(    dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.cmd.read() );
    m_dma_cmd_contig_fifo.update( dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.contig.read() );
    m_dma_cmd_cons_fifo.update(   dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.cons.read() );
    m_dma_cmd_plen_fifo.update(   dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.plen.read() );
    m_dma_cmd_wrap_fifo.update(   dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.wrap.read() );
    m_dma_cmd_cfixed_fifo.update( dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.cfixed.read() );
    m_dma_cmd_clen_fifo.update(   dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.clen.read() );
    m_dma_cmd_srcid_fifo.update(  dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  dma_cmd_fifo_srcid );
    m_dma_cmd_trdid_fifo.update(  dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.trdid.read() );
    m_dma_cmd_pktid_fifo.update(  dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.pktid.read() );
    m_dma_cmd_data_fifo.update(   dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.wdata.read() );
    m_dma_cmd_be_fifo.update(     dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.be.read() );
    m_dma_cmd_eop_fifo.update(    dma_cmd_fifo_get,
                                  dma_cmd_fifo_put,
                                  p_vci_tgt_iox.eop.read() );

    //////////////////////////////////////////////////////////////
    // DMA_RSP fifo update
    // writer : DMA_RSP FSM
    //////////////////////////////////////////////////////////////

    m_dma_rsp_data_fifo.update(   dma_rsp_fifo_get,
                                  dma_rsp_fifo_put,
                                  dma_rsp_fifo_rdata );
    m_dma_rsp_rsrcid_fifo.update( dma_rsp_fifo_get,
                                  dma_rsp_fifo_put,
                                  dma_rsp_fifo_rsrcid );
    m_dma_rsp_rtrdid_fifo.update( dma_rsp_fifo_get,
                                  dma_rsp_fifo_put,
                                  dma_rsp_fifo_rtrdid );
    m_dma_rsp_rpktid_fifo.update( dma_rsp_fifo_get,
                                  dma_rsp_fifo_put,
                                  dma_rsp_fifo_rpktid );
    m_dma_rsp_reop_fifo.update(   dma_rsp_fifo_get,
                                  dma_rsp_fifo_put,
                                  dma_rsp_fifo_reop );
    m_dma_rsp_rerror_fifo.update( dma_rsp_fifo_get,
                                  dma_rsp_fifo_put,
                                  dma_rsp_fifo_rerror );

    ////////////////////////////////////////////////////////////////
    // CONFIG_CMD fifo update
    // writer : CONFIG_CMD FSM
    ////////////////////////////////////////////////////////////////

    m_config_cmd_addr_fifo.update(   config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_address.read() );
    m_config_cmd_cmd_fifo.update(    config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_cmd.read() );
    m_config_cmd_contig_fifo.update( config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_contig.read() );
    m_config_cmd_cons_fifo.update(   config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_cons.read() );
    m_config_cmd_plen_fifo.update(   config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_plen.read() );
    m_config_cmd_wrap_fifo.update(   config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_wrap.read() );
    m_config_cmd_cfixed_fifo.update( config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_cfixed.read() );
    m_config_cmd_clen_fifo.update(   config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_clen.read() );
    m_config_cmd_trdid_fifo.update(  config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_trdid.read() );
    m_config_cmd_pktid_fifo.update(  config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_pktid.read() );
    m_config_cmd_data_fifo.update(   config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_wdata.read() );
    m_config_cmd_be_fifo.update(     config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_be.read() );
    m_config_cmd_eop_fifo.update(    config_cmd_fifo_get,
                                     config_cmd_fifo_put,
                                     r_config_cmd_eop.read() );

    //////////////////////////////////////////////////////////////////////////
    // CONFIG_RSP fifo update
    // writer : CONFIG_RSP FSM
    //////////////////////////////////////////////////////////////////////////

    m_config_rsp_data_fifo.update(   config_rsp_fifo_get,
                                     config_rsp_fifo_put,
                                     config_rsp_fifo_rdata );
    m_config_rsp_rsrcid_fifo.update( config_rsp_fifo_get,
                                     config_rsp_fifo_put,
                                     config_rsp_fifo_rsrcid );
    m_config_rsp_rtrdid_fifo.update( config_rsp_fifo_get,
                                     config_rsp_fifo_put,
                                     config_rsp_fifo_rtrdid );
    m_config_rsp_rpktid_fifo.update( config_rsp_fifo_get,
                                     config_rsp_fifo_put,
                                     config_rsp_fifo_rpktid );
    m_config_rsp_reop_fifo.update(   config_rsp_fifo_get,
                                     config_rsp_fifo_put,
                                     config_rsp_fifo_reop );
    m_config_rsp_rerror_fifo.update( config_rsp_fifo_get,
                                     config_rsp_fifo_put,
                                     config_rsp_fifo_rerror );

    ////////////////////////////////////////////////////////////////
    // MISS_WTI_CMD fifo update
    // One writer : MISS_WTI switch
    ////////////////////////////////////////////////////////////////

    m_miss_wti_cmd_addr_fifo.update(   miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       miss_wti_cmd_fifo_address );
    m_miss_wti_cmd_cmd_fifo.update(    miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       miss_wti_cmd_fifo_cmd );
    m_miss_wti_cmd_contig_fifo.update( config_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       true );
    m_miss_wti_cmd_cons_fifo.update(   miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       false );
    m_miss_wti_cmd_plen_fifo.update(   miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       miss_wti_cmd_fifo_plen );
    m_miss_wti_cmd_wrap_fifo.update(   miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       false );
    m_miss_wti_cmd_cfixed_fifo.update( config_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       false );
    m_miss_wti_cmd_clen_fifo.update(   miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       0 );
    m_miss_wti_cmd_srcid_fifo.update(  miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       miss_wti_cmd_fifo_srcid );
    m_miss_wti_cmd_trdid_fifo.update(  miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       miss_wti_cmd_fifo_trdid );
    m_miss_wti_cmd_pktid_fifo.update(  miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       miss_wti_cmd_fifo_pktid );
    m_miss_wti_cmd_data_fifo.update(   miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       miss_wti_cmd_fifo_wdata );
    m_miss_wti_cmd_be_fifo.update(     miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       0xF );
    m_miss_wti_cmd_eop_fifo.update(    miss_wti_cmd_fifo_get,
                                       miss_wti_cmd_fifo_put,
                                       true );

} // end transition()

//////////////////////////////////////////////////////////////////////////
tmpl(void)::genMoore()
//////////////////////////////////////////////////////////////////////////
{
    /////////////////  p_vci_ini_ram  /////////////////////////////

    // VCI initiator command on RAM network
    // directly the content of the dma_cmd FIFO
    p_vci_ini_ram.cmdval  = m_dma_cmd_addr_fifo.rok();
    p_vci_ini_ram.address = m_dma_cmd_addr_fifo.read();
    p_vci_ini_ram.be      = m_dma_cmd_be_fifo.read();
    p_vci_ini_ram.cmd     = m_dma_cmd_cmd_fifo.read();
    p_vci_ini_ram.contig  = m_dma_cmd_contig_fifo.read();
    p_vci_ini_ram.wdata   = m_dma_cmd_data_fifo.read();
    p_vci_ini_ram.eop     = m_dma_cmd_eop_fifo.read();
    p_vci_ini_ram.cons    = m_dma_cmd_cons_fifo.read();
    p_vci_ini_ram.plen    = m_dma_cmd_plen_fifo.read();
    p_vci_ini_ram.wrap    = m_dma_cmd_wrap_fifo.read();
    p_vci_ini_ram.cfixed  = m_dma_cmd_cfixed_fifo.read();
    p_vci_ini_ram.clen    = m_dma_cmd_clen_fifo.read();
    p_vci_ini_ram.trdid   = m_dma_cmd_trdid_fifo.read();
    p_vci_ini_ram.pktid   = m_dma_cmd_pktid_fifo.read();
    p_vci_ini_ram.srcid   = m_dma_cmd_srcid_fifo.read();

    // VCI initiator response on the RAM Network
    // depends on the DMA_RSP FSM state
    p_vci_ini_ram.rspack = m_dma_rsp_data_fifo.wok() and
                           (r_dma_rsp_fsm.read() == DMA_RSP_PUT_DMA);

    /////////////////  p_vci_tgt_iox  /////////////////////////////

    // VCI target response on IOX network is
    // directly the content of the DMA_RSP FIFO
    p_vci_tgt_iox.rspval  = m_dma_rsp_data_fifo.rok();
    p_vci_tgt_iox.rsrcid  = m_dma_rsp_rsrcid_fifo.read();
    p_vci_tgt_iox.rtrdid  = m_dma_rsp_rtrdid_fifo.read();
    p_vci_tgt_iox.rpktid  = m_dma_rsp_rpktid_fifo.read();
    p_vci_tgt_iox.rdata   = m_dma_rsp_data_fifo.read();
    p_vci_tgt_iox.rerror  = m_dma_rsp_rerror_fifo.read();
    p_vci_tgt_iox.reop    = m_dma_rsp_reop_fifo.read();

    // VCI target command ack on IOX network
    // depends on the DMA_CMD FSM state
    switch ( r_dma_cmd_fsm.read() )
    {
        case DMA_CMD_IDLE:
             p_vci_tgt_iox.cmdack  = false;
             break;
        case DMA_CMD_DMA_REQ:
             p_vci_tgt_iox.cmdack  = m_dma_cmd_addr_fifo.wok();
             break;
        case DMA_CMD_WTI_IOX_REQ:
             p_vci_tgt_iox.cmdack  = not r_dma_cmd_to_miss_wti_cmd_req.read();
             break;
        case DMA_CMD_ERR_WTI_REQ:
             p_vci_tgt_iox.cmdack  = false;
             break;
        case DMA_CMD_ERR_WAIT_EOP:
             p_vci_tgt_iox.cmdack  = true;
             break;
        case DMA_CMD_ERR_RSP_REQ:
             p_vci_tgt_iox.cmdack  = false;
             break;
        case DMA_CMD_TLB_MISS_WAIT:
             p_vci_tgt_iox.cmdack  = false;
             break;
    }

    //////////////////  p_vci_ini_iox  /////////////////////////////

    // VCI initiator command on IOX network is
    // directly the content of the CONFIG_CMD FIFO
    p_vci_ini_iox.cmdval  = m_config_cmd_addr_fifo.rok();
    p_vci_ini_iox.address = m_config_cmd_addr_fifo.read();
    p_vci_ini_iox.be      = m_config_cmd_be_fifo.read();
    p_vci_ini_iox.cmd     = m_config_cmd_cmd_fifo.read();
    p_vci_ini_iox.contig  = m_config_cmd_contig_fifo.read();
    p_vci_ini_iox.wdata   = m_config_cmd_data_fifo.read();
    p_vci_ini_iox.eop     = m_config_cmd_eop_fifo.read();
    p_vci_ini_iox.cons    = m_config_cmd_cons_fifo.read();
    p_vci_ini_iox.plen    = m_config_cmd_plen_fifo.read();
    p_vci_ini_iox.wrap    = m_config_cmd_wrap_fifo.read();
    p_vci_ini_iox.cfixed  = m_config_cmd_cfixed_fifo.read();
    p_vci_ini_iox.clen    = m_config_cmd_clen_fifo.read();
    p_vci_ini_iox.trdid   = m_config_cmd_trdid_fifo.read();
    p_vci_ini_iox.pktid   = m_config_cmd_pktid_fifo.read();
    p_vci_ini_iox.srcid   = m_iox_srcid;

    // VCI initiator response on IOX Network
    // it depends on the CONFIG_RSP FSM state
    p_vci_ini_iox.rspack = m_config_rsp_data_fifo.wok() and
                           ( (r_config_rsp_fsm.read() == CONFIG_RSP_PUT_UNC) or
                             (r_config_rsp_fsm.read() == CONFIG_RSP_PUT_HI) );

    /////////////////  p_vci_tgt_int  ////////////////////////////////

    // VCI target response on INT network
    // directly the content of the CONFIG_RSP FIFO
    p_vci_tgt_int.rspval  = m_config_rsp_data_fifo.rok();
    p_vci_tgt_int.rsrcid  = m_config_rsp_rsrcid_fifo.read();
    p_vci_tgt_int.rtrdid  = m_config_rsp_rtrdid_fifo.read();
    p_vci_tgt_int.rpktid  = m_config_rsp_rpktid_fifo.read();
    p_vci_tgt_int.rdata   = m_config_rsp_data_fifo.read();
    p_vci_tgt_int.rerror  = m_config_rsp_rerror_fifo.read();
    p_vci_tgt_int.reop    = m_config_rsp_reop_fifo.read();

    // VCI target command ack on INT network
    // it depends on the CONFIG_CMD FSM state
    switch ( r_config_cmd_fsm.read() )
    {
    case CONFIG_CMD_IDLE:
        p_vci_tgt_int.cmdack = true;
        break;
    case CONFIG_CMD_WAIT:
        p_vci_tgt_int.cmdack = false;
        break;
    case CONFIG_CMD_HI:
        p_vci_tgt_int.cmdack = true;
        break;
    case CONFIG_CMD_LO:
        p_vci_tgt_int.cmdack = true;
        break;
    case CONFIG_CMD_PUT:
        p_vci_tgt_int.cmdack = false;
        break;
    case CONFIG_CMD_RSP:
        p_vci_tgt_int.cmdack = false;
        break;
    }

    /////////////////  p_vci_ini_int  ////////////////////////////////

    // VCI initiator command  on INT network
    // directly the content of the MISS_WTI_CMD FIFO
    p_vci_ini_int.cmdval  = m_miss_wti_cmd_addr_fifo.rok();
    p_vci_ini_int.address = m_miss_wti_cmd_addr_fifo.read();
    p_vci_ini_int.be      = m_miss_wti_cmd_be_fifo.read();
    p_vci_ini_int.cmd     = m_miss_wti_cmd_cmd_fifo.read();
    p_vci_ini_int.contig  = m_miss_wti_cmd_contig_fifo.read();
    p_vci_ini_int.wdata   = m_miss_wti_cmd_data_fifo.read();
    p_vci_ini_int.eop     = m_miss_wti_cmd_eop_fifo.read();
    p_vci_ini_int.cons    = m_miss_wti_cmd_cons_fifo.read();
    p_vci_ini_int.plen    = m_miss_wti_cmd_plen_fifo.read();
    p_vci_ini_int.wrap    = m_miss_wti_cmd_wrap_fifo.read();
    p_vci_ini_int.cfixed  = m_miss_wti_cmd_cfixed_fifo.read();
    p_vci_ini_int.clen    = m_miss_wti_cmd_clen_fifo.read();
    p_vci_ini_int.trdid   = m_miss_wti_cmd_trdid_fifo.read();
    p_vci_ini_int.pktid   = m_miss_wti_cmd_pktid_fifo.read();
    p_vci_ini_int.srcid   = m_miss_wti_cmd_srcid_fifo.read();

    // VCI initiator response on INT network
    // It depends on the MISS_WTI_RSP FSM state

    if ( r_miss_wti_rsp_fsm.read() == MISS_WTI_RSP_IDLE )
    {
        p_vci_ini_int.rspack = false;
    }
    else if ( r_miss_wti_rsp_fsm.read() == MISS_WTI_RSP_WTI_IOX )
    {
        p_vci_ini_int.rspack = not r_miss_wti_rsp_to_dma_rsp_req.read();
    }
    else  //  MISS_WTI_RSP_MISS or MISS_WTI_RESP_WTI_MMU
    {
        p_vci_ini_int.rspack = true;
    }

} // end genMoore

}}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
