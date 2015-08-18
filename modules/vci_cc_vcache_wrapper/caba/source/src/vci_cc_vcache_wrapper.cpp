/* -*- c++ -*-
 * File : vci_cc_vcache_wrapper.cpp
 * Copyright (c) UPMC, Lip6, SoC
 * Authors : Alain GREINER, Yang GAO
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

#include <cassert>
#include <signal.h>

#include "arithmetics.h"
#include "../include/vci_cc_vcache_wrapper.h"

#define DEBUG_DCACHE    1
#define DEBUG_ICACHE    1
#define DEBUG_CMD       0

namespace soclib {
namespace caba {

namespace {
const char * icache_fsm_state_str[] = {
        "ICACHE_IDLE",

        "ICACHE_XTN_TLB_FLUSH",
        "ICACHE_XTN_CACHE_FLUSH",
        "ICACHE_XTN_CACHE_FLUSH_GO",
        "ICACHE_XTN_TLB_INVAL",
        "ICACHE_XTN_CACHE_INVAL_VA",
        "ICACHE_XTN_CACHE_INVAL_PA",
        "ICACHE_XTN_CACHE_INVAL_GO",

        "ICACHE_TLB_WAIT",

        "ICACHE_MISS_SELECT",
        "ICACHE_MISS_CLEAN",
        "ICACHE_MISS_WAIT",
        "ICACHE_MISS_DATA_UPDT",
        "ICACHE_MISS_DIR_UPDT",

        "ICACHE_UNC_WAIT",

        "ICACHE_CC_CHECK",
        "ICACHE_CC_UPDT",
        "ICACHE_CC_INVAL",
    };

const char * dcache_fsm_state_str[] = {
        "DCACHE_IDLE",

        "DCACHE_TLB_MISS",
        "DCACHE_TLB_PTE1_GET",
        "DCACHE_TLB_PTE1_SELECT",
        "DCACHE_TLB_PTE1_UPDT",
        "DCACHE_TLB_PTE2_GET",
        "DCACHE_TLB_PTE2_SELECT",
        "DCACHE_TLB_PTE2_UPDT",
        "DCACHE_TLB_LR_UPDT",
        "DCACHE_TLB_LR_WAIT",
        "DCACHE_TLB_RETURN",

        "DCACHE_XTN_SWITCH",
        "DCACHE_XTN_SYNC",
        "DCACHE_XTN_IC_INVAL_VA",
        "DCACHE_XTN_IC_FLUSH",
        "DCACHE_XTN_IC_INVAL_PA",
        "DCACHE_XTN_IC_PADDR_EXT",
        "DCACHE_XTN_IT_INVAL",
        "DCACHE_XTN_DC_FLUSH",
        "DCACHE_XTN_DC_FLUSH_GO",
        "DCACHE_XTN_DC_INVAL_VA",
        "DCACHE_XTN_DC_INVAL_PA",
        "DCACHE_XTN_DC_INVAL_END",
        "DCACHE_XTN_DC_INVAL_GO",
        "DCACHE_XTN_DT_INVAL",

        "DCACHE_DIRTY_GET_PTE",
        "DCACHE_DIRTY_WAIT",

        "DCACHE_MISS_SELECT",
        "DCACHE_MISS_CLEAN",
        "DCACHE_MISS_WAIT",
        "DCACHE_MISS_DATA_UPDT",
        "DCACHE_MISS_DIR_UPDT",

        "DCACHE_UNC_WAIT",
        "DCACHE_LL_WAIT",
        "DCACHE_SC_WAIT",

        "DCACHE_CC_CHECK",
        "DCACHE_CC_UPDT",
        "DCACHE_CC_INVAL",

        "DCACHE_INVAL_TLB_SCAN",
    };

const char * cmd_fsm_state_str[] = {
        "CMD_IDLE",
        "CMD_INS_MISS",
        "CMD_INS_UNC",
        "CMD_DATA_MISS",
        "CMD_DATA_UNC_READ",
        "CMD_DATA_UNC_WRITE",
        "CMD_DATA_WRITE",
        "CMD_DATA_LL",
        "CMD_DATA_SC",
        "CMD_DATA_CAS",
    };

const char * vci_pktid_type_str[] = {
        "TYPE_DATA_UNC",
        "TYPE_READ_DATA_MISS",
        "TYPE_READ_INS_UNC",
        "TYPE_READ_INS_MISS",
        "TYPE_WRITE",
        "TYPE_CAS",
        "TYPE_LL",
        "TYPE_SC",
    };

const char * vci_cmd_type_str[] = {
        "NOP or STORE_COND",
        "READ",
        "WRITE",
        "LOCKED_READ"
    };

const char * rsp_fsm_state_str[] = {
        "RSP_IDLE",
        "RSP_INS_MISS",
        "RSP_INS_UNC",
        "RSP_DATA_MISS",
        "RSP_DATA_UNC",
        "RSP_DATA_LL",
        "RSP_DATA_WRITE",
    };

const char * cc_receive_fsm_state_str[] = {
        "CC_RECEIVE_IDLE",
        "CC_RECEIVE_BRDCAST_HEADER",
        "CC_RECEIVE_BRDCAST_NLINE",
        "CC_RECEIVE_INS_INVAL_HEADER",
        "CC_RECEIVE_INS_INVAL_NLINE",
        "CC_RECEIVE_INS_UPDT_HEADER",
        "CC_RECEIVE_INS_UPDT_NLINE",
        "CC_RECEIVE_INS_UPDT_DATA",
        "CC_RECEIVE_DATA_INVAL_HEADER",
        "CC_RECEIVE_DATA_INVAL_NLINE",
        "CC_RECEIVE_DATA_UPDT_HEADER",
        "CC_RECEIVE_DATA_UPDT_NLINE",
        "CC_RECEIVE_DATA_UPDT_DATA",
    };

const char * cc_send_fsm_state_str[] = {
        "CC_SEND_IDLE",
        "CC_SEND_CLEANUP_1",
        "CC_SEND_CLEANUP_2",
        "CC_SEND_MULTI_ACK",
    };
}

#define tmpl(...) \
   template<typename vci_param, \
            size_t   dspin_in_width, \
            size_t   dspin_out_width, \
            typename iss_t> __VA_ARGS__ \
   VciCcVCacheWrapper<vci_param, dspin_in_width, dspin_out_width, iss_t>

using namespace soclib::common;

/////////////////////////////////
tmpl(/**/)::VciCcVCacheWrapper(
    sc_module_name name,
    const int proc_id,
    const MappingTable &mtd,
    const IntTab &srcid,
    const size_t cc_global_id,
    const size_t itlb_ways,
    const size_t itlb_sets,
    const size_t dtlb_ways,
    const size_t dtlb_sets,
    const size_t icache_ways,
    const size_t icache_sets,
    const size_t icache_words,
    const size_t dcache_ways,
    const size_t dcache_sets,
    const size_t dcache_words,
    const size_t wbuf_nlines,
    const size_t wbuf_nwords,
    const size_t x_width,
    const size_t y_width,
    const uint32_t max_frozen_cycles,
    const uint32_t debug_start_cycle,
    const bool debug_ok)
    : soclib::caba::BaseModule(name),

      p_clk("p_clk"),
      p_resetn("p_resetn"),
      p_vci("p_vci"),
      p_dspin_m2p("p_dspin_m2p"),
      p_dspin_p2m("p_dspin_p2m"),
      p_dspin_clack("p_dspin_clack"),

      m_cacheability_table( mtd.getCacheabilityTable()),
      m_srcid(mtd.indexForId(srcid)),
      m_cc_global_id(cc_global_id),
      m_nline_width(vci_param::N - (uint32_log2(dcache_words)) - 2),
      m_itlb_ways(itlb_ways),
      m_itlb_sets(itlb_sets),
      m_dtlb_ways(dtlb_ways),
      m_dtlb_sets(dtlb_sets),
      m_icache_ways(icache_ways),
      m_icache_sets(icache_sets),
      m_icache_yzmask((~0) << (uint32_log2(icache_words) + 2)),
      m_icache_words(icache_words),
      m_dcache_ways(dcache_ways),
      m_dcache_sets(dcache_sets),
      m_dcache_yzmask((~0) << (uint32_log2(dcache_words) + 2)),
      m_dcache_words(dcache_words),
      m_x_width(x_width),
      m_y_width(y_width),
      m_proc_id(proc_id),
      m_max_frozen_cycles(max_frozen_cycles),
      m_paddr_nbits(vci_param::N),
      m_debug_start_cycle(debug_start_cycle),
      m_debug_ok(debug_ok),
      m_dcache_paddr_ext_reset(0),
      m_icache_paddr_ext_reset(0),

      r_mmu_ptpr("r_mmu_ptpr"),
      r_mmu_mode("r_mmu_mode"),
      r_mmu_word_lo("r_mmu_word_lo"),
      r_mmu_word_hi("r_mmu_word_hi"),
      r_mmu_ibvar("r_mmu_ibvar"),
      r_mmu_dbvar("r_mmu_dbvar"),
      r_mmu_ietr("r_mmu_ietr"),
      r_mmu_detr("r_mmu_detr"),

      r_icache_fsm("r_icache_fsm"),
      r_icache_fsm_save("r_icache_fsm_save"),
      r_icache_vci_paddr("r_icache_vci_paddr"),
      r_icache_vaddr_save("r_icache_vaddr_save"),

      r_icache_miss_way("r_icache_miss_way"),
      r_icache_miss_set("r_icache_miss_set"),
      r_icache_miss_word("r_icache_miss_word"),
      r_icache_miss_inval("r_icache_miss_inval"),
      r_icache_miss_clack("r_icache_miss_clack"),

      r_icache_cc_way("r_icache_cc_way"),
      r_icache_cc_set("r_icache_cc_set"),
      r_icache_cc_word("r_icache_cc_word"),
      r_icache_cc_need_write("r_icache_cc_need_write"),

      r_icache_flush_count("r_icache_flush_count"),

      r_icache_miss_req("r_icache_miss_req"),
      r_icache_unc_req("r_icache_unc_req"),

      r_icache_tlb_miss_req("r_icache_tlb_read_req"),
      r_icache_tlb_rsp_error("r_icache_tlb_rsp_error"),

      r_icache_cleanup_victim_req("r_icache_cleanup_victim_req"),
      r_icache_cleanup_victim_nline("r_icache_cleanup_victim_nline"),

      r_icache_cc_send_req("r_icache_cc_send_req"),
      r_icache_cc_send_type("r_icache_cc_send_type"),
      r_icache_cc_send_nline("r_icache_cc_send_nline"),
      r_icache_cc_send_way("r_icache_cc_send_way"),
      r_icache_cc_send_updt_tab_idx("r_icache_cc_send_updt_tab_idx"),

      r_dcache_fsm("r_dcache_fsm"),
      r_dcache_fsm_cc_save("r_dcache_fsm_cc_save"),
      r_dcache_fsm_scan_save("r_dcache_fsm_scan_save"),

      r_dcache_wbuf_req("r_dcache_wbuf_req"),
      r_dcache_updt_req("r_dcache_updt_req"),
      r_dcache_save_vaddr("r_dcache_save_vaddr"),
      r_dcache_save_wdata("r_dcache_save_wdata"),
      r_dcache_save_be("r_dcache_save_be"),
      r_dcache_save_paddr("r_dcache_save_paddr"),
      r_dcache_save_cache_way("r_dcache_save_cache_way"),
      r_dcache_save_cache_set("r_dcache_save_cache_set"),
      r_dcache_save_cache_word("r_dcache_save_cache_word"),

      r_dcache_dirty_paddr("r_dcache_dirty_paddr"),
      r_dcache_dirty_way("r_dcache_dirty_way"),
      r_dcache_dirty_set("r_dcache_dirty_set"),

      r_dcache_vci_paddr("r_dcache_vci_paddr"),
      r_dcache_vci_wdata("r_dcache_vci_wdata"),
      r_dcache_vci_miss_req("r_dcache_vci_miss_req"),
      r_dcache_vci_unc_req("r_dcache_vci_unc_req"),
      r_dcache_vci_unc_be("r_dcache_vci_unc_be"),
      r_dcache_vci_unc_write("r_dcache_vci_unc_write"),
      r_dcache_vci_cas_req("r_dcache_vci_cas_req"),
      r_dcache_vci_cas_old("r_dcache_vci_cas_old"),
      r_dcache_vci_cas_new("r_dcache_vci_cas_new"),
      r_dcache_vci_ll_req("r_dcache_vci_ll_req"),
      r_dcache_vci_sc_req("r_dcache_vci_sc_req"),
      r_dcache_vci_sc_data("r_dcache_vci_sc_data"),

      r_dcache_xtn_way("r_dcache_xtn_way"),
      r_dcache_xtn_set("r_dcache_xtn_set"),

      r_dcache_miss_type("r_dcache_miss_type"),
      r_dcache_miss_word("r_dcache_miss_word"),
      r_dcache_miss_way("r_dcache_miss_way"),
      r_dcache_miss_set("r_dcache_miss_set"),
      r_dcache_miss_inval("r_dcache_miss_inval"),

      r_dcache_cc_way("r_dcache_cc_way"),
      r_dcache_cc_set("r_dcache_cc_set"),
      r_dcache_cc_word("r_dcache_cc_word"),
      r_dcache_cc_need_write("r_dcache_cc_need_write"),

      r_dcache_flush_count("r_dcache_flush_count"),

      r_dcache_ll_rsp_count("r_dcache_ll_rsp_count"),

      r_dcache_tlb_vaddr("r_dcache_tlb_vaddr"),
      r_dcache_tlb_ins("r_dcache_tlb_ins"),
      r_dcache_tlb_pte_flags("r_dcache_tlb_pte_flags"),
      r_dcache_tlb_pte_ppn("r_dcache_tlb_pte_ppn"),
      r_dcache_tlb_cache_way("r_dcache_tlb_cache_way"),
      r_dcache_tlb_cache_set("r_dcache_tlb_cache_set"),
      r_dcache_tlb_cache_word("r_dcache_tlb_cache_word"),
      r_dcache_tlb_way("r_dcache_tlb_way"),
      r_dcache_tlb_set("r_dcache_tlb_set"),

      r_dcache_tlb_inval_line("r_dcache_tlb_inval_line"),
      r_dcache_tlb_inval_set("r_dcache_tlb_inval_set"),

      r_dcache_xtn_req("r_dcache_xtn_req"),
      r_dcache_xtn_opcode("r_dcache_xtn_opcode"),

      r_dcache_cleanup_victim_req("r_dcache_cleanup_victim_req"),
      r_dcache_cleanup_victim_nline("r_dcache_cleanup_victim_nline"),

      r_dcache_cc_send_req("r_dcache_cc_send_req"),
      r_dcache_cc_send_type("r_dcache_cc_send_type"),
      r_dcache_cc_send_nline("r_dcache_cc_send_nline"),
      r_dcache_cc_send_way("r_dcache_cc_send_way"),
      r_dcache_cc_send_updt_tab_idx("r_dcache_cc_send_updt_tab_idx"),

      r_vci_cmd_fsm("r_vci_cmd_fsm"),
      r_vci_cmd_min("r_vci_cmd_min"),
      r_vci_cmd_max("r_vci_cmd_max"),
      r_vci_cmd_cpt("r_vci_cmd_cpt"),
      r_vci_cmd_imiss_prio("r_vci_cmd_imiss_prio"),

      r_vci_rsp_fsm("r_vci_rsp_fsm"),
      r_vci_rsp_cpt("r_vci_rsp_cpt"),
      r_vci_rsp_ins_error("r_vci_rsp_ins_error"),
      r_vci_rsp_data_error("r_vci_rsp_data_error"),
      r_vci_rsp_fifo_icache("r_vci_rsp_fifo_icache", 2), // 2 words depth
      r_vci_rsp_fifo_dcache("r_vci_rsp_fifo_dcache", 2), // 2 words depth

      r_cc_send_fsm("r_cc_send_fsm"),
      r_cc_send_last_client("r_cc_send_last_client"),

      r_cc_receive_fsm("r_cc_receive_fsm"),
      r_cc_receive_data_ins("r_cc_receive_data_ins"),
      r_cc_receive_word_idx("r_cc_receive_word_idx"),
      r_cc_receive_updt_fifo_be("r_cc_receive_updt_fifo_be", 2), // 2 words depth
      r_cc_receive_updt_fifo_data("r_cc_receive_updt_fifo_data", 2), // 2 words depth
      r_cc_receive_updt_fifo_eop("r_cc_receive_updt_fifo_eop", 2), // 2 words depth

      r_cc_receive_icache_req("r_cc_receive_icache_req"),
      r_cc_receive_icache_type("r_cc_receive_icache_type"),
      r_cc_receive_icache_way("r_cc_receive_icache_way"),
      r_cc_receive_icache_set("r_cc_receive_icache_set"),
      r_cc_receive_icache_updt_tab_idx("r_cc_receive_icache_updt_tab_idx"),
      r_cc_receive_icache_nline("r_cc_receive_icache_nline"),

      r_cc_receive_dcache_req("r_cc_receive_dcache_req"),
      r_cc_receive_dcache_type("r_cc_receive_dcache_type"),
      r_cc_receive_dcache_way("r_cc_receive_dcache_way"),
      r_cc_receive_dcache_set("r_cc_receive_dcache_set"),
      r_cc_receive_dcache_updt_tab_idx("r_cc_receive_dcache_updt_tab_idx"),
      r_cc_receive_dcache_nline("r_cc_receive_dcache_nline"),

      r_iss(this->name(), proc_id),
      r_wbuf("wbuf", wbuf_nwords, wbuf_nlines, dcache_words ),
      r_icache("icache", icache_ways, icache_sets, icache_words),
      r_dcache("dcache", dcache_ways, dcache_sets, dcache_words),
      r_itlb("itlb", proc_id, itlb_ways,itlb_sets,vci_param::N),
      r_dtlb("dtlb", proc_id, dtlb_ways,dtlb_sets,vci_param::N)
{
    std::cout << "  - Building VciCcVcacheWrapper : " << name << std::endl;

    assert(((icache_words*vci_param::B) < (1 << vci_param::K)) and
             "Need more PLEN bits.");

    assert((vci_param::T > 2) and ((1 << (vci_param::T - 1)) >= (wbuf_nlines)) and
             "Need more TRDID bits.");

    assert((icache_words == dcache_words) and
             "icache_words and dcache_words parameters must be equal");

    assert((itlb_sets == dtlb_sets) and
             "itlb_sets and dtlb_sets parameters must be etqual");

    assert((itlb_ways == dtlb_ways) and
             "itlb_ways and dtlb_ways parameters must be etqual");

    r_mmu_params = (uint32_log2(m_dtlb_ways)   << 29) | (uint32_log2(m_dtlb_sets)   << 25) |
                   (uint32_log2(m_dcache_ways) << 22) | (uint32_log2(m_dcache_sets) << 18) |
                   (uint32_log2(m_itlb_ways)   << 15) | (uint32_log2(m_itlb_sets)   << 11) |
                   (uint32_log2(m_icache_ways) << 8)  | (uint32_log2(m_icache_sets) << 4)  |
                   (uint32_log2(m_icache_words << 2));

    r_mmu_release = (uint32_t) (1 << 16) | 0x1;

    r_dcache_in_tlb       = new bool[dcache_ways * dcache_sets];
    r_dcache_contains_ptd = new bool[dcache_ways * dcache_sets];

    SC_METHOD(transition);
    dont_initialize();
    sensitive << p_clk.pos();

    SC_METHOD(genMoore);
    dont_initialize();
    sensitive << p_clk.neg();

    typename iss_t::CacheInfo cache_info;
    cache_info.has_mmu = true;
    cache_info.icache_line_size = icache_words * sizeof(uint32_t);
    cache_info.icache_assoc = icache_ways;
    cache_info.icache_n_lines = icache_sets;
    cache_info.dcache_line_size = dcache_words * sizeof(uint32_t);
    cache_info.dcache_assoc = dcache_ways;
    cache_info.dcache_n_lines = dcache_sets;
    r_iss.setCacheInfo(cache_info);
}

/////////////////////////////////////
tmpl(/**/)::~VciCcVCacheWrapper()
/////////////////////////////////////
{
    delete [] r_dcache_in_tlb;
    delete [] r_dcache_contains_ptd;
}

////////////////////////
tmpl(void)::print_cpi()
////////////////////////
{
    std::cout << name() << " CPI = "
        << (float)m_cpt_total_cycles/(m_cpt_total_cycles - m_cpt_frz_cycles) << std::endl ;
}

////////////////////////////////////
tmpl(void)::print_trace(size_t mode)
////////////////////////////////////
{
    // b0 : write buffer trace
    // b1 : dump processor registers
    // b2 : dcache trace
    // b3 : icache trace
    // b4 : dtlb trace
    // b5 : itlb trace
    // b6 : SR (ISS register 32)

    std::cout << std::dec << "PROC " << name() << std::endl;

    std::cout << "  " << m_ireq << std::endl;
    std::cout << "  " << m_irsp << std::endl;
    std::cout << "  " << m_dreq << std::endl;
    std::cout << "  " << m_drsp << std::endl;

    std::cout << "  " << icache_fsm_state_str[r_icache_fsm.read()]
              << " | " << dcache_fsm_state_str[r_dcache_fsm.read()]
              << " | " << cmd_fsm_state_str[r_vci_cmd_fsm.read()]
              << " | " << rsp_fsm_state_str[r_vci_rsp_fsm.read()]
              << " | " << cc_receive_fsm_state_str[r_cc_receive_fsm.read()]
              << " | " << cc_send_fsm_state_str[r_cc_send_fsm.read()]
              << " | MMU = " << r_mmu_mode.read();

    if (r_dcache_updt_req.read()) std::cout << " | P1_UPDT";
    if (r_dcache_wbuf_req.read()) std::cout << " | P1_WBUF";
    std::cout << std::endl;

    if (mode & 0x01)
    {
        if (r_icache_miss_req.read())     std::cout << "  IMISS_REQ" << std::endl;
        if (r_icache_unc_req.read())      std::cout << "  IUNC_REQ" << std::endl;
        if (r_dcache_vci_miss_req.read()) std::cout << "  DMISS_REQ" << std::endl;
        if (r_dcache_vci_unc_req.read())  std::cout << "  DUNC_REQ" << std::endl;

        r_wbuf.printTrace((mode >> 1) & 1);
    }
    if (mode & 0x02)
    {
        r_iss.dump();
    }
    if (mode & 0x04)
    {
        std::cout << "  Data Cache" << std::endl;
        r_dcache.printTrace();
    }
    if (mode & 0x08)
    {
        std::cout << "  Instruction Cache" << std::endl;
        r_icache.printTrace();
    }
    if (mode & 0x10)
    {
        std::cout << "  Data TLB" << std::endl;
        r_dtlb.printTrace();
    }
    if (mode & 0x20)
    {
        std::cout << "  Instruction TLB" << std::endl;
        r_itlb.printTrace();
    }
    if (mode & 0x40)
    {
        uint32_t status = r_iss.debugGetRegisterValue(32);
        std::cout << name();
        if (status != m_previous_status ) std::cout << " NEW ";
        std::cout << " status = " << std::hex << status << " " << std::endl;
        m_previous_status = status;
    }
}

//////////////////////////////////////////
tmpl(void)::cache_monitor(paddr_t addr)
//////////////////////////////////////////
{
    bool cache_hit;
    size_t cache_way = 0;
    size_t cache_set = 0;
    size_t cache_word = 0;
    uint32_t cache_rdata = 0;

    cache_hit = r_dcache.read_neutral(addr,
                                      &cache_rdata,
                                      &cache_way,
                                      &cache_set,
                                      &cache_word);

    if (cache_hit != m_debug_previous_d_hit)
    {
        std::cout << "Monitor PROC " << name()
                  << " DCACHE at cycle " << std::dec << m_cpt_total_cycles
                  << " / HIT = " << cache_hit
                  << " / PADDR = " << std::hex << addr
                  << " / DATA = " << cache_rdata
                  << " / WAY = " << cache_way << std::endl;
        m_debug_previous_d_hit = cache_hit;
    }

    cache_hit = r_icache.read_neutral(addr,
                                      &cache_rdata,
                                      &cache_way,
                                      &cache_set,
                                      &cache_word);

    if (cache_hit != m_debug_previous_i_hit)
    {
        std::cout << "Monitor PROC " << name()
                  << " ICACHE at cycle " << std::dec << m_cpt_total_cycles
                  << " / HIT = " << cache_hit
                  << " / PADDR = " << std::hex << addr
                  << " / DATA = " << cache_rdata
                  << " / WAY = " << cache_way << std::endl;
        m_debug_previous_i_hit = cache_hit;
    }
}

/*
////////////////////////
tmpl(void)::print_stats()
////////////////////////
{
    float run_cycles = (float)(m_cpt_total_cycles - m_cpt_frz_cycles);
    std::cout << name() << std::endl
        << "- CPI                    = " << (float)m_cpt_total_cycles/run_cycles << std::endl
        << "- READ RATE              = " << (float)m_cpt_read/run_cycles << std::endl
        << "- WRITE RATE             = " << (float)m_cpt_write/run_cycles << std::endl
        << "- IMISS_RATE             = " << (float)m_cpt_ins_miss/m_cpt_ins_read << std::endl
        << "- DMISS RATE             = " << (float)m_cpt_data_miss/(m_cpt_read-m_cpt_unc_read) << std::endl
        << "- INS MISS COST          = " << (float)m_cost_ins_miss_frz/m_cpt_ins_miss << std::endl
        << "- DATA MISS COST         = " << (float)m_cost_data_miss_frz/m_cpt_data_miss << std::endl
        << "- WRITE COST             = " << (float)m_cost_write_frz/m_cpt_write << std::endl
        << "- UNC COST               = " << (float)m_cost_unc_read_frz/m_cpt_unc_read << std::endl
        << "- UNCACHED READ RATE     = " << (float)m_cpt_unc_read/m_cpt_read << std::endl
        << "- CACHED WRITE RATE      = " << (float)m_cpt_write_cached/m_cpt_write << std::endl
        << "- INS TLB MISS RATE      = " << (float)m_cpt_ins_tlb_miss/m_cpt_ins_tlb_read << std::endl
        << "- DATA TLB MISS RATE     = " << (float)m_cpt_data_tlb_miss/m_cpt_data_tlb_read << std::endl
        << "- ITLB MISS COST         = " << (float)m_cost_ins_tlb_miss_frz/m_cpt_ins_tlb_miss << std::endl
        << "- DTLB MISS COST         = " << (float)m_cost_data_tlb_miss_frz/m_cpt_data_tlb_miss << std::endl
        << "- ITLB UPDATE ACC COST   = " << (float)m_cost_ins_tlb_update_acc_frz/m_cpt_ins_tlb_update_acc << std::endl
        << "- DTLB UPDATE ACC COST   = " << (float)m_cost_data_tlb_update_acc_frz/m_cpt_data_tlb_update_acc << std::endl
        << "- DTLB UPDATE DIRTY COST = " << (float)m_cost_data_tlb_update_dirty_frz/m_cpt_data_tlb_update_dirty << std::endl
        << "- ITLB HIT IN DCACHE RATE= " << (float)m_cpt_ins_tlb_hit_dcache/m_cpt_ins_tlb_miss << std::endl
        << "- DTLB HIT IN DCACHE RATE= " << (float)m_cpt_data_tlb_hit_dcache/m_cpt_data_tlb_miss << std::endl
        << "- DCACHE FROZEN BY ITLB  = " << (float)m_cost_ins_tlb_occup_cache_frz/m_cpt_dcache_frz_cycles << std::endl
        << "- DCACHE FOR TLB %       = " << (float)m_cpt_tlb_occup_dcache/(m_dcache_ways*m_dcache_sets) << std::endl
        << "- NB CC BROADCAST        = " << m_cpt_cc_broadcast << std::endl
        << "- NB CC UPDATE DATA      = " << m_cpt_cc_update_data << std::endl
        << "- NB CC INVAL DATA       = " << m_cpt_cc_inval_data << std::endl
        << "- NB CC INVAL INS        = " << m_cpt_cc_inval_ins << std::endl
        << "- CC BROADCAST COST      = " << (float)m_cost_broadcast_frz/m_cpt_cc_broadcast << std::endl
        << "- CC UPDATE DATA COST    = " << (float)m_cost_updt_data_frz/m_cpt_cc_update_data << std::endl
        << "- CC INVAL DATA COST     = " << (float)m_cost_inval_data_frz/m_cpt_cc_inval_data << std::endl
        << "- CC INVAL INS COST      = " << (float)m_cost_inval_ins_frz/m_cpt_cc_inval_ins << std::endl
        << "- NB CC CLEANUP DATA     = " << m_cpt_cc_cleanup_data << std::endl
        << "- NB CC CLEANUP INS      = " << m_cpt_cc_cleanup_ins << std::endl
        << "- IMISS TRANSACTION      = " << (float)m_cost_imiss_transaction/m_cpt_imiss_transaction << std::endl
        << "- DMISS TRANSACTION      = " << (float)m_cost_dmiss_transaction/m_cpt_dmiss_transaction << std::endl
        << "- UNC TRANSACTION        = " << (float)m_cost_unc_transaction/m_cpt_unc_transaction << std::endl
        << "- WRITE TRANSACTION      = " << (float)m_cost_write_transaction/m_cpt_write_transaction << std::endl
        << "- WRITE LENGTH           = " << (float)m_length_write_transaction/m_cpt_write_transaction << std::endl
        << "- ITLB MISS TRANSACTION  = " << (float)m_cost_itlbmiss_transaction/m_cpt_itlbmiss_transaction << std::endl
        << "- DTLB MISS TRANSACTION  = " << (float)m_cost_dtlbmiss_transaction/m_cpt_dtlbmiss_transaction << std::endl;
}

////////////////////////
tmpl(void)::clear_stats()
////////////////////////
{
    m_cpt_dcache_data_read  = 0;
    m_cpt_dcache_data_write = 0;
    m_cpt_dcache_dir_read   = 0;
    m_cpt_dcache_dir_write  = 0;
    m_cpt_icache_data_read  = 0;
    m_cpt_icache_data_write = 0;
    m_cpt_icache_dir_read   = 0;
    m_cpt_icache_dir_write  = 0;

    m_cpt_frz_cycles        = 0;
    m_cpt_dcache_frz_cycles = 0;
    m_cpt_total_cycles      = 0;

    m_cpt_read         = 0;
    m_cpt_write        = 0;
    m_cpt_data_miss    = 0;
    m_cpt_ins_miss     = 0;
    m_cpt_unc_read     = 0;
    m_cpt_write_cached = 0;
    m_cpt_ins_read     = 0;

    m_cost_write_frz     = 0;
    m_cost_data_miss_frz = 0;
    m_cost_unc_read_frz  = 0;
    m_cost_ins_miss_frz  = 0;

    m_cpt_imiss_transaction      = 0;
    m_cpt_dmiss_transaction      = 0;
    m_cpt_unc_transaction        = 0;
    m_cpt_write_transaction      = 0;
    m_cpt_icache_unc_transaction = 0;

    m_cost_imiss_transaction      = 0;
    m_cost_dmiss_transaction      = 0;
    m_cost_unc_transaction        = 0;
    m_cost_write_transaction      = 0;
    m_cost_icache_unc_transaction = 0;
    m_length_write_transaction    = 0;

    m_cpt_ins_tlb_read       = 0;
    m_cpt_ins_tlb_miss       = 0;
    m_cpt_ins_tlb_update_acc = 0;

    m_cpt_data_tlb_read         = 0;
    m_cpt_data_tlb_miss         = 0;
    m_cpt_data_tlb_update_acc   = 0;
    m_cpt_data_tlb_update_dirty = 0;
    m_cpt_ins_tlb_hit_dcache    = 0;
    m_cpt_data_tlb_hit_dcache   = 0;
    m_cpt_ins_tlb_occup_cache   = 0;
    m_cpt_data_tlb_occup_cache  = 0;

    m_cost_ins_tlb_miss_frz          = 0;
    m_cost_data_tlb_miss_frz         = 0;
    m_cost_ins_tlb_update_acc_frz    = 0;
    m_cost_data_tlb_update_acc_frz   = 0;
    m_cost_data_tlb_update_dirty_frz = 0;
    m_cost_ins_tlb_occup_cache_frz   = 0;
    m_cost_data_tlb_occup_cache_frz  = 0;

    m_cpt_itlbmiss_transaction      = 0;
    m_cpt_itlb_ll_transaction       = 0;
    m_cpt_itlb_sc_transaction       = 0;
    m_cpt_dtlbmiss_transaction      = 0;
    m_cpt_dtlb_ll_transaction       = 0;
    m_cpt_dtlb_sc_transaction       = 0;
    m_cpt_dtlb_ll_dirty_transaction = 0;
    m_cpt_dtlb_sc_dirty_transaction = 0;

    m_cost_itlbmiss_transaction      = 0;
    m_cost_itlb_ll_transaction       = 0;
    m_cost_itlb_sc_transaction       = 0;
    m_cost_dtlbmiss_transaction      = 0;
    m_cost_dtlb_ll_transaction       = 0;
    m_cost_dtlb_sc_transaction       = 0;
    m_cost_dtlb_ll_dirty_transaction = 0;
    m_cost_dtlb_sc_dirty_transaction = 0;

    m_cpt_cc_update_data = 0;
    m_cpt_cc_inval_ins   = 0;
    m_cpt_cc_inval_data  = 0;
    m_cpt_cc_broadcast   = 0;

    m_cost_updt_data_frz  = 0;
    m_cost_inval_ins_frz  = 0;
    m_cost_inval_data_frz = 0;
    m_cost_broadcast_frz  = 0;

    m_cpt_cc_cleanup_data = 0;
    m_cpt_cc_cleanup_ins  = 0;
}

*/

/////////////////////////
tmpl(void)::transition()
/////////////////////////
{
    if (not p_resetn.read())
    {
        r_iss.reset();
        r_wbuf.reset();
        r_icache.reset();
        r_dcache.reset();
        r_itlb.reset();
        r_dtlb.reset();

        r_dcache_fsm     = DCACHE_IDLE;
        r_icache_fsm     = ICACHE_IDLE;
        r_vci_cmd_fsm    = CMD_IDLE;
        r_vci_rsp_fsm    = RSP_IDLE;
        r_cc_receive_fsm = CC_RECEIVE_IDLE;
        r_cc_send_fsm    = CC_SEND_IDLE;

        // reset data physical address extension
        r_dcache_paddr_ext = m_dcache_paddr_ext_reset;

        // reset inst physical address extension
        r_icache_paddr_ext = m_icache_paddr_ext_reset;

        // reset dcache directory extension
        for (size_t i = 0; i< m_dcache_ways * m_dcache_sets; i++)
        {
            r_dcache_in_tlb[i] = false;
            r_dcache_contains_ptd[i] = false;
        }

        // Response FIFOs and cleanup buffer
        r_vci_rsp_fifo_icache.init();
        r_vci_rsp_fifo_dcache.init();

        // ICACHE & DCACHE activated
        // ITLB & DTLB desactivated
        r_mmu_mode = 0x3;

        // No request from ICACHE FSM to CMD FSM
        r_icache_miss_req          = false;
        r_icache_unc_req           = false;

        // No request from ICACHE_FSM to DCACHE FSM
        r_icache_tlb_miss_req      = false;

        // No request from ICACHE_FSM to CC_SEND FSM
        r_icache_cc_send_req       = false;
        r_icache_cleanup_victim_req = false;

        r_icache_clack_req         = false;

        // No pending write in pipeline
        r_dcache_wbuf_req          = false;
        r_dcache_updt_req          = false;

        // No request from DCACHE_FSM to CMD_FSM
        r_dcache_vci_miss_req      = false;
        r_dcache_vci_unc_req       = false;
        r_dcache_vci_cas_req       = false;
        r_dcache_vci_ll_req        = false;
        r_dcache_vci_sc_req        = false;

        // No processor XTN request pending
        r_dcache_xtn_req           = false;

        // No request from DCACHE FSM to CC_SEND FSM
        r_dcache_cc_send_req        = false;
        r_dcache_cleanup_victim_req = false;

        r_dcache_clack_req         = false;

        // No request from CC_RECEIVE FSM to ICACHE/DCACHE FSMs
        r_cc_receive_icache_req    = false;
        r_cc_receive_dcache_req    = false;

        // last cc_send client was dcache
        r_cc_send_last_client      = false;

        // No pending cleanup after a replacement
        r_icache_miss_clack        = false;
        r_dcache_miss_clack        = false;

        // No signalisation of a coherence request matching a pending miss
        r_icache_miss_inval        = false;
        r_dcache_miss_inval        = false;

        r_dspin_clack_req          = false;

        // No signalisation  of errors
        r_vci_rsp_ins_error        = false;
        r_vci_rsp_data_error       = false;

        // Debug variables
        m_debug_previous_i_hit     = false;
        m_debug_previous_d_hit     = false;
        m_debug_icache_fsm         = false;
        m_debug_dcache_fsm         = false;
        m_debug_cmd_fsm            = false;

        // activity counters
        m_cpt_dcache_data_read  = 0;
        m_cpt_dcache_data_write = 0;
        m_cpt_dcache_dir_read   = 0;
        m_cpt_dcache_dir_write  = 0;
        m_cpt_icache_data_read  = 0;
        m_cpt_icache_data_write = 0;
        m_cpt_icache_dir_read   = 0;
        m_cpt_icache_dir_write  = 0;

        m_cpt_frz_cycles        = 0;
        m_cpt_total_cycles      = 0;
        m_cpt_stop_simulation   = 0;

        m_cpt_data_miss         = 0;
        m_cpt_ins_miss          = 0;
        m_cpt_unc_read          = 0;
        m_cpt_write_cached      = 0;
        m_cpt_ins_read          = 0;

        m_cost_write_frz        = 0;
        m_cost_data_miss_frz    = 0;
        m_cost_unc_read_frz     = 0;
        m_cost_ins_miss_frz     = 0;

        m_cpt_imiss_transaction = 0;
        m_cpt_dmiss_transaction = 0;
        m_cpt_unc_transaction   = 0;
        m_cpt_write_transaction = 0;
        m_cpt_icache_unc_transaction = 0;

        m_cost_imiss_transaction      = 0;
        m_cost_dmiss_transaction      = 0;
        m_cost_unc_transaction        = 0;
        m_cost_write_transaction      = 0;
        m_cost_icache_unc_transaction = 0;
        m_length_write_transaction    = 0;

        m_cpt_ins_tlb_read       = 0;
        m_cpt_ins_tlb_miss       = 0;
        m_cpt_ins_tlb_update_acc = 0;

        m_cpt_data_tlb_read         = 0;
        m_cpt_data_tlb_miss         = 0;
        m_cpt_data_tlb_update_acc   = 0;
        m_cpt_data_tlb_update_dirty = 0;
        m_cpt_ins_tlb_hit_dcache    = 0;
        m_cpt_data_tlb_hit_dcache   = 0;
        m_cpt_ins_tlb_occup_cache   = 0;
        m_cpt_data_tlb_occup_cache  = 0;

        m_cost_ins_tlb_miss_frz          = 0;
        m_cost_data_tlb_miss_frz         = 0;
        m_cost_ins_tlb_update_acc_frz    = 0;
        m_cost_data_tlb_update_acc_frz   = 0;
        m_cost_data_tlb_update_dirty_frz = 0;
        m_cost_ins_tlb_occup_cache_frz   = 0;
        m_cost_data_tlb_occup_cache_frz  = 0;

        m_cpt_ins_tlb_inval       = 0;
        m_cpt_data_tlb_inval      = 0;
        m_cost_ins_tlb_inval_frz  = 0;
        m_cost_data_tlb_inval_frz = 0;

        m_cpt_cc_broadcast   = 0;

        m_cost_updt_data_frz  = 0;
        m_cost_inval_ins_frz  = 0;
        m_cost_inval_data_frz = 0;
        m_cost_broadcast_frz  = 0;

        m_cpt_cc_cleanup_data = 0;
        m_cpt_cc_cleanup_ins  = 0;

        m_cpt_itlbmiss_transaction      = 0;
        m_cpt_itlb_ll_transaction       = 0;
        m_cpt_itlb_sc_transaction       = 0;
        m_cpt_dtlbmiss_transaction      = 0;
        m_cpt_dtlb_ll_transaction       = 0;
        m_cpt_dtlb_sc_transaction       = 0;
        m_cpt_dtlb_ll_dirty_transaction = 0;
        m_cpt_dtlb_sc_dirty_transaction = 0;

        m_cost_itlbmiss_transaction      = 0;
        m_cost_itlb_ll_transaction       = 0;
        m_cost_itlb_sc_transaction       = 0;
        m_cost_dtlbmiss_transaction      = 0;
        m_cost_dtlb_ll_transaction       = 0;
        m_cost_dtlb_sc_transaction       = 0;
        m_cost_dtlb_ll_dirty_transaction = 0;
        m_cost_dtlb_sc_dirty_transaction = 0;
/*
        m_cpt_dcache_frz_cycles = 0;
        m_cpt_read = 0;
        m_cpt_write = 0;
        m_cpt_cc_update_data = 0;
        m_cpt_cc_inval_ins   = 0;
        m_cpt_cc_inval_data  = 0;
*/

        for (uint32_t i = 0; i < 32; ++i) m_cpt_fsm_icache[i] = 0;
        for (uint32_t i = 0; i < 32; ++i) m_cpt_fsm_dcache[i] = 0;
        for (uint32_t i = 0; i < 32; ++i) m_cpt_fsm_cmd[i] = 0;
        for (uint32_t i = 0; i < 32; ++i) m_cpt_fsm_rsp[i] = 0;

        // init the llsc reservation buffer
        r_dcache_llsc_valid = false;
        m_monitor_ok = false;

        return;
    }

    // Response FIFOs default values
    bool     vci_rsp_fifo_icache_get  = false;
    bool     vci_rsp_fifo_icache_put  = false;
    uint32_t vci_rsp_fifo_icache_data = 0;

    bool     vci_rsp_fifo_dcache_get  = false;
    bool     vci_rsp_fifo_dcache_put  = false;
    uint32_t vci_rsp_fifo_dcache_data = 0;

    // updt fifo
    bool     cc_receive_updt_fifo_get  = false;
    bool     cc_receive_updt_fifo_put  = false;
    uint32_t cc_receive_updt_fifo_be   = 0;
    uint32_t cc_receive_updt_fifo_data = 0;
    bool     cc_receive_updt_fifo_eop  = false;

#ifdef INSTRUMENTATION
    m_cpt_fsm_dcache [r_dcache_fsm.read() ] ++;
    m_cpt_fsm_icache [r_icache_fsm.read() ] ++;
    m_cpt_fsm_cmd    [r_vci_cmd_fsm.read()] ++;
    m_cpt_fsm_rsp    [r_vci_rsp_fsm.read()] ++;
    m_cpt_fsm_tgt    [r_tgt_fsm.read()    ] ++;
    m_cpt_fsm_cleanup[r_cleanup_cmd_fsm.read()] ++;
#endif

    m_cpt_total_cycles++;

    m_debug_icache_fsm = m_debug_icache_fsm ||
        ((m_cpt_total_cycles > m_debug_start_cycle) and m_debug_ok);
    m_debug_dcache_fsm = m_debug_dcache_fsm ||
        ((m_cpt_total_cycles > m_debug_start_cycle) and m_debug_ok);
    m_debug_cmd_fsm = m_debug_cmd_fsm ||
        ((m_cpt_total_cycles > m_debug_start_cycle) and m_debug_ok);

    /////////////////////////////////////////////////////////////////////
    // Get data and instruction requests from processor
    ///////////////////////////////////////////////////////////////////////

    r_iss.getRequests(m_ireq, m_dreq);

    ////////////////////////////////////////////////////////////////////////////////////
    //      ICACHE_FSM
    //
    // 1/ Coherence operations
    //    They are handled as interrupts generated by the CC_RECEIVE FSM.
    //    - There is a coherence request when r_tgt_icache_req is set.
    //    They are taken in IDLE, MISS_WAIT, MISS_DIR_UPDT, UNC_WAIT, states.
    //    - There is a cleanup ack request when r_cleanup_icache_req is set.
    //    They are taken in IDLE, MISS_SELECT, MISS_CLEAN, MISS_WAIT,
    //    MISS_DATA_UPDT, MISS_DIR_UPDT and UNC_WAIT states.
    //    - For both types of requests, actions associated to the pre-empted state
    //    are not executed. The DCACHE FSM goes to the proper sub-FSM (CC_CHECK
    //    or CC_CLACK) to execute the requested coherence operation, and returns
    //    to the pre-empted state.
    //
    // 2/ Processor requests
    //    They are taken in IDLE state only. In case of cache miss, or uncacheable
    //    instruction, the ICACHE FSM request a VCI transaction to CMD FSM,
    //    using the r_icache_miss_req or r_icache_unc_req flip-flops. These
    //    flip-flops are reset when the transaction starts.
    //    - In case of miss the ICACHE FSM  goes to the ICACHE_MISS_SELECT state
    //    to select a slot and possibly request a cleanup transaction to the CC_SEND FSM.
    //    It goes next to the ICACHE_MISS_WAIT state waiting a response from RSP FSM,
    //    The availability of the missing cache line is signaled by the response fifo,
    //    and the cache update is done (one word per cycle) in the ICACHE_MISS_DATA_UPDT
    //    and ICACHE_MISS_DIR_UPDT states.
    //    - In case of uncacheable instruction, the ICACHE FSM goes to ICACHE_UNC_WAIT
    //    to wait the response from the RSP FSM, through the response fifo.
    //    The missing instruction is directly returned to processor in this state.
    //
    // 3/ TLB miss
    //    In case of tlb miss, the ICACHE FSM request to the DCACHE FSM to update the
    //    ITLB using the r_icache_tlb_miss_req flip-flop and the r_icache_tlb_miss_vaddr
    //    register, and goes to the ICACHE_TLB_WAIT state.
    //    The tlb update is entirely done by the DCACHE FSM (who becomes the owner
    //    of ITLB until the update is completed, and reset r_icache_tlb_miss_req
    //    to signal the completion.
    //
    // 4/ XTN requests
    //    The DCACHE FSM signals XTN processor requests to ICACHE_FSM
    //    using the r_dcache_xtn_req flip-flop.
    //    The request opcode and the address to be invalidated are transmitted
    //    in the r_dcache_xtn_opcode and r_dcache_save_wdata registers respectively.
    //    The r_dcache_xtn_req flip-flop is reset by the ICACHE_FSM when the operation
    //    is completed.
    //
    // 5/ Error Handling
    //    The r_vci_rsp_ins_error flip-flop is set by the RSP FSM in case of bus error
    //    in a cache miss or uncacheable read VCI transaction. Nothing is written
    //    in the response fifo. This flip-flop is reset by the ICACHE-FSM.
    ////////////////////////////////////////////////////////////////////////////////////////

    // default value for m_irsp
    m_irsp.valid = false;
    m_irsp.error = false;
    m_irsp.instruction = 0;

    switch (r_icache_fsm.read())
    {
    /////////////////
    case ICACHE_IDLE:   // In this state, we handle processor requests, XTN requests,
                        // and coherence requests with a fixed priority:
                        // 1/ Coherence requests                        => ICACHE_CC_CHECK
                        // 2/ XTN processor requests (from DCACHE FSM)  => ICACHE_XTN_*
                        // 3/ tlb miss                                  => ICACHE_TLB_WAIT
                        // 4/ cacheable read miss                       => ICACHE_MISS_SELECT
                        // 5/ uncacheable read miss                     => ICACHE_UNC_REQ
    {
        // coherence clack interrupt
        if (r_icache_clack_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        // coherence interrupt
        if (r_cc_receive_icache_req.read() and not r_icache_cc_send_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        // XTN requests sent by DCACHE FSM
        // These request are not executed in this IDLE state (except XTN_INST_PADDR_EXT),
        // because they require access to icache or itlb, that are already accessed
        if (r_dcache_xtn_req.read())
        {
            if ((int) r_dcache_xtn_opcode.read() == (int) iss_t::XTN_PTPR )
            {
                r_icache_fsm = ICACHE_XTN_TLB_FLUSH;
            }
            else if ((int) r_dcache_xtn_opcode.read() == (int) iss_t::XTN_ICACHE_FLUSH)
            {
                r_icache_flush_count = 0;
                r_icache_fsm = ICACHE_XTN_CACHE_FLUSH;
            }
            else if ((int) r_dcache_xtn_opcode.read() == (int) iss_t::XTN_ITLB_INVAL)
            {
                r_icache_fsm = ICACHE_XTN_TLB_INVAL;
            }
            else if ((int) r_dcache_xtn_opcode.read() == (int) iss_t::XTN_ICACHE_INVAL)
            {
                r_icache_fsm = ICACHE_XTN_CACHE_INVAL_VA;
            }
            else if ((int) r_dcache_xtn_opcode.read() == (int) iss_t::XTN_MMU_ICACHE_PA_INV)
            {
                uint64_t pa = ((uint64_t)r_mmu_word_hi.read() << 32) |
                              ((uint64_t)r_mmu_word_lo.read());

                r_icache_vci_paddr = (paddr_t)pa;
                r_icache_fsm = ICACHE_XTN_CACHE_INVAL_PA;
            }
            else if ((int) r_dcache_xtn_opcode.read() == (int) iss_t::XTN_INST_PADDR_EXT)
            {
                r_icache_paddr_ext = r_dcache_save_wdata.read();
                r_dcache_xtn_req   = false;
            }
            else
            {
               assert(false and
               "undefined XTN request received by ICACHE FSM");
            }
            break;
        } // end if xtn_req

        // processor request
        if (m_ireq.valid )
        {
            bool       cacheable;
            paddr_t    paddr;
            bool       tlb_hit = false;
            pte_info_t tlb_flags;
            size_t     tlb_way;
            size_t     tlb_set;
            paddr_t    tlb_nline;
            uint32_t   cache_inst = 0;
            size_t     cache_way;
            size_t     cache_set;
            size_t     cache_word;
            int        cache_state = CACHE_SLOT_STATE_EMPTY;

            // We register processor request
            r_icache_vaddr_save = m_ireq.addr;
            paddr = (paddr_t) m_ireq.addr;

            // sytematic itlb access (if activated)
            if (r_mmu_mode.read() & INS_TLB_MASK)
            {

#ifdef INSTRUMENTATION
                m_cpt_itlb_read++;
#endif
                tlb_hit = r_itlb.translate(m_ireq.addr,
                                           &paddr,
                                           &tlb_flags,
                                           &tlb_nline, // unused
                                           &tlb_way,   // unused
                                           &tlb_set);  // unused
            }
            else if (vci_param::N > 32)
            {
                paddr = paddr | ((paddr_t) r_icache_paddr_ext.read() << 32);
            }

            // systematic icache access (if activated)
            if (r_mmu_mode.read() & INS_CACHE_MASK)
            {


#ifdef INSTRUMENTATION
                m_cpt_icache_data_read++;
                m_cpt_icache_dir_read++;
#endif
                r_icache.read(paddr,
                              &cache_inst,
                              &cache_way,
                              &cache_set,
                              &cache_word,
                              &cache_state);
            }

            // We compute cacheability and check access rights:
            // - If MMU activated : cacheability is defined by the C bit in the PTE,
            //   and the access rights are defined by the U and X bits in the PTE.
            // - If MMU not activated : cacheability is defined by the segment table,
            //   and there is no access rights checking

            if (not (r_mmu_mode.read() & INS_TLB_MASK)) // tlb not activated:
            {
                // cacheability
                if   (not (r_mmu_mode.read() & INS_CACHE_MASK)) cacheable = false;
                else cacheable = m_cacheability_table[(uint64_t) m_ireq.addr];
            }
            else // itlb activated
            {
                if (tlb_hit) // ITLB hit
                {
                    // cacheability
                    if (not (r_mmu_mode.read() & INS_CACHE_MASK)) cacheable = false;
                    else  cacheable = tlb_flags.c;

                    // access rights checking
                    if (not tlb_flags.u && (m_ireq.mode == iss_t::MODE_USER))
                    {

#if DEBUG_ICACHE
if ( m_debug_icache_fsm )
std::cout << "  <PROC " << name() << " ICACHE_IDLE> MMU Privilege Violation"
          << " : PADDR = " << std::hex << paddr << std::endl;
#endif
                        r_mmu_ietr          = MMU_READ_PRIVILEGE_VIOLATION;
                        r_mmu_ibvar         = m_ireq.addr;
                        m_irsp.valid        = true;
                        m_irsp.error        = true;
                        m_irsp.instruction  = 0;
                        break;
                    }
                    else if (not tlb_flags.x)
                    {

#if DEBUG_ICACHE
if ( m_debug_icache_fsm )
std::cout << "  <PROC " << name() << " ICACHE_IDLE> MMU Executable Violation"
          << " : PADDR = " << std::hex << paddr << std::endl;
#endif
                        r_mmu_ietr          = MMU_READ_EXEC_VIOLATION;
                        r_mmu_ibvar         = m_ireq.addr;
                        m_irsp.valid        = true;
                        m_irsp.error        = true;
                        m_irsp.instruction  = 0;
                        break;
                    }
                }
                else // ITLB miss
                {

#ifdef INSTRUMENTATION
                    m_cpt_itlb_miss++;
#endif
                    r_icache_fsm          = ICACHE_TLB_WAIT;
                    r_icache_tlb_miss_req = true;
                    break;
                }
            } // end if itlb activated

            // physical address registration
            r_icache_vci_paddr = paddr;

            // Finally, we send the response to processor, and compute next state
            if (cacheable)
            {
                if (cache_state == CACHE_SLOT_STATE_EMPTY) // cache miss
                {

#ifdef INSTRUMENTATION
                    m_cpt_icache_miss++;
#endif
                    // we request a VCI transaction
                    r_icache_fsm = ICACHE_MISS_SELECT;
#if DEBUG_ICACHE
                    if (m_debug_icache_fsm)
                        std::cout << "  <PROC " << name() << " ICACHE_IDLE> READ MISS in icache"
                            << " : PADDR = " << std::hex << paddr << std::endl;
#endif
                   r_icache_miss_req = true;
                }
                else if (cache_state == CACHE_SLOT_STATE_ZOMBI ) // pending cleanup
                {
                    // stalled until cleanup is acknowledged
                    r_icache_fsm = ICACHE_IDLE;
                }
                else // cache hit
                {

#ifdef INSTRUMENTATION
                    m_cpt_ins_read++;
#endif
                    // return instruction to processor
                    m_irsp.valid       = true;
                    m_irsp.instruction = cache_inst;
                    r_icache_fsm       = ICACHE_IDLE;
#if DEBUG_ICACHE
                    if (m_debug_icache_fsm)
                        std::cout << "  <PROC " << name() << " ICACHE_IDLE> READ HIT in icache"
                            << " : PADDR = " << std::hex << paddr
                            << " / INST  = " << cache_inst << std::endl;
#endif
                }
            }
            else // non cacheable read
            {
                r_icache_unc_req = true;
                r_icache_fsm     = ICACHE_UNC_WAIT;

#if DEBUG_ICACHE
                if (m_debug_icache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " ICACHE_IDLE> READ UNCACHEABLE in icache"
                        << " : PADDR = " << std::hex << paddr << std::endl;
                }
#endif
            }
        }    // end if m_ireq.valid
        break;
    }
    /////////////////////
    case ICACHE_TLB_WAIT:   // Waiting the itlb update by the DCACHE FSM after a tlb miss
                            // the itlb is udated by the DCACHE FSM, as well as the
                            // r_mmu_ietr and r_mmu_ibvar registers in case of error.
                            // the itlb is not accessed by ICACHE FSM until DCACHE FSM
                            // reset the r_icache_tlb_miss_req flip-flop
                            // external coherence request are accepted in this state.
    {
        // coherence clack interrupt
        if (r_icache_clack_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        // coherence interrupt
        if (r_cc_receive_icache_req.read() and not r_icache_cc_send_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        if (m_ireq.valid) m_cost_ins_tlb_miss_frz++;

        // DCACHE FSM signals response by reseting the request flip-flop
        if (not r_icache_tlb_miss_req.read())
        {
            if (r_icache_tlb_rsp_error.read()) // error reported : tlb not updated
            {
                r_icache_tlb_rsp_error = false;
                m_irsp.error = true;
                m_irsp.valid = true;
                r_icache_fsm = ICACHE_IDLE;
            }
            else // tlb updated : return to IDLE state
            {
                r_icache_fsm  = ICACHE_IDLE;
            }
        }
        break;
    }
    //////////////////////////
    case ICACHE_XTN_TLB_FLUSH:  // invalidate in one cycle all non global TLB entries
    {
        r_itlb.flush();
        r_dcache_xtn_req = false;
        r_icache_fsm     = ICACHE_IDLE;
        break;
    }
    ////////////////////////////
    case ICACHE_XTN_CACHE_FLUSH:    // Invalidate sequencially all cache lines, using
                                    // r_icache_flush_count as a slot counter,
                                    // looping in this state until all slots are visited.
                                    // It can require two cycles per slot:
                                    // We test here the slot state, and make the actual inval
                                    // (if line is valid) in ICACHE_XTN_CACHE_FLUSH_GO state.
                                    // A cleanup request is generated for each valid line
    {
        // coherence clack interrupt
        if (r_icache_clack_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_icache_req.read() and not r_icache_cc_send_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        if (not r_icache_cc_send_req.read()) // blocked until previous cc_send request is sent
        {
            int state;
            paddr_t tag;
            size_t way = r_icache_flush_count.read() / m_icache_sets;
            size_t set = r_icache_flush_count.read() % m_icache_sets;

#ifdef INSTRUMENTATION
            m_cpt_icache_dir_read++;
#endif
            r_icache.read_dir(way,
                              set,
                              &tag,
                              &state);

            if (state == CACHE_SLOT_STATE_VALID)    // inval required
            {
                // request cleanup
                r_icache_cc_send_req   = true;
                r_icache_cc_send_nline = tag * m_icache_sets + set;
                r_icache_cc_send_way   = way;
                r_icache_cc_send_type  = CC_TYPE_CLEANUP;

                // goes to ICACHE_XTN_CACHE_FLUSH_GO to make inval
                r_icache_miss_way = way;
                r_icache_miss_set = set;
                r_icache_fsm      = ICACHE_XTN_CACHE_FLUSH_GO;
            }
            else if (r_icache_flush_count.read() ==
                      (m_icache_sets*m_icache_ways - 1))  // last slot
            {
                r_dcache_xtn_req = false;
                m_drsp.valid = true;
                r_icache_fsm = ICACHE_IDLE;
            }

            // saturation counter, to have the same last slot condition
            // in ICACHE_XTN_CACHE_FLUSH and ICACHE_XTN_CACHE_FLUSH_GO states
            if (r_icache_flush_count.read() < (m_icache_sets * m_icache_ways - 1))
            {
                r_icache_flush_count = r_icache_flush_count.read() + 1;
            }
        }
        break;
    }
    ///////////////////////////////
    case ICACHE_XTN_CACHE_FLUSH_GO:   // Switch slot state to ZOMBI for an XTN flush
    {
        size_t way = r_icache_miss_way.read();
        size_t set = r_icache_miss_set.read();

#ifdef INSTRUMENTATION
        m_cpt_icache_dir_write++;
#endif

        r_icache.write_dir(way,
                           set,
                           CACHE_SLOT_STATE_ZOMBI);

        if (r_icache_flush_count.read() ==
                      (m_icache_sets*m_icache_ways - 1))  // last slot
        {
            r_dcache_xtn_req = false;
            m_drsp.valid = true;
            r_icache_fsm = ICACHE_IDLE;
        }
        else
        {
            r_icache_fsm = ICACHE_XTN_CACHE_FLUSH;
        }
        break;
    }

    //////////////////////////
    case ICACHE_XTN_TLB_INVAL: // invalidate one TLB entry selected by the virtual address
                               // stored in the r_dcache_save_wdata register
    {
        r_itlb.inval(r_dcache_save_wdata.read());
        r_dcache_xtn_req = false;
        r_icache_fsm     = ICACHE_IDLE;
        break;
    }
    ///////////////////////////////
    case ICACHE_XTN_CACHE_INVAL_VA: // Selective cache line invalidate with virtual address
                                    // requires 3 cycles (in case of hit on itlb and icache).
                                    // In this state, access TLB to translate virtual address
                                    // stored in the r_dcache_save_wdata register.
    {
        paddr_t paddr;
        bool    hit;

        // read physical address in TLB when MMU activated
        if (r_mmu_mode.read() & INS_TLB_MASK) // itlb activated
        {

#ifdef INSTRUMENTATION
            m_cpt_itlb_read++;
#endif
            hit = r_itlb.translate(r_dcache_save_wdata.read(), &paddr);
        }
        else // itlb not activated
        {
            paddr = (paddr_t) r_dcache_save_wdata.read();
            hit   = true;
        }

        if (hit) // continue the selective inval process
        {
            r_icache_vci_paddr = paddr;
            r_icache_fsm       = ICACHE_XTN_CACHE_INVAL_PA;
        }
        else // miss : send a request to DCACHE FSM
        {

#ifdef INSTRUMENTATION
            m_cpt_itlb_miss++;
#endif
            r_icache_tlb_miss_req = true;
            r_icache_vaddr_save   = r_dcache_save_wdata.read();
            r_icache_fsm          = ICACHE_TLB_WAIT;
        }
        break;
    }
    ///////////////////////////////
    case ICACHE_XTN_CACHE_INVAL_PA: // selective invalidate cache line with physical address
                                    // require 2 cycles. In this state, we read directory
                                    // with address stored in r_icache_vci_paddr register.
    {
        int    state;
        size_t way;
        size_t set;
        size_t word;

#ifdef INSTRUMENTATION
        m_cpt_icache_dir_read++;
#endif
        r_icache.read_dir(r_icache_vci_paddr.read(),
                          &state,
                          &way,
                          &set,
                          &word);

        if (state == CACHE_SLOT_STATE_VALID) // inval to be done
        {
            r_icache_miss_way = way;
            r_icache_miss_set = set;
            r_icache_fsm      = ICACHE_XTN_CACHE_INVAL_GO;
        }
        else // miss : acknowlege the XTN request and return
        {
            r_dcache_xtn_req = false;
            r_icache_fsm     = ICACHE_IDLE;
        }
        break;
    }
    ///////////////////////////////
    case ICACHE_XTN_CACHE_INVAL_GO:  // Switch slot to ZOMBI state for an XTN inval
    {
        if (not r_icache_cc_send_req.read())  // blocked until previous cc_send request not sent
        {

#ifdef INSTRUMENTATION
            m_cpt_icache_dir_write++;
#endif
            r_icache.write_dir(r_icache_miss_way.read(),
                               r_icache_miss_set.read(),
                               CACHE_SLOT_STATE_ZOMBI);

            // request cleanup
            r_icache_cc_send_req   = true;
            r_icache_cc_send_nline = r_icache_vci_paddr.read() / (m_icache_words << 2);
            r_icache_cc_send_way   = r_icache_miss_way.read();
            r_icache_cc_send_type  = CC_TYPE_CLEANUP;

            // acknowledge the XTN request and return
            r_dcache_xtn_req = false;
            r_icache_fsm     = ICACHE_IDLE;
        }
        break;
    }
    ////////////////////////
    case ICACHE_MISS_SELECT:       // Try to select a slot in associative set,
                                   // Waiting in this state if no slot available.
                                   // If a victim slot has been choosen and the r_icache_cc_send_req is false,
                                   // we send the cleanup request in this state.
                                   // If not, a r_icache_cleanup_victim_req flip-flop is
                                   // utilized for saving this cleanup request, and it will be sent later
                                   // in state ICACHE_MISS_WAIT or ICACHE_MISS_UPDT_DIR.
                                   // The r_icache_miss_clack flip-flop is set
                                   // when a cleanup is required
    {
        if (m_ireq.valid) m_cost_ins_miss_frz++;

        // coherence clack interrupt
        if (r_icache_clack_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        // coherence interrupt
        if (r_cc_receive_icache_req.read() and not r_icache_cc_send_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }


        bool found;
        bool cleanup;
        size_t way;
        size_t set;
        paddr_t victim;

#ifdef INSTRUMENTATION
        m_cpt_icache_dir_read++;
#endif
        r_icache.read_select(r_icache_vci_paddr.read(),
                             &victim,
                             &way,
                             &set,
                             &found,
                             &cleanup);
        if (not found)
        {
            break;
        }
        else
        {
            r_icache_miss_way = way;
            r_icache_miss_set = set;

            if (cleanup)
            {
                if (not r_icache_cc_send_req.read())
                {
                    r_icache_cc_send_req   = true;
                    r_icache_cc_send_nline = victim;
                    r_icache_cc_send_way   = way;
                    r_icache_cc_send_type  = CC_TYPE_CLEANUP;
                }
                else
                {
                    r_icache_cleanup_victim_req   = true;
                    r_icache_cleanup_victim_nline = victim;
                }

                r_icache_miss_clack = true;
                r_icache_fsm        = ICACHE_MISS_CLEAN;
            }
            else
            {
                r_icache_fsm = ICACHE_MISS_WAIT;
            }

#if DEBUG_ICACHE
            if (m_debug_icache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " ICACHE_MISS_SELECT> Select a slot:" << std::dec
                    << " / WAY = " << way
                    << " / SET = " << set;
                if (cleanup) std::cout << " / VICTIM = " << std::hex << victim << std::endl;
                else         std::cout << std::endl;
            }
#endif
        }
        break;
    }
    ///////////////////////
    case ICACHE_MISS_CLEAN:   // switch the slot to zombi state
    {
        if (m_ireq.valid) m_cost_ins_miss_frz++;

#ifdef INSTRUMENTATION
        m_cpt_icache_dir_write++;
#endif
        r_icache.write_dir(r_icache_miss_way.read(),
                           r_icache_miss_set.read(),
                           CACHE_SLOT_STATE_ZOMBI);
#if DEBUG_ICACHE
        if (m_debug_icache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " ICACHE_MISS_CLEAN> Switch to ZOMBI state" << std::dec
                << " / WAY = " << r_icache_miss_way.read()
                << " / SET = " << r_icache_miss_set.read() << std::endl;
        }
#endif

        r_icache_fsm = ICACHE_MISS_WAIT;
        break;
    }
    //////////////////////
    case ICACHE_MISS_WAIT: // waiting response from VCI_RSP FSM
    {
        if (m_ireq.valid) m_cost_ins_miss_frz++;

        // send cleanup victim request
        if (r_icache_cleanup_victim_req.read() and not r_icache_cc_send_req.read())
        {
            r_icache_cc_send_req        = true;
            r_icache_cc_send_nline      = r_icache_cleanup_victim_nline;
            r_icache_cc_send_way        = r_icache_miss_way;
            r_icache_cc_send_type       = CC_TYPE_CLEANUP;
            r_icache_cleanup_victim_req = false;
        }

        // coherence clack interrupt
        if (r_icache_clack_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        // coherence interrupt
        if (r_cc_receive_icache_req.read() and not r_icache_cc_send_req.read() and not r_icache_cleanup_victim_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        if (r_vci_rsp_ins_error.read()) // bus error
        {
            r_mmu_ietr          = MMU_READ_DATA_ILLEGAL_ACCESS;
            r_mmu_ibvar         = r_icache_vaddr_save.read();
            m_irsp.valid        = true;
            m_irsp.error        = true;
            r_vci_rsp_ins_error = false;
            r_icache_fsm        = ICACHE_IDLE;
        }
        else if (r_vci_rsp_fifo_icache.rok()) // response available
        {
            r_icache_miss_word = 0;
            r_icache_fsm       = ICACHE_MISS_DATA_UPDT;
        }
        break;
    }
    ///////////////////////////
    case ICACHE_MISS_DATA_UPDT:  // update the cache (one word per cycle)
    {
        if (m_ireq.valid) m_cost_ins_miss_frz++;

        if (r_vci_rsp_fifo_icache.rok()) // response available
        {

#ifdef INSTRUMENTATION
            m_cpt_icache_data_write++;
#endif
            r_icache.write(r_icache_miss_way.read(),
                           r_icache_miss_set.read(),
                           r_icache_miss_word.read(),
                           r_vci_rsp_fifo_icache.read());
#if DEBUG_ICACHE
            if (m_debug_icache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " ICACHE_MISS_DATA_UPDT> Write one word:"
                    << " WDATA = " << std::hex << r_vci_rsp_fifo_icache.read()
                    << " WAY = " << r_icache_miss_way.read()
                    << " SET = " << r_icache_miss_set.read()
                    << " WORD = " << r_icache_miss_word.read() << std::endl;
            }
#endif
            vci_rsp_fifo_icache_get = true;
            r_icache_miss_word = r_icache_miss_word.read() + 1;

            if (r_icache_miss_word.read() == m_icache_words - 1) // last word
            {
                r_icache_fsm = ICACHE_MISS_DIR_UPDT;
            }
        }
        break;
    }
    //////////////////////////
    case ICACHE_MISS_DIR_UPDT:  // Stalled if a victim line has been evicted,
                                // and the cleanup ack has not been received,
                                // as indicated by r_icache_miss_clack.
                                // - If no matching coherence request (r_icache_miss_inval)
                                //   switch directory slot to VALID state.
                                // - If matching coherence request, switch directory slot
                                //   to ZOMBI state, and send a cleanup request.
    {
        if (m_ireq.valid ) m_cost_ins_miss_frz++;

        // send cleanup victim request
        if (r_icache_cleanup_victim_req.read() and not r_icache_cc_send_req.read())
        {
            r_icache_cc_send_req        = true;
            r_icache_cc_send_nline      = r_icache_cleanup_victim_nline;
            r_icache_cc_send_way        = r_icache_miss_way;
            r_icache_cc_send_type       = CC_TYPE_CLEANUP;
            r_icache_cleanup_victim_req = false;
        }

        // coherence clack interrupt
        if (r_icache_clack_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        // coherence interrupt
        if (r_cc_receive_icache_req.read() and not r_icache_cc_send_req.read() and not r_icache_cleanup_victim_req.read())
        {
            r_icache_fsm = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        if (not r_icache_miss_clack.read()) // waiting cleanup acknowledge for victim line
        {
            if (r_icache_miss_inval) // Switch slot to ZOMBI state, and new cleanup
            {
                if (not r_icache_cc_send_req.read())
                {
                    r_icache_miss_inval    = false;
                    // request cleanup
                    r_icache_cc_send_req   = true;
                    r_icache_cc_send_nline = r_icache_vci_paddr.read() / (m_icache_words << 2);
                    r_icache_cc_send_way   = r_icache_miss_way.read();
                    r_icache_cc_send_type  = CC_TYPE_CLEANUP;

#ifdef INSTRUMENTATION
                    m_cpt_icache_dir_write++;
#endif
                    r_icache.write_dir(r_icache_vci_paddr.read(),
                                       r_icache_miss_way.read(),
                                       r_icache_miss_set.read(),
                                       CACHE_SLOT_STATE_ZOMBI);
#if DEBUG_ICACHE
                    if (m_debug_icache_fsm)
                    {
                        std::cout << "  <PROC " << name()
                            << " ICACHE_MISS_DIR_UPDT> Switch cache slot to ZOMBI state"
                            << " PADDR = " << std::hex << r_icache_vci_paddr.read()
                            << " WAY = " << std::dec << r_icache_miss_way.read()
                            << " SET = " << r_icache_miss_set.read() << std::endl;
                    }
#endif
                }
                else
                    break;
            }
            else // Switch slot to VALID state
            {

#ifdef INSTRUMENTATION
                m_cpt_icache_dir_write++;
#endif
                r_icache.write_dir(r_icache_vci_paddr.read(),
                                   r_icache_miss_way.read(),
                                   r_icache_miss_set.read(),
                                   CACHE_SLOT_STATE_VALID);
#if DEBUG_ICACHE
                if (m_debug_icache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " ICACHE_MISS_DIR_UPDT> Switch cache slot to VALID state"
                        << " PADDR = " << std::hex << r_icache_vci_paddr.read()
                        << " WAY = " << std::dec << r_icache_miss_way.read()
                        << " SET = " << r_icache_miss_set.read() << std::endl;
                }
#endif
            }

            r_icache_fsm = ICACHE_IDLE;
        }
        break;
    }
    ////////////////////
    case ICACHE_UNC_WAIT: // waiting a response to an uncacheable read from VCI_RSP FSM
    {
        // coherence clack interrupt
        if (r_icache_clack_req.read())
        {
            r_icache_fsm      = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        // coherence interrupt
        if (r_cc_receive_icache_req.read() and not r_icache_cc_send_req.read())
        {
            r_icache_fsm      = ICACHE_CC_CHECK;
            r_icache_fsm_save = r_icache_fsm.read();
            break;
        }

        if (r_vci_rsp_ins_error.read()) // bus error
        {
            r_mmu_ietr          = MMU_READ_DATA_ILLEGAL_ACCESS;
            r_mmu_ibvar         = m_ireq.addr;
            r_vci_rsp_ins_error = false;
            m_irsp.valid        = true;
            m_irsp.error        = true;
            r_icache_fsm        = ICACHE_IDLE;
        }
        else if (r_vci_rsp_fifo_icache.rok()) // instruction available
        {
            vci_rsp_fifo_icache_get = true;
            r_icache_fsm            = ICACHE_IDLE;
            if (m_ireq.valid and
                (m_ireq.addr == r_icache_vaddr_save.read())) // request unmodified
            {
                m_irsp.valid       = true;
                m_irsp.instruction = r_vci_rsp_fifo_icache.read();
            }
        }
        break;
    }
    /////////////////////
    case ICACHE_CC_CHECK:   // This state is the entry point of a sub-fsm
                            // handling coherence requests.
                            // if there is a matching pending miss, it is
                            // signaled in the r_icache_miss_inval flip-flop.
                            // The return state is defined in r_icache_fsm_save.
    {
        paddr_t paddr = r_cc_receive_icache_nline.read() * m_icache_words * 4;
        paddr_t mask  = ~((m_icache_words << 2) - 1);

        // CLACK handler
        // We switch the directory slot to EMPTY state
        // and reset r_icache_miss_clack if the cleanup ack
        // is matching a pending miss.
        if (r_icache_clack_req.read())
        {

            if (m_ireq.valid) m_cost_ins_miss_frz++;

#ifdef INSTRUMENTATION
            m_cpt_icache_dir_write++;
#endif
            r_icache.write_dir(0,
                               r_icache_clack_way.read(),
                               r_icache_clack_set.read(),
                               CACHE_SLOT_STATE_EMPTY);

            if ((r_icache_miss_set.read() == r_icache_clack_set.read()) and
                 (r_icache_miss_way.read() == r_icache_clack_way.read()))
            {
                r_icache_miss_clack = false;
            }

            r_icache_clack_req = false;

            // return to cc_save state
            r_icache_fsm = r_icache_fsm_save.read();

#if DEBUG_ICACHE
            if (m_debug_icache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " ICACHE_CC_CHECK>  CC_TYPE_CLACK slot returns to empty state"
                    << " set = " << r_icache_clack_set.read()
                    << " / way = " << r_icache_clack_way.read() << std::endl;
            }
#endif

            break;
        }

        assert(not r_icache_cc_send_req.read() and "CC_SEND must be available in ICACHE_CC_CHECK");

        // Match between MISS address and CC address
        if (r_cc_receive_icache_req.read() and
          ((r_icache_fsm_save.read() == ICACHE_MISS_SELECT)  or
           (r_icache_fsm_save.read() == ICACHE_MISS_WAIT)  or
           (r_icache_fsm_save.read() == ICACHE_MISS_DIR_UPDT)) and
          ((r_icache_vci_paddr.read() & mask) == (paddr & mask))) // matching
        {
            // signaling the matching
            r_icache_miss_inval = true;

            // in case of update, go to CC_UPDT
            // JUST TO POP THE FIFO
            if (r_cc_receive_icache_type.read() == CC_TYPE_UPDT)
            {
                r_icache_fsm = ICACHE_CC_UPDT;
                r_icache_cc_word = r_cc_receive_word_idx.read();

                // just pop the fifo , don't write in icache
                r_icache_cc_need_write = false;
            }
            // the request is dealt with
            else
            {
                r_cc_receive_icache_req = false;
                r_icache_fsm = r_icache_fsm_save.read();
            }
#if DEBUG_ICACHE
            if (m_debug_icache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " ICACHE_CC_CHECK> Coherence request matching a pending miss:"
                    << " PADDR = " << std::hex << paddr << std::endl;
            }
#endif
        }

        // CC request handler

        int    state = 0;
        size_t way = 0;
        size_t set = 0;
        size_t word = 0;

#ifdef INSTRUMENTATION
        m_cpt_icache_dir_read++;
#endif
        r_icache.read_dir(paddr,
                          &state,
                          &way,
                          &set,
                          &word);

        r_icache_cc_way = way;
        r_icache_cc_set = set;

        if (state == CACHE_SLOT_STATE_VALID)            // hit
        {
            // need to update the cache state
            if (r_cc_receive_icache_type.read() == CC_TYPE_UPDT)  // hit update
            {
                r_icache_cc_need_write = true;
                r_icache_fsm = ICACHE_CC_UPDT;
                r_icache_cc_word = r_cc_receive_word_idx.read();
            }
            else if (r_cc_receive_icache_type.read() == CC_TYPE_INVAL) // hit inval
            {
                r_icache_fsm = ICACHE_CC_INVAL;
            }
        }
        else                                      // miss
        {
            // multicast acknowledgement required in case of update
            if (r_cc_receive_icache_type.read() == CC_TYPE_UPDT)
            {
                r_icache_fsm = ICACHE_CC_UPDT;
                r_icache_cc_word = r_cc_receive_word_idx.read();

                // just pop the fifo , don't write in icache
                r_icache_cc_need_write = false;
            }
            else // No response needed
            {
                r_cc_receive_icache_req = false;
                r_icache_fsm = r_icache_fsm_save.read();
            }
        }
        break;
    }
    /////////////////////
    case ICACHE_CC_INVAL:  // hit inval : switch slot to ZOMBI state
    {
        assert (not r_icache_cc_send_req.read() &&
                "ERROR in ICACHE_CC_INVAL: the r_icache_cc_send_req "
                "must not be set");

#ifdef INSTRUMENTATION
        m_cpt_icache_dir_read++;
#endif

        // Switch slot state to ZOMBI and send CLEANUP command
        r_icache.write_dir(r_icache_cc_way.read(),
                           r_icache_cc_set.read(),
                           CACHE_SLOT_STATE_ZOMBI);

        // coherence request completed
        r_icache_cc_send_req   = true;
        r_icache_cc_send_nline = r_cc_receive_icache_nline.read();
        r_icache_cc_send_way   = r_icache_cc_way.read();
        r_icache_cc_send_type  = CC_TYPE_CLEANUP;

        r_icache_fsm = r_icache_fsm_save.read();

#if DEBUG_ICACHE
        if (m_debug_icache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " ICACHE_CC_INVAL> slot returns to ZOMBI state"
                << " set = " << r_icache_cc_set.read()
                << " / way = " << r_icache_cc_way.read() << std::endl;
        }
#endif

        break;
    }
    ////////////////////
    case ICACHE_CC_UPDT: // hit update : write one word per cycle
    {
        assert (not r_icache_cc_send_req.read() &&
                "ERROR in ICACHE_CC_UPDT: the r_icache_cc_send_req "
                "must not be set");

        if (not r_cc_receive_updt_fifo_be.rok()) break;


        size_t word = r_icache_cc_word.read();
        size_t way  = r_icache_cc_way.read();
        size_t set  = r_icache_cc_set.read();

        if (r_icache_cc_need_write.read())
        {
            r_icache.write(way,
                           set,
                           word,
                           r_cc_receive_updt_fifo_data.read(),
                           r_cc_receive_updt_fifo_be.read());

            r_icache_cc_word = word + 1;

#ifdef INSTRUMENTATION
            m_cpt_icache_data_write++;
#endif

#if DEBUG_ICACHE
            if (m_debug_icache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " ICACHE_CC_UPDT> Write one word "
                    << " set = " << r_icache_cc_set.read()
                    << " / way = " << r_icache_cc_way.read()
                    << " / word = " << r_icache_cc_word.read() << std::endl;
            }
#endif
        }

        if (r_cc_receive_updt_fifo_eop.read()) // last word
        {
            // no need to write in the cache anymore
            r_icache_cc_need_write = false;

            // coherence request completed
            r_cc_receive_icache_req = false;

            // request multicast acknowledgement
            r_icache_cc_send_req          = true;
            r_icache_cc_send_nline        = r_cc_receive_icache_nline.read();
            r_icache_cc_send_updt_tab_idx = r_cc_receive_icache_updt_tab_idx.read();
            r_icache_cc_send_type         = CC_TYPE_MULTI_ACK;

            r_icache_fsm = r_icache_fsm_save.read();
        }
        //consume fifo if not eop
        cc_receive_updt_fifo_get = true;

        break;
    }

    } // end switch r_icache_fsm

    ////////////////////////////////////////////////////////////////////////////////////
    //      DCACHE FSM
    //
    // 1/ Coherence operations
    //    They are handled as interrupts generated by the CC_RECEIVE FSM.
    //    - There is a coherence request when r_tgt_dcache_req is set.
    //    They are taken in IDLE, MISS_WAIT, MISS_DIR_UPDT, UNC_WAIT, LL_WAIT
    //    and SC_WAIT states.
    //    - There is a cleanup acknowledge request when r_cleanup_dcache_req is set.
    //    They are taken in IDLE, MISS_SELECT, MISS_CLEAN, MISS_WAIT, MISS_DATA_UPDT,
    //    MISS_DIR_UPDT, UNC_WAIT, LL_WAIT, SC_WAIT states.
    //    - For both types of requests, actions associated to the pre-empted state
    //    are not executed. The DCACHE FSM goes to the proper sub-FSM (CC_CHECK
    //    or CC_CLACK) to execute the requested coherence operation, and returns
    //    to the pre-empted state.
    //
    // 2/ TLB miss
    //    The page tables are generally cacheable.
    //    In case of miss in itlb or dtlb, the tlb miss is handled by a dedicated
    //    sub-fsm (DCACHE_TLB_MISS state), that handle possible miss in DCACHE,
    //    this sub-fsm implement the table-walk...
    //
    // 3/ processor requests
    //    Processor requests are taken in IDLE state only.
    //    The IDLE state implements a two stages pipe-line to handle write bursts:
    //    - Both DTLB and DCACHE are accessed in stage P0 (if processor request valid).
    //    - The registration in wbuf and the dcache update is done in stage P1
    //      (if the processor request is a write).
    //    The two r_dcache_wbuf_req and r_dcache_updt_req flip-flops define
    //    the operations that must be done in P1 stage, and the access type
    //    (read or write) to the DATA part of DCACHE depends on r_dcache_updt_req.
    //    READ requests are delayed if a cache update is requested.
    //    WRITE or SC requests can require a PTE Dirty bit update (in memory),
    //    that is done (before handling the processor request) by a dedicated sub-fsm.
    //    If a PTE is modified, both the itlb and dtlb are selectively, but sequencially
    //    cleared by a dedicated sub_fsm (DCACHE_INVAL_TLB_SCAN state).
    //
    // 4/ Atomic instructions LL/SC
    //    The LL/SC address are non cacheable (systematic access to memory).
    //    The llsc buffer contains a registration for an active LL/SC operation
    //    (with an address, a registration key, an aging counter and a valid bit).
    //    - LL requests from the processor are transmitted as a one flit VCI command
    //      (CMD_LOCKED_READ as CMD, and TYPE_LL as PKTID value). PLEN must
    //      be 8 as the response is 2 flits long (data and registration key)
    //    - SC requests from the processor are systematically transmitted to the
    //      memory cache as 2 flits VCI command (CMD_STORE_COND as CMD, and TYPE_SC
    //      as PKTID value).  The first flit contains the registration key, the second
    //      flit contains the data to write in case of success.
    //      The cache is not updated, as this is done in case of success by the
    //      coherence transaction.
    //
    // 5/ Non cacheable access:
    //    This component implement a strong order between non cacheable access
    //    (read or write) : A new non cacheable VCI transaction starts only when
    //    the previous non cacheable transaction is completed. After send the VCI
    //    transaction, the DCACHE FSM wait for the respone in the DCACHE_UNC_WAIT state.
    //    So the processor is blocked until the respone arrives in CACHE L1.
    //
    // 6/ Error handling:
    //    When the MMU is not activated, Read Bus Errors are synchronous events,
    //    Some Write Bus Errors are synchronous events when the request is a non cacheable access
    //    but some Write Bus Errors are asynchronous events when the request is cacheable access
    //    (processor is not frozen).
    //    - If a Read Bus Error or a Non Cacheable Write Bus Error is detected, the VCI_RSP FSM sets the
    //      r_vci_rsp_data_error flip-flop, without writing any data in the
    //      r_vci_rsp_fifo_dcache FIFO, and the synchronous error is signaled
    //      by the DCACHE FSM.
    //    - If a Cacheable Write Bus Error is detected, the VCI_RSP_FSM signals
    //    the asynchronous error using the setWriteBerr() method.
    //    When the MMU is activated bus error are rare events, as the MMU
    //    checks the physical address before the VCI transaction starts.
    ////////////////////////////////////////////////////////////////////////////////////////

    // default value for m_drsp
    m_drsp.valid = false;
    m_drsp.error = false;
    m_drsp.rdata = 0;

    switch (r_dcache_fsm.read())
    {
    case DCACHE_IDLE: // There are 10 conditions to exit the IDLE state :
                      // 1) ITLB/DTLB inval request (update)  => DCACHE_INVAL_TLB_SCAN
                      // 2) Coherence request (TGT FSM)       => DCACHE_CC_CHECK
                      // 3) ITLB miss request (ICACHE FSM)    => DCACHE_TLB_MISS
                      // 4) XTN request (processor)           => DCACHE_XTN_*
                      // 5) DTLB miss (processor)             => DCACHE_TLB_MISS
                      // 6) Dirty bit update (processor)      => DCACHE_DIRTY_GET_PTE
                      // 7) Cacheable read miss (processor)   => DCACHE_MISS_SELECT
                      // 8) Uncacheable read/write (processor)=> DCACHE_UNC_WAIT
                      // 9) LL access (processor)             => DCACHE_LL_WAIT
                      // 10) SC access (processor)            => DCACHE_SC_WAIT
                      //
                      // There is a fixed priority to handle requests to DCACHE:
                      //    1/ the ITLB/DTLB invalidate requests
                      //    2/ the coherence requests,
                      //    3/ the processor requests (including DTLB miss),
                      //    4/ the ITLB miss requests,
                      // The address space processor request are handled as follows:
                      // - WRITE request is blocked if the Dirty bit mus be set.
                      // If DTLB hit, the P1 stage is activated (writes WBUF, and
                      // updates DCACHE if DCACHE hit) & processor request acknowledged.
                      // - READ request generate a simultaneouss access to  DCACHE.DATA
                      // and DCACHE.DIR, but is delayed if DCACHE update required.
                      //
                      // There is 4 configurations defining the access type to
                      // DTLB, DCACHE.DATA, and DCACHE.DIR, depending on the
                      // dreq.valid (dreq) and r_dcache_updt_req (updt) signals:
                      //    dreq / updt / DTLB  / DCACHE.DIR / DCACHE.DATA /
                      //     0   /  0   / NOP   / NOP        / NOP         /
                      //     0   /  1   / NOP   / NOP        / WRITE       /
                      //     1   /  0   / READ  / READ       / NOP         /
                      //     1   /  1   / READ  / READ       / WRITE       /
                      // Those two registers are set at each cycle from the 3 signals
                      // updt_request, wbuf_request, wbuf_write_miss.
    {
        paddr_t paddr;
        pte_info_t tlb_flags;
        size_t   tlb_way;
        size_t   tlb_set;
        paddr_t  tlb_nline = 0;
        size_t   cache_way;
        size_t   cache_set;
        size_t   cache_word;
        uint32_t cache_rdata = 0;
        bool     tlb_hit = false;
        int      cache_state = CACHE_SLOT_STATE_EMPTY;

        bool tlb_inval_required = false; // request TLB inval after cache update
        bool wbuf_write_miss = false;    // miss a WBUF write request
        bool updt_request = false;       // request DCACHE update in P1 stage
        bool wbuf_request = false;       // request WBUF write in P1 stage

        // physical address computation : systematic DTLB access if activated
        paddr = (paddr_t) m_dreq.addr;
        if (m_dreq.valid)
        {
            if (r_mmu_mode.read() & DATA_TLB_MASK)  // DTLB activated
            {
                tlb_hit = r_dtlb.translate(m_dreq.addr,
                                           &paddr,
                                           &tlb_flags,
                                           &tlb_nline,
                                           &tlb_way,
                                           &tlb_set);
#ifdef INSTRUMENTATION
                m_cpt_dtlb_read++;
#endif
            }
            else // identity mapping
            {
                // we take into account the paddr extension
                if (vci_param::N > 32)
                    paddr = paddr | ((paddr_t) (r_dcache_paddr_ext.read()) << 32);
            }
        } // end physical address computation

        // systematic DCACHE access depending on r_dcache_updt_req (if activated)
        if (r_mmu_mode.read() & DATA_CACHE_MASK)
        {

            if (m_dreq.valid and r_dcache_updt_req.read()) // read DIR and write DATA
            {
                r_dcache.read_dir(paddr,
                                  &cache_state,
                                  &cache_way,
                                  &cache_set,
                                  &cache_word);

                r_dcache.write(r_dcache_save_cache_way.read(),
                               r_dcache_save_cache_set.read(),
                               r_dcache_save_cache_word.read(),
                               r_dcache_save_wdata.read(),
                               r_dcache_save_be.read());
#ifdef INSTRUMENTATION
                m_cpt_dcache_dir_read++;
                m_cpt_dcache_data_write++;
#endif
            }
            else if (m_dreq.valid and not r_dcache_updt_req.read()) // read DIR and DATA
            {
                r_dcache.read(paddr,
                              &cache_rdata,
                              &cache_way,
                              &cache_set,
                              &cache_word,
                              &cache_state);

#ifdef INSTRUMENTATION
                m_cpt_dcache_dir_read++;
                m_cpt_dcache_data_read++;
#endif
            }
            else if (not m_dreq.valid and r_dcache_updt_req.read()) // write DATA
            {
                r_dcache.write(r_dcache_save_cache_way.read(),
                               r_dcache_save_cache_set.read(),
                               r_dcache_save_cache_word.read(),
                               r_dcache_save_wdata.read(),
                               r_dcache_save_be.read());
#ifdef INSTRUMENTATION
                m_cpt_dcache_data_write++;
#endif
            }
        } // end dcache access

        // DCACHE update in P1 stage can require ITLB / DTLB inval or flush
        if (r_dcache_updt_req.read())
        {
            size_t way = r_dcache_save_cache_way.read();
            size_t set = r_dcache_save_cache_set.read();

            if (r_dcache_in_tlb[way * m_dcache_sets + set])
            {
                tlb_inval_required      = true;
                r_dcache_tlb_inval_set  = 0;
                r_dcache_tlb_inval_line = r_dcache_save_paddr.read() >>
                                           (uint32_log2(m_dcache_words << 2));
                r_dcache_in_tlb[way * m_dcache_sets + set] = false;
            }
            else if (r_dcache_contains_ptd[way * m_dcache_sets + set])
            {
                r_itlb.reset();
                r_dtlb.reset();
                r_dcache_contains_ptd[way * m_dcache_sets + set] = false;
            }

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
                std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                    << " Cache update in P1 stage" << std::dec
                    << " / WAY = " << r_dcache_save_cache_way.read()
                    << " / SET = " << r_dcache_save_cache_set.read()
                    << " / WORD = " << r_dcache_save_cache_word.read() << std::hex
                    << " / WDATA = " << r_dcache_save_wdata.read()
                    << " / BE = " << r_dcache_save_be.read() << std::endl;
#endif
        } // end test TLB inval

        // Try WBUF update in P1 stage
        // Miss if the write request is non cacheable, and there is a pending
        // non cacheable write, or if the write buffer is full.
        if (r_dcache_wbuf_req.read())
        {
            bool wok = r_wbuf.write(r_dcache_save_paddr.read(),
                                    r_dcache_save_be.read(),
                                    r_dcache_save_wdata.read(),
                                    true);
#ifdef INSTRUMENTATION
            m_cpt_wbuf_write++;
#endif
            if (not wok ) // miss if write buffer full
            {
                wbuf_write_miss = true;
            }
        } // end WBUF update

        // Computing the response to processor,
        // and the next value for r_dcache_fsm

        // itlb/dtlb invalidation self-request
        if (tlb_inval_required)
        {
            r_dcache_fsm_scan_save = r_dcache_fsm.read();
            r_dcache_fsm           = DCACHE_INVAL_TLB_SCAN;
        }

        // coherence clack request (from DSPIN CLACK)
        else if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
        }
        // coherence request (from CC_RECEIVE FSM)
        else if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
        }

        // processor request (READ, WRITE, LL, SC, XTN_READ, XTN_WRITE)
        // we don't take the processor request, and registers
        // are frozen in case of wbuf_write_miss
        else if (m_dreq.valid and not wbuf_write_miss)
        {
            // register processor request and DCACHE response
            r_dcache_save_vaddr      = m_dreq.addr;
            r_dcache_save_be         = m_dreq.be;
            r_dcache_save_wdata      = m_dreq.wdata;
            r_dcache_save_paddr      = paddr;
            r_dcache_save_cache_way  = cache_way;
            r_dcache_save_cache_set  = cache_set;
            r_dcache_save_cache_word = cache_word;

            // READ XTN requests from processor
            // They are executed in this DCACHE_IDLE state.
            // The processor must not be in user mode
            if (m_dreq.type == iss_t::XTN_READ)
            {
                int xtn_opcode = (int) m_dreq.addr / 4;

                // checking processor mode:
                if (m_dreq.mode  == iss_t::MODE_USER)
                {
                    r_mmu_detr   = MMU_READ_PRIVILEGE_VIOLATION;
                    r_mmu_dbvar  = m_dreq.addr;
                    m_drsp.valid = true;
                    m_drsp.error = true;
                    m_drsp.rdata = 0;
                    r_dcache_fsm = DCACHE_IDLE;
                }
                else
                {
                    switch (xtn_opcode)
                    {
                    case iss_t::XTN_INS_ERROR_TYPE:
                        m_drsp.rdata = r_mmu_ietr.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_DATA_ERROR_TYPE:
                        m_drsp.rdata = r_mmu_detr.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_INS_BAD_VADDR:
                        m_drsp.rdata = r_mmu_ibvar.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_DATA_BAD_VADDR:
                        m_drsp.rdata = r_mmu_dbvar.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_PTPR:
                        m_drsp.rdata = r_mmu_ptpr.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_TLB_MODE:
                        m_drsp.rdata = r_mmu_mode.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_MMU_PARAMS:
                        m_drsp.rdata = r_mmu_params;
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_MMU_RELEASE:
                        m_drsp.rdata = r_mmu_release;
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_MMU_WORD_LO:
                        m_drsp.rdata = r_mmu_word_lo.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_MMU_WORD_HI:
                        m_drsp.rdata = r_mmu_word_hi.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_DATA_PADDR_EXT:
                        m_drsp.rdata = r_dcache_paddr_ext.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    case iss_t::XTN_INST_PADDR_EXT:
                        m_drsp.rdata = r_icache_paddr_ext.read();
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        break;

                    default:
                        r_mmu_detr   = MMU_READ_UNDEFINED_XTN;
                        r_mmu_dbvar  = m_dreq.addr;
                        m_drsp.valid = true;
                        m_drsp.error = true;
                        m_drsp.rdata = 0;
                        break;
                    } // end switch xtn_opcode
                } // end else
            } // end if XTN_READ

            // Handling WRITE XTN requests from processor.
            // They are not executed in this DCACHE_IDLE state
            // if they require access to the caches or the TLBs
            // that are already accessed.
            // Caches can be invalidated or flushed in user mode,
            // and the sync instruction can be executed in user mode
            else if (m_dreq.type == iss_t::XTN_WRITE)
            {
                int xtn_opcode = (int)m_dreq.addr / 4;
                r_dcache_xtn_opcode = xtn_opcode;

                // checking processor mode:
                if ((m_dreq.mode  == iss_t::MODE_USER) &&
                     (xtn_opcode != iss_t::XTN_SYNC) &&
                     (xtn_opcode != iss_t::XTN_DCACHE_INVAL) &&
                     (xtn_opcode != iss_t::XTN_DCACHE_FLUSH) &&
                     (xtn_opcode != iss_t::XTN_ICACHE_INVAL) &&
                     (xtn_opcode != iss_t::XTN_ICACHE_FLUSH))
                {
                    r_mmu_detr   = MMU_WRITE_PRIVILEGE_VIOLATION;
                    r_mmu_dbvar  = m_dreq.addr;
                    m_drsp.valid = true;
                    m_drsp.error = true;
                    m_drsp.rdata = 0;
                    r_dcache_fsm = DCACHE_IDLE;
                }
                else
                {
                    switch (xtn_opcode)
                    {
                    case iss_t::XTN_PTPR: // itlb & dtlb must be flushed
                        r_dcache_xtn_req = true;
                        r_dcache_fsm     = DCACHE_XTN_SWITCH;
                        break;

                    case iss_t::XTN_TLB_MODE: // no cache or tlb access
                        r_mmu_mode   = m_dreq.wdata;
                        m_drsp.valid = true;
                        r_dcache_fsm = DCACHE_IDLE;
                        break;

                    case iss_t::XTN_DTLB_INVAL: // dtlb access
                        r_dcache_fsm = DCACHE_XTN_DT_INVAL;
                        break;

                    case iss_t::XTN_ITLB_INVAL: // itlb access
                        r_dcache_xtn_req = true;
                        r_dcache_fsm     = DCACHE_XTN_IT_INVAL;
                        break;

                    case iss_t::XTN_DCACHE_INVAL:  // dcache, dtlb & itlb access
                        r_dcache_fsm = DCACHE_XTN_DC_INVAL_VA;
                        break;

                    case iss_t::XTN_MMU_DCACHE_PA_INV: // dcache, dtlb & itlb access
                    {
                        uint64_t pa = ((uint64_t)r_mmu_word_hi.read() << 32) |
                                      ((uint64_t)r_mmu_word_lo.read());

                        r_dcache_save_paddr = (paddr_t)pa;
                        r_dcache_fsm = DCACHE_XTN_DC_INVAL_PA;
                        break;
                    }
                    case iss_t::XTN_DCACHE_FLUSH: // itlb and dtlb must be reset
                        r_dcache_flush_count = 0;
                        r_dcache_fsm         = DCACHE_XTN_DC_FLUSH;
                        break;

                    case iss_t::XTN_ICACHE_INVAL: // icache and itlb access
                        r_dcache_xtn_req = true;
                        r_dcache_fsm     = DCACHE_XTN_IC_INVAL_VA;
                        break;

                    case iss_t::XTN_MMU_ICACHE_PA_INV: // icache access
                        r_dcache_xtn_req = true;
                        r_dcache_fsm     = DCACHE_XTN_IC_INVAL_PA;
                        break;

                    case iss_t::XTN_ICACHE_FLUSH:   // icache access
                        r_dcache_xtn_req = true;
                        r_dcache_fsm     = DCACHE_XTN_IC_FLUSH;
                        break;

                    case iss_t::XTN_SYNC:           // wait until write buffer empty
                        r_dcache_fsm = DCACHE_XTN_SYNC;
                        break;

                    case iss_t::XTN_MMU_WORD_LO:    // no cache or tlb access
                        r_mmu_word_lo = m_dreq.wdata;
                        m_drsp.valid  = true;
                        r_dcache_fsm  = DCACHE_IDLE;
                        break;

                    case iss_t::XTN_MMU_WORD_HI:    // no cache or tlb access
                        r_mmu_word_hi = m_dreq.wdata;
                        m_drsp.valid  = true;
                        r_dcache_fsm  = DCACHE_IDLE;
                        break;

                    case iss_t::XTN_MMU_LL_RESET:   // no cache or tlb access
                        r_dcache_llsc_valid = false;
                        m_drsp.valid        = true;
                        r_dcache_fsm        = DCACHE_IDLE;
                    break;

                    case iss_t::XTN_DATA_PADDR_EXT:  // no cache or tlb access
                        r_dcache_paddr_ext = m_dreq.wdata;
                        m_drsp.valid       = true;
                        r_dcache_fsm       = DCACHE_IDLE;
                    break;

                    case iss_t::XTN_INST_PADDR_EXT:  // no cache or tlb access
                        r_dcache_xtn_req = true;
                        r_dcache_fsm     = DCACHE_XTN_IC_PADDR_EXT;
                    break;

                    case iss_t::XTN_ICACHE_PREFETCH: // not implemented : no action
                    case iss_t::XTN_DCACHE_PREFETCH: // not implemented : no action
                        m_drsp.valid = true;
                        r_dcache_fsm = DCACHE_IDLE;
                    break;

                    case iss_t::XTN_DEBUG_MASK:     // debug mask
                        m_debug_dcache_fsm = ((m_dreq.wdata & 0x1) != 0);
                        m_debug_icache_fsm = ((m_dreq.wdata & 0x2) != 0);
                        m_debug_cmd_fsm = ((m_dreq.wdata & 0x4) != 0);
                        m_drsp.valid = true;
                        r_dcache_fsm = DCACHE_IDLE;
                        break;

                    default:
                        r_mmu_detr   = MMU_WRITE_UNDEFINED_XTN;
                        r_mmu_dbvar  = m_dreq.addr;
                        m_drsp.valid = true;
                        m_drsp.error = true;
                        r_dcache_fsm = DCACHE_IDLE;
                        break;
                    } // end switch xtn_opcode
                } // end else
            } // end if XTN_WRITE

            // Handling processor requests to address space (READ/WRITE/LL/SC)
            // The dtlb and dcache can be activated or not.
            // We compute the cacheability, and check processor request validity:
            // - If DTLB not activated : cacheability is defined by the segment table,
            //   and there is no access rights checking.
            // - If DTLB activated : cacheability is defined by the C bit in the PTE,
            //   and the U & W bits of the PTE are checked, as well as the DTLB hit.
            //   Jumps to the TLB_MISS sub-fsm in case of dtlb miss.
            else
            {
                bool valid_req;
                bool cacheable;

                if (not (r_mmu_mode.read() & DATA_TLB_MASK)) // dtlb not activated
                {
                    valid_req = true;

                    if (not (r_mmu_mode.read() & DATA_CACHE_MASK)) cacheable = false;
                    else cacheable = m_cacheability_table[(uint64_t)m_dreq.addr];
                }
                else // dtlb activated
                {
                    if (tlb_hit) // tlb hit
                    {
                        // cacheability
                        if (not (r_mmu_mode.read() & DATA_CACHE_MASK)) cacheable = false;
                        else cacheable = tlb_flags.c;

                        // access rights checking
                        if (not tlb_flags.u and (m_dreq.mode == iss_t::MODE_USER))
                        {
                            if ((m_dreq.type == iss_t::DATA_READ) or
                                 (m_dreq.type == iss_t::DATA_LL))
                            {
                                r_mmu_detr = MMU_READ_PRIVILEGE_VIOLATION;
                            }
                            else
                            {
                                r_mmu_detr = MMU_WRITE_PRIVILEGE_VIOLATION;
                            }
                            valid_req    = false;
                            r_mmu_dbvar  = m_dreq.addr;
                            m_drsp.valid = true;
                            m_drsp.error = true;
                            m_drsp.rdata = 0;
#if DEBUG_DCACHE
                            if (m_debug_dcache_fsm)
                                std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                                    << " HIT in dtlb, but privilege violation" << std::endl;
#endif
                        }
                        else if (not tlb_flags.w and
                                  ((m_dreq.type == iss_t::DATA_WRITE) or
                                   (m_dreq.type == iss_t::DATA_SC)))
                        {
                            r_mmu_detr   = MMU_WRITE_ACCES_VIOLATION;
                            valid_req    = false;
                            r_mmu_dbvar  = m_dreq.addr;
                            m_drsp.valid = true;
                            m_drsp.error = true;
                            m_drsp.rdata = 0;
#if DEBUG_DCACHE
                            if (m_debug_dcache_fsm)
                                std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                                    << " HIT in dtlb, but writable violation" << std::endl;
#endif
                        }
                        else
                        {
                            valid_req = true;
                        }
                    }
                    else // tlb miss
                    {
                        valid_req          = false;
                        r_dcache_tlb_vaddr = m_dreq.addr;
                        r_dcache_tlb_ins   = false;
                        r_dcache_fsm       = DCACHE_TLB_MISS;
                    }
                }    // end DTLB activated

                if (valid_req) // processor request is valid (after MMU check)
                {
                    // READ request
                    // The read requests are taken only if there is no cache update.
                    // We request a VCI transaction to CMD FSM if miss or uncachable

                    if (((m_dreq.type == iss_t::DATA_READ))
                          and not r_dcache_updt_req.read())
                    {
                        if (cacheable) // cacheable read
                        {
                            if (cache_state == CACHE_SLOT_STATE_EMPTY)   // cache miss
                            {
#ifdef INSTRUMENTATION
                                m_cpt_dcache_miss++;
#endif
                                // request a VCI DMISS transaction
                                r_dcache_vci_paddr    = paddr;
                                r_dcache_vci_miss_req = true;
                                r_dcache_miss_type    = PROC_MISS;
                                r_dcache_fsm          = DCACHE_MISS_SELECT;
#if DEBUG_DCACHE
                                if (m_debug_dcache_fsm)
                                    std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                                        << " READ MISS in dcache"
                                        << " / PADDR = " << std::hex << paddr << std::endl;
#endif
                            }
                            else if (cache_state == CACHE_SLOT_STATE_ZOMBI) // pending cleanup
                            {
                                // stalled until cleanup is acknowledged
                                r_dcache_fsm   = DCACHE_IDLE;
#if DEBUG_DCACHE
                                if (m_debug_dcache_fsm)
                                    std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                                        << " Pending cleanup, stalled until cleanup acknowledge"
                                        << " / PADDR = " << std::hex << paddr << std::endl;
#endif
                            }
                            else                                      // cache hit
                            {
#ifdef INSTRUMENTATION
                                m_cpt_data_read++;
#endif
                                // returns data to processor
                                m_drsp.valid = true;
                                m_drsp.error = false;
                                m_drsp.rdata = cache_rdata;
#if DEBUG_DCACHE
                                if (m_debug_dcache_fsm)
                                    std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                                        << " READ HIT in dcache"
                                        << " : PADDR = " << std::hex << paddr
                                        << " / DATA  = " << std::hex << cache_rdata << std::endl;
#endif
                            }
                        }
                        else // uncacheable read
                        {
                            r_dcache_vci_paddr     = paddr;
                            r_dcache_vci_unc_be    = m_dreq.be;
                            r_dcache_vci_unc_write = false;
                            r_dcache_vci_unc_req   = true;
                            r_dcache_fsm           = DCACHE_UNC_WAIT;
#if DEBUG_DCACHE
                            if (m_debug_dcache_fsm)
                                std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                                    << " READ UNCACHEABLE in dcache"
                                    << " / PADDR = " << std::hex << paddr << std::endl;
#endif
                        }
                    } // end READ

                    // LL request (non cachable)
                    // We request a VCI LL transaction to CMD FSM and register
                    // the LL/SC operation in llsc buffer.
                    else if (m_dreq.type == iss_t::DATA_LL)
                    {
                        // register paddr in LLSC buffer
                        r_dcache_llsc_paddr = paddr;
                        r_dcache_llsc_count = LLSC_TIMEOUT;
                        r_dcache_llsc_valid = true;

                        // request an LL VCI transaction and go to DCACHE_LL_WAIT state
                        r_dcache_vci_ll_req   = true;
                        r_dcache_vci_paddr    = paddr;
                        r_dcache_ll_rsp_count = 0;
                        r_dcache_fsm          = DCACHE_LL_WAIT;

                    }// end LL

                    // WRITE request:
                    // If the TLB is activated and the PTE Dirty bit is not set, we stall
                    // the processor and set the Dirty bit before handling the write request,
                    // going to the DCACHE_DIRTY_GT_PTE state.
                    // If we don't need to set the Dirty bit, we can acknowledge
                    // the processor request, as the write arguments (including the
                    // physical address) are registered in r_dcache_save registers,
                    // and the write will be done in the P1 pipeline stage.
                    else if (m_dreq.type == iss_t::DATA_WRITE)
                    {
                        if ((r_mmu_mode.read() & DATA_TLB_MASK)
                              and not tlb_flags.d) // Dirty bit must be set
                        {
                            // The PTE physical address is obtained from the nline value (dtlb),
                            // and from the virtual address (word index)
                            if (tlb_flags.b ) // PTE1
                            {
                                r_dcache_dirty_paddr = (paddr_t)(tlb_nline * (m_dcache_words << 2)) |
                                                       (paddr_t)((m_dreq.addr >> 19) & 0x3c);
                            }
                            else // PTE2
                            {
                                r_dcache_dirty_paddr = (paddr_t) (tlb_nline * (m_dcache_words << 2)) |
                                                       (paddr_t) ((m_dreq.addr >> 9) & 0x38);
                            }
                            r_dcache_fsm = DCACHE_DIRTY_GET_PTE;
                        }
                        else // Write request accepted
                        {
#ifdef INSTRUMENTATION
                            m_cpt_data_write++;
#endif
                            // cleaning llsc buffer if address matching
                            if (paddr == r_dcache_llsc_paddr.read())
                                r_dcache_llsc_valid = false;

                            if (not cacheable) // uncacheable write
                            {
                                r_dcache_vci_paddr     = paddr;
                                r_dcache_vci_wdata     = m_dreq.wdata;
                                r_dcache_vci_unc_write = true;
                                r_dcache_vci_unc_be    = m_dreq.be;
                                r_dcache_vci_unc_req   = true;
                                r_dcache_fsm           = DCACHE_UNC_WAIT;

#if DEBUG_DCACHE
                                if (m_debug_dcache_fsm)
                                {
                                    std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                                        << " Request WRITE UNCACHEABLE" << std::hex
                                        << " / VADDR = " << m_dreq.addr
                                        << " / PADDR = " << paddr
                                        << std::dec << std::endl;
                                }
#endif
                            }
                            else
                            {
                                // response to processor
                                m_drsp.valid = true;
                                // activating P1 stage
                                wbuf_request = true;
                                updt_request = (cache_state == CACHE_SLOT_STATE_VALID);

#if DEBUG_DCACHE
                                if (m_debug_dcache_fsm)
                                {
                                    std::cout << "  <PROC " << name() << " DCACHE_IDLE>"
                                        << " Request WBUF WRITE" << std::hex
                                        << " / VADDR = " << m_dreq.addr
                                        << " / PADDR = " << paddr
                                        << std::dec << std::endl;
                                }
#endif
                            }
                        }
                    } // end WRITE

                    // SC request:
                    // If the TLB is activated and the PTE Dirty bit is not set, we stall
                    // the processor and set the Dirty bit before handling the write request,
                    // going to the DCACHE_DIRTY_GT_PTE state.
                    // If we don't need to set the Dirty bit, we test the llsc buffer:
                    // If failure, we send a negative response to processor.
                    // If success, we request a SC transaction to CMD FSM and go
                    // to DCACHE_SC_WAIT state.
                    // We don't check a possible write hit in dcache, as the cache update
                    // is done by the coherence transaction induced by the SC...
                    else if (m_dreq.type == iss_t::DATA_SC)
                    {
                        if ((r_mmu_mode.read() & DATA_TLB_MASK)
                              and not tlb_flags.d) // Dirty bit must be set
                        {
                            // The PTE physical address is obtained from the nline value (dtlb),
                            // and the word index (virtual address)
                            if (tlb_flags.b) // PTE1
                            {
                                r_dcache_dirty_paddr = (paddr_t) (tlb_nline * (m_dcache_words << 2)) |
                                                       (paddr_t) ((m_dreq.addr >> 19) & 0x3c);
                            }
                            else // PTE2
                            {
                                r_dcache_dirty_paddr = (paddr_t) (tlb_nline * (m_dcache_words << 2)) |
                                                       (paddr_t) ((m_dreq.addr >> 9) & 0x38);
                            }
                            r_dcache_fsm = DCACHE_DIRTY_GET_PTE;
                            m_drsp.valid = false;
                            m_drsp.error = false;
                            m_drsp.rdata = 0;
                        }
                        else // SC request accepted
                        {
#ifdef INSTRUMENTATION
                            m_cpt_data_sc++;
#endif
                            // checking local success
                            if (r_dcache_llsc_valid.read() and
                                (r_dcache_llsc_paddr.read() == paddr)) // local success
                            {
                                // request an SC CMD and go to DCACHE_SC_WAIT state
                                r_dcache_vci_paddr   = paddr;
                                r_dcache_vci_sc_req  = true;
                                r_dcache_vci_sc_data = m_dreq.wdata;
                                r_dcache_fsm         = DCACHE_SC_WAIT;
                            }
                            else // local fail
                            {
                                m_drsp.valid = true;
                                m_drsp.error = false;
                                m_drsp.rdata = 0x1;
                            }
                        }
                    } // end SC
                } // end valid_req
            }  // end if read/write/ll/sc request
        } // end processor request

        // itlb miss request
        else if (r_icache_tlb_miss_req.read() and not wbuf_write_miss)
        {
            r_dcache_tlb_ins    = true;
            r_dcache_tlb_vaddr  = r_icache_vaddr_save.read();
            r_dcache_fsm        = DCACHE_TLB_MISS;
        }

        // Computing requests for P1 stage : r_dcache_wbuf_req & r_dcache_updt_req
        r_dcache_updt_req = updt_request;
        r_dcache_wbuf_req = wbuf_request or
                            (r_dcache_wbuf_req.read() and wbuf_write_miss);
        break;
    }
    /////////////////////
    case DCACHE_TLB_MISS: // This is the entry point for the sub-fsm handling all tlb miss.
                          // Input arguments are:
                          // - r_dcache_tlb_vaddr
                          // - r_dcache_tlb_ins (true when itlb miss)
                          // The sub-fsm access the dcache to find the missing TLB entry,
                          // and activates the cache miss procedure in case of miss.
                          // It bypass the first level page table access if possible.
                          // It uses atomic access to update the R/L access bits
                          // in the page table if required.
                          // It directly updates the itlb or dtlb, and writes into the
                          // r_mmu_ins_* or r_mmu_data* error reporting registers.
    {
        uint32_t ptba = 0;
        bool     bypass;
        paddr_t  pte_paddr;

        // evaluate bypass in order to skip first level page table access
        if (r_dcache_tlb_ins.read()) // itlb miss
        {
            bypass = r_itlb.get_bypass(r_dcache_tlb_vaddr.read(), &ptba);
        }
        else // dtlb miss
        {
            bypass = r_dtlb.get_bypass(r_dcache_tlb_vaddr.read(), &ptba);
        }

        if (not bypass) // Try to read PTE1/PTD1 in dcache
        {
            pte_paddr = (((paddr_t) r_mmu_ptpr.read()) << (INDEX1_NBITS + 2)) |
                       ((((paddr_t) r_dcache_tlb_vaddr.read()) >> PAGE_M_NBITS) << 2);
            r_dcache_tlb_paddr = pte_paddr;
            r_dcache_fsm       = DCACHE_TLB_PTE1_GET;
        }
        else // Try to read PTE2 in dcache
        {
            pte_paddr = (paddr_t) ptba << PAGE_K_NBITS |
                        (paddr_t) (r_dcache_tlb_vaddr.read() & PTD_ID2_MASK) >> (PAGE_K_NBITS - 3);
            r_dcache_tlb_paddr = pte_paddr;
            r_dcache_fsm       = DCACHE_TLB_PTE2_GET;
        }

#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            if (r_dcache_tlb_ins.read())
                std::cout << "  <PROC " << name() << " DCACHE_TLB_MISS> ITLB miss";
            else
                std::cout << "  <PROC " << name() << " DCACHE_TLB_MISS> DTLB miss";
            std::cout << " / VADDR = " << std::hex << r_dcache_tlb_vaddr.read()
                << " / ptpr  = " << (((paddr_t)r_mmu_ptpr.read()) << (INDEX1_NBITS+2))
                << " / BYPASS = " << bypass
                << " / PTE_ADR = " << pte_paddr << std::endl;
        }
#endif

        break;
    }
    /////////////////////////
    case DCACHE_TLB_PTE1_GET: // try to read a PT1 entry in dcache
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        uint32_t entry;
        size_t way;
        size_t set;
        size_t word;
        int    cache_state;
        r_dcache.read(r_dcache_tlb_paddr.read(),
                      &entry,
                      &way,
                      &set,
                      &word,
                      &cache_state);
#ifdef INSTRUMENTATION
        m_cpt_dcache_data_read++;
        m_cpt_dcache_dir_read++;
#endif
        if (cache_state == CACHE_SLOT_STATE_VALID)   // hit in dcache
        {
            if (not (entry & PTE_V_MASK)) // unmapped
            {
                if (r_dcache_tlb_ins.read())
                {
                    r_mmu_ietr             = MMU_READ_PT1_UNMAPPED;
                    r_mmu_ibvar            = r_dcache_tlb_vaddr.read();
                    r_icache_tlb_miss_req  = false;
                    r_icache_tlb_rsp_error = true;
                }
                else
                {
                    r_mmu_detr   = MMU_READ_PT1_UNMAPPED;
                    r_mmu_dbvar  = r_dcache_tlb_vaddr.read();
                    m_drsp.valid = true;
                    m_drsp.error = true;
                }
                r_dcache_fsm = DCACHE_IDLE;

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE1_GET> HIT in dcache, but unmapped"
                        << std::hex << " / paddr = " << r_dcache_tlb_paddr.read()
                        << std::dec << " / way = " << way
                        << std::dec << " / set = " << set
                        << std::dec << " / word = " << word
                        << std::hex << " / PTE1 = " << entry << std::endl;
                }
#endif

            }
            else if (entry & PTE_T_MASK) //  PTD : me must access PT2
            {
                // mark the cache line ac containing a PTD
                r_dcache_contains_ptd[m_dcache_sets * way + set] = true;

                // register bypass
                if (r_dcache_tlb_ins.read()) // itlb
                {
                    r_itlb.set_bypass(r_dcache_tlb_vaddr.read(),
                                      entry & ((1 << (m_paddr_nbits-PAGE_K_NBITS)) - 1),
                                      r_dcache_tlb_paddr.read() / (m_icache_words << 2));
                }
                else // dtlb
                {
                    r_dtlb.set_bypass(r_dcache_tlb_vaddr.read(),
                                      entry & ((1 << (m_paddr_nbits-PAGE_K_NBITS)) - 1),
                                      r_dcache_tlb_paddr.read() / (m_dcache_words << 2));
                }
                r_dcache_tlb_paddr =
                    (paddr_t)(entry & ((1 << (m_paddr_nbits - PAGE_K_NBITS)) - 1)) << PAGE_K_NBITS |
                    (paddr_t)(((r_dcache_tlb_vaddr.read() & PTD_ID2_MASK) >> PAGE_K_NBITS) << 3);
                r_dcache_fsm = DCACHE_TLB_PTE2_GET;

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE1_GET> HIT in dcache"
                        << std::hex << " / paddr = " << r_dcache_tlb_paddr.read()
                        << std::dec << " / way = " << way
                        << std::dec << " / set = " << set
                        << std::dec << " / word = " << word
                        << std::hex << " / PTD = " << entry << std::endl;
                }
#endif
            }
            else //  PTE1 :  we must update the TLB
            {
                r_dcache_in_tlb[m_icache_sets * way + set] = true;
                r_dcache_tlb_pte_flags  = entry;
                r_dcache_tlb_cache_way  = way;
                r_dcache_tlb_cache_set  = set;
                r_dcache_tlb_cache_word = word;
                r_dcache_fsm            = DCACHE_TLB_PTE1_SELECT;

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE1_GET> HIT in dcache"
                        << std::hex << " / paddr = " << r_dcache_tlb_paddr.read()
                        << std::dec << " / way = " << way
                        << std::dec << " / set = " << set
                        << std::dec << " / word = " << word
                        << std::hex << " / PTE1 = " << entry << std::endl;
                }
#endif
            }
        }
        else if (cache_state == CACHE_SLOT_STATE_ZOMBI) // pending cleanup
        {
            // stalled until cleanup is acknowledged
            r_dcache_fsm = DCACHE_TLB_PTE1_GET;
        }
        else // we must load the missing cache line in dcache
        {
            r_dcache_vci_miss_req = true;
            r_dcache_vci_paddr    = r_dcache_tlb_paddr.read();
            r_dcache_save_paddr   = r_dcache_tlb_paddr.read();
            r_dcache_miss_type    = PTE1_MISS;
            r_dcache_fsm          = DCACHE_MISS_SELECT;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_PTE1_GET> MISS in dcache:"
                    << " PTE1 address = " << std::hex << r_dcache_tlb_paddr.read() << std::endl;
            }
#endif
        }
        break;
    }
    ////////////////////////////
    case DCACHE_TLB_PTE1_SELECT: // select a slot for PTE1
    {
        size_t way;
        size_t set;

        if (r_dcache_tlb_ins.read())
        {
            r_itlb.select(r_dcache_tlb_vaddr.read(),
                          true,  // PTE1
                          &way,
                          &set);
#ifdef INSTRUMENTATION
            m_cpt_itlb_read++;
#endif
        }
        else
        {
            r_dtlb.select(r_dcache_tlb_vaddr.read(),
                          true,  // PTE1
                          &way,
                          &set);
#ifdef INSTRUMENTATION
            m_cpt_dtlb_read++;
#endif
        }
        r_dcache_tlb_way = way;
        r_dcache_tlb_set = set;
        r_dcache_fsm     = DCACHE_TLB_PTE1_UPDT;

#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            if (r_dcache_tlb_ins.read())
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_PTE1_SELECT> Select a slot in ITLB:";
            else
                std::cout << "  <PROC " << name()
                    << ".DCACHE_TLB_PTE1_SELECT> Select a slot in DTLB:";
            std::cout << " way = " << std::dec << way
                << " / set = " << set << std::endl;
        }
#endif
        break;
    }
    //////////////////////////
    case DCACHE_TLB_PTE1_UPDT:  // write a new PTE1 in tlb after testing the L/R bit
                                // - if L/R bit already set, exit the sub-fsm.
                                // - if not, we update the page table but we dont write
                                //   neither in DCACHE, nor in TLB, as this will be done by
                                //   the coherence mechanism.
    {
        paddr_t nline = r_dcache_tlb_paddr.read() >> (uint32_log2(m_dcache_words) + 2);
        uint32_t pte  = r_dcache_tlb_pte_flags.read();
        bool pt_updt  = false;
        bool local    = true;

        // We should compute the access locality:
        // The PPN MSB bits define the destination cluster index.
        // The m_srcid MSB bits define the source cluster index.
        // The number of bits to compare depends on the number of clusters,
        // and can be obtained in the mapping table.
        // As long as this computation is not done, all access are local.

        if (local) // local access
        {
            if (not ((pte & PTE_L_MASK) == PTE_L_MASK)) // we must set the L bit
            {
                pt_updt                = true;
                r_dcache_vci_cas_old   = pte;
                r_dcache_vci_cas_new   = pte | PTE_L_MASK;
                pte                    = pte | PTE_L_MASK;
                r_dcache_tlb_pte_flags = pte;
            }
        }
        else // remote access
        {
            if (not ((pte & PTE_R_MASK) == PTE_R_MASK)) // we must set the R bit
            {
                pt_updt                = true;
                r_dcache_vci_cas_old   = pte;
                r_dcache_vci_cas_new   = pte | PTE_R_MASK;
                pte                    = pte | PTE_R_MASK;
                r_dcache_tlb_pte_flags = pte;
            }
        }

        if (not pt_updt) // update TLB and return
        {
            if (r_dcache_tlb_ins.read())
            {
                r_itlb.write(true, // 2M page
                             pte,
                             0, // argument unused for a PTE1
                             r_dcache_tlb_vaddr.read(),
                             r_dcache_tlb_way.read(),
                             r_dcache_tlb_set.read(),
                             nline);
#ifdef INSTRUMENTATION
                m_cpt_itlb_write++;
#endif

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE1_UPDT> write PTE1 in ITLB"
                        << " / set = " << std::dec << r_dcache_tlb_set.read()
                        << " / way = " << r_dcache_tlb_way.read() << std::endl;
                    r_itlb.printTrace();
                }
#endif
            }
            else
            {
                r_dtlb.write(true, // 2M page
                             pte,
                             0, // argument unused for a PTE1
                             r_dcache_tlb_vaddr.read(),
                             r_dcache_tlb_way.read(),
                             r_dcache_tlb_set.read(),
                             nline);
#ifdef INSTRUMENTATION
                m_cpt_dtlb_write++;
#endif

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE1_UPDT> write PTE1 in DTLB"
                        << " / set = " << std::dec << r_dcache_tlb_set.read()
                        << " / way = " << r_dcache_tlb_way.read() << std::endl;
                    r_dtlb.printTrace();
                }
#endif
            }
            r_dcache_fsm = DCACHE_TLB_RETURN;
        }
        else                            // update page table but not TLB
        {
            r_dcache_fsm = DCACHE_TLB_LR_UPDT;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_PTE1_UPDT> L/R bit update required"
                    << std::endl;
            }
#endif
        }
        break;
    }
    /////////////////////////
    case DCACHE_TLB_PTE2_GET: // Try to get a PTE2 (64 bits) in the dcache
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        uint32_t pte_flags;
        uint32_t pte_ppn;
        size_t   way;
        size_t   set;
        size_t   word;
        int      cache_state;

        r_dcache.read(r_dcache_tlb_paddr.read(),
                      &pte_flags,
                      &pte_ppn,
                      &way,
                      &set,
                      &word,
                      &cache_state);
#ifdef INSTRUMENTATION
        m_cpt_dcache_data_read++;
        m_cpt_dcache_dir_read++;
#endif
        if (cache_state == CACHE_SLOT_STATE_VALID) // hit in dcache
        {
            if (not (pte_flags & PTE_V_MASK)) // unmapped
            {
                if (r_dcache_tlb_ins.read())
                {
                    r_mmu_ietr             = MMU_READ_PT2_UNMAPPED;
                    r_mmu_ibvar            = r_dcache_tlb_vaddr.read();
                    r_icache_tlb_miss_req  = false;
                    r_icache_tlb_rsp_error = true;
                }
                else
                {
                    r_mmu_detr   = MMU_READ_PT2_UNMAPPED;
                    r_mmu_dbvar  = r_dcache_tlb_vaddr.read();
                    m_drsp.valid = true;
                    m_drsp.error = true;
                }
                r_dcache_fsm = DCACHE_IDLE;

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE2_GET> HIT in dcache, but PTE unmapped"
                        << " PTE_FLAGS = " << std::hex << pte_flags
                        << " PTE_PPN = " << std::hex << pte_ppn << std::endl;
                }
#endif
            }
            else // mapped : we must update the TLB
            {
                r_dcache_in_tlb[m_dcache_sets * way + set] = true;
                r_dcache_tlb_pte_flags  = pte_flags;
                r_dcache_tlb_pte_ppn    = pte_ppn;
                r_dcache_tlb_cache_way  = way;
                r_dcache_tlb_cache_set  = set;
                r_dcache_tlb_cache_word = word;
                r_dcache_fsm            = DCACHE_TLB_PTE2_SELECT;

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE2_GET> HIT in dcache:"
                        << " PTE_FLAGS = " << std::hex << pte_flags
                        << " PTE_PPN = " << std::hex << pte_ppn << std::endl;
                }
#endif
             }
        }
        else if (cache_state == CACHE_SLOT_STATE_ZOMBI) // pending cleanup
        {
            // stalled until cleanup is acknowledged
            r_dcache_fsm   = DCACHE_TLB_PTE2_GET;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_PTE2_GET> ZOMBI in dcache: waiting cleanup ack"
                    << std::endl;
            }
#endif
        }
        else            // we must load the missing cache line in dcache
        {
            r_dcache_fsm          = DCACHE_MISS_SELECT;
            r_dcache_vci_miss_req = true;
            r_dcache_vci_paddr    = r_dcache_tlb_paddr.read();
            r_dcache_save_paddr   = r_dcache_tlb_paddr.read();
            r_dcache_miss_type    = PTE2_MISS;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_PTE2_GET> MISS in dcache:"
                    << " PTE address = " << std::hex << r_dcache_tlb_paddr.read() << std::endl;
            }
#endif
        }
        break;
    }
    ////////////////////////////
    case DCACHE_TLB_PTE2_SELECT:    // select a slot for PTE2
    {
        size_t way;
        size_t set;

        if (r_dcache_tlb_ins.read())
        {
            r_itlb.select(r_dcache_tlb_vaddr.read(),
                          false, // PTE2
                          &way,
                          &set);
#ifdef INSTRUMENTATION
            m_cpt_itlb_read++;
#endif
        }
        else
        {
            r_dtlb.select(r_dcache_tlb_vaddr.read(),
                          false, // PTE2
                          &way,
                          &set);
#ifdef INSTRUMENTATION
            m_cpt_dtlb_read++;
#endif
        }

#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            if (r_dcache_tlb_ins.read())
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_PTE2_SELECT> Select a slot in ITLB:";
            else
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_PTE2_SELECT> Select a slot in DTLB:";
            std::cout << " way = " << std::dec << way
                << " / set = " << set << std::endl;
        }
#endif
        r_dcache_tlb_way = way;
        r_dcache_tlb_set = set;
        r_dcache_fsm     = DCACHE_TLB_PTE2_UPDT;
        break;
    }
    //////////////////////////
    case DCACHE_TLB_PTE2_UPDT:  // write a new PTE2 in tlb after testing the L/R bit
                                // - if L/R bit already set, exit the sub-fsm.
                                // - if not, we update the page table but we dont write
                                //   neither in DCACHE, nor in TLB, as this will be done by
                                //   the coherence mechanism.
    {
        paddr_t  nline     = r_dcache_tlb_paddr.read() >> (uint32_log2(m_dcache_words) + 2);
        uint32_t pte_flags = r_dcache_tlb_pte_flags.read();
        uint32_t pte_ppn   = r_dcache_tlb_pte_ppn.read();
        bool     pt_updt   = false;
        bool     local     = true;

        // We should compute the access locality:
        // The PPN MSB bits define the destination cluster index.
        // The m_srcid MSB bits define the source cluster index.
        // The number of bits to compare depends on the number of clusters,
        // and can be obtained in the mapping table.
        // As long as this computation is not done, all access are local.

        if (local) // local access
        {
            if (not ((pte_flags & PTE_L_MASK) == PTE_L_MASK)) // we must set the L bit
            {
                pt_updt                = true;
                r_dcache_vci_cas_old   = pte_flags;
                r_dcache_vci_cas_new   = pte_flags | PTE_L_MASK;
                pte_flags              = pte_flags | PTE_L_MASK;
                r_dcache_tlb_pte_flags = pte_flags;
            }
        }
        else                                                    // remote access
        {
            if (not ((pte_flags & PTE_R_MASK) == PTE_R_MASK)) // we must set the R bit
            {
                pt_updt                = true;
                r_dcache_vci_cas_old   = pte_flags;
                r_dcache_vci_cas_new   = pte_flags | PTE_R_MASK;
                pte_flags              = pte_flags | PTE_R_MASK;
                r_dcache_tlb_pte_flags = pte_flags;
            }
        }

        if (not pt_updt) // update TLB
        {
            if (r_dcache_tlb_ins.read())
            {
                r_itlb.write( false, // 4K page
                              pte_flags,
                              pte_ppn,
                              r_dcache_tlb_vaddr.read(),
                              r_dcache_tlb_way.read(),
                              r_dcache_tlb_set.read(),
                              nline );
#ifdef INSTRUMENTATION
                m_cpt_itlb_write++;
#endif

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE2_UPDT> write PTE2 in ITLB"
                        << " / set = " << std::dec << r_dcache_tlb_set.read()
                        << " / way = " << r_dcache_tlb_way.read() << std::endl;
                    r_itlb.printTrace();
                }
#endif
            }
            else
            {
                r_dtlb.write(false, // 4K page
                             pte_flags,
                             pte_ppn,
                             r_dcache_tlb_vaddr.read(),
                             r_dcache_tlb_way.read(),
                             r_dcache_tlb_set.read(),
                             nline);
#ifdef INSTRUMENTATION
                m_cpt_dtlb_write++;
#endif

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                {
                    std::cout << "  <PROC " << name()
                        << " DCACHE_TLB_PTE2_UPDT> write PTE2 in DTLB"
                        << " / set = " << std::dec << r_dcache_tlb_set.read()
                        << " / way = " << r_dcache_tlb_way.read() << std::endl;
                    r_dtlb.printTrace();
                }
#endif

            }
            r_dcache_fsm = DCACHE_TLB_RETURN;
        }
        else                                   // update page table but not TLB
        {
            r_dcache_fsm = DCACHE_TLB_LR_UPDT; // dcache and page table update

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_PTE2_UPDT> L/R bit update required" << std::endl;
            }
#endif
        }
        break;
    }
    ////////////////////////
    case DCACHE_TLB_LR_UPDT:        // request a CAS transaction to update L/R bit
    {
#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " DCACHE_TLB_LR_UPDT> Update dcache: (L/R) bit" << std::endl;
        }
#endif
        // r_dcache_vci_cas_old & r_dcache_vci_cas_new registers are already set
        r_dcache_vci_paddr = r_dcache_tlb_paddr.read();

        // checking llsc reservation buffer
        if (r_dcache_llsc_paddr.read() == r_dcache_tlb_paddr.read())
            r_dcache_llsc_valid = false;

        // request a CAS CMD and go to DCACHE_TLB_LR_WAIT state
        r_dcache_vci_cas_req = true;
        r_dcache_fsm = DCACHE_TLB_LR_WAIT;
        break;
    }
    ////////////////////////
    case DCACHE_TLB_LR_WAIT:        // Waiting the response to SC transaction for DIRTY bit.
                                    // We consume the response in rsp FIFO,
                                    // and exit the sub-fsm, but we don't
                                    // analyse the response, because we don't
                                    // care if the L/R bit update is not done.
                                    // We must take the coherence requests because
                                    // there is a risk of dead-lock

    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (r_vci_rsp_data_error.read()) // bus error
        {
            std::cout << "BUS ERROR in DCACHE_TLB_LR_WAIT state" << std::endl;
            std::cout << "This should not happen in this state" << std::endl;
            exit(0);
        }
        else if (r_vci_rsp_fifo_dcache.rok()) // response available
        {
#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_TLB_LR_WAIT> SC response received" << std::endl;
            }
#endif
            vci_rsp_fifo_dcache_get = true;
            r_dcache_fsm = DCACHE_TLB_RETURN;
        }
        break;
    }
    ///////////////////////
    case DCACHE_TLB_RETURN:  // return to caller depending on tlb miss type
    {
#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " DCACHE_TLB_RETURN> TLB MISS completed" << std::endl;
        }
#endif
        if (r_dcache_tlb_ins.read()) r_icache_tlb_miss_req = false;
        r_dcache_fsm = DCACHE_IDLE;
        break;
    }
    ///////////////////////
    case DCACHE_XTN_SWITCH:     // The r_ptpr registers must be written,
                                // and both itlb and dtlb must be flushed.
                                // Caution : the itlb miss requests must be taken
                                // to avoid dead-lock in case of simultaneous ITLB miss
                                // Caution : the clack and cc requests must be taken
                                // to avoid dead-lock
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // itlb miss request
        if (r_icache_tlb_miss_req.read())
        {
            r_dcache_tlb_ins   = true;
            r_dcache_tlb_vaddr = r_icache_vaddr_save.read();
            r_dcache_fsm       = DCACHE_TLB_MISS;
            break;
        }

        if (not r_dcache_xtn_req.read())
        {
            r_dtlb.flush();
            r_mmu_ptpr   = m_dreq.wdata;
            r_dcache_fsm = DCACHE_IDLE;
            m_drsp.valid = true;
        }
        break;
    }
    /////////////////////
    case DCACHE_XTN_SYNC:  // waiting until write buffer empty
                           // The coherence request must be taken
                           // as there is a risk of dead-lock
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (r_wbuf.empty())
        {
            m_drsp.valid = true;
            r_dcache_fsm = DCACHE_IDLE;
        }
        break;
    }
    ////////////////////////
    case DCACHE_XTN_IC_FLUSH:       // Waiting completion of an XTN request to the ICACHE FSM
    case DCACHE_XTN_IC_INVAL_VA:    // Caution : the itlb miss requests must be taken
    case DCACHE_XTN_IC_INVAL_PA:    // because the XTN_ICACHE_INVAL request to icache
    case DCACHE_XTN_IC_PADDR_EXT:   // can generate an itlb miss,
    case DCACHE_XTN_IT_INVAL:       // and because it can exist a simultaneous ITLB miss

    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // itlb miss request
        if (r_icache_tlb_miss_req.read())
        {
            r_dcache_tlb_ins   = true;
            r_dcache_tlb_vaddr = r_icache_vaddr_save.read();
            r_dcache_fsm       = DCACHE_TLB_MISS;
            break;
        }

        // test if XTN request to icache completed
        if (not r_dcache_xtn_req.read())
        {
            r_dcache_fsm = DCACHE_IDLE;
            m_drsp.valid = true;
        }
        break;
    }
    /////////////////////////
    case DCACHE_XTN_DC_FLUSH:   // Invalidate sequencially all cache lines, using
                                // r_dcache_flush_count as a slot counter,
                                // looping in this state until all slots have been visited.
                                // It can require two cycles per slot:
                                // We test here the slot state, and make the actual inval
                                // (if line is valid) in DCACHE_XTN_DC_FLUSH_GO state.
                                // A cleanup request is generated for each valid line.
                                // returns to IDLE and flush TLBs when last slot
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (not r_dcache_cc_send_req.read()) // blocked until previous cc_send request is sent
        {
            int     state;
            paddr_t tag;
            size_t  way = r_dcache_flush_count.read() / m_dcache_sets;
            size_t  set = r_dcache_flush_count.read() % m_dcache_sets;

#ifdef INSTRUMENTATION
            m_cpt_dcache_dir_read++;
#endif
            r_dcache.read_dir(way,
                              set,
                              &tag,
                              &state);

            if (state == CACHE_SLOT_STATE_VALID) // inval required
            {
                // request cleanup
                r_dcache_cc_send_req   = true;
                r_dcache_cc_send_nline = tag * m_dcache_sets + set;
                r_dcache_cc_send_way   = way;
                r_dcache_cc_send_type  = CC_TYPE_CLEANUP;

                // goes to DCACHE_XTN_DC_FLUSH_GO to inval directory
                r_dcache_miss_way = way;
                r_dcache_miss_set = set;
                r_dcache_fsm      = DCACHE_XTN_DC_FLUSH_GO;
            }
            else if (r_dcache_flush_count.read() ==
                      (m_dcache_sets*m_dcache_ways - 1))  // last slot
            {
                r_dtlb.reset();
                r_itlb.reset();
                r_dcache_fsm = DCACHE_IDLE;
                m_drsp.valid = true;
            }

            // saturation counter
            if (r_dcache_flush_count.read() < (m_dcache_sets * m_dcache_ways - 1))
                r_dcache_flush_count = r_dcache_flush_count.read() + 1;
        }
        break;
    }
    ////////////////////////////
    case DCACHE_XTN_DC_FLUSH_GO:    // Switch the cache slot to ZOMBI state
                                    // and reset directory extension.
                                    // returns to IDLE and flush TLBs when last slot
    {
        size_t way = r_dcache_miss_way.read();
        size_t set = r_dcache_miss_set.read();

        r_dcache_in_tlb[m_dcache_sets * way + set]       = false;
        r_dcache_contains_ptd[m_dcache_sets * way + set] = false;

#ifdef INSTRUMENTATION
        m_cpt_dcache_dir_write++;
#endif
        r_dcache.write_dir(way,
                           set,
                           CACHE_SLOT_STATE_ZOMBI);

        if (r_dcache_flush_count.read() ==
             (m_dcache_sets*m_dcache_ways - 1))  // last slot
        {
            r_dtlb.reset();
            r_itlb.reset();
            r_dcache_fsm = DCACHE_IDLE;
            m_drsp.valid = true;
        }
        else
        {
            r_dcache_fsm = DCACHE_XTN_DC_FLUSH;
        }
        break;
    }
    /////////////////////////
    case DCACHE_XTN_DT_INVAL: // handling processor XTN_DTLB_INVAL request
    {
        r_dtlb.inval(r_dcache_save_wdata.read());
        r_dcache_fsm = DCACHE_IDLE;
        m_drsp.valid = true;
        break;
    }
    ////////////////////////////
    case DCACHE_XTN_DC_INVAL_VA:  // selective cache line invalidate with virtual address
                                  // requires 3 cycles: access tlb, read cache, inval cache
                                  // we compute the physical address in this state
    {
        paddr_t paddr;
        bool hit;

        if (r_mmu_mode.read() & DATA_TLB_MASK) // dtlb activated
        {

#ifdef INSTRUMENTATION
            m_cpt_dtlb_read++;
#endif
            hit = r_dtlb.translate(r_dcache_save_wdata.read(),
                                   &paddr);
        }
        else // dtlb not activated
        {
            paddr = (paddr_t)r_dcache_save_wdata.read();
            if (vci_param::N > 32)
                paddr = paddr | ((paddr_t)(r_dcache_paddr_ext.read()) << 32);
            hit = true;
        }

        if (hit) // tlb hit
        {
            r_dcache_save_paddr = paddr;
            r_dcache_fsm = DCACHE_XTN_DC_INVAL_PA;
        }
        else // tlb miss
        {

#ifdef INSTRUMENTATION
            m_cpt_dtlb_miss++;
#endif
            r_dcache_tlb_ins   = false; // dtlb
            r_dcache_tlb_vaddr = r_dcache_save_wdata.read();
            r_dcache_fsm       = DCACHE_TLB_MISS;
        }

#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " DCACHE_XTN_DC_INVAL_VA> Compute physical address" << std::hex
                << " / VADDR = " << r_dcache_save_wdata.read()
                << " / PADDR = " << paddr << std::endl;
        }
#endif

        break;
    }
    ////////////////////////////
    case DCACHE_XTN_DC_INVAL_PA:  // selective cache line invalidate with physical address
                                  // requires 2 cycles: read cache / inval cache
                                  // In this state we read dcache.
    {
        size_t way;
        size_t set;
        size_t word;
        int    state;

#ifdef INSTRUMENTATION
        m_cpt_dcache_dir_read++;
#endif
        r_dcache.read_dir(r_dcache_save_paddr.read(),
                          &state,
                          &way,
                          &set,
                          &word);

        if (state == CACHE_SLOT_STATE_VALID) // inval to be done
        {
            r_dcache_xtn_way = way;
            r_dcache_xtn_set = set;
            r_dcache_fsm = DCACHE_XTN_DC_INVAL_GO;
        }
        else // miss : nothing to do
        {
            r_dcache_fsm = DCACHE_IDLE;
            m_drsp.valid = true;
        }

#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " DCACHE_XTN_DC_INVAL_PA> Test hit in dcache" << std::hex
                << " / PADDR = " << r_dcache_save_paddr.read() << std::dec
                << " / HIT = " << (state == CACHE_SLOT_STATE_VALID)
                << " / SET = " << set
                << " / WAY = " << way << std::endl;
        }
#endif
        break;
    }
    ////////////////////////////
    case DCACHE_XTN_DC_INVAL_GO:  // In this state, we invalidate the cache line
                                  // Blocked if previous cleanup not completed
                                  // Test if itlb or dtlb inval is required
    {
        if (not r_dcache_cc_send_req.read()) // blocked until previous cc_send request is sent
        {
            size_t way    = r_dcache_xtn_way.read();
            size_t set    = r_dcache_xtn_set.read();
            paddr_t nline = r_dcache_save_paddr.read() / (m_dcache_words << 2);

#ifdef INSTRUMENTATION
            m_cpt_dcache_dir_write++;
#endif
            r_dcache.write_dir(way,
                               set,
                               CACHE_SLOT_STATE_ZOMBI);

            // request cleanup
            r_dcache_cc_send_req   = true;
            r_dcache_cc_send_nline = nline;
            r_dcache_cc_send_way   = way;
            r_dcache_cc_send_type  = CC_TYPE_CLEANUP;

            // possible itlb & dtlb invalidate
            if (r_dcache_in_tlb[way * m_dcache_sets + set])
            {
                r_dcache_tlb_inval_line = nline;
                r_dcache_tlb_inval_set  = 0;
                r_dcache_fsm_scan_save  = DCACHE_XTN_DC_INVAL_END;
                r_dcache_fsm            = DCACHE_INVAL_TLB_SCAN;
                r_dcache_in_tlb[way * m_dcache_sets + set] = false;
            }
            else if (r_dcache_contains_ptd[way * m_dcache_sets + set])
            {
                r_itlb.reset();
                r_dtlb.reset();
                r_dcache_contains_ptd[way * m_dcache_sets + set] = false;
                r_dcache_fsm = DCACHE_IDLE;
                m_drsp.valid = true;
            }
            else
            {
                r_dcache_fsm = DCACHE_IDLE;
                m_drsp.valid = true;
            }

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_XTN_DC_INVAL_GO> Actual dcache inval" << std::hex
                    << " / PADDR = " << r_dcache_save_paddr.read() << std::endl;
            }
#endif
        }
        break;
    }
    //////////////////////////////
    case DCACHE_XTN_DC_INVAL_END: // send response to processor XTN request
    {
        r_dcache_fsm = DCACHE_IDLE;
        m_drsp.valid = true;
        break;
    }
    ////////////////////////
    case DCACHE_MISS_SELECT:       // Try to select a slot in associative set,
                                   // Waiting in this state if no slot available.
                                   // If a victim slot has been choosen and the r_icache_cc_send_req is false,
                                   // we send the cleanup request in this state.
                                   // If not, a r_icache_cleanup_victim_req flip-flop is
                                   // utilized for saving this cleanup request, and it will be sent later
                                   // in state ICACHE_MISS_WAIT or ICACHE_MISS_UPDT_DIR.
                                   // The r_icache_miss_clack flip-flop is set
                                   // when a cleanup is required
    {
        if (m_dreq.valid) m_cost_data_miss_frz++;

        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        bool    found = false;
        bool    cleanup = false;
        size_t  way = 0;
        size_t  set = 0;
        paddr_t victim = 0;

#ifdef INSTRUMENTATION
        m_cpt_dcache_dir_read++;
#endif
        r_dcache.read_select(r_dcache_save_paddr.read(),
                             &victim,
                             &way,
                             &set,
                             &found,
                             &cleanup);

        if (not found)
        {
            break;
        }
        else
        {
            r_dcache_miss_way = way;
            r_dcache_miss_set = set;

            if (cleanup)
            {
                if (not r_dcache_cc_send_req.read())
                {
                    r_dcache_cc_send_req   = true;
                    r_dcache_cc_send_nline = victim;
                    r_dcache_cc_send_way   = way;
                    r_dcache_cc_send_type  = CC_TYPE_CLEANUP;

                }
                else
                {
                    r_dcache_cleanup_victim_req   = true;
                    r_dcache_cleanup_victim_nline = victim;
                }

                r_dcache_miss_clack = true;
                r_dcache_fsm        = DCACHE_MISS_CLEAN;
            }
            else
            {
                r_dcache_fsm = DCACHE_MISS_WAIT;
            }

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_MISS_SELECT> Select a slot:" << std::dec
                    << " / WAY = "   << way
                    << " / SET = "   << set
                    << " / PADDR = " << std::hex << r_dcache_save_paddr.read();
                if (cleanup) std::cout << " / VICTIM = " << (victim*m_dcache_words*4) << std::endl;
                else        std::cout << std::endl;
            }
#endif
        } // end found
        break;
    }
    ///////////////////////
    case DCACHE_MISS_CLEAN:     // switch the slot to ZOMBI state
                                // and possibly request itlb or dtlb invalidate
    {
        if (m_dreq.valid) m_cost_data_miss_frz++;

        size_t way = r_dcache_miss_way.read();
        size_t set = r_dcache_miss_set.read();

#ifdef INSTRUMENTATION
        m_cpt_dcache_dir_read++;
#endif
        r_dcache.write_dir(way,
                           set,
                           CACHE_SLOT_STATE_ZOMBI);
#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " DCACHE_MISS_CLEAN> Switch to ZOMBI state" << std::dec
                << " / way = "   << way
                << " / set = "   << set << std::endl;
        }
#endif
        // if selective itlb & dtlb invalidate are required
        // the miss response is not handled before invalidate completed
        if (r_dcache_in_tlb[way * m_dcache_sets + set])
        {
            r_dcache_in_tlb[way * m_dcache_sets + set] = false;

            if (not r_dcache_cleanup_victim_req.read())
                r_dcache_tlb_inval_line = r_dcache_cc_send_nline.read();
            else
                r_dcache_tlb_inval_line = r_dcache_cleanup_victim_nline.read();

            r_dcache_tlb_inval_set = 0;
            r_dcache_fsm_scan_save = DCACHE_MISS_WAIT;
            r_dcache_fsm           = DCACHE_INVAL_TLB_SCAN;
        }
        else if (r_dcache_contains_ptd[way * m_dcache_sets + set])
        {
            r_itlb.reset();
            r_dtlb.reset();
            r_dcache_contains_ptd[way * m_dcache_sets + set] = false;
            r_dcache_fsm = DCACHE_MISS_WAIT;
        }
        else
        {
            r_dcache_fsm = DCACHE_MISS_WAIT;
        }
        break;
    }
    //////////////////////
    case DCACHE_MISS_WAIT: // waiting the response to a miss request from VCI_RSP FSM
                            // This state is in charge of error signaling
                            // There is 5 types of error depending on the requester
    {
        if (m_dreq.valid) m_cost_data_miss_frz++;

        // send cleanup victim request
        if (r_dcache_cleanup_victim_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_cc_send_req        = true;
            r_dcache_cc_send_nline      = r_dcache_cleanup_victim_nline;
            r_dcache_cc_send_way        = r_dcache_miss_way;
            r_dcache_cc_send_type       = CC_TYPE_CLEANUP;
            r_dcache_cleanup_victim_req = false;
        }

        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and
             not r_dcache_cc_send_req.read() and
             not r_dcache_cleanup_victim_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (r_vci_rsp_data_error.read()) // bus error
        {
            switch (r_dcache_miss_type.read())
            {
                case PROC_MISS:
                {
                    r_mmu_detr   = MMU_READ_DATA_ILLEGAL_ACCESS;
                    r_mmu_dbvar  = r_dcache_save_vaddr.read();
                    m_drsp.valid = true;
                    m_drsp.error = true;
                    r_dcache_fsm = DCACHE_IDLE;
                    break;
                }
                case PTE1_MISS:
                {
                    if (r_dcache_tlb_ins.read())
                    {
                        r_mmu_ietr             = MMU_READ_PT1_ILLEGAL_ACCESS;
                        r_mmu_ibvar            = r_dcache_tlb_vaddr.read();
                        r_icache_tlb_miss_req  = false;
                        r_icache_tlb_rsp_error = true;
                    }
                    else
                    {
                        r_mmu_detr   = MMU_READ_PT1_ILLEGAL_ACCESS;
                        r_mmu_dbvar  = r_dcache_tlb_vaddr.read();
                        m_drsp.valid = true;
                        m_drsp.error = true;
                    }
                    r_dcache_fsm = DCACHE_IDLE;
                    break;
                }
                case PTE2_MISS:
                {
                    if (r_dcache_tlb_ins.read())
                    {
                        r_mmu_ietr             = MMU_READ_PT2_ILLEGAL_ACCESS;
                        r_mmu_ibvar            = r_dcache_tlb_vaddr.read();
                        r_icache_tlb_miss_req  = false;
                        r_icache_tlb_rsp_error = true;
                    }
                    else
                    {
                        r_mmu_detr   = MMU_READ_PT2_ILLEGAL_ACCESS;
                        r_mmu_dbvar  = r_dcache_tlb_vaddr.read();
                        m_drsp.valid  = true;
                        m_drsp.error  = true;
                    }
                    r_dcache_fsm = DCACHE_IDLE;
                    break;
                }
            } // end switch type
            r_vci_rsp_data_error = false;
        }
        else if (r_vci_rsp_fifo_dcache.rok()) // valid response available
        {
            r_dcache_miss_word = 0;
            r_dcache_fsm       = DCACHE_MISS_DATA_UPDT;
        }
        break;
    }
    //////////////////////////
    case DCACHE_MISS_DATA_UPDT:  // update the dcache (one word per cycle)
    {
        if (m_dreq.valid) m_cost_data_miss_frz++;

        if (r_vci_rsp_fifo_dcache.rok()) // one word available
        {
#ifdef INSTRUMENTATION
            m_cpt_dcache_data_write++;
#endif
            r_dcache.write(r_dcache_miss_way.read(),
                               r_dcache_miss_set.read(),
                               r_dcache_miss_word.read(),
                               r_vci_rsp_fifo_dcache.read());
#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_MISS_DATA_UPDT> Write one word:"
                    << " / DATA = "  << std::hex << r_vci_rsp_fifo_dcache.read()
                    << " / WAY = "   << std::dec << r_dcache_miss_way.read()
                    << " / SET = "   << r_dcache_miss_set.read()
                    << " / WORD = "  << r_dcache_miss_word.read() << std::endl;
            }
#endif
            vci_rsp_fifo_dcache_get = true;
            r_dcache_miss_word = r_dcache_miss_word.read() + 1;

            if (r_dcache_miss_word.read() == (m_dcache_words - 1)) // last word
            {
                r_dcache_fsm = DCACHE_MISS_DIR_UPDT;
            }
        }
        break;
    }
    //////////////////////////
    case DCACHE_MISS_DIR_UPDT:  // Stalled if a victim line has been evicted
                                // and the cleanup ack has not been received,
                                // as indicated by the r_dcache_miss clack.
                                // - If no matching coherence request (r_dcache_inval_miss)
                                //   switch directory slot to VALID state.
                                // - If matching coherence request, switch directory slot
                                //   to ZOMBI state, and send a cleanup request.
    {
        if (m_dreq.valid) m_cost_data_miss_frz++;

        // send cleanup victim request
        if (r_dcache_cleanup_victim_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_cc_send_req        = true;
            r_dcache_cc_send_nline      = r_dcache_cleanup_victim_nline;
            r_dcache_cc_send_way        = r_dcache_miss_way;
            r_dcache_cc_send_type       = CC_TYPE_CLEANUP;
            r_dcache_cleanup_victim_req = false;
        }

        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and
             not r_dcache_cc_send_req.read() and
             not r_dcache_cleanup_victim_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (not r_dcache_miss_clack.read())  // waiting cleanup acknowledge
        {
            if (r_dcache_miss_inval.read()) // switch slot to ZOMBI state, and new cleanup
            {
                if (not r_dcache_cc_send_req.read()) // blocked until previous request sent
                {
                    r_dcache_miss_inval     = false;
                    // request cleanup
                    r_dcache_cc_send_req   = true;
                    r_dcache_cc_send_nline = r_dcache_save_paddr.read() / (m_dcache_words << 2);
                    r_dcache_cc_send_way   = r_dcache_miss_way.read();
                    r_dcache_cc_send_type  = CC_TYPE_CLEANUP;

#ifdef INSTRUMENTATION
                    m_cpt_dcache_dir_write++;
#endif
                    r_dcache.write_dir( r_dcache_save_paddr.read(),
                                        r_dcache_miss_way.read(),
                                        r_dcache_miss_set.read(),
                                        CACHE_SLOT_STATE_ZOMBI );
#if DEBUG_DCACHE
                    if (m_debug_dcache_fsm)
                        std::cout << "  <PROC " << name()
                            << " DCACHE_MISS_DIR_UPDT> Switch slot to ZOMBI state"
                            << " PADDR = " << std::hex << r_dcache_save_paddr.read()
                            << " / WAY = "   << std::dec << r_dcache_miss_way.read()
                            << " / SET = "   << r_dcache_miss_set.read() << std::endl;
#endif
                }
                else
                    break;
            }
            else                              // switch slot to VALID state
            {

#ifdef INSTRUMENTATION
                m_cpt_dcache_dir_write++;
#endif
                r_dcache.write_dir(r_dcache_save_paddr.read(),
                                   r_dcache_miss_way.read(),
                                   r_dcache_miss_set.read(),
                                   CACHE_SLOT_STATE_VALID);

#if DEBUG_DCACHE
                if (m_debug_dcache_fsm)
                    std::cout << "  <PROC " << name()
                        << " DCACHE_MISS_DIR_UPDT> Switch slot to VALID state"
                        << " PADDR = " << std::hex << r_dcache_save_paddr.read()
                        << " / WAY = "   << std::dec << r_dcache_miss_way.read()
                        << " / SET = "   << r_dcache_miss_set.read() << std::endl;
#endif
                // reset directory extension
                size_t way = r_dcache_miss_way.read();
                size_t set = r_dcache_miss_set.read();
                r_dcache_in_tlb[way * m_dcache_sets + set] = false;
                r_dcache_contains_ptd[way * m_dcache_sets + set] = false;
            }
            if      (r_dcache_miss_type.read() == PTE1_MISS) r_dcache_fsm = DCACHE_TLB_PTE1_GET;
            else if (r_dcache_miss_type.read() == PTE2_MISS) r_dcache_fsm = DCACHE_TLB_PTE2_GET;
            else                                             r_dcache_fsm = DCACHE_IDLE;
        }
        break;
    }
    /////////////////////
    case DCACHE_UNC_WAIT:  // waiting a response to an uncacheable read/write
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (r_vci_rsp_data_error.read()) // bus error
        {
            if (r_dcache_vci_unc_write.read())
                r_mmu_detr = MMU_WRITE_DATA_ILLEGAL_ACCESS;
            else
                r_mmu_detr = MMU_READ_DATA_ILLEGAL_ACCESS;

            r_mmu_dbvar          = m_dreq.addr;
            r_vci_rsp_data_error = false;
            m_drsp.error         = true;
            m_drsp.valid         = true;
            r_dcache_fsm         = DCACHE_IDLE;
            break;
        }
        else if (r_vci_rsp_fifo_dcache.rok())     // data available
        {
            // consume data
            vci_rsp_fifo_dcache_get = true;
            r_dcache_fsm            = DCACHE_IDLE;

            // acknowledge the processor request if it has not been modified
            if (m_dreq.valid and (m_dreq.addr == r_dcache_save_vaddr.read()))
            {
                m_drsp.valid = true;
                m_drsp.error = false;
                m_drsp.rdata = r_vci_rsp_fifo_dcache.read();
            }
        }
        break;
    }
    /////////////////////
    case DCACHE_LL_WAIT:    // waiting VCI response to a LL transaction
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (r_vci_rsp_data_error.read()) // bus error
        {
            r_mmu_detr           = MMU_READ_DATA_ILLEGAL_ACCESS;
            r_mmu_dbvar          = m_dreq.addr;
            r_vci_rsp_data_error = false;
            m_drsp.error         = true;
            m_drsp.valid         = true;
            r_dcache_fsm         = DCACHE_IDLE;
            break;
        }
        else if (r_vci_rsp_fifo_dcache.rok())     // data available
        {
            // consume data
            vci_rsp_fifo_dcache_get = true;

            if (r_dcache_ll_rsp_count.read() == 0) // first flit
            {
                // set key value in llsc reservation buffer
                r_dcache_llsc_key     = r_vci_rsp_fifo_dcache.read();
                r_dcache_ll_rsp_count = r_dcache_ll_rsp_count.read() + 1;
            }
            else                                  // last flit
            {
                // acknowledge the processor request if it has not been modified
                if (m_dreq.valid and (m_dreq.addr == r_dcache_save_vaddr.read()))
                {
                    m_drsp.valid = true;
                    m_drsp.error = false;
                    m_drsp.rdata = r_vci_rsp_fifo_dcache.read();
                }
                r_dcache_fsm = DCACHE_IDLE;
            }
        }
        break;
    }
    ////////////////////
    case DCACHE_SC_WAIT: // waiting VCI response to a SC transaction
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (r_vci_rsp_data_error.read()) // bus error
        {
            r_mmu_detr           = MMU_READ_DATA_ILLEGAL_ACCESS;
            r_mmu_dbvar          = m_dreq.addr;
            r_vci_rsp_data_error = false;
            m_drsp.error         = true;
            m_drsp.valid         = true;
            r_dcache_fsm         = DCACHE_IDLE;
            break;
        }
        else if (r_vci_rsp_fifo_dcache.rok()) // response available
        {
            // consume response
            vci_rsp_fifo_dcache_get = true;
            m_drsp.valid            = true;
            m_drsp.rdata            = r_vci_rsp_fifo_dcache.read();
            r_dcache_fsm            = DCACHE_IDLE;
        }
        break;
    }
    //////////////////////////
    case DCACHE_DIRTY_GET_PTE:  // This sub_fsm set the PTE Dirty bit in memory
                                // before handling a processor WRITE or SC request
                                // Input argument is r_dcache_dirty_paddr
                                // In this first state, we get PTE value in dcache
                                // and post a CAS request to CMD FSM
    {
        // get PTE in dcache
        uint32_t pte;
        size_t   way;
        size_t   set;
        size_t   word; // unused
        int      state;

#ifdef INSTRUMENTATION
        m_cpt_dcache_data_read++;
        m_cpt_dcache_dir_read++;
#endif
        r_dcache.read(r_dcache_dirty_paddr.read(),
                      &pte,
                      &way,
                      &set,
                      &word,
                      &state);

        assert( (state == CACHE_SLOT_STATE_VALID) and
        "error in DCACHE_DIRTY_TLB_SET: the PTE should be in dcache" );

        // request CAS transaction to CMD_FSM
        r_dcache_dirty_way = way;
        r_dcache_dirty_set = set;

        // check llsc reservation buffer
        if (r_dcache_llsc_paddr.read() == r_dcache_dirty_paddr.read())
            r_dcache_llsc_valid = false;

        // request a CAS CMD and go to DCACHE_DIRTY_WAIT state
        r_dcache_vci_cas_req = true;
        r_dcache_vci_paddr   = r_dcache_dirty_paddr.read();
        r_dcache_vci_cas_old = pte;
        r_dcache_vci_cas_new = pte | PTE_D_MASK;
        r_dcache_fsm         = DCACHE_DIRTY_WAIT;

#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " DCACHE_DIRTY_GET_PTE> CAS request" << std::hex
                << " / PTE_PADDR = " << r_dcache_dirty_paddr.read()
                << " / PTE_VALUE = " << pte << std::dec
                << " / SET = " << set
                << " / WAY = " << way << std::endl;
        }
#endif
        break;
    }
    ///////////////////////
    case DCACHE_DIRTY_WAIT:    // wait completion of CAS for PTE Dirty bit,
                               // and return to IDLE state when response is received.
                               // we don't care if the CAS is a failure:
                               // - if the CAS is a success, the coherence mechanism
                               //   updates the local copy.
                               // - if the CAS is a failure, we just retry the write.
    {
        // coherence clack request (from DSPIN CLACK)
        if (r_dcache_clack_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        // coherence request (from CC_RECEIVE FSM)
        if (r_cc_receive_dcache_req.read() and not r_dcache_cc_send_req.read())
        {
            r_dcache_fsm = DCACHE_CC_CHECK;
            r_dcache_fsm_cc_save = r_dcache_fsm.read();
            break;
        }

        if (r_vci_rsp_data_error.read())      // bus error
        {
            std::cout << "BUS ERROR in DCACHE_DIRTY_WAIT state" << std::endl;
            std::cout << "This should not happen in this state" << std::endl;
            exit(0);
        }
        else if (r_vci_rsp_fifo_dcache.rok()) // response available
        {
            vci_rsp_fifo_dcache_get = true;
            r_dcache_fsm            = DCACHE_IDLE;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_DIRTY_WAIT> CAS completed" << std::endl;
            }
#endif
        }
        break;
    }
    /////////////////////
    case DCACHE_CC_CHECK:   // This state is the entry point for the sub-FSM
                            // handling coherence requests for DCACHE.
                            // If there is a matching pending miss on the modified cache
                            // line this is signaled in the r_dcache_miss inval flip-flop.
                            // If the updated (or invalidated) cache line has copies in TLBs
                            // these TLB copies are invalidated.
                            // The return state is defined in r_dcache_fsm_cc_save
    {
        paddr_t paddr = r_cc_receive_dcache_nline.read() * m_dcache_words * 4;
        paddr_t mask = ~((m_dcache_words << 2) - 1);

        // CLACK handler
        // We switch the directory slot to EMPTY state and reset
        // r_dcache_miss_clack if the cleanup ack is matching a pending miss.
        if (r_dcache_clack_req.read())
        {
            if (m_dreq.valid ) m_cost_data_miss_frz++;

#ifdef INSTRUMENTATION
            m_cpt_dcache_dir_write++;
#endif
            r_dcache.write_dir(0,
                               r_dcache_clack_way.read(),
                               r_dcache_clack_set.read(),
                               CACHE_SLOT_STATE_EMPTY);

            if ((r_dcache_miss_set.read() == r_dcache_clack_set.read()) and
                (r_dcache_miss_way.read() == r_dcache_clack_way.read()))
            {
                  r_dcache_miss_clack = false;
            }

            r_dcache_clack_req = false;

            // return to cc_save state
            r_dcache_fsm = r_dcache_fsm_cc_save.read() ;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_CC_CHECK> CLACK for PADDR " << paddr
                    << " Switch slot to EMPTY state : "
                    << " set = " << r_dcache_clack_set.read()
                    << " / way = " << r_dcache_clack_way.read() << std::endl;
            }
#endif
            break;
        }

        assert(not r_dcache_cc_send_req.read() and 
        "CC_SEND must be available in DCACHE_CC_CHECK");

        // Match between MISS address and CC address
        if (r_cc_receive_dcache_req.read() and
          ((r_dcache_fsm_cc_save == DCACHE_MISS_SELECT)  or
           (r_dcache_fsm_cc_save == DCACHE_MISS_WAIT)  or
           (r_dcache_fsm_cc_save == DCACHE_MISS_DIR_UPDT)) and
          ((r_dcache_vci_paddr.read() & mask) == (paddr & mask))) // matching
        {
            // signaling matching
            r_dcache_miss_inval = true;

            // in case of update, go to CC_UPDT
            // JUST TO POP THE FIFO
            if (r_cc_receive_dcache_type.read() == CC_TYPE_UPDT)
            {
                r_dcache_fsm     = DCACHE_CC_UPDT;
                r_dcache_cc_word = r_cc_receive_word_idx.read();

                // just pop the fifo , don't write in icache
                r_dcache_cc_need_write = false;
            }
            // the request is dealt with
            else
            {
                r_cc_receive_dcache_req = false;
                r_dcache_fsm            = r_dcache_fsm_cc_save.read();
            }

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_CC_CHECK> Coherence request matching a pending miss:"
                    << " PADDR = " << std::hex << paddr << std::endl;
            }
#endif
        }

        // CC request handler

        int    state = 0;
        size_t way   = 0;
        size_t set   = 0;
        size_t word  = 0;

#ifdef INSTRUMENTATION
        m_cpt_dcache_dir_read++;
#endif
        r_dcache.read_dir(paddr,
                          &state,
                          &way,
                          &set,
                          &word); // unused

        r_dcache_cc_way = way;
        r_dcache_cc_set = set;

        if (state == CACHE_SLOT_STATE_VALID) // hit
        {
            // need to update the cache state
            if (r_cc_receive_dcache_type.read() == CC_TYPE_UPDT) // hit update
            {
                r_dcache_cc_need_write = true;
                r_dcache_fsm           = DCACHE_CC_UPDT;
                r_dcache_cc_word       = r_cc_receive_word_idx.read();
            }
            else if (r_cc_receive_dcache_type.read() == CC_TYPE_INVAL) // hit inval
            {
                r_dcache_fsm = DCACHE_CC_INVAL;
            }
        }
        else                                  // miss
        {
            // multicast acknowledgement required in case of update
            if (r_cc_receive_dcache_type.read() == CC_TYPE_UPDT)
            {
                r_dcache_fsm     = DCACHE_CC_UPDT;
                r_dcache_cc_word = r_cc_receive_word_idx.read();

                // just pop the fifo , don't write in icache
                r_dcache_cc_need_write = false;
            }
            else // No response needed
            {
                r_cc_receive_dcache_req = false;
                r_dcache_fsm = r_dcache_fsm_cc_save.read();
            }
        }

#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " DCACHE_CC_CHECK> Coherence request received:"
                << " PADDR = " << std::hex << paddr
                << " / TYPE = " << std::dec << r_cc_receive_dcache_type.read()
                << " / HIT = " << (state == CACHE_SLOT_STATE_VALID) << std::endl;
        }
#endif

        break;
    }
    /////////////////////
    case DCACHE_CC_INVAL: // hit inval: switch slot to ZOMBI state and send a
                          // CLEANUP after possible invalidation of copies in
                          // TLBs
    {
        size_t way = r_dcache_cc_way.read();
        size_t set = r_dcache_cc_set.read();

        if (r_dcache_in_tlb[way * m_dcache_sets + set])       // selective TLB inval
        {
            r_dcache_in_tlb[way * m_dcache_sets + set] = false;
            r_dcache_tlb_inval_line = r_cc_receive_dcache_nline.read();
            r_dcache_tlb_inval_set  = 0;
            r_dcache_fsm_scan_save  = r_dcache_fsm.read();
            r_dcache_fsm            = DCACHE_INVAL_TLB_SCAN;
            break;
        }

        if (r_dcache_contains_ptd[way * m_dcache_sets + set]) // TLB flush
        {
            r_itlb.reset();
            r_dtlb.reset();
            r_dcache_contains_ptd[way * m_dcache_sets + set] = false;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                          << " DCACHE_CC_INVAL> Flush DTLB & ITLB" << std::endl;
            }
#endif
        }

        assert(not r_dcache_cc_send_req.read() &&
                "ERROR in DCACHE_CC_INVAL: the r_dcache_cc_send_req "
                "must not be set");

        // Switch slot state to ZOMBI and send CLEANUP command
        r_dcache.write_dir(way,
                           set,
                           CACHE_SLOT_STATE_ZOMBI);

        // coherence request completed
        r_cc_receive_dcache_req = false;
        r_dcache_cc_send_req    = true;
        r_dcache_cc_send_nline  = r_cc_receive_dcache_nline.read();
        r_dcache_cc_send_way    = r_dcache_cc_way.read();
        r_dcache_cc_send_type   = CC_TYPE_CLEANUP;
        r_dcache_fsm            = r_dcache_fsm_cc_save.read();

#if DEBUG_DCACHE
        if (m_debug_dcache_fsm)
        {
            std::cout << "  <PROC " << name()
                << " DCACHE_CC_INVAL> Switch slot to EMPTY state:" << std::dec
                << " / WAY = " << way
                << " / SET = " << set << std::endl;
        }
#endif
        break;
    }
    ///////////////////
    case DCACHE_CC_UPDT: // hit update: write one word per cycle,
                         // after possible invalidation of copies in TLBs
    {
        size_t word = r_dcache_cc_word.read();
        size_t way  = r_dcache_cc_way.read();
        size_t set  = r_dcache_cc_set.read();

        if (r_dcache_in_tlb[way * m_dcache_sets + set])       // selective TLB inval
        {
            r_dcache_in_tlb[way * m_dcache_sets + set] = false;
            r_dcache_tlb_inval_line = r_cc_receive_dcache_nline.read();
            r_dcache_tlb_inval_set  = 0;
            r_dcache_fsm_scan_save  = r_dcache_fsm.read();
            r_dcache_fsm            = DCACHE_INVAL_TLB_SCAN;

            break;
        }

        if (r_dcache_contains_ptd[way * m_dcache_sets + set]) // TLB flush
        {
            r_itlb.reset();
            r_dtlb.reset();
            r_dcache_contains_ptd[way * m_dcache_sets + set] = false;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_CC_UPDT> Flush DTLB & ITLB" << std::endl;
            }
#endif
        }

        assert (not r_dcache_cc_send_req.read() &&
                "ERROR in DCACHE_CC_INVAL: the r_dcache_cc_send_req "
                "must not be set");

        if (not r_cc_receive_updt_fifo_be.rok()) break;

        if (r_dcache_cc_need_write.read())
        {

#ifdef INSTRUMENTATION
            m_cpt_dcache_data_write++;
#endif
            r_dcache.write(way,
                           set,
                           word,
                           r_cc_receive_updt_fifo_data.read(),
                           r_cc_receive_updt_fifo_be.read());

            r_dcache_cc_word = word + 1;

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm)
            {
                std::cout << "  <PROC " << name()
                    << " DCACHE_CC_UPDT> Write one word" << std::dec
                    << " / WAY = " << way
                    << " / SET = " << set
                    << " / WORD = " << word
                    << " / VALUE = " << std::hex << r_cc_receive_updt_fifo_data.read() << std::endl;
            }
#endif
        }

        if (r_cc_receive_updt_fifo_eop.read())  // last word
        {
            // no need to write in the cache anymore
            r_dcache_cc_need_write = false;

            // coherence request completed
            r_cc_receive_dcache_req = false;

            // request multicast acknowledgement
            r_dcache_cc_send_req          = true;
            r_dcache_cc_send_nline        = r_cc_receive_dcache_nline.read();
            r_dcache_cc_send_updt_tab_idx = r_cc_receive_dcache_updt_tab_idx.read();
            r_dcache_cc_send_type         = CC_TYPE_MULTI_ACK;
            r_dcache_fsm                  = r_dcache_fsm_cc_save.read();
        }

        //consume fifo if not eop
        cc_receive_updt_fifo_get  = true;

        break;
    }
    ///////////////////////////
    case DCACHE_INVAL_TLB_SCAN:  // Scan sequencially all sets for both ITLB & DTLB
                                 // It makes assumption: m_itlb_sets == m_dtlb_sets
                                 // All ways are handled in parallel.
                                 // We enter this state when a DCACHE line is modified,
                                 // and there is a copy in itlb or dtlb.
                                 // It can be caused by:
                                 // - a coherence inval or updt transaction,
                                 // - a line inval caused by a cache miss
                                 // - a processor XTN inval request,
                                 // - a WRITE hit,
                                 // - a Dirty bit update
                                 // Input arguments are:
                                 // - r_dcache_tlb_inval_line
                                 // - r_dcache_tlb_inval_set
                                 // - r_dcache_fsm_scan_save
    {
        paddr_t line = r_dcache_tlb_inval_line.read();
        size_t set = r_dcache_tlb_inval_set.read();
        size_t way;
        bool ok;

        for (way = 0; way < m_itlb_ways; way++)
        {
            ok = r_itlb.inval(line, way, set);

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm and ok)
            {
                std::cout << "  <PROC " << name()
                    << ".DCACHE_INVAL_TLB_SCAN> Invalidate ITLB entry:" << std::hex
                    << " line = " << line << std::dec
                    << " / set = " << set
                    << " / way = " << way << std::endl;
            }
#endif
        }

        for (way = 0; way < m_dtlb_ways; way++)
        {
            ok = r_dtlb.inval( line, way, set);

#if DEBUG_DCACHE
            if (m_debug_dcache_fsm and ok)
                std::cout << "  <PROC " << name() << " DCACHE_INVAL_TLB_SCAN>"
                    << " Invalidate DTLB entry" << std::hex
                    << " / line = " << line << std::dec
                    << " / set = " << set
                    << " / way = " << way << std::endl;
#endif
        }

        // return to the calling state when TLB inval completed
        if (r_dcache_tlb_inval_set.read() == (m_dtlb_sets - 1))
        {
            r_dcache_fsm = r_dcache_fsm_scan_save.read();
        }
        r_dcache_tlb_inval_set = r_dcache_tlb_inval_set.read() + 1;
        break;
    }
    } // end switch r_dcache_fsm

    ///////////////// wbuf update ///////////////////////////////////////////////////////
    r_wbuf.update();

    ///////////////// llsc update ///////////////////////////////////////////////////////
    if (r_dcache_llsc_valid.read()) r_dcache_llsc_count = r_dcache_llsc_count.read() - 1;
    if (r_dcache_llsc_count.read() == 1) r_dcache_llsc_valid = false;

    //////////////// test processor frozen //////////////////////////////////////////////
    // The simulation exit if the number of consecutive frozen cycles
    // is larger than the m_max_frozen_cycles (constructor parameter)
    if ((m_ireq.valid and not m_irsp.valid) or (m_dreq.valid and not m_drsp.valid))
    {
        m_cpt_frz_cycles++;      // used for instrumentation
        m_cpt_stop_simulation++; // used for debug
        if (m_cpt_stop_simulation > m_max_frozen_cycles)
        {
            std::cout << std::dec << "ERROR in CC_VCACHE_WRAPPER " << name() << std::endl
                      << " stop at cycle " << m_cpt_total_cycles << std::endl
                      << " frozen since cycle " << m_cpt_total_cycles - m_max_frozen_cycles
                      << std::endl;
                      r_iss.dump();
            r_wbuf.printTrace();
            raise(SIGINT);
        }
    }
    else
    {
        m_cpt_stop_simulation = 0;
    }

    /////////// execute one iss cycle /////////////////////////////////
    {
        uint32_t it = 0;
        for (size_t i = 0; i < (size_t) iss_t::n_irq; i++) if (p_irq[i].read()) it |= (1 << i);
        r_iss.executeNCycles(1, m_irsp, m_drsp, it);
    }

    ////////////////////////////////////////////////////////////////////////////
    // The VCI_CMD FSM controls the following ressources:
    // - r_vci_cmd_fsm
    // - r_vci_cmd_min
    // - r_vci_cmd_max
    // - r_vci_cmd_cpt
    // - r_vci_cmd_imiss_prio
    // - wbuf (reset)
    // - r_icache_miss_req (reset)
    // - r_icache_unc_req (reset)
    // - r_dcache_vci_miss_req (reset)
    // - r_dcache_vci_unc_req (reset)
    // - r_dcache_vci_ll_req (reset)
    // - r_dcache_vci_sc_req (reset in case of local sc fail)
    // - r_dcache_vci_cas_req (reset)
    //
    // This FSM handles requests from both the DCACHE FSM & the ICACHE FSM.
    // There are 8 request types, with the following priorities :
    // 1 - Data Read Miss         : r_dcache_vci_miss_req and miss in the write buffer
    // 2 - Data Read Uncachable   : r_dcache_vci_unc_req
    // 3 - Instruction Miss       : r_icache_miss_req and miss in the write buffer
    // 4 - Instruction Uncachable : r_icache_unc_req
    // 5 - Data Write             : r_wbuf.rok()
    // 6 - Data Linked Load       : r_dcache_vci_ll_req
    // 7 - Data Store Conditionnal: r_dcache_vci_sc_req
    // 8 - Compare And Swap       : r_dcache_vci_cas_req
    //
    // As we want to support several simultaneous VCI transactions, the VCI_CMD_FSM
    // and the VCI_RSP_FSM are fully desynchronized.
    //
    // VCI formats:
    // According to the VCI advanced specification, all read requests packets
    // (data Uncached, Miss data, instruction Uncached, Miss instruction)
    // are one word packets.
    // For write burst packets, all words are in the same cache line,
    // and addresses must be contiguous (the BE field is 0 in case of "holes").
    // The sc command packet implements actually a compare-and-swap mechanism
    // and the packet contains two flits.
    ////////////////////////////////////////////////////////////////////////////////////


    switch (r_vci_cmd_fsm.read())
    {
        //////////////
        case CMD_IDLE:
        {
            // DCACHE read requests (r_dcache_vci_miss_req or r_dcache_vci_ll_req), and
            // ICACHE read requests (r_icache_miss_req) require both a write_buffer access
            // to check a possible pending write on the same cache line.
            // As there is only one possible access per cycle to write buffer, we implement
            // a round-robin priority between DCACHE and ICACHE for this access,
            // using the r_vci_cmd_imiss_prio flip-flop.

            size_t wbuf_min;
            size_t wbuf_max;

            bool dcache_miss_req = r_dcache_vci_miss_req.read() and
                 (not r_icache_miss_req.read() or not r_vci_cmd_imiss_prio.read());

            bool dcache_ll_req = r_dcache_vci_ll_req.read() and
                 (not r_icache_miss_req.read() or not r_vci_cmd_imiss_prio.read());

            bool dcache_sc_req = r_dcache_vci_sc_req.read() and
                 (not r_icache_miss_req.read() or not r_vci_cmd_imiss_prio.read());

            bool dcache_cas_req = r_dcache_vci_cas_req.read() and
                 (not r_icache_miss_req.read() or not r_vci_cmd_imiss_prio.read());

            bool icache_miss_req = r_icache_miss_req.read() and
                 (not (r_dcache_vci_miss_req.read() or
                       r_dcache_vci_ll_req.read()   or
                       r_dcache_vci_cas_req.read()  or
                       r_dcache_vci_sc_req.read())  or
                       r_vci_cmd_imiss_prio.read());

            // 1 - Data unc write
            if (r_dcache_vci_unc_req.read() and r_dcache_vci_unc_write.read())
            {
                r_vci_cmd_fsm        = CMD_DATA_UNC_WRITE;
                r_dcache_vci_unc_req = false;
            }
            // 2 data read miss
            else if (dcache_miss_req and r_wbuf.miss(r_dcache_vci_paddr.read()))
            {
                r_vci_cmd_fsm         = CMD_DATA_MISS;
                r_dcache_vci_miss_req = false;
                r_vci_cmd_imiss_prio  = true;
            }
            // 3 - Data Read Uncachable
            else if (r_dcache_vci_unc_req.read() and not r_dcache_vci_unc_write.read())
            {
                r_vci_cmd_fsm        = CMD_DATA_UNC_READ;
                r_dcache_vci_unc_req = false;
            }
            // 4 - Data Linked Load
            else if (dcache_ll_req and r_wbuf.miss(r_dcache_vci_paddr.read()))
            {
                r_vci_cmd_fsm         = CMD_DATA_LL;
                r_dcache_vci_ll_req   = false;
                r_vci_cmd_imiss_prio  = true;
            }
            // 5 - Instruction Miss
            else if (icache_miss_req and r_wbuf.miss(r_icache_vci_paddr.read()))
            {
                r_vci_cmd_fsm        = CMD_INS_MISS;
                r_icache_miss_req    = false;
                r_vci_cmd_imiss_prio = false;
            }
            // 6 - Instruction Uncachable
            else if (r_icache_unc_req.read())
            {
                r_vci_cmd_fsm    = CMD_INS_UNC;
                r_icache_unc_req = false;
            }
            // 7 - Data Write
            else if (r_wbuf.rok(&wbuf_min, &wbuf_max))
            {
                r_vci_cmd_fsm = CMD_DATA_WRITE;
                r_vci_cmd_cpt = wbuf_min;
                r_vci_cmd_min = wbuf_min;
                r_vci_cmd_max = wbuf_max;
            }
            // 8 - Data Store Conditionnal
            else if (dcache_sc_req and r_wbuf.miss(r_dcache_vci_paddr.read()))
            {
                r_vci_cmd_fsm        = CMD_DATA_SC;
                r_dcache_vci_sc_req  = false;
                r_vci_cmd_imiss_prio = true;
                r_vci_cmd_cpt        = 0;
            }
            // 9 - Compare And Swap
            else if (dcache_cas_req and r_wbuf.miss(r_dcache_vci_paddr.read()))
            {
                r_vci_cmd_fsm        = CMD_DATA_CAS;
                r_dcache_vci_cas_req = false;
                r_vci_cmd_imiss_prio = true;
                r_vci_cmd_cpt        = 0;
            }

#if DEBUG_CMD
            if (m_debug_cmd_fsm )
            {
                std::cout << "  <PROC " << name() << " CMD_IDLE>"
                    << " / dmiss_req = " << dcache_miss_req
                    << " / imiss_req = " << icache_miss_req
                    << std::endl;
            }
#endif
            break;
        }
        ////////////////////
        case CMD_DATA_WRITE:
        {
            if (p_vci.cmdack.read())
            {
                r_vci_cmd_cpt = r_vci_cmd_cpt + 1;
                if (r_vci_cmd_cpt == r_vci_cmd_max) // last flit sent
                {
                    r_vci_cmd_fsm = CMD_IDLE;
                    r_wbuf.sent();
                }
            }
            break;
        }
        /////////////////
        case CMD_DATA_SC:
        case CMD_DATA_CAS:
        {
            // The CAS and SC VCI commands contain two flits
            if (p_vci.cmdack.read())
            {
               r_vci_cmd_cpt = r_vci_cmd_cpt + 1;
               if (r_vci_cmd_cpt == 1) r_vci_cmd_fsm = CMD_IDLE ;
            }
            break;
        }
        //////////////////
        case CMD_INS_MISS:
        case CMD_INS_UNC:
        case CMD_DATA_MISS:
        case CMD_DATA_UNC_READ:
        case CMD_DATA_UNC_WRITE:
        case CMD_DATA_LL:
        {
            // all read VCI commands contain one single flit
            if (p_vci.cmdack.read()) {
                r_vci_cmd_fsm = CMD_IDLE;
            }
            break;
        }

    } // end  switch r_vci_cmd_fsm

    //////////////////////////////////////////////////////////////////////////
    // The VCI_RSP FSM controls the following ressources:
    // - r_vci_rsp_fsm:
    // - r_vci_rsp_fifo_icache (push)
    // - r_vci_rsp_fifo_dcache (push)
    // - r_vci_rsp_data_error (set)
    // - r_vci_rsp_ins_error (set)
    // - r_vci_rsp_cpt
    // - r_dcache_vci_sc_req (reset when SC response recieved)
    //
    // As the VCI_RSP and VCI_CMD are fully desynchronized to support several
    // simultaneous VCI transactions, this FSM uses the VCI RPKTID field
    // to identify the transactions.
    //
    // VCI vormat:
    // This component checks the response packet length and accepts only
    // single word packets for write response packets.
    //
    // Error handling:
    // This FSM analyzes the VCI error code and signals directly the Write Bus Error.
    // In case of Read Data Error, the VCI_RSP FSM sets the r_vci_rsp_data_error
    // flip_flop and the error is signaled by the DCACHE FSM.
    // In case of Instruction Error, the VCI_RSP FSM sets the r_vci_rsp_ins_error
    // flip_flop and the error is signaled by the ICACHE FSM.
    // In case of Cleanup Error, the simulation stops with an error message...
    //////////////////////////////////////////////////////////////////////////

    switch (r_vci_rsp_fsm.read())
    {
    //////////////
    case RSP_IDLE:
    {
        if (p_vci.rspval.read())
        {
            r_vci_rsp_cpt = 0;

            if ((p_vci.rpktid.read() & 0x7) ==  TYPE_DATA_UNC)
            {
                r_vci_rsp_fsm = RSP_DATA_UNC;
            }
            else if ((p_vci.rpktid.read() & 0x7) ==  TYPE_READ_DATA_MISS)
            {
                r_vci_rsp_fsm = RSP_DATA_MISS;
            }
            else if ((p_vci.rpktid.read() & 0x7) ==  TYPE_READ_INS_UNC)
            {
                r_vci_rsp_fsm = RSP_INS_UNC;
            }
            else if ((p_vci.rpktid.read() & 0x7) ==  TYPE_READ_INS_MISS)
            {
                r_vci_rsp_fsm = RSP_INS_MISS;
            }
            else if ((p_vci.rpktid.read() & 0x7) ==  TYPE_WRITE)
            {
                r_vci_rsp_fsm = RSP_DATA_WRITE;
            }
            else if ((p_vci.rpktid.read() & 0x7) ==  TYPE_CAS)
            {
                r_vci_rsp_fsm = RSP_DATA_UNC;
            }
            else if ((p_vci.rpktid.read() & 0x7) ==  TYPE_LL)
            {
                r_vci_rsp_fsm = RSP_DATA_LL;
            }
            else if ((p_vci.rpktid.read() & 0x7) == TYPE_SC)
            {
                r_vci_rsp_fsm = RSP_DATA_UNC;
            }
            else
            {
                assert(false and "Unexpected VCI response");
            }
        }
        break;
    }
        //////////////////
        case RSP_INS_MISS:
        {
            if (p_vci.rspval.read())
            {
                if ((p_vci.rerror.read() & 0x1) != 0)  // error reported
                {
                    r_vci_rsp_ins_error = true;
                    if (p_vci.reop.read()) r_vci_rsp_fsm = RSP_IDLE;
                }
                else                                        // no error reported
                {
                    if (r_vci_rsp_fifo_icache.wok())
                    {
                        if (r_vci_rsp_cpt.read() >= m_icache_words)
                        {
                            std::cout << "ERROR in VCI_CC_VCACHE " << name()
                                      << " VCI response packet too long "
                                      << " for instruction miss" << std::endl;
                            exit(0);
                        }
                        r_vci_rsp_cpt            = r_vci_rsp_cpt.read() + 1;
                        vci_rsp_fifo_icache_put  = true,
                        vci_rsp_fifo_icache_data = p_vci.rdata.read();
                        if (p_vci.reop.read())
                        {
                            if (r_vci_rsp_cpt.read() != (m_icache_words - 1))
                            {
                                std::cout << "ERROR in VCI_CC_VCACHE " << name()
                                          << " VCI response packet too short"
                                          << " for instruction miss" << std::endl;
                                exit(0);
                            }
                            r_vci_rsp_fsm = RSP_IDLE;
                        }
                    }
                }
            }
            break;
        }
        /////////////////
        case RSP_INS_UNC:
        {
            if (p_vci.rspval.read())
            {
                assert(p_vci.reop.read() and
                "illegal VCI response packet for uncachable instruction");

                if ((p_vci.rerror.read() & 0x1) != 0)  // error reported
                {
                    r_vci_rsp_ins_error = true;
                    r_vci_rsp_fsm = RSP_IDLE;
                }
                else                                         // no error reported
                {
                    if (r_vci_rsp_fifo_icache.wok())
                    {
                        vci_rsp_fifo_icache_put  = true;
                        vci_rsp_fifo_icache_data = p_vci.rdata.read();
                        r_vci_rsp_fsm = RSP_IDLE;
                    }
                }
            }
            break;
        }
        ///////////////////
        case RSP_DATA_MISS:
        {
            if (p_vci.rspval.read())
            {
                if ((p_vci.rerror.read() & 0x1) != 0)  // error reported
                {
                    r_vci_rsp_data_error = true;
                    if (p_vci.reop.read()) r_vci_rsp_fsm = RSP_IDLE;
                }
                else                                        // no error reported
                {
                    if (r_vci_rsp_fifo_dcache.wok())
                    {
                        assert((r_vci_rsp_cpt.read() < m_dcache_words) and
                        "The VCI response packet for data miss is too long");

                        r_vci_rsp_cpt            = r_vci_rsp_cpt.read() + 1;
                        vci_rsp_fifo_dcache_put  = true,
                        vci_rsp_fifo_dcache_data = p_vci.rdata.read();
                        if (p_vci.reop.read())
                        {
                            assert((r_vci_rsp_cpt.read() == m_dcache_words - 1) and
                            "The VCI response packet for data miss is too short");

                            r_vci_rsp_fsm = RSP_IDLE;
                        }
                    }
                }
            }
            break;
        }
        //////////////////
        case RSP_DATA_UNC:
        {
            if (p_vci.rspval.read())
            {
                assert(p_vci.reop.read() and
                "illegal VCI response packet for uncachable read data");

                if ((p_vci.rerror.read() & 0x1) != 0)  // error reported
                {
                    r_vci_rsp_data_error = true;
                    r_vci_rsp_fsm = RSP_IDLE;
                }
                else // no error reported
                {
                    if (r_vci_rsp_fifo_dcache.wok())
                    {
                        vci_rsp_fifo_dcache_put = true;
                        vci_rsp_fifo_dcache_data = p_vci.rdata.read();
                        r_vci_rsp_fsm = RSP_IDLE;
                    }
                }
            }
            break;
        }
        /////////////////
        case RSP_DATA_LL:
        {
            if (p_vci.rspval.read())
            {
                if ((p_vci.rerror.read() & 0x1) != 0)  // error reported
                {
                    r_vci_rsp_data_error = true;
                    r_vci_rsp_fsm = RSP_IDLE;
                    break;
                }
                if (r_vci_rsp_cpt.read() == 0) //first flit
                {
                    if (r_vci_rsp_fifo_dcache.wok())
                    {
                        assert(!p_vci.reop.read() &&
                            "illegal VCI response packet for LL");
                        vci_rsp_fifo_dcache_put  = true;
                        vci_rsp_fifo_dcache_data = p_vci.rdata.read();
                        r_vci_rsp_cpt            = r_vci_rsp_cpt.read() + 1;
                    }
                    break;
                }
                else // last flit
                {
                    if (r_vci_rsp_fifo_dcache.wok())
                    {
                        assert(p_vci.reop.read() &&
                            "illegal VCI response packet for LL");
                        vci_rsp_fifo_dcache_put  = true;
                        vci_rsp_fifo_dcache_data = p_vci.rdata.read();
                        r_vci_rsp_fsm            = RSP_IDLE;
                    }
                    break;
                }
            }
            break;
        }
        ////////////////////
        case RSP_DATA_WRITE:
        {
            if (p_vci.rspval.read())
            {
                assert(p_vci.reop.read() and
                "a VCI response packet must contain one flit for a write transaction");

                r_vci_rsp_fsm = RSP_IDLE;
                uint32_t wbuf_index = p_vci.rtrdid.read();
                r_wbuf.completed(wbuf_index);
                if ((p_vci.rerror.read() & 0x1) != 0) r_iss.setWriteBerr();
            }
            break;
        }
    } // end switch r_vci_rsp_fsm

    /////////////////////////////////////////////////////////////////////////////////////
    // The CC_SEND FSM is in charge of sending cleanups and the multicast
    // acknowledgements on the coherence network. It has two clients (DCACHE FSM
    // and ICACHE FSM) that are served with a round-robin priority.
    // The CC_SEND FSM resets the r_*cache_cc_send_req request flip-flops as
    // soon as the request has been sent.
    /////////////////////////////////////////////////////////////////////////////////////
    switch (r_cc_send_fsm.read())
    {
        ///////////////////////////
        case CC_SEND_IDLE:
        {
            ///////////////////////////////////////////////////////
            // handling round robin between icache and dcache :  //
            // we first check for the last client and listen for //
            // a request of the other, then update the client    //
            // r_cc_send_last_client : 0 dcache / 1 icache
            ///////////////////////////////////////////////////////
            bool update_last_client = r_cc_send_last_client.read();
            if (r_cc_send_last_client.read() == 0) // last client was dcache
            {
                if (r_icache_cc_send_req.read()) // request from icache
                    update_last_client = 1; // update last client to icache
            }
            else // last client was icache
            {
                if (r_dcache_cc_send_req.read()) // request from dcache
                    update_last_client = 0; // update last client to dcache
            }
            r_cc_send_last_client = update_last_client;

            // if there is an actual request
            if (r_dcache_cc_send_req.read() or r_icache_cc_send_req.read())
            {
                // the new client is dcache and has a cleanup request
                if ((update_last_client == 0) and
                          (r_dcache_cc_send_type.read() == CC_TYPE_CLEANUP))
                    r_cc_send_fsm = CC_SEND_CLEANUP_1;
                // the new client is dcache and has a multi acknowledgement request
                else if ((update_last_client == 0) and
                          (r_dcache_cc_send_type.read() == CC_TYPE_MULTI_ACK))
                    r_cc_send_fsm = CC_SEND_MULTI_ACK;
                // the new client is icache and has a cleanup request
                else if ((update_last_client == 1) and
                          (r_icache_cc_send_type.read() == CC_TYPE_CLEANUP))
                    r_cc_send_fsm = CC_SEND_CLEANUP_1;
                // the new client is icache and has a multi acknowledgement request
                else if ((update_last_client == 1) and
                        (r_icache_cc_send_type.read() == CC_TYPE_MULTI_ACK))
                    r_cc_send_fsm = CC_SEND_MULTI_ACK;
            }
            break;
        }
        ///////////////////////////
        case CC_SEND_CLEANUP_1:
        {
            // wait for the first flit to be consumed
            if (p_dspin_p2m.read.read())
                r_cc_send_fsm = CC_SEND_CLEANUP_2;

            break;
        }
        ///////////////////////////
        case CC_SEND_CLEANUP_2:
        {
            // wait for the second flit to be consumed
            if (p_dspin_p2m.read.read())
            {
                if (r_cc_send_last_client.read() == 0) // dcache active request
                    r_dcache_cc_send_req = false; // reset dcache request
                else // icache active request
                    r_icache_cc_send_req = false; // reset icache request

                // go back to idle state
                r_cc_send_fsm = CC_SEND_IDLE;
            }
            break;
        }
        ///////////////////////////
        case CC_SEND_MULTI_ACK:
        {
            // wait for the flit to be consumed
            if (p_dspin_p2m.read.read())
            {
                if (r_cc_send_last_client.read() == 0) // dcache active request
                    r_dcache_cc_send_req = false; // reset dcache request
                else // icache active request
                    r_icache_cc_send_req = false; // reset icache request
                // go back to idle state
                r_cc_send_fsm = CC_SEND_IDLE;
            }
            break;
        }
    } // end switch CC_SEND FSM

    ///////////////////////////////////////////////////////////////////////////////
    //  CC_RECEIVE  FSM
    // This FSM receive all coherence packets on a DSPIN40 port.
    // There is 5 packet types:
    // - CC_DATA_INVAL : DCACHE invalidate request
    // - CC_DATA_UPDT  : DCACHE update request (multi-words)
    // - CC_INST_INVAL : ICACHE invalidate request
    // - CC_INST_UPDT  : ICACHE update request (multi-words)
    // - CC_BROADCAST  : Broadcast invalidate request (both DCACHE & ICACHE)
    //////////////////////////////////////////////////////////////////////////////
    switch (r_cc_receive_fsm.read())
    {
        /////////////////////
        case CC_RECEIVE_IDLE:
        {
            // a coherence request has arrived
            if (p_dspin_m2p.write.read())
            {
                // initialize dspin received data
                uint64_t receive_data = p_dspin_m2p.data.read();
                // initialize coherence packet type
                uint64_t receive_type = DspinDhccpParam::dspin_get(receive_data,
                                            DspinDhccpParam::M2P_TYPE);
                // test for a broadcast
                if (DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::M2P_BC))
                {
                    r_cc_receive_fsm = CC_RECEIVE_BRDCAST_HEADER;
                }
                // test for a multi updt
                else if (receive_type == DspinDhccpParam::TYPE_MULTI_UPDT_DATA)
                {
                    r_cc_receive_fsm = CC_RECEIVE_DATA_UPDT_HEADER;
                }
                else if (receive_type == DspinDhccpParam::TYPE_MULTI_UPDT_INST)
                {
                    r_cc_receive_fsm = CC_RECEIVE_INS_UPDT_HEADER;
                }
                // test for a multi inval
                else if (receive_type == DspinDhccpParam::TYPE_MULTI_INVAL_DATA)
                {
                    r_cc_receive_fsm = CC_RECEIVE_DATA_INVAL_HEADER;
                }
                else
                {
                    r_cc_receive_fsm = CC_RECEIVE_INS_INVAL_HEADER;
                }
            }
            break;
        }
        ///////////////////////////////
        case CC_RECEIVE_BRDCAST_HEADER:
        {
            // no actual data in the HEADER, just skip to second flit
            r_cc_receive_fsm = CC_RECEIVE_BRDCAST_NLINE;
            break;
        }
        //////////////////////////////
        case CC_RECEIVE_BRDCAST_NLINE:
        {
            // initialize dspin received data
            uint64_t receive_data = p_dspin_m2p.data.read();
            // wait for both dcache and icache to take the request
            // TODO maybe we need to wait for both only to leave the state, but
            // not to actually post a request to an available cache => need a
            // flip_flop to check that ?
            if (not (r_cc_receive_icache_req.read()) and
                not (r_cc_receive_dcache_req.read()) and
                (p_dspin_m2p.write.read()))
            {
                // request dcache to handle the BROADCAST
                r_cc_receive_dcache_req = true;
                r_cc_receive_dcache_nline = DspinDhccpParam::dspin_get(receive_data,
                                             DspinDhccpParam::BROADCAST_NLINE);
                r_cc_receive_dcache_type = CC_TYPE_INVAL;
                // request icache to handle the BROADCAST
                r_cc_receive_icache_req = true;
                r_cc_receive_icache_nline = DspinDhccpParam::dspin_get(receive_data,
                                             DspinDhccpParam::BROADCAST_NLINE);
                r_cc_receive_icache_type = CC_TYPE_INVAL;
                // get back to idle state
                r_cc_receive_fsm = CC_RECEIVE_IDLE;
                break;
            }
            // keep waiting for the caches to accept the request
            break;
        }
        /////////////////////////////
        case CC_RECEIVE_DATA_INVAL_HEADER:
        {
            // sample updt tab index in the HEADER, then skip to second flit
            r_cc_receive_fsm = CC_RECEIVE_DATA_INVAL_NLINE;
            break;
        }
        /////////////////////////////
        case CC_RECEIVE_INS_INVAL_HEADER:
        {
            // sample updt tab index in the HEADER, then skip to second flit
            r_cc_receive_fsm = CC_RECEIVE_INS_INVAL_NLINE;
            break;
        }
        ////////////////////////////
        case CC_RECEIVE_DATA_INVAL_NLINE:
        {
            // sample nline in the second flit
            uint64_t receive_data = p_dspin_m2p.data.read();
            // for data INVAL, wait for dcache to take the request
            if (p_dspin_m2p.write.read()           and
                not r_cc_receive_dcache_req.read())
            {
                // request dcache to handle the INVAL
                r_cc_receive_dcache_req = true;
                r_cc_receive_dcache_nline = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_INVAL_NLINE);
                r_cc_receive_dcache_type = CC_TYPE_INVAL;
                // get back to idle state
                r_cc_receive_fsm = CC_RECEIVE_IDLE;
                break;
            }
            break;
        }
        //////////////////////////////
        case CC_RECEIVE_INS_INVAL_NLINE:
        {
            // sample nline in the second flit
            uint64_t receive_data = p_dspin_m2p.data.read();
            // for ins INVAL, wait for icache to take the request
            if (p_dspin_m2p.write.read()           and
                not r_cc_receive_icache_req.read())
            {
                // request icache to handle the INVAL
                r_cc_receive_icache_req = true;
                r_cc_receive_icache_nline = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_INVAL_NLINE);
                r_cc_receive_icache_type = CC_TYPE_INVAL;
                // get back to idle state
                r_cc_receive_fsm = CC_RECEIVE_IDLE;
                break;
            }
            break;
        }
        ////////////////////////////
        case CC_RECEIVE_DATA_UPDT_HEADER:
        {
            // sample updt tab index in the HEADER, than skip to second flit
            uint64_t receive_data = p_dspin_m2p.data.read();
            // for data INVAL, wait for dcache to take the request and fifo to
            // be empty
            if (not r_cc_receive_dcache_req.read())
            {
                r_cc_receive_dcache_updt_tab_idx = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_UPDT_INDEX);
                r_cc_receive_fsm = CC_RECEIVE_DATA_UPDT_NLINE;
                break;
            }
            break;
        }
        ////////////////////////////
        case CC_RECEIVE_INS_UPDT_HEADER:
        {
            // sample updt tab index in the HEADER, than skip to second flit
            uint64_t receive_data = p_dspin_m2p.data.read();
            // for ins INVAL, wait for icache to take the request and fifo to be
            // empty
            if (not r_cc_receive_icache_req.read())
            {
                r_cc_receive_icache_updt_tab_idx = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_UPDT_INDEX);
                r_cc_receive_fsm = CC_RECEIVE_INS_UPDT_NLINE;
                break;
            }
            // keep waiting for the correct cache to accept the request
            break;
        }
        ///////////////////////////
        case CC_RECEIVE_DATA_UPDT_NLINE:
        {
            // sample nline and word index in the second flit
            uint64_t receive_data = p_dspin_m2p.data.read();
            // for data INVAL, wait for dcache to take the request and fifo to
            // be empty
            if (r_cc_receive_updt_fifo_be.empty() and
                 p_dspin_m2p.write.read())
            {
                r_cc_receive_dcache_req   = true;
                r_cc_receive_dcache_nline = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_NLINE);
                r_cc_receive_word_idx     = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_WORD_INDEX);
                r_cc_receive_dcache_type  = CC_TYPE_UPDT;
                // get back to idle state
                r_cc_receive_fsm = CC_RECEIVE_DATA_UPDT_DATA;
                break;
            }
            break;
        }
        ////////////////////////////
        case CC_RECEIVE_INS_UPDT_NLINE:
        {
            // sample nline and word index in the second flit
            uint64_t receive_data = p_dspin_m2p.data.read();
            // for ins INVAL, wait for icache to take the request and fifo to be
            // empty
            if (r_cc_receive_updt_fifo_be.empty() and
                 p_dspin_m2p.write.read())
            {
                r_cc_receive_icache_req   = true;
                r_cc_receive_icache_nline = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_NLINE);
                r_cc_receive_word_idx     = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_WORD_INDEX);
                r_cc_receive_icache_type  = CC_TYPE_UPDT;
                // get back to idle state
                r_cc_receive_fsm = CC_RECEIVE_INS_UPDT_DATA;
                break;
            }
            break;
        }
        //////////////////////////
        case CC_RECEIVE_DATA_UPDT_DATA:
        {
            // wait for the fifo
            if (r_cc_receive_updt_fifo_be.wok() and (p_dspin_m2p.write.read()))
            {
                uint64_t receive_data = p_dspin_m2p.data.read();
                bool     receive_eop  = p_dspin_m2p.eop.read();
                cc_receive_updt_fifo_be   = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_BE);
                cc_receive_updt_fifo_data = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_DATA);
                cc_receive_updt_fifo_eop  = receive_eop;
                cc_receive_updt_fifo_put  = true;
                if (receive_eop ) r_cc_receive_fsm = CC_RECEIVE_IDLE;
            }
            break;
        }
        //////////////////////////
        case CC_RECEIVE_INS_UPDT_DATA:
        {
            // wait for the fifo
            if (r_cc_receive_updt_fifo_be.wok() and (p_dspin_m2p.write.read()))
            {
                uint64_t receive_data = p_dspin_m2p.data.read();
                bool     receive_eop  = p_dspin_m2p.eop.read();
                cc_receive_updt_fifo_be   = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_BE);
                cc_receive_updt_fifo_data = DspinDhccpParam::dspin_get(receive_data,DspinDhccpParam::MULTI_UPDT_DATA);
                cc_receive_updt_fifo_eop  = receive_eop;
                cc_receive_updt_fifo_put  = true;
                if (receive_eop ) r_cc_receive_fsm = CC_RECEIVE_IDLE;
            }
            break;
        }

    } // end switch CC_RECEIVE FSM

    ///////////////// DSPIN CLACK interface ///////////////

    uint64_t clack_type = DspinDhccpParam::dspin_get(r_dspin_clack_flit.read(),
                                                     DspinDhccpParam::CLACK_TYPE);

    size_t clack_way = DspinDhccpParam::dspin_get(r_dspin_clack_flit.read(),
                                                   DspinDhccpParam::CLACK_WAY);

    size_t clack_set = DspinDhccpParam::dspin_get(r_dspin_clack_flit.read(),
                                                   DspinDhccpParam::CLACK_SET);

    bool dspin_clack_get = false;
    bool dcache_clack_request = (clack_type == DspinDhccpParam::TYPE_CLACK_DATA);
    bool icache_clack_request = (clack_type == DspinDhccpParam::TYPE_CLACK_INST);

    if (r_dspin_clack_req.read())
    {
        // CLACK DATA: Send request to DCACHE FSM
        if (dcache_clack_request and not r_dcache_clack_req.read())
        {
            r_dcache_clack_req = true;
            r_dcache_clack_way = clack_way & ((1ULL << (uint32_log2(m_dcache_ways))) - 1);
            r_dcache_clack_set = clack_set & ((1ULL << (uint32_log2(m_dcache_sets))) - 1);
            dspin_clack_get    = true;
        }

        // CLACK INST: Send request to ICACHE FSM
        else if (icache_clack_request and not r_icache_clack_req.read())
        {
            r_icache_clack_req = true;
            r_icache_clack_way = clack_way & ((1ULL<<(uint32_log2(m_dcache_ways)))-1);
            r_icache_clack_set = clack_set & ((1ULL<<(uint32_log2(m_icache_sets)))-1);
            dspin_clack_get    = true;
        }
    }
    else
    {
        dspin_clack_get = true;
    }

    if (dspin_clack_get)
    {
        r_dspin_clack_req  = p_dspin_clack.write.read();
        r_dspin_clack_flit = p_dspin_clack.data.read();
    }

    ///////////////// Response FIFOs update  //////////////////////
    r_vci_rsp_fifo_icache.update(vci_rsp_fifo_icache_get,
                                 vci_rsp_fifo_icache_put,
                                 vci_rsp_fifo_icache_data);

    r_vci_rsp_fifo_dcache.update(vci_rsp_fifo_dcache_get,
                                 vci_rsp_fifo_dcache_put,
                                 vci_rsp_fifo_dcache_data);

    ///////////////// updt FIFO update  //////////////////////
    //TODO check this
    r_cc_receive_updt_fifo_be.update(cc_receive_updt_fifo_get,
                                 cc_receive_updt_fifo_put,
                                 cc_receive_updt_fifo_be);
    r_cc_receive_updt_fifo_data.update(cc_receive_updt_fifo_get,
                                 cc_receive_updt_fifo_put,
                                 cc_receive_updt_fifo_data);
    r_cc_receive_updt_fifo_eop.update(cc_receive_updt_fifo_get,
                                 cc_receive_updt_fifo_put,
                                 cc_receive_updt_fifo_eop);

} // end transition()

///////////////////////
tmpl(void)::genMoore()
///////////////////////
{

    // VCI initiator command on the direct network
    // it depends on the CMD FSM state

    bool is_sc_or_cas  = (r_vci_cmd_fsm.read() == CMD_DATA_CAS) or
                         (r_vci_cmd_fsm.read() == CMD_DATA_SC);

    p_vci.pktid  = 0;
    p_vci.srcid  = m_srcid;
    p_vci.cons   = is_sc_or_cas;
    p_vci.contig = not is_sc_or_cas;
    p_vci.wrap   = false;
    p_vci.clen   = 0;
    p_vci.cfixed = false;

    if (m_monitor_ok) {
        if (p_vci.cmdack.read() == true and p_vci.cmdval == true) {
            if (((p_vci.address.read()) >= m_monitor_base) and
                ((p_vci.address.read()) < m_monitor_base + m_monitor_length)) {
                std::cout << "CC_VCACHE Monitor " << name() << std::hex
                          << " Access type = " << vci_cmd_type_str[p_vci.cmd.read()]
                          << " Pktid type = " << vci_pktid_type_str[p_vci.pktid.read()]
                          << " : address = " << p_vci.address.read()
                          << " / be = " << p_vci.be.read();
                if (p_vci.cmd.read() == vci_param::CMD_WRITE ) {
                    std::cout << " / data = " << p_vci.wdata.read();
                }
                std::cout << std::dec << std::endl;
            }
        }
    }

    switch (r_vci_cmd_fsm.read()) {

    case CMD_IDLE:
        p_vci.cmdval  = false;
        p_vci.address = 0;
        p_vci.wdata   = 0;
        p_vci.be      = 0;
        p_vci.trdid   = 0;
        p_vci.pktid   = 0;
        p_vci.plen    = 0;
        p_vci.cmd     = vci_param::CMD_NOP;
        p_vci.eop     = false;
        break;

    case CMD_INS_MISS:
        p_vci.cmdval  = true;
        p_vci.address = r_icache_vci_paddr.read() & m_icache_yzmask;
        p_vci.wdata   = 0;
        p_vci.be      = 0xF;
        p_vci.trdid   = 0;
        p_vci.pktid   = TYPE_READ_INS_MISS;
        p_vci.plen    = m_icache_words << 2;
        p_vci.cmd     = vci_param::CMD_READ;
        p_vci.eop     = true;
        break;

    case CMD_INS_UNC:
        p_vci.cmdval  = true;
        p_vci.address = r_icache_vci_paddr.read() & ~0x3;
        p_vci.wdata   = 0;
        p_vci.be      = 0xF;
        p_vci.trdid   = 0;
        p_vci.pktid   = TYPE_READ_INS_UNC;
        p_vci.plen    = 4;
        p_vci.cmd     = vci_param::CMD_READ;
        p_vci.eop     = true;
        break;

    case CMD_DATA_MISS:
        p_vci.cmdval  = true;
        p_vci.address = r_dcache_vci_paddr.read() & m_dcache_yzmask;
        p_vci.wdata   = 0;
        p_vci.be      = 0xF;
        p_vci.trdid   = 0;
        p_vci.pktid   = TYPE_READ_DATA_MISS;
        p_vci.plen    = m_dcache_words << 2;
        p_vci.cmd     = vci_param::CMD_READ;
        p_vci.eop     = true;
        break;

    case CMD_DATA_UNC_READ:
        p_vci.cmdval  = true;
        p_vci.address = r_dcache_vci_paddr.read() & ~0x3;
        p_vci.wdata   = 0;
        p_vci.be      = r_dcache_vci_unc_be.read();
        p_vci.trdid   = 0;
        p_vci.pktid   = TYPE_DATA_UNC;
        p_vci.plen    = 4;
        p_vci.cmd     = vci_param::CMD_READ;
        p_vci.eop     = true;
        break;

    case CMD_DATA_UNC_WRITE:
        p_vci.cmdval  = true;
        p_vci.address = r_dcache_vci_paddr.read() & ~0x3;
        p_vci.wdata   = r_dcache_vci_wdata.read();
        p_vci.be      = r_dcache_vci_unc_be.read();
        p_vci.trdid   = 0;
        p_vci.pktid   = TYPE_DATA_UNC;
        p_vci.plen    = 4;
        p_vci.cmd     = vci_param::CMD_WRITE;
        p_vci.eop     = true;
        break;

    case CMD_DATA_WRITE:
        p_vci.cmdval  = true;
        p_vci.address = r_wbuf.getAddress(r_vci_cmd_cpt.read()) & ~0x3;
        p_vci.wdata   = r_wbuf.getData(r_vci_cmd_cpt.read());
        p_vci.be      = r_wbuf.getBe(r_vci_cmd_cpt.read());
        p_vci.trdid   = r_wbuf.getIndex();
        p_vci.pktid   = TYPE_WRITE;
        p_vci.plen    = (r_vci_cmd_max.read() - r_vci_cmd_min.read() + 1) << 2;
        p_vci.cmd     = vci_param::CMD_WRITE;
        p_vci.eop     = (r_vci_cmd_cpt.read() == r_vci_cmd_max.read());
        break;

    case CMD_DATA_LL:
        p_vci.cmdval  = true;
        p_vci.address = r_dcache_vci_paddr.read() & ~0x3;
        p_vci.wdata   = 0;
        p_vci.be      = 0xF;
        p_vci.trdid   = 0;
        p_vci.pktid   = TYPE_LL;
        p_vci.plen    = 8;
        p_vci.cmd     = vci_param::CMD_LOCKED_READ;
        p_vci.eop     = true;
        break;

    case CMD_DATA_SC:
        p_vci.cmdval  = true;
        p_vci.address = r_dcache_vci_paddr.read() & ~0x3;
        if (r_vci_cmd_cpt.read() == 0) p_vci.wdata = r_dcache_llsc_key.read();
        else                           p_vci.wdata = r_dcache_vci_sc_data.read();
        p_vci.be      = 0xF;
        p_vci.trdid   = 0;
        p_vci.pktid   = TYPE_SC;
        p_vci.plen    = 8;
        p_vci.cmd     = vci_param::CMD_NOP;
        p_vci.eop     = (r_vci_cmd_cpt.read() == 1);
        break;

    case CMD_DATA_CAS:
        p_vci.cmdval  = true;
        p_vci.address = r_dcache_vci_paddr.read() & ~0x3;
        if (r_vci_cmd_cpt.read() == 0) p_vci.wdata = r_dcache_vci_cas_old.read();
        else                           p_vci.wdata = r_dcache_vci_cas_new.read();
        p_vci.be      = 0xF;
        p_vci.trdid   = 0;
        p_vci.pktid   = TYPE_CAS;
        p_vci.plen    = 8;
        p_vci.cmd     = vci_param::CMD_NOP;
        p_vci.eop     = (r_vci_cmd_cpt.read() == 1);
        break;
    } // end switch r_vci_cmd_fsm

    // VCI initiator response on the direct network
    // it depends on the VCI_RSP FSM

    switch (r_vci_rsp_fsm.read())
    {
        case RSP_DATA_WRITE : p_vci.rspack = true; break;
        case RSP_INS_MISS   : p_vci.rspack = r_vci_rsp_fifo_icache.wok(); break;
        case RSP_INS_UNC    : p_vci.rspack = r_vci_rsp_fifo_icache.wok(); break;
        case RSP_DATA_MISS  : p_vci.rspack = r_vci_rsp_fifo_dcache.wok(); break;
        case RSP_DATA_UNC   : p_vci.rspack = r_vci_rsp_fifo_dcache.wok(); break;
        case RSP_DATA_LL    : p_vci.rspack = r_vci_rsp_fifo_dcache.wok(); break;
        case RSP_IDLE       : p_vci.rspack = false; break;
    } // end switch r_vci_rsp_fsm


    // Send coherence packets on DSPIN P2M
    // it depends on the CC_SEND FSM

    uint64_t dspin_send_data = 0;
    switch (r_cc_send_fsm.read())
    {
        //////////////////
        case CC_SEND_IDLE:
        {
            p_dspin_p2m.write = false;
            break;
        }
        ///////////////////////
        case CC_SEND_CLEANUP_1:
        {
            // initialize dspin send data
            DspinDhccpParam::dspin_set(dspin_send_data,
                                       m_cc_global_id,
                                       DspinDhccpParam::CLEANUP_SRCID);
            DspinDhccpParam::dspin_set(dspin_send_data,
                                       0,
                                       DspinDhccpParam::P2M_BC);

            if (r_cc_send_last_client.read() == 0) // dcache active request
            {
                uint64_t dest = (uint64_t) r_dcache_cc_send_nline.read()
                                >> (m_nline_width - m_x_width - m_y_width)
                                << (DspinDhccpParam::GLOBALID_WIDTH - m_x_width - m_y_width);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           dest,
                                           DspinDhccpParam::CLEANUP_DEST);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           (r_dcache_cc_send_nline.read() & 0x300000000ULL)>>32,
                                           DspinDhccpParam::CLEANUP_NLINE_MSB);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           r_dcache_cc_send_way.read(),
                                           DspinDhccpParam::CLEANUP_WAY_INDEX);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           DspinDhccpParam::TYPE_CLEANUP_DATA,
                                           DspinDhccpParam::P2M_TYPE);
            }
            else                                // icache active request
            {
                uint64_t dest = (uint64_t) r_icache_cc_send_nline.read()
                                >> (m_nline_width - m_x_width - m_y_width)
                                << (DspinDhccpParam::GLOBALID_WIDTH - m_x_width - m_y_width);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           dest,
                                           DspinDhccpParam::CLEANUP_DEST);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           (r_icache_cc_send_nline.read() & 0x300000000ULL) >> 32,
                                           DspinDhccpParam::CLEANUP_NLINE_MSB);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           r_icache_cc_send_way.read(),
                                           DspinDhccpParam::CLEANUP_WAY_INDEX);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           DspinDhccpParam::TYPE_CLEANUP_INST,
                                           DspinDhccpParam::P2M_TYPE);
            }
            // send flit
            p_dspin_p2m.data  = dspin_send_data;
            p_dspin_p2m.write = true;
            p_dspin_p2m.eop   = false;
            break;
        }
        ///////////////////////
        case CC_SEND_CLEANUP_2:
        {
            // initialize dspin send data

            if (r_cc_send_last_client.read() == 0) // dcache active request
            {
                DspinDhccpParam::dspin_set(dspin_send_data,
                                           r_dcache_cc_send_nline.read() & 0xFFFFFFFFULL,
                                           DspinDhccpParam::CLEANUP_NLINE_LSB);
            }
            else                                  // icache active request
            {
                DspinDhccpParam::dspin_set(dspin_send_data,
                                           r_icache_cc_send_nline.read() & 0xFFFFFFFFULL,
                                           DspinDhccpParam::CLEANUP_NLINE_LSB);
            }
            // send flit
            p_dspin_p2m.data  = dspin_send_data;
            p_dspin_p2m.write = true;
            p_dspin_p2m.eop   = true;
            break;
        }
        ///////////////////////
        case CC_SEND_MULTI_ACK:
        {
            // initialize dspin send data
            DspinDhccpParam::dspin_set(dspin_send_data,
                                       0,
                                       DspinDhccpParam::P2M_BC);
            DspinDhccpParam::dspin_set(dspin_send_data,
                                       DspinDhccpParam::TYPE_MULTI_ACK,
                                       DspinDhccpParam::P2M_TYPE);

            if (r_cc_send_last_client.read() == 0) // dcache active request
            {
                uint64_t dest = (uint64_t) r_dcache_cc_send_nline.read()
                                >> (m_nline_width - m_x_width - m_y_width)
                                << (DspinDhccpParam::GLOBALID_WIDTH - m_x_width - m_y_width);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           dest,
                                           DspinDhccpParam::MULTI_ACK_DEST);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           r_dcache_cc_send_updt_tab_idx.read(),
                                           DspinDhccpParam::MULTI_ACK_UPDT_INDEX);
            }
            else                                    // icache active request
            {
                uint64_t dest = (uint64_t) r_icache_cc_send_nline.read()
                                >> (m_nline_width - m_x_width - m_y_width)
                                << (DspinDhccpParam::GLOBALID_WIDTH - m_x_width - m_y_width);


                DspinDhccpParam::dspin_set(dspin_send_data,
                                           dest,
                                           DspinDhccpParam::MULTI_ACK_DEST);

                DspinDhccpParam::dspin_set(dspin_send_data,
                                           r_icache_cc_send_updt_tab_idx.read(),
                                           DspinDhccpParam::MULTI_ACK_UPDT_INDEX);
            }
            // send flit
            p_dspin_p2m.data  = dspin_send_data;
            p_dspin_p2m.write = true;
            p_dspin_p2m.eop   = true;

            break;
        }
    } // end switch CC_SEND FSM

    // Receive coherence packets
    // It depends on the CC_RECEIVE FSM
    switch (r_cc_receive_fsm.read())
    {
        /////////////////////
        case CC_RECEIVE_IDLE:
        {
            p_dspin_m2p.read = false;
            break;
        }
        ///////////////////////////////
        case CC_RECEIVE_BRDCAST_HEADER:
        {
            p_dspin_m2p.read = true;
            break;
        }
        //////////////////////////////
        case CC_RECEIVE_BRDCAST_NLINE:
        {
            // TODO maybe we need to wait for both only to leave the state, but
            // not to actually post a request to an available cache => need a
            // flip_flop to check that ?
            if (not (r_cc_receive_icache_req.read()) and not (r_cc_receive_dcache_req.read()))
                p_dspin_m2p.read = true;
            else
                p_dspin_m2p.read = false;
            break;
        }
        /////////////////////////////
        case CC_RECEIVE_DATA_INVAL_HEADER:
        case CC_RECEIVE_INS_INVAL_HEADER:
        {
            p_dspin_m2p.read = true;
            break;
        }
        ////////////////////////////
        case CC_RECEIVE_DATA_INVAL_NLINE:
        {
            p_dspin_m2p.read = not r_cc_receive_dcache_req.read();
            break;
        }
        case CC_RECEIVE_INS_INVAL_NLINE:
        {
            p_dspin_m2p.read = not r_cc_receive_icache_req.read();
            break;
        }
        ///////////////////////////
        case CC_RECEIVE_DATA_UPDT_HEADER:
        {
            if (not r_cc_receive_dcache_req.read())
                p_dspin_m2p.read = true;
            else
                p_dspin_m2p.read = false;
            break;
        }
        ////////////////////////////
        case CC_RECEIVE_INS_UPDT_HEADER:
        {
            if (not r_cc_receive_icache_req.read())
                p_dspin_m2p.read = true;
            else
                p_dspin_m2p.read = false;
            break;
        }
        ///////////////////////////
        case CC_RECEIVE_DATA_UPDT_NLINE:
        case CC_RECEIVE_INS_UPDT_NLINE:
        {
            if (r_cc_receive_updt_fifo_be.empty())
                p_dspin_m2p.read = true;
            else
                p_dspin_m2p.read = false;
            break;
        }
        ///////////////////////////
        case CC_RECEIVE_DATA_UPDT_DATA:
        case CC_RECEIVE_INS_UPDT_DATA:
        {
            if (r_cc_receive_updt_fifo_be.wok())
                p_dspin_m2p.read = true;
            else
                p_dspin_m2p.read = false;
            break;
        }
    } // end switch CC_RECEIVE FSM


    int clack_type = DspinDhccpParam::dspin_get(r_dspin_clack_flit.read(),
                                                DspinDhccpParam::CLACK_TYPE);

    bool dspin_clack_get = false;
    bool dcache_clack_request = (clack_type == DspinDhccpParam::TYPE_CLACK_DATA);
    bool icache_clack_request = (clack_type == DspinDhccpParam::TYPE_CLACK_INST);

    if (r_dspin_clack_req.read())
    {
        // CLACK DATA: wait if pending request to DCACHE FSM
        if (dcache_clack_request and not r_dcache_clack_req.read())
        {
            dspin_clack_get = true;
        }

        // CLACK INST: wait if pending request to ICACHE FSM
        else if (icache_clack_request and not r_icache_clack_req.read())
        {
            dspin_clack_get = true;
        }
    }
    else
    {
        dspin_clack_get = true;
    }

    p_dspin_clack.read = dspin_clack_get;
} // end genMoore

tmpl(void)::start_monitor(paddr_t base, paddr_t length)
// This version of monitor print both Read and Write request
{
    m_monitor_ok     = true;
    m_monitor_base   = base;
    m_monitor_length = length;
}

tmpl(void)::stop_monitor()
{
    m_monitor_ok = false;
}

}}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
