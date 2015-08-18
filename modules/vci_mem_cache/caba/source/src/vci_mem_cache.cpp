/* -*- c++ -*-
 *
 * File       : vci_mem_cache.cpp
 * Date       : 30/10/2008
 * Copyright  : UPMC / LIP6
 * Authors    : Alain Greiner / Eric Guthmuller
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


#include "../include/vci_mem_cache.h"
#include "mem_cache.h"

//////   debug services   /////////////////////////////////////////////////////////////
// All debug messages are conditionned by two variables:
// - compile time   : DEBUG_MEMC_*** : defined below
// - execution time : m_debug  = (m_debug_ok) and (m_cpt_cycle > m_debug_start_cycle)
///////////////////////////////////////////////////////////////////////////////////////

#define DEBUG_MEMC_GLOBAL    0 // synthetic trace of all FSMs
#define DEBUG_MEMC_CONFIG    1 // detailed trace of CONFIG FSM
#define DEBUG_MEMC_READ      1 // detailed trace of READ FSM
#define DEBUG_MEMC_WRITE     1 // detailed trace of WRITE FSM
#define DEBUG_MEMC_CAS       1 // detailed trace of CAS FSM
#define DEBUG_MEMC_IXR_CMD   1 // detailed trace of IXR_CMD FSM
#define DEBUG_MEMC_IXR_RSP   1 // detailed trace of IXR_RSP FSM
#define DEBUG_MEMC_XRAM_RSP  1 // detailed trace of XRAM_RSP FSM
#define DEBUG_MEMC_CC_SEND   1 // detailed trace of CC_SEND FSM
#define DEBUG_MEMC_MULTI_ACK 1 // detailed trace of MULTI_ACK FSM
#define DEBUG_MEMC_TGT_CMD   1 // detailed trace of TGT_CMD FSM
#define DEBUG_MEMC_TGT_RSP   1 // detailed trace of TGT_RSP FSM
#define DEBUG_MEMC_CLEANUP   1 // detailed trace of CLEANUP FSM

#define RANDOMIZE_CAS        1

namespace soclib { namespace caba {

    const char *tgt_cmd_fsm_str[] =
    {
        "TGT_CMD_IDLE",
        "TGT_CMD_READ",
        "TGT_CMD_WRITE",
        "TGT_CMD_CAS",
        "TGT_CMD_CONFIG",
        "TGT_CMD_ERROR"
    };
    const char *tgt_rsp_fsm_str[] =
    {
        "TGT_RSP_READ_IDLE",
        "TGT_RSP_WRITE_IDLE",
        "TGT_RSP_CAS_IDLE",
        "TGT_RSP_XRAM_IDLE",
        "TGT_RSP_MULTI_ACK_IDLE",
        "TGT_RSP_CLEANUP_IDLE",
        "TGT_RSP_TGT_CMD_IDLE",
        "TGT_RSP_CONFIG_IDLE",
        "TGT_RSP_READ",
        "TGT_RSP_WRITE",
        "TGT_RSP_CAS",
        "TGT_RSP_XRAM",
        "TGT_RSP_MULTI_ACK",
        "TGT_RSP_CLEANUP",
        "TGT_RSP_TGT_CMD",
        "TGT_RSP_CONFIG"
    };
    const char *cc_receive_fsm_str[] =
    {
        "CC_RECEIVE_IDLE",
        "CC_RECEIVE_CLEANUP",
        "CC_RECEIVE_CLEANUP_EOP",
        "CC_RECEIVE_MULTI_ACK"
    };
    const char *cc_send_fsm_str[] =
    {
        "CC_SEND_XRAM_RSP_IDLE",
        "CC_SEND_WRITE_IDLE",
        "CC_SEND_CAS_IDLE",
        "CC_SEND_CONFIG_IDLE",
        "CC_SEND_XRAM_RSP_BRDCAST_HEADER",
        "CC_SEND_XRAM_RSP_BRDCAST_NLINE",
        "CC_SEND_XRAM_RSP_INVAL_HEADER",
        "CC_SEND_XRAM_RSP_INVAL_NLINE",
        "CC_SEND_WRITE_BRDCAST_HEADER",
        "CC_SEND_WRITE_BRDCAST_NLINE",
        "CC_SEND_WRITE_UPDT_HEADER",
        "CC_SEND_WRITE_UPDT_NLINE",
        "CC_SEND_WRITE_UPDT_DATA",
        "CC_SEND_CAS_BRDCAST_HEADER",
        "CC_SEND_CAS_BRDCAST_NLINE",
        "CC_SEND_CAS_UPDT_HEADER",
        "CC_SEND_CAS_UPDT_NLINE",
        "CC_SEND_CAS_UPDT_DATA",
        "CC_SEND_CAS_UPDT_DATA_HIGH",
        "CC_SEND_CONFIG_INVAL_HEADER",
        "CC_SEND_CONFIG_INVAL_NLINE",
        "CC_SEND_CONFIG_BRDCAST_HEADER",
        "CC_SEND_CONFIG_BRDCAST_NLINE"
    };
    const char *multi_ack_fsm_str[] =
    {
        "MULTI_ACK_IDLE",
        "MULTI_ACK_UPT_LOCK",
        "MULTI_ACK_UPT_CLEAR",
        "MULTI_ACK_WRITE_RSP"
    };
    const char *config_fsm_str[] =
    {
        "CONFIG_IDLE",
        "CONFIG_LOOP",
        "CONFIG_WAIT",
        "CONFIG_RSP",
        "CONFIG_DIR_REQ",
        "CONFIG_DIR_ACCESS",
        "CONFIG_IVT_LOCK",
        "CONFIG_BC_SEND",
        "CONFIG_INVAL_SEND",
        "CONFIG_HEAP_REQ",
        "CONFIG_HEAP_SCAN",
        "CONFIG_HEAP_LAST",
        "CONFIG_TRT_LOCK",
        "CONFIG_TRT_SET",
        "CONFIG_PUT_REQ"
    };
    const char *read_fsm_str[] =
    {
        "READ_IDLE",
        "READ_DIR_REQ",
        "READ_DIR_LOCK",
        "READ_DIR_HIT",
        "READ_HEAP_REQ",
        "READ_HEAP_LOCK",
        "READ_HEAP_WRITE",
        "READ_HEAP_ERASE",
        "READ_HEAP_LAST",
        "READ_RSP",
        "READ_TRT_LOCK",
        "READ_TRT_SET",
        "READ_TRT_REQ"
    };
    const char *write_fsm_str[] =
    {
        "WRITE_IDLE",
        "WRITE_NEXT",
        "WRITE_DIR_REQ",
        "WRITE_DIR_LOCK",
        "WRITE_DIR_HIT",
        "WRITE_UPT_LOCK",
        "WRITE_UPT_HEAP_LOCK",
        "WRITE_UPT_REQ",
        "WRITE_UPT_NEXT",
        "WRITE_UPT_DEC",
        "WRITE_RSP",
        "WRITE_MISS_TRT_LOCK",
        "WRITE_MISS_TRT_DATA",
        "WRITE_MISS_TRT_SET",
        "WRITE_MISS_XRAM_REQ",
        "WRITE_BC_DIR_READ",
        "WRITE_BC_TRT_LOCK",
        "WRITE_BC_IVT_LOCK",
        "WRITE_BC_DIR_INVAL",
        "WRITE_BC_CC_SEND",
        "WRITE_BC_XRAM_REQ",
        "WRITE_WAIT"
    };
    const char *ixr_rsp_fsm_str[] =
    {
        "IXR_RSP_IDLE",
        "IXR_RSP_TRT_ERASE",
        "IXR_RSP_TRT_READ"
    };
    const char *xram_rsp_fsm_str[] =
    {
        "XRAM_RSP_IDLE",
        "XRAM_RSP_TRT_COPY",
        "XRAM_RSP_TRT_DIRTY",
        "XRAM_RSP_DIR_LOCK",
        "XRAM_RSP_DIR_UPDT",
        "XRAM_RSP_DIR_RSP",
        "XRAM_RSP_IVT_LOCK",
        "XRAM_RSP_INVAL_WAIT",
        "XRAM_RSP_INVAL",
        "XRAM_RSP_WRITE_DIRTY",
        "XRAM_RSP_HEAP_REQ",
        "XRAM_RSP_HEAP_ERASE",
        "XRAM_RSP_HEAP_LAST",
        "XRAM_RSP_ERROR_ERASE",
        "XRAM_RSP_ERROR_RSP"
    };
    const char *ixr_cmd_fsm_str[] =
    {
        "IXR_CMD_READ_IDLE",
        "IXR_CMD_WRITE_IDLE",
        "IXR_CMD_CAS_IDLE",
        "IXR_CMD_XRAM_IDLE",
        "IXR_CMD_CONFIG_IDLE",
        "IXR_CMD_READ_TRT",
        "IXR_CMD_WRITE_TRT",
        "IXR_CMD_CAS_TRT",
        "IXR_CMD_XRAM_TRT",
        "IXR_CMD_CONFIG_TRT",
        "IXR_CMD_READ_SEND",
        "IXR_CMD_WRITE_SEND",
        "IXR_CMD_CAS_SEND",
        "IXR_CMD_XRAM_SEND",
        "IXR_CMD_CONFIG_SEND"
    };
    const char *cas_fsm_str[] =
    {
        "CAS_IDLE",
        "CAS_DIR_REQ",
        "CAS_DIR_LOCK",
        "CAS_DIR_HIT_READ",
        "CAS_DIR_HIT_COMPARE",
        "CAS_DIR_HIT_WRITE",
        "CAS_UPT_LOCK",
        "CAS_UPT_HEAP_LOCK",
        "CAS_UPT_REQ",
        "CAS_UPT_NEXT",
        "CAS_BC_TRT_LOCK",
        "CAS_BC_IVT_LOCK",
        "CAS_BC_DIR_INVAL",
        "CAS_BC_CC_SEND",
        "CAS_BC_XRAM_REQ",
        "CAS_RSP_FAIL",
        "CAS_RSP_SUCCESS",
        "CAS_MISS_TRT_LOCK",
        "CAS_MISS_TRT_SET",
        "CAS_MISS_XRAM_REQ",
        "CAS_WAIT"
    };
    const char *cleanup_fsm_str[] =
    {
        "CLEANUP_IDLE",
        "CLEANUP_GET_NLINE",
        "CLEANUP_DIR_REQ",
        "CLEANUP_DIR_LOCK",
        "CLEANUP_DIR_WRITE",
        "CLEANUP_HEAP_REQ",
        "CLEANUP_HEAP_LOCK",
        "CLEANUP_HEAP_SEARCH",
        "CLEANUP_HEAP_CLEAN",
        "CLEANUP_HEAP_FREE",
        "CLEANUP_IVT_LOCK",
        "CLEANUP_IVT_DECREMENT",
        "CLEANUP_IVT_CLEAR",
        "CLEANUP_WRITE_RSP",
        "CLEANUP_SEND_CLACK"
    };
    const char *alloc_dir_fsm_str[] =
    {
        "ALLOC_DIR_RESET",
        "ALLOC_DIR_READ",
        "ALLOC_DIR_WRITE",
        "ALLOC_DIR_CAS",
        "ALLOC_DIR_CLEANUP",
        "ALLOC_DIR_XRAM_RSP",
        "ALLOC_DIR_CONFIG"
    };
    const char *alloc_trt_fsm_str[] =
    {
        "ALLOC_TRT_READ",
        "ALLOC_TRT_WRITE",
        "ALLOC_TRT_CAS",
        "ALLOC_TRT_XRAM_RSP",
        "ALLOC_TRT_IXR_RSP",
        "ALLOC_TRT_IXR_CMD",
        "ALLOC_TRT_CONFIG"
    };
    const char *alloc_upt_fsm_str[] =
    {
        "ALLOC_UPT_WRITE",
        "ALLOC_UPT_CAS",
        "ALLOC_UPT_MULTI_ACK"
    };
    const char *alloc_ivt_fsm_str[] =
    {
        "ALLOC_IVT_WRITE",
        "ALLOC_IVT_XRAM_RSP",
        "ALLOC_IVT_CLEANUP",
        "ALLOC_IVT_CAS",
        "ALLOC_IVT_CONFIG"
    };
    const char *alloc_heap_fsm_str[] =
    {
        "ALLOC_HEAP_RESET",
        "ALLOC_HEAP_READ",
        "ALLOC_HEAP_WRITE",
        "ALLOC_HEAP_CAS",
        "ALLOC_HEAP_CLEANUP",
        "ALLOC_HEAP_XRAM_RSP",
        "ALLOC_HEAP_CONFIG"
    };

#define tmpl(x) \
    template<typename vci_param_int, \
    typename vci_param_ext, \
    size_t memc_dspin_in_width,  \
    size_t memc_dspin_out_width> x \
    VciMemCache<vci_param_int, vci_param_ext, memc_dspin_in_width, memc_dspin_out_width>

    using namespace soclib::common;

    ////////////////////////////////
    //  Constructor
    ////////////////////////////////

    tmpl(/**/)::VciMemCache(
            sc_module_name     name,
            const MappingTable &mtp,       // mapping table for direct network
            const MappingTable &mtx,       // mapping table for external network
            const IntTab       &srcid_x,   // global index on external network
            const IntTab       &tgtid_d,   // global index on direct network
            const size_t       x_width,    // number of x bits in platform
            const size_t       y_width,    // number of x bits in platform
            const size_t       nways,      // number of ways per set
            const size_t       nsets,      // number of associative sets
            const size_t       nwords,     // number of words in cache line
            const size_t       max_copies, // max number of copies in heap
            const size_t       heap_size,  // number of heap entries
            const size_t       trt_lines,  // number of TRT entries
            const size_t       upt_lines,  // number of UPT entries
            const size_t       ivt_lines,  // number of IVT entries
            const size_t       debug_start_cycle,
            const bool         debug_ok)

        : soclib::caba::BaseModule(name),

        p_clk("p_clk"),
        p_resetn("p_resetn"),
        p_irq ("p_irq"),
        p_vci_tgt("p_vci_tgt"),
        p_vci_ixr("p_vci_ixr"),
        p_dspin_p2m("p_dspin_p2m"),
        p_dspin_m2p("p_dspin_m2p"),
        p_dspin_clack("p_dspin_clack"),

        m_seglist(mtp.getSegmentList(tgtid_d)),
        m_nseg(0),
        m_srcid_x(mtx.indexForId(srcid_x)),
        m_initiators(1 << vci_param_int::S),
        m_heap_size(heap_size),
        m_ways(nways),
        m_sets(nsets),
        m_words(nwords),
        m_x_width(x_width),
        m_y_width(y_width),
        m_debug_start_cycle(debug_start_cycle),
        m_debug_ok(debug_ok),
        m_trt_lines(trt_lines),
        m_trt(this->name(), trt_lines, nwords),
        m_upt_lines(upt_lines),
        m_upt(upt_lines),
        m_ivt(ivt_lines),
        m_cache_directory(nways, nsets, nwords, vci_param_int::N),
        m_cache_data(nways, nsets, nwords),
        m_heap(m_heap_size),
        m_max_copies(max_copies),
        m_llsc_table(),

#define L2 soclib::common::uint32_log2
        m_x(L2(m_words), 2),
        m_y(L2(m_sets), L2(m_words) + 2),
        m_z(vci_param_int::N - L2(m_sets) - L2(m_words) - 2, L2(m_sets) + L2(m_words) + 2),
        m_nline(vci_param_int::N - L2(m_words) - 2, L2(m_words) + 2),
#undef L2

        // XMIN(5 bits) / XMAX(5 bits) / YMIN(5 bits) / YMAX(5 bits)
        //   0b00000    /   0b11111    /   0b00000    /   0b11111
        m_broadcast_boundaries(0x7C1F),

        // CONFIG interface
        m_config_addr_mask((1 << 12) - 1),

        m_config_regr_width(7),
        m_config_func_width(3),
        m_config_regr_idx_mask((1 << m_config_regr_width) - 1),
        m_config_func_idx_mask((1 << m_config_func_width) - 1),

        //  FIFOs
        m_cmd_read_addr_fifo("m_cmd_read_addr_fifo", 4),
        m_cmd_read_length_fifo("m_cmd_read_length_fifo", 4),
        m_cmd_read_srcid_fifo("m_cmd_read_srcid_fifo", 4),
        m_cmd_read_trdid_fifo("m_cmd_read_trdid_fifo", 4),
        m_cmd_read_pktid_fifo("m_cmd_read_pktid_fifo", 4),

        m_cmd_write_addr_fifo("m_cmd_write_addr_fifo",8),
        m_cmd_write_eop_fifo("m_cmd_write_eop_fifo",8),
        m_cmd_write_srcid_fifo("m_cmd_write_srcid_fifo",8),
        m_cmd_write_trdid_fifo("m_cmd_write_trdid_fifo",8),
        m_cmd_write_pktid_fifo("m_cmd_write_pktid_fifo",8),
        m_cmd_write_data_fifo("m_cmd_write_data_fifo",8),
        m_cmd_write_be_fifo("m_cmd_write_be_fifo",8),

        m_cmd_cas_addr_fifo("m_cmd_cas_addr_fifo",4),
        m_cmd_cas_eop_fifo("m_cmd_cas_eop_fifo",4),
        m_cmd_cas_srcid_fifo("m_cmd_cas_srcid_fifo",4),
        m_cmd_cas_trdid_fifo("m_cmd_cas_trdid_fifo",4),
        m_cmd_cas_pktid_fifo("m_cmd_cas_pktid_fifo",4),
        m_cmd_cas_wdata_fifo("m_cmd_cas_wdata_fifo",4),

        m_cc_receive_to_cleanup_fifo("m_cc_receive_to_cleanup_fifo", 4),
        m_cc_receive_to_multi_ack_fifo("m_cc_receive_to_multi_ack_fifo", 4),

        r_tgt_cmd_fsm("r_tgt_cmd_fsm"),

        r_config_fsm("r_config_fsm"),

        m_config_to_cc_send_inst_fifo("m_config_to_cc_send_inst_fifo", 8),
        m_config_to_cc_send_srcid_fifo("m_config_to_cc_send_srcid_fifo", 8),

        r_read_fsm("r_read_fsm"),

        r_write_fsm("r_write_fsm"),

        m_write_to_cc_send_inst_fifo("m_write_to_cc_send_inst_fifo",8),
        m_write_to_cc_send_srcid_fifo("m_write_to_cc_send_srcid_fifo",8),

        r_multi_ack_fsm("r_multi_ack_fsm"),

        r_cleanup_fsm("r_cleanup_fsm"),

        r_cas_fsm("r_cas_fsm"),

        m_cas_to_cc_send_inst_fifo("m_cas_to_cc_send_inst_fifo",8),
        m_cas_to_cc_send_srcid_fifo("m_cas_to_cc_send_srcid_fifo",8),

        r_ixr_rsp_fsm("r_ixr_rsp_fsm"),
        r_xram_rsp_fsm("r_xram_rsp_fsm"),

        m_xram_rsp_to_cc_send_inst_fifo("m_xram_rsp_to_cc_send_inst_fifo",8),
        m_xram_rsp_to_cc_send_srcid_fifo("m_xram_rsp_to_cc_send_srcid_fifo",8),

        r_ixr_cmd_fsm("r_ixr_cmd_fsm"),

        r_tgt_rsp_fsm("r_tgt_rsp_fsm"),

        r_cc_send_fsm("r_cc_send_fsm"),
        r_cc_receive_fsm("r_cc_receive_fsm"),

        r_alloc_dir_fsm("r_alloc_dir_fsm"),
        r_alloc_dir_reset_cpt("r_alloc_dir_reset_cpt"),
        r_alloc_trt_fsm("r_alloc_trt_fsm"),
        r_alloc_upt_fsm("r_alloc_upt_fsm"),
        r_alloc_ivt_fsm("r_alloc_ivt_fsm"),
        r_alloc_heap_fsm("r_alloc_heap_fsm"),
        r_alloc_heap_reset_cpt("r_alloc_heap_reset_cpt")

#if MONITOR_MEMCACHE_FSM == 1
        ,
        p_read_fsm("p_read_fsm"),
        p_write_fsm("p_write_fsm"),
        p_xram_rsp_fsm("p_xram_rsp_fsm"),
        p_cas_fsm("p_cas_fsm"),
        p_cleanup_fsm("p_cleanup_fsm"),
        p_config_fsm("p_config_fsm"),
        p_alloc_heap_fsm("p_alloc_heap_fsm"),
        p_alloc_dir_fsm("p_alloc_dir_fsm"),
        p_alloc_trt_fsm("p_alloc_trt_fsm"),
        p_alloc_upt_fsm("p_alloc_upt_fsm"),
        p_alloc_ivt_fsm("p_alloc_ivt_fsm"),
        p_tgt_cmd_fsm("p_tgt_cmd_fsm"),
        p_tgt_rsp_fsm("p_tgt_rsp_fsm"),
        p_ixr_cmd_fsm("p_ixr_cmd_fsm"),
        p_ixr_rsp_fsm("p_ixr_rsp_fsm"),
        p_cc_send_fsm("p_cc_send_fsm"),
        p_cc_receive_fsm("p_cc_receive_fsm"),
        p_multi_ack_fsm("p_multi_ack_fsm")
#endif
        {
            std::cout << "  - Building VciMemCache : " << name << std::endl;

            assert(IS_POW_OF_2(nsets));
            assert(IS_POW_OF_2(nwords));
            assert(IS_POW_OF_2(nways));
            assert(nsets);
            assert(nwords);
            assert(nways);

            // check Transaction table size
            assert((uint32_log2(trt_lines) <= vci_param_ext::T) and
                    "MEMC ERROR : Need more bits for VCI TRDID field");

            // check internal and external data width
            assert((vci_param_int::B == 4) and
                    "MEMC ERROR : VCI internal data width must be 32 bits");

            assert((vci_param_ext::B == 8) and
                    "MEMC ERROR : VCI external data width must be 64 bits");

            // Check coherence between internal & external addresses
            assert((vci_param_int::N == vci_param_ext::N) and
                    "MEMC ERROR : VCI internal & external addresses must have the same width");

            // Get the segments associated to the MemCache
            std::list<soclib::common::Segment>::iterator seg;
            size_t i = 0;

            for (seg = m_seglist.begin(); seg != m_seglist.end(); seg++)
            {
                std::cout << "    => segment " << seg->name()
                    << " / base = " << std::hex << seg->baseAddress()
                    << " / size = " << seg->size() << std::endl;
                m_nseg++;
            }

            assert((m_nseg > 0) and
                    "MEMC ERROR : At least one segment must be mapped to this component");

            m_seg = new soclib::common::Segment*[m_nseg];

            for (seg = m_seglist.begin(); seg != m_seglist.end(); seg++)
            {
                if (seg->special()) m_seg_config = i;
                m_seg[i] = & (*seg);
                i++;
            }

            addr_t gid = m_seg[0]->baseAddress() >> (vci_param_int::N - x_width - y_width);
            m_x_self = (gid >> m_y_width) & ((1 << m_x_width) - 1);
            m_y_self =  gid               & ((1 << m_y_width) - 1);

            // Allocation for IXR_RSP FSM
            r_ixr_rsp_to_xram_rsp_rok  = new sc_signal<bool>[m_trt_lines];

            // Allocation for XRAM_RSP FSM
            r_xram_rsp_victim_data     = new sc_signal<data_t>[nwords];
            r_xram_rsp_to_tgt_rsp_data = new sc_signal<data_t>[nwords];

            // Allocation for READ FSM
            r_read_data                = new sc_signal<data_t>[nwords];
            r_read_to_tgt_rsp_data     = new sc_signal<data_t>[nwords];

            // Allocation for WRITE FSM
            r_write_data               = new sc_signal<data_t>[nwords];
            r_write_be                 = new sc_signal<be_t>[nwords];
            r_write_to_cc_send_data    = new sc_signal<data_t>[nwords];
            r_write_to_cc_send_be      = new sc_signal<be_t>[nwords];

            // Allocation for CAS FSM
            r_cas_data                 = new sc_signal<data_t>[nwords];
            r_cas_rdata                = new sc_signal<data_t>[2];

            // Allocation for IXR_CMD FSM
            r_ixr_cmd_wdata            = new sc_signal<data_t>[nwords];

            // Allocation for debug
            m_debug_previous_data      = new data_t[nwords];
            m_debug_data               = new data_t[nwords];

            SC_METHOD(transition);
            dont_initialize();
            sensitive << p_clk.pos();

            SC_METHOD(genMoore);
            dont_initialize();
            sensitive << p_clk.neg();
        } // end constructor


    //////////////////////////////////////////////////////////
    tmpl(void)::cache_monitor(addr_t addr, bool single_word)
    //////////////////////////////////////////////////////////
    {
        size_t way  = 0;
        size_t set  = 0;
        size_t word = ((size_t) addr & 0x3F) >> 2;

        DirectoryEntry entry = m_cache_directory.read_neutral(addr, &way, &set);

        // read data and compute data_change
        bool data_change = false;
        if (entry.valid)
        {
            if (single_word)
            {
                m_debug_data[word] = m_cache_data.read(way, set, word);
                if (m_debug_previous_valid and
                     (m_debug_data[word] != m_debug_previous_data[word]))
                {
                    data_change = true;
                }
            }
            else
            {
                for (size_t wcur = 0; wcur < m_words; wcur++)
                {
                    m_debug_data[wcur] = m_cache_data.read(way, set, wcur);
                    if (m_debug_previous_valid and
                         (m_debug_data[wcur] != m_debug_previous_data[wcur]))
                    {
                        data_change = true;
                    }
                }
            }
        }

        // print values if any change
        if ((entry.valid != m_debug_previous_valid) or
            (entry.valid and (entry.count != m_debug_previous_count)) or
            (entry.valid and (entry.dirty != m_debug_previous_dirty)) or data_change)
        {
            std::cout << "Monitor MEMC " << name()
                      << " at cycle " << std::dec << m_cpt_cycles
                      << " : address = " << std::hex << addr
                      << " / VAL = " << std::dec << entry.valid
                      << " / WAY = " << way
                      << " / COUNT = " << entry.count
                      << " / DIRTY = " << entry.dirty
                      << " / DATA_CHANGE = " << data_change;
            if (single_word)
            {
                 std::cout << std::hex << " / value = " << m_debug_data[word] << std::endl;
            }
            else
            {
                std::cout << std::hex << std::endl
                          << "/0:" << m_debug_data[0]
                          << "/1:" << m_debug_data[1]
                          << "/2:" << m_debug_data[2]
                          << "/3:" << m_debug_data[3]
                          << "/4:" << m_debug_data[4]
                          << "/5:" << m_debug_data[5]
                          << "/6:" << m_debug_data[6]
                          << "/7:" << m_debug_data[7]
                          << "/8:" << m_debug_data[8]
                          << "/9:" << m_debug_data[9]
                          << "/A:" << m_debug_data[10]
                          << "/B:" << m_debug_data[11]
                          << "/C:" << m_debug_data[12]
                          << "/D:" << m_debug_data[13]
                          << "/E:" << m_debug_data[14]
                          << "/F:" << m_debug_data[15]
                          << std::endl;
            }
        }

        // register values
        m_debug_previous_count = entry.count;
        m_debug_previous_valid = entry.valid;
        m_debug_previous_dirty = entry.dirty;
        for (size_t wcur = 0; wcur < m_words; wcur++)
        {
            m_debug_previous_data[wcur] = m_debug_data[wcur];
        }
    }


    /////////////////////////////////////////////////////
    tmpl(uint32_t)::req_distance(uint32_t req_srcid)
    /////////////////////////////////////////////////////
    {
        const uint32_t srcid_width = vci_param_int::S;

        uint8_t req_x = (req_srcid >> (srcid_width - m_x_width));
        uint8_t req_y = (req_srcid >> (srcid_width - m_x_width - m_y_width)) & ((1 << m_y_width) - 1);

        return abs(m_x_self - req_x) + abs(m_y_self - req_y) + 1;
    }


    /////////////////////////////////////////////////////
    tmpl(bool)::is_local_req(uint32_t req_srcid)
    /////////////////////////////////////////////////////
    {
        return req_distance(req_srcid) == 1;
    }

    /////////////////////////////////////////////////////
    tmpl(int)::read_instrumentation(uint32_t regr, uint32_t & rdata)
    /////////////////////////////////////////////////////
    {
        int error = 0;

        switch(regr)
        {
            ///////////////////////////////////////////////////////
            //       DIRECT instrumentation registers            //
            // Registers of 32 bits and therefore only LO is     //
            // implemented.                                      //
            //                                                   //
            // The HI may be used in future implementations      //
            ///////////////////////////////////////////////////////

            // LOCAL

            case MEMC_LOCAL_READ_LO   : rdata = m_cpt_read_local        ; break;
            case MEMC_LOCAL_WRITE_LO  : rdata = m_cpt_write_flits_local ; break;
            case MEMC_LOCAL_LL_LO     : rdata = m_cpt_ll_local          ; break;
            case MEMC_LOCAL_SC_LO     : rdata = m_cpt_sc_local          ; break;
            case MEMC_LOCAL_CAS_LO    : rdata = m_cpt_cas_local         ; break;
            case MEMC_LOCAL_READ_HI   :
            case MEMC_LOCAL_WRITE_HI  :
            case MEMC_LOCAL_LL_HI     :
            case MEMC_LOCAL_SC_HI     :
            case MEMC_LOCAL_CAS_HI    : rdata = 0; break;

            // REMOTE

            case MEMC_REMOTE_READ_LO  : rdata = m_cpt_read_remote        ; break;
            case MEMC_REMOTE_WRITE_LO : rdata = m_cpt_write_flits_remote ; break;
            case MEMC_REMOTE_LL_LO    : rdata = m_cpt_ll_remote          ; break;
            case MEMC_REMOTE_SC_LO    : rdata = m_cpt_sc_remote          ; break;
            case MEMC_REMOTE_CAS_LO   : rdata = m_cpt_cas_remote         ; break;
            case MEMC_REMOTE_READ_HI  :
            case MEMC_REMOTE_WRITE_HI :
            case MEMC_REMOTE_LL_HI    :
            case MEMC_REMOTE_SC_HI    :
            case MEMC_REMOTE_CAS_HI   : rdata = 0; break;

            // COST

            case MEMC_COST_READ_LO    : rdata = m_cpt_read_cost ; break;
            case MEMC_COST_WRITE_LO   : rdata = m_cpt_write_cost; break;
            case MEMC_COST_LL_LO      : rdata = m_cpt_ll_cost   ; break;
            case MEMC_COST_SC_LO      : rdata = m_cpt_sc_cost   ; break;
            case MEMC_COST_CAS_LO     : rdata = m_cpt_cas_cost  ; break;
            case MEMC_COST_READ_HI    :
            case MEMC_COST_WRITE_HI   :
            case MEMC_COST_LL_HI      :
            case MEMC_COST_SC_HI      :
            case MEMC_COST_CAS_HI     : rdata = 0; break;

            ///////////////////////////////////////////////////////
            //       COHERENCE instrumentation registers         //
            // Registers of 32 bits and therefore only LO is     //
            // implemented.                                      //
            //                                                   //
            // The HI may be used in future implementations      //
            ///////////////////////////////////////////////////////

            // LOCAL

            case MEMC_LOCAL_MUPDATE_LO  : rdata = m_cpt_update_local ; break;
            case MEMC_LOCAL_MINVAL_LO   : rdata = m_cpt_minval_local ; break;
            case MEMC_LOCAL_CLEANUP_LO  : rdata = m_cpt_cleanup_local; break;
            case MEMC_LOCAL_MUPDATE_HI  :
            case MEMC_LOCAL_MINVAL_HI   :
            case MEMC_LOCAL_CLEANUP_HI  : rdata = 0; break;

            // REMOTE

            case MEMC_REMOTE_MUPDATE_LO : rdata = m_cpt_update_remote ; break;
            case MEMC_REMOTE_MINVAL_LO  : rdata = m_cpt_minval_remote ; break;
            case MEMC_REMOTE_CLEANUP_LO : rdata = m_cpt_cleanup_remote; break;
            case MEMC_REMOTE_MUPDATE_HI :
            case MEMC_REMOTE_MINVAL_HI  :
            case MEMC_REMOTE_CLEANUP_HI : rdata = 0; break;

            // COST

            case MEMC_COST_MUPDATE_LO   : rdata = m_cpt_update_cost   ; break;
            case MEMC_COST_MINVAL_LO    : rdata = m_cpt_minval_cost   ; break;
            case MEMC_COST_CLEANUP_LO   : rdata = m_cpt_cleanup_cost  ; break;
            case MEMC_COST_MUPDATE_HI   :
            case MEMC_COST_MINVAL_HI    :
            case MEMC_COST_CLEANUP_HI   : rdata = 0; break;

            // TOTAL

            case MEMC_TOTAL_MUPDATE_LO  : rdata = m_cpt_update ; break;
            case MEMC_TOTAL_MINVAL_LO   : rdata = m_cpt_minval ; break;
            case MEMC_TOTAL_BINVAL_LO   : rdata = m_cpt_binval ; break;
            case MEMC_TOTAL_MUPDATE_HI  :
            case MEMC_TOTAL_MINVAL_HI   :
            case MEMC_TOTAL_BINVAL_HI   : rdata = 0; break;

            // unknown register

            default                     : error = 1;
        }

        return error;
    }

    //////////////////////////////////////////////////
    tmpl(void)::print_trace(size_t detailed)
    //////////////////////////////////////////////////
    {
        std::cout << "MEMC " << name() << std::endl;
        std::cout << "  "  << tgt_cmd_fsm_str[r_tgt_cmd_fsm.read()]
            << " | " << tgt_rsp_fsm_str[r_tgt_rsp_fsm.read()]
            << " | " << read_fsm_str[r_read_fsm.read()]
            << " | " << write_fsm_str[r_write_fsm.read()]
            << " | " << cas_fsm_str[r_cas_fsm.read()]
            << " | " << config_fsm_str[r_config_fsm.read()]
            << " | " << cleanup_fsm_str[r_cleanup_fsm.read()] << std::endl;
        std::cout << "  "  << cc_send_fsm_str[r_cc_send_fsm.read()]
            << " | " << cc_receive_fsm_str[r_cc_receive_fsm.read()]
            << " | " << multi_ack_fsm_str[r_multi_ack_fsm.read()]
            << " | " << ixr_cmd_fsm_str[r_ixr_cmd_fsm.read()]
            << " | " << ixr_rsp_fsm_str[r_ixr_rsp_fsm.read()]
            << " | " << xram_rsp_fsm_str[r_xram_rsp_fsm.read()] << std::endl;
        std::cout << "  "  << alloc_dir_fsm_str[r_alloc_dir_fsm.read()]
            << " | " << alloc_trt_fsm_str[r_alloc_trt_fsm.read()]
            << " | " << alloc_upt_fsm_str[r_alloc_upt_fsm.read()]
            << " | " << alloc_ivt_fsm_str[r_alloc_ivt_fsm.read()]
            << " | " << alloc_heap_fsm_str[r_alloc_heap_fsm.read()] << std::endl;

        if (detailed) m_trt.print(0);
    }


    /////////////////////////////////////////
    tmpl(void)::reset_counters()
    /////////////////////////////////////////
    {
        m_cpt_reset_count        = m_cpt_cycles;

        m_cpt_read_local         = 0;
        m_cpt_read_remote        = 0;
        m_cpt_read_cost          = 0;

        m_cpt_write_local        = 0;
        m_cpt_write_remote       = 0;
        m_cpt_write_flits_local  = 0;
        m_cpt_write_flits_remote = 0;
        m_cpt_write_cost         = 0;

        m_cpt_ll_local           = 0;
        m_cpt_ll_remote          = 0;
        m_cpt_ll_cost            = 0;

        m_cpt_sc_local           = 0;
        m_cpt_sc_remote          = 0;
        m_cpt_sc_cost            = 0;

        m_cpt_cas_local          = 0;
        m_cpt_cas_remote         = 0;
        m_cpt_cas_cost           = 0;

        m_cpt_update             = 0;
        m_cpt_update_local       = 0;
        m_cpt_update_remote      = 0;
        m_cpt_update_cost        = 0;

        m_cpt_minval             = 0;
        m_cpt_minval_local       = 0;
        m_cpt_minval_remote      = 0;
        m_cpt_minval_cost        = 0;

        m_cpt_binval             = 0;
        m_cpt_write_broadcast    = 0;

        m_cpt_cleanup_local      = 0;
        m_cpt_cleanup_remote     = 0;
        m_cpt_cleanup_cost       = 0;

        m_cpt_read_miss          = 0;
        m_cpt_write_miss         = 0;
        m_cpt_write_dirty        = 0;

        m_cpt_trt_rb             = 0;
        m_cpt_trt_full           = 0;
        m_cpt_get                = 0;
        m_cpt_put                = 0;
    }

    //////////////////////////////////////////////////////////////
    tmpl(void)::print_stats(bool activity_counters, bool stats)
    /////////////////////////////////////////////////////////////
    {
        std::cout << "**********************************" << std::dec << std::endl;
        std::cout << "*** MEM_CACHE " << name() << std::endl;
        std::cout << "**********************************" << std::dec << std::endl;
        if (activity_counters)
        {
            std::cout << "----------------------------------" << std::dec << std::endl;
            std::cout << "---     Activity Counters      ---" << std::dec << std::endl;
            std::cout << "----------------------------------" << std::dec << std::endl;
            std::cout
                << "[000] COUNTERS RESET AT CYCLE   = " << m_cpt_reset_count << std::endl
                << "[001] NUMBER OF CYCLES          = " << m_cpt_cycles << std::endl
                << std::endl
                << "[010] LOCAL READ                = " << m_cpt_read_local << std::endl
                << "[011] REMOTE READ               = " << m_cpt_read_remote << std::endl
                << "[012] READ COST (FLITS * DIST)  = " << m_cpt_read_cost << std::endl
                << std::endl
                << "[020] LOCAL WRITE               = " << m_cpt_write_local << std::endl
                << "[021] REMOTE WRITE              = " << m_cpt_write_remote << std::endl
                << "[022] WRITE FLITS LOCAL         = " << m_cpt_write_flits_local << std::endl
                << "[023] WRITE FLITS REMOTE        = " << m_cpt_write_flits_remote << std::endl
                << "[024] WRITE COST (FLITS * DIST) = " << m_cpt_write_cost << std::endl
                << "[025] WRITE L1 MISS NCC         = " << "0" << std::endl
                << std::endl
                << "[030] LOCAL LL                  = " << m_cpt_ll_local << std::endl
                << "[031] REMOTE LL                 = " << m_cpt_ll_remote << std::endl
                << "[032] LL COST (FLITS * DIST)    = " << m_cpt_ll_cost << std::endl
                << std::endl
                << "[040] LOCAL SC                  = " << m_cpt_sc_local << std::endl
                << "[041] REMOTE SC                 = " << m_cpt_sc_remote << std::endl
                << "[042] SC COST (FLITS * DIST)    = " << m_cpt_sc_cost << std::endl
                << std::endl
                << "[050] LOCAL CAS                 = " << m_cpt_cas_local << std::endl
                << "[051] REMOTE CAS                = " << m_cpt_cas_remote << std::endl
                << "[052] CAS COST (FLITS * DIST)   = " << m_cpt_cas_cost << std::endl
                << std::endl
                << "[060] REQUESTS TRIG. UPDATE     = " << m_cpt_update << std::endl
                << "[061] LOCAL UPDATE              = " << m_cpt_update_local << std::endl
                << "[062] REMOTE UPDATE             = " << m_cpt_update_remote << std::endl
                << "[063] UPDT COST (FLITS * DIST)  = " << m_cpt_update_cost << std::endl
                << std::endl
                << "[070] REQUESTS TRIG. M_INV      = " << m_cpt_minval << std::endl
                << "[071] LOCAL M_INV               = " << m_cpt_minval_local << std::endl
                << "[072] REMOTE M_INV              = " << m_cpt_minval_remote << std::endl
                << "[073] M_INV COST (FLITS * DIST) = " << m_cpt_minval_cost << std::endl
                << std::endl
                << "[080] BROADCAT INVAL            = " << m_cpt_binval << std::endl
                << "[081] WRITE BROADCAST           = " << m_cpt_write_broadcast << std::endl
                << "[082] GETM BROADCAST            = " << "0" << std::endl
                << std::endl
                << "[090] LOCAL CLEANUP             = " << m_cpt_cleanup_local << std::endl
                << "[091] REMOTE CLEANUP            = " << m_cpt_cleanup_remote << std::endl
                << "[092] CLNUP COST (FLITS * DIST) = " << m_cpt_cleanup_cost << std::endl
                << "[093] LOCAL CLEANUP DATA        = " << "0" << std::endl
                << "[094] REMOTE CLEANUP DATA       = " << "0" << std::endl
                << "[095] CLEANUP DATA COST         = " << "0" << std::endl
                << std::endl
                << "[100] READ MISS                 = " << m_cpt_read_miss << std::endl
                << "[101] WRITE MISS                = " << m_cpt_write_miss << std::endl
                << "[102] WRITE DIRTY               = " << m_cpt_write_dirty << std::endl
                << "[103] GETM MISS                 = " << "0" << std::endl
                << std::endl
                << "[110] RD BLOCKED BY HIT IN TRT  = " << m_cpt_trt_rb << std::endl
                << "[111] TRANS BLOCKED BY FULL TRT = " << m_cpt_trt_full << std::endl
                << "[120] PUT (UNIMPLEMENTED)       = " << m_cpt_put << std::endl
                << "[121] GET (UNIMPLEMENTED)       = " << m_cpt_get << std::endl
                << "[130] MIN HEAP SLOT AV. (UNIMP) = " << "0" << std::endl
                << std::endl
                << "[140] NCC TO CC (READ)          = " << "0" << std::endl
                << "[141] NCC TO CC (WRITE)         = " << "0" << std::endl
                << std::endl
                << "[150] LOCAL GETM                = " << "0" << std::endl
                << "[151] REMOTE GETM               = " << "0" << std::endl
                << "[152] GETM COST (FLITS * DIST)  = " << "0" << std::endl
                << std::endl
                << "[160] LOCAL INVAL RO            = " << "0" << std::endl
                << "[161] REMOTE INVAL RO           = " << "0" << std::endl
                << "[162] INVAL RO COST             = " << "0" << std::endl
                << std::endl;
        }
        // No more computed stats
    }


    /////////////////////////////////
    tmpl(/**/)::~VciMemCache()
    /////////////////////////////////
    {
        delete [] m_seg;

        delete [] r_ixr_rsp_to_xram_rsp_rok;
        delete [] r_xram_rsp_victim_data;
        delete [] r_xram_rsp_to_tgt_rsp_data;

        delete [] r_read_data;
        delete [] r_read_to_tgt_rsp_data;

        delete [] r_write_data;
        delete [] r_write_be;
        delete [] r_write_to_cc_send_data;
        delete [] r_write_to_cc_send_be;

        delete [] r_cas_data;
        delete [] r_cas_rdata;

        delete [] r_ixr_cmd_wdata;
        delete [] m_debug_previous_data;
        delete [] m_debug_data;

        //print_stats();
    }

    //////////////////////////////////
    tmpl(void)::transition()
    //////////////////////////////////
    {
        using soclib::common::uint32_log2;

        // RESET
        if (!p_resetn.read())
        {

            // Initializing FSMs
            r_tgt_cmd_fsm    = TGT_CMD_IDLE;
            r_config_fsm     = CONFIG_IDLE;
            r_tgt_rsp_fsm    = TGT_RSP_READ_IDLE;
            r_cc_send_fsm    = CC_SEND_XRAM_RSP_IDLE;
            r_cc_receive_fsm = CC_RECEIVE_IDLE;
            r_multi_ack_fsm  = MULTI_ACK_IDLE;
            r_read_fsm       = READ_IDLE;
            r_write_fsm      = WRITE_IDLE;
            r_cas_fsm        = CAS_IDLE;
            r_cleanup_fsm    = CLEANUP_IDLE;
            r_alloc_dir_fsm  = ALLOC_DIR_RESET;
            r_alloc_heap_fsm = ALLOC_HEAP_RESET;
            r_alloc_trt_fsm  = ALLOC_TRT_READ;
            r_alloc_upt_fsm  = ALLOC_UPT_WRITE;
            r_alloc_ivt_fsm  = ALLOC_IVT_WRITE;
            r_ixr_rsp_fsm    = IXR_RSP_IDLE;
            r_xram_rsp_fsm   = XRAM_RSP_IDLE;
            r_ixr_cmd_fsm    = IXR_CMD_READ_IDLE;

            m_debug                = false;
            m_debug_previous_valid = false;
            m_debug_previous_dirty = false;
            m_debug_previous_count = 0;

            //  Initializing Tables
            m_trt.init();
            m_upt.init();
            m_ivt.init();
            m_llsc_table.init();

            // initializing FIFOs and communication Buffers

            m_cmd_read_addr_fifo.init();
            m_cmd_read_length_fifo.init();
            m_cmd_read_srcid_fifo.init();
            m_cmd_read_trdid_fifo.init();
            m_cmd_read_pktid_fifo.init();

            m_cmd_write_addr_fifo.init();
            m_cmd_write_eop_fifo.init();
            m_cmd_write_srcid_fifo.init();
            m_cmd_write_trdid_fifo.init();
            m_cmd_write_pktid_fifo.init();
            m_cmd_write_data_fifo.init();

            m_cmd_cas_addr_fifo.init()  ;
            m_cmd_cas_srcid_fifo.init() ;
            m_cmd_cas_trdid_fifo.init() ;
            m_cmd_cas_pktid_fifo.init() ;
            m_cmd_cas_wdata_fifo.init() ;
            m_cmd_cas_eop_fifo.init()   ;

            r_config_cmd  = MEMC_CMD_NOP;

            m_config_to_cc_send_inst_fifo.init();
            m_config_to_cc_send_srcid_fifo.init();

            r_tgt_cmd_to_tgt_rsp_req = false;

            r_read_to_tgt_rsp_req = false;
            r_read_to_ixr_cmd_req = false;

            r_write_to_tgt_rsp_req          = false;
            r_write_to_ixr_cmd_req          = false;
            r_write_to_cc_send_multi_req    = false;
            r_write_to_cc_send_brdcast_req  = false;
            r_write_to_multi_ack_req        = false;

            m_write_to_cc_send_inst_fifo.init();
            m_write_to_cc_send_srcid_fifo.init();

            r_cleanup_to_tgt_rsp_req      = false;

            m_cc_receive_to_cleanup_fifo.init();

            r_multi_ack_to_tgt_rsp_req    = false;

            m_cc_receive_to_multi_ack_fifo.init();

            r_cas_to_tgt_rsp_req          = false;
            r_cas_cpt                     = 0    ;
            r_cas_lfsr                    = -1   ;
            r_cas_to_ixr_cmd_req          = false;
            r_cas_to_cc_send_multi_req    = false;
            r_cas_to_cc_send_brdcast_req  = false;

            m_cas_to_cc_send_inst_fifo.init();
            m_cas_to_cc_send_srcid_fifo.init();

            for (size_t i = 0; i < m_trt_lines ; i++)
            {
                r_ixr_rsp_to_xram_rsp_rok[i] = false;
            }

            r_xram_rsp_to_tgt_rsp_req          = false;
            r_xram_rsp_to_cc_send_multi_req    = false;
            r_xram_rsp_to_cc_send_brdcast_req  = false;
            r_xram_rsp_to_ixr_cmd_req          = false;
            r_xram_rsp_trt_index               = 0;
            r_xram_rsp_rerror_irq              = false;
            r_xram_rsp_rerror_irq_enable       = false;

            m_xram_rsp_to_cc_send_inst_fifo.init();
            m_xram_rsp_to_cc_send_srcid_fifo.init();

            r_alloc_dir_reset_cpt  = 0;
            r_alloc_heap_reset_cpt = 0;

            r_tgt_rsp_key_sent  = false;

            // Activity counters
            m_cpt_cycles             = 0;
            m_cpt_reset_count        = 0;

            m_cpt_read_local         = 0;
            m_cpt_read_remote        = 0;
            m_cpt_read_cost          = 0;

            m_cpt_write_local        = 0;
            m_cpt_write_remote       = 0;
            m_cpt_write_flits_local  = 0;
            m_cpt_write_flits_remote = 0;
            m_cpt_write_cost         = 0;

            m_cpt_ll_local           = 0;
            m_cpt_ll_remote          = 0;
            m_cpt_ll_cost            = 0;

            m_cpt_sc_local           = 0;
            m_cpt_sc_remote          = 0;
            m_cpt_sc_cost            = 0;

            m_cpt_cas_local          = 0;
            m_cpt_cas_remote         = 0;
            m_cpt_cas_cost           = 0;

            m_cpt_update             = 0;
            m_cpt_update_local       = 0;
            m_cpt_update_remote      = 0;
            m_cpt_update_cost        = 0;

            m_cpt_minval             = 0;
            m_cpt_minval_local       = 0;
            m_cpt_minval_remote      = 0;
            m_cpt_minval_cost        = 0;

            m_cpt_binval             = 0;
            m_cpt_write_broadcast    = 0;

            m_cpt_cleanup_local      = 0;
            m_cpt_cleanup_remote     = 0;
            m_cpt_cleanup_cost       = 0;

            m_cpt_read_miss          = 0;
            m_cpt_write_miss         = 0;
            m_cpt_write_dirty        = 0;

            m_cpt_trt_rb             = 0;
            m_cpt_trt_full           = 0;
            m_cpt_get                = 0;
            m_cpt_put                = 0;

            return;
        }

        bool   cmd_read_fifo_put = false;
        bool   cmd_read_fifo_get = false;

        bool   cmd_write_fifo_put = false;
        bool   cmd_write_fifo_get = false;

        bool   cmd_cas_fifo_put = false;
        bool   cmd_cas_fifo_get = false;

        bool   cc_receive_to_cleanup_fifo_get = false;
        bool   cc_receive_to_cleanup_fifo_put = false;

        bool   cc_receive_to_multi_ack_fifo_get = false;
        bool   cc_receive_to_multi_ack_fifo_put = false;

        bool   write_to_cc_send_fifo_put   = false;
        bool   write_to_cc_send_fifo_get   = false;
        bool   write_to_cc_send_fifo_inst  = false;
        size_t write_to_cc_send_fifo_srcid = 0;

        bool   xram_rsp_to_cc_send_fifo_put   = false;
        bool   xram_rsp_to_cc_send_fifo_get   = false;
        bool   xram_rsp_to_cc_send_fifo_inst  = false;
        size_t xram_rsp_to_cc_send_fifo_srcid = 0;

        bool   config_rsp_lines_incr          = false;
        bool   config_rsp_lines_cleanup_decr  = false;
        bool   config_rsp_lines_ixr_rsp_decr  = false;

        bool   config_to_cc_send_fifo_put   = false;
        bool   config_to_cc_send_fifo_get   = false;
        bool   config_to_cc_send_fifo_inst  = false;
        size_t config_to_cc_send_fifo_srcid = 0;

        bool   cas_to_cc_send_fifo_put   = false;
        bool   cas_to_cc_send_fifo_get   = false;
        bool   cas_to_cc_send_fifo_inst  = false;
        size_t cas_to_cc_send_fifo_srcid = 0;

        m_debug = (m_cpt_cycles > m_debug_start_cycle) and m_debug_ok;

#if DEBUG_MEMC_GLOBAL
        if (m_debug)
        {
            std::cout
                << "---------------------------------------------"           << std::dec << std::endl
                << "MEM_CACHE "            << name()
                << " ; Time = "            << m_cpt_cycles                                << std::endl
                << " - TGT_CMD FSM    = "  << tgt_cmd_fsm_str[r_tgt_cmd_fsm.read()]       << std::endl
                << " - TGT_RSP FSM    = "  << tgt_rsp_fsm_str[r_tgt_rsp_fsm.read()]       << std::endl
                << " - CC_SEND FSM  = "    << cc_send_fsm_str[r_cc_send_fsm.read()]       << std::endl
                << " - CC_RECEIVE FSM  = " << cc_receive_fsm_str[r_cc_receive_fsm.read()] << std::endl
                << " - MULTI_ACK FSM  = "  << multi_ack_fsm_str[r_multi_ack_fsm.read()]   << std::endl
                << " - READ FSM       = "  << read_fsm_str[r_read_fsm.read()]             << std::endl
                << " - WRITE FSM      = "  << write_fsm_str[r_write_fsm.read()]           << std::endl
                << " - CAS FSM        = "  << cas_fsm_str[r_cas_fsm.read()]               << std::endl
                << " - CLEANUP FSM    = "  << cleanup_fsm_str[r_cleanup_fsm.read()]       << std::endl
                << " - IXR_CMD FSM    = "  << ixr_cmd_fsm_str[r_ixr_cmd_fsm.read()]       << std::endl
                << " - IXR_RSP FSM    = "  << ixr_rsp_fsm_str[r_ixr_rsp_fsm.read()]       << std::endl
                << " - XRAM_RSP FSM   = "  << xram_rsp_fsm_str[r_xram_rsp_fsm.read()]     << std::endl
                << " - ALLOC_DIR FSM  = "  << alloc_dir_fsm_str[r_alloc_dir_fsm.read()]   << std::endl
                << " - ALLOC_TRT FSM  = "  << alloc_trt_fsm_str[r_alloc_trt_fsm.read()]   << std::endl
                << " - ALLOC_UPT FSM  = "  << alloc_upt_fsm_str[r_alloc_upt_fsm.read()]   << std::endl
                << " - ALLOC_HEAP FSM = "  << alloc_heap_fsm_str[r_alloc_heap_fsm.read()] << std::endl;
        }
#endif

        ////////////////////////////////////////////////////////////////////////////////////
        //    TGT_CMD FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The TGT_CMD_FSM controls the incoming VCI command pakets from the processors,
        // and dispatch these commands to the proper FSM through dedicated FIFOs.
        // These READ/WRITE commands can be for the XRAM segment, or for the
        // CONFIG segment:
        //
        // There are 5 types of commands accepted for the XRAM segment:
        // - READ   : A READ request has a length of 1 VCI flit. It can be a single word
        //            or an entire cache line, depending on the PLEN value => READ FSM
        // - WRITE  : A WRITE request has a maximum length of 16 flits, and can only
        //            concern words in a same line => WRITE FSM
        // - CAS    : A CAS request has a length of 2 flits or 4 flits => CAS FSM
        // - LL     : An LL request has a length of 1 flit => READ FSM
        // - SC     : An SC request has a length of 2 flits. First flit contains the
        //            acces key, second flit the data to write => WRITE FSM.
        //
        // The READ/WRITE commands accepted for the CONFIG segment are targeting
        // configuration or status registers. They must contain one single flit.
        // - For almost all addressable registers, the response is returned immediately.
        // - For MEMC_CMD_TYPE, the response is delayed until the operation is completed.
        ////////////////////////////////////////////////////////////////////////////////////


        switch (r_tgt_cmd_fsm.read())
        {
            //////////////////
            case TGT_CMD_IDLE:     // waiting a VCI command (RAM or CONFIG)
                if (p_vci_tgt.cmdval)
                {
#if DEBUG_MEMC_TGT_CMD
if (m_debug)
std::cout << "  <MEMC " << name()
          << " TGT_CMD_IDLE> Receive command from srcid "
          << std::hex << p_vci_tgt.srcid.read()
          << " / address " << std::hex << p_vci_tgt.address.read() << std::endl;
#endif
                    // checking segmentation violation
                    addr_t   address = p_vci_tgt.address.read();
                    uint32_t plen    = p_vci_tgt.plen.read();
                    bool     config  = false;

                    for (size_t seg_id = 0; (seg_id < m_nseg) ; seg_id++)
                    {
                        if (m_seg[seg_id]->contains(address) &&
                                m_seg[seg_id]->contains(address + plen - vci_param_int::B))
                        {
                            if (m_seg[seg_id]->special()) config = true;
                        }
                    }

                    if (config)     /////////// configuration command
                    {
                        if (!p_vci_tgt.eop.read()) r_tgt_cmd_fsm = TGT_CMD_ERROR;
                        else                       r_tgt_cmd_fsm = TGT_CMD_CONFIG;
                    }
                    else           //////////// memory access
                    {
                        if (p_vci_tgt.cmd.read() == vci_param_int::CMD_READ)
                        {
                            // check that the pktid is either :
                            // TYPE_READ_DATA_UNC
                            // TYPE_READ_DATA_MISS
                            // TYPE_READ_INS_UNC
                            // TYPE_READ_INS_MISS
                            // ==> bit2 must be zero with the TSAR encoding
                            // ==> mask = 0b0100 = 0x4
                            assert(((p_vci_tgt.pktid.read() & 0x4) == 0x0) and
                            "The type specified in the pktid field is incompatible with the READ CMD");
                            r_tgt_cmd_fsm = TGT_CMD_READ;
                        }
                        else if (p_vci_tgt.cmd.read() == vci_param_int::CMD_WRITE)
                        {
                            // check that the pktid is TYPE_WRITE
                            // ==> TYPE_WRITE = X100 with the TSAR encoding
                            // ==> mask = 0b0111 = 0x7
                            assert((((p_vci_tgt.pktid.read() & 0x7) == 0x4)  or 
                                  ((p_vci_tgt.pktid.read() & 0x7) == 0x0)) and
                            "The type specified in the pktid field is incompatible with the WRITE CMD");
                            r_tgt_cmd_fsm = TGT_CMD_WRITE;
                        }
                        else if (p_vci_tgt.cmd.read() == vci_param_int::CMD_LOCKED_READ)
                        {
                            // check that the pktid is TYPE_LL
                            // ==> TYPE_LL = X110 with the TSAR encoding
                            // ==> mask = 0b0111 = 0x7
                            assert(((p_vci_tgt.pktid.read() & 0x7) == 0x6) and
                            "The type specified in the pktid field is incompatible with the LL CMD");
                            r_tgt_cmd_fsm = TGT_CMD_READ;
                        }
                        else if (p_vci_tgt.cmd.read() == vci_param_int::CMD_NOP)
                        {
                            // check that the pktid is either :
                            // TYPE_CAS
                            // TYPE_SC
                            // ==> TYPE_CAS = X101 with the TSAR encoding
                            // ==> TYPE_SC  = X111 with the TSAR encoding
                            // ==> mask = 0b0101 = 0x5
                            assert(((p_vci_tgt.pktid.read() & 0x5) == 0x5) and
                            "The type specified in the pktid field is incompatible with the NOP CMD");

                            if ((p_vci_tgt.pktid.read() & 0x7) == TYPE_CAS)
                            {
                                r_tgt_cmd_fsm = TGT_CMD_CAS;
                            }
                            else
                            {
                                r_tgt_cmd_fsm = TGT_CMD_WRITE;
                            }
                        }
                        else
                        {
                            r_tgt_cmd_fsm = TGT_CMD_ERROR;
                        }
                    }
                }
                break;

            ///////////////////
            case TGT_CMD_ERROR:  // response error must be sent

                // wait if pending request
                if (r_tgt_cmd_to_tgt_rsp_req.read()) break;

                // consume all the command packet flits before sending response error
                if (p_vci_tgt.cmdval and p_vci_tgt.eop)
                {
                    r_tgt_cmd_to_tgt_rsp_srcid = p_vci_tgt.srcid.read();
                    r_tgt_cmd_to_tgt_rsp_trdid = p_vci_tgt.trdid.read();
                    r_tgt_cmd_to_tgt_rsp_pktid = p_vci_tgt.pktid.read();
                    r_tgt_cmd_to_tgt_rsp_req   = true;
                    r_tgt_cmd_to_tgt_rsp_error = 1;
                    r_tgt_cmd_fsm              = TGT_CMD_IDLE;

#if DEBUG_MEMC_TGT_CMD
if (m_debug)
std::cout << "  <MEMC " << name()
          << " TGT_CMD_ERROR> Segmentation violation:"
          << " address = " << std::hex << p_vci_tgt.address.read()
          << " / srcid = " << p_vci_tgt.srcid.read()
          << " / trdid = " << p_vci_tgt.trdid.read()
          << " / pktid = " << p_vci_tgt.pktid.read()
          << " / plen = " << std::dec << p_vci_tgt.plen.read() << std::endl;
#endif
                }
                break;

            ////////////////////
            case TGT_CMD_CONFIG:    // execute config request and return response
            {
                ///////////////////////////////////////////////////////////
                //  Decoding CONFIG interface commands                   //
                //                                                       //
                //  VCI ADDRESS                                          //
                //  ================================================     //
                //  GLOBAL | LOCAL | ... | FUNC_IDX | REGS_IDX | 00      //
                //   IDX   |  IDX  |     | (3 bits) | (7 bits) |         //
                //  ================================================     //
                //                                                       //
                //  For instrumentation : FUNC_IDX = 0b001               //
                //                                                       //
                //  REGS_IDX                                             //
                //  ============================================         //
                //       Z     |    Y      |    X     |   W              //
                //    (1 bit)  | (2 bits)  | (3 bits) | (1 bit)          //
                //  ============================================         //
                //                                                       //
                //  Z : DIRECT / COHERENCE                               //
                //  Y : SUBTYPE (LOCAL, REMOTE, OTHER)                   //
                //  X : REGISTER INDEX                                   //
                //  W : HI / LO                                          //
                //                                                       //
                //  For configuration: FUNC_IDX = 0b000                  //
                //                                                       //
                //  REGS_IDX                                             //
                //  ============================================         //
                //             RESERVED             |    X     |         //
                //             (4 bits)             | (3 bits) |         //
                //  ============================================         //
                //                                                       //
                //  X : REGISTER INDEX                                   //
                //                                                       //
                //  For WRITE MISS error signaling: FUNC = 0x010         //
                //                                                       //
                //  REGS_IDX                                             //
                //  ============================================         //
                //             RESERVED             |    X     |         //
                //             (4 bits)             | (3 bits) |         //
                //  ============================================         //
                //                                                       //
                //  X : REGISTER INDEX                                   //
                //                                                       //
                ///////////////////////////////////////////////////////////

                addr_t addr_lsb = p_vci_tgt.address.read() & m_config_addr_mask;
                addr_t cell     = (addr_lsb / vci_param_int::B);
                size_t regr     = cell & m_config_regr_idx_mask;
                size_t func     = (cell >> m_config_regr_width) & m_config_func_idx_mask;

                bool     need_rsp;
                int      error;
                uint32_t rdata = 0; // default value
                uint32_t wdata = p_vci_tgt.wdata.read();

                switch (func)
                {
                    // memory operation
                    case MEMC_CONFIG:
                    {
                        if ((p_vci_tgt.cmd.read() == vci_param_int::CMD_WRITE)   // set addr_lo
                                and (regr == MEMC_ADDR_LO))
                        {
                            assert(((wdata % (m_words * vci_param_int::B)) == 0) and
                            "VCI_MEM_CACHE CONFIG ERROR: The buffer must be aligned on a cache line");

                            need_rsp = true;
                            error    = 0;
                            r_config_address = (r_config_address.read() & 0xFFFFFFFF00000000LL) |
                                ((addr_t)wdata);
                        }
                        else if ((p_vci_tgt.cmd.read() == vci_param_int::CMD_WRITE)   // set addr_hi
                                and (regr == MEMC_ADDR_HI))

                        {
                            need_rsp = true;
                            error    = 0;
                            r_config_address = (r_config_address.read() & 0x00000000FFFFFFFFLL) |
                                (((addr_t) wdata) << 32);
                        }
                        else if ((p_vci_tgt.cmd.read() == vci_param_int::CMD_WRITE)   // set buf_lines
                                and (regr == MEMC_BUF_LENGTH))
                        {
                            need_rsp = true;
                            error    = 0;
                            size_t lines = wdata / (m_words << 2);
                            if (wdata % (m_words << 2)) lines++;
                            r_config_cmd_lines = lines;
                            r_config_rsp_lines = 0;
                        }
                        else if ((p_vci_tgt.cmd.read() == vci_param_int::CMD_WRITE)   // set cmd type
                                and (regr == MEMC_CMD_TYPE))
                        {
                            need_rsp     = false;
                            error        = 0;
                            r_config_cmd = wdata;

                            // prepare delayed response from CONFIG FSM
                            r_config_srcid = p_vci_tgt.srcid.read();
                            r_config_trdid = p_vci_tgt.trdid.read();
                            r_config_pktid = p_vci_tgt.pktid.read();
                        }
                        else
                        {
                            need_rsp = true;
                            error    = 1;
                        }

                        break;
                    }

                    // instrumentation registers
                    case MEMC_INSTRM:
                    {
                        need_rsp = true;

                        if (p_vci_tgt.cmd.read() == vci_param_int::CMD_READ)
                        {
                            error = read_instrumentation(regr, rdata);
                        }
                        else
                        {
                            error = 1;
                        }

                        break;
                    }

                    // xram GET bus error registers
                    case MEMC_RERROR:
                    {
                        need_rsp = true;
                        error    = 0;

                        if (p_vci_tgt.cmd.read() == vci_param_int::CMD_WRITE)
                        {
                            switch (regr)
                            {
                                case MEMC_RERROR_IRQ_ENABLE:
                                    r_xram_rsp_rerror_irq_enable =
                                        (p_vci_tgt.wdata.read() != 0);

                                    break;

                                default:
                                    error = 1;
                                    break;
                            }
                        }
                        else if (p_vci_tgt.cmd.read() == vci_param_int::CMD_READ)
                        {
                            switch (regr)
                            {
                                case MEMC_RERROR_SRCID:
                                    rdata = (uint32_t)
                                        r_xram_rsp_rerror_rsrcid.read();
                                    break;

                                case MEMC_RERROR_ADDR_LO:
                                    rdata = (uint32_t)
                                        (r_xram_rsp_rerror_address.read()) & ((1ULL << 32) - 1);

                                    break;

                                case MEMC_RERROR_ADDR_HI:
                                    rdata = (uint32_t)
                                        (r_xram_rsp_rerror_address.read() >> 32) & ((1ULL << 32) - 1);
                                    break;

                                case MEMC_RERROR_IRQ_RESET:
                                    if (not r_xram_rsp_rerror_irq.read()) break;
                                    r_xram_rsp_rerror_irq = false;
                                    break;

                                case MEMC_RERROR_IRQ_ENABLE:
                                    rdata = (uint32_t) (r_xram_rsp_rerror_irq_enable.read()) ? 1 : 0;
                                    break;

                                default:
                                    error = 1;
                                    break;
                            }
                        }
                        else
                        {
                            error = 1;
                        }

                        break;
                    }

                    //unknown function
                    default:
                    {
                        need_rsp = true;
                        error = 1;
                        break;
                    }
                }

                if (need_rsp)
                {
                    // blocked if previous pending request to TGT_RSP FSM
                    if (r_tgt_cmd_to_tgt_rsp_req.read()) break;

                    r_tgt_cmd_to_tgt_rsp_srcid = p_vci_tgt.srcid.read();
                    r_tgt_cmd_to_tgt_rsp_trdid = p_vci_tgt.trdid.read();
                    r_tgt_cmd_to_tgt_rsp_pktid = p_vci_tgt.pktid.read();
                    r_tgt_cmd_to_tgt_rsp_req   = true;
                    r_tgt_cmd_to_tgt_rsp_error = error;
                    r_tgt_cmd_to_tgt_rsp_rdata = rdata;
                }

                r_tgt_cmd_fsm = TGT_CMD_IDLE;

#if DEBUG_MEMC_TGT_CMD
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " TGT_CMD_CONFIG> Configuration request:"
                        << " address = " << std::hex << p_vci_tgt.address.read()
                        << " / func = " << func
                        << " / regr = " << regr
                        << " / rdata = " << rdata
                        << " / wdata = " << p_vci_tgt.wdata.read()
                        << " / need_rsp = " << need_rsp
                        << " / error = " << error << std::endl;
                }
#endif
                break;
            }
            //////////////////
            case TGT_CMD_READ:    // Push a read request into read fifo

                // check that the read does not cross a cache line limit.
                if (((m_x[(addr_t) p_vci_tgt.address.read()] + (p_vci_tgt.plen.read() >> 2)) > 16) and
                        (p_vci_tgt.cmd.read() != vci_param_int::CMD_LOCKED_READ))
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " TGT_CMD_READ state"
                        << " illegal address/plen for VCI read command" << std::endl;
                    exit(0);
                }
                // check single flit
                if (!p_vci_tgt.eop.read())
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " TGT_CMD_READ state"
                        << " read command packet must contain one single flit" << std::endl;
                    exit(0);
                }
                // check plen for LL
                if ((p_vci_tgt.cmd.read() == vci_param_int::CMD_LOCKED_READ) and
                        (p_vci_tgt.plen.read() != 8))
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " TGT_CMD_READ state"
                        << " ll command packets must have a plen of 8" << std::endl;
                    exit(0);
                }

                if (p_vci_tgt.cmdval and m_cmd_read_addr_fifo.wok())
                {

#if DEBUG_MEMC_TGT_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " TGT_CMD_READ> Push into read_fifo:"
                            << " address = " << std::hex << p_vci_tgt.address.read()
                            << " / srcid = " << p_vci_tgt.srcid.read()
                            << " / trdid = " << p_vci_tgt.trdid.read()
                            << " / pktid = " << p_vci_tgt.pktid.read()
                            << " / plen = " << std::dec << p_vci_tgt.plen.read() << std::endl;
                    }
#endif
                    cmd_read_fifo_put = true;
                    // <Activity counters>
                    if (p_vci_tgt.cmd.read() == vci_param_int::CMD_LOCKED_READ)
                    {
                        if (is_local_req(p_vci_tgt.srcid.read()))
                        {
                            m_cpt_ll_local++;
                        }
                        else
                        {
                            m_cpt_ll_remote++;
                        }
                        // (1 (CMD) + 2 (RSP)) VCI flits for LL => 2 + 3 dspin flits
                        m_cpt_ll_cost += 5 * req_distance(p_vci_tgt.srcid.read());
                    }
                    else
                    {
                        if (is_local_req(p_vci_tgt.srcid.read()))
                        {
                            m_cpt_read_local++;
                        }
                        else
                        {
                            m_cpt_read_remote++;
                        }
                        // (1 (CMD) + m_words (RSP)) flits VCI => 2 + m_words + 1 flits dspin
                        m_cpt_read_cost += (3 + m_words) * req_distance(p_vci_tgt.srcid.read());
                    }
                    // </Activity counters>
                    r_tgt_cmd_fsm = TGT_CMD_IDLE;
                }
                break;

                ///////////////////
            case TGT_CMD_WRITE:
                if (p_vci_tgt.cmdval and m_cmd_write_addr_fifo.wok())
                {
                    uint32_t plen = p_vci_tgt.plen.read();
#if DEBUG_MEMC_TGT_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " TGT_CMD_WRITE> Push into write_fifo:"
                            << " address = " << std::hex << p_vci_tgt.address.read()
                            << " / srcid = " << p_vci_tgt.srcid.read()
                            << " / trdid = " << p_vci_tgt.trdid.read()
                            << " / pktid = " << p_vci_tgt.pktid.read()
                            << " / wdata = " << p_vci_tgt.wdata.read()
                            << " / be = " << p_vci_tgt.be.read()
                            << " / plen = " << std::dec << p_vci_tgt.plen.read() << std::endl;
                    }
#endif
                    cmd_write_fifo_put = true;
                    // <Activity counters>
                    if (p_vci_tgt.cmd.read() != vci_param_int::CMD_NOP)
                    {
                        if (is_local_req(p_vci_tgt.srcid.read()))
                        {
                            m_cpt_write_flits_local++;
                        }
                        else
                        {
                            m_cpt_write_flits_remote++;
                        }
                    }
                    // </Activity counters>

                    if (p_vci_tgt.eop)
                    {
                        // <Activity counters>
                        if (p_vci_tgt.cmd.read() == vci_param_int::CMD_NOP)
                        {
                            // SC
                            // (2 (CMD) + 1 (RSP)) flits VCI => 4 + (1 (success) || 2 (failure)) flits dspin
                            m_cpt_sc_cost += 5 * req_distance(p_vci_tgt.srcid.read());
                            if (is_local_req(p_vci_tgt.srcid.read()))
                            {
                                m_cpt_sc_local++;
                            }
                            else
                            {
                                m_cpt_sc_remote++;
                            }
                        }
                        else
                        {
                            // Writes
                            // (burst_size + 1 (CMD) + 1 (RSP)) flits VCI => 2 + burst_size + 1 flits dspin
                            m_cpt_write_cost += (3 + (plen >> 2)) * req_distance(p_vci_tgt.srcid.read());

                            if (is_local_req(p_vci_tgt.srcid.read()))
                            {
                                m_cpt_write_local++;
                            }
                            else
                            {
                                m_cpt_write_remote++;
                            }
                        }
                        // </Activity counters>
                        r_tgt_cmd_fsm = TGT_CMD_IDLE;
                    }
                }
                break;

                /////////////////
            case TGT_CMD_CAS:
                if ((p_vci_tgt.plen.read() != 8) and (p_vci_tgt.plen.read() != 16))
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " TGT_CMD_CAS state"
                        << "illegal format for CAS command " << std::endl;
                    exit(0);
                }

                if (p_vci_tgt.cmdval and m_cmd_cas_addr_fifo.wok())
                {

#if DEBUG_MEMC_TGT_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " TGT_CMD_CAS> Pushing command into cmd_cas_fifo:"
                            << " address = " << std::hex << p_vci_tgt.address.read()
                            << " srcid = " << p_vci_tgt.srcid.read()
                            << " trdid = " << p_vci_tgt.trdid.read()
                            << " pktid = " << p_vci_tgt.pktid.read()
                            << " wdata = " << p_vci_tgt.wdata.read()
                            << " be = " << p_vci_tgt.be.read()
                            << " plen = " << std::dec << p_vci_tgt.plen.read() << std::endl;
                    }
#endif
                    cmd_cas_fifo_put = true;
                    if (p_vci_tgt.eop)
                    {
                        // <Activity counters>
                        if (is_local_req(p_vci_tgt.srcid.read()))
                        {
                            m_cpt_cas_local++;
                        }
                        else
                        {
                            m_cpt_cas_remote++;
                        }
                        // (2 (CMD) + 1 (RSP)) flits VCI => 4 + (1 (success) || 2 (failure)) flits dspin
                        m_cpt_cas_cost += 5 * req_distance(p_vci_tgt.srcid.read());
                        // </Activity counters>
                        r_tgt_cmd_fsm = TGT_CMD_IDLE;
                    }
                }
                break;
        } // end switch tgt_cmd_fsm

        /////////////////////////////////////////////////////////////////////////
        //    MULTI_ACK FSM
        /////////////////////////////////////////////////////////////////////////
        // This FSM controls the response to the multicast update requests sent
        // by the memory cache to the L1 caches and update the UPT.
        //
        // - The FSM decrements the proper entry in UPT,
        //   and clear the UPT entry when all responses have been received.
        // - If required, it sends a request to the TGT_RSP FSM to complete
        //   a pending  write transaction.
        //
        // All those multi-ack packets are one flit packet.
        // The index in the UPT is defined in the TRDID field.
        ////////////////////////////////////////////////////////////////////////

        switch (r_multi_ack_fsm.read())
        {
            ////////////////////
            case MULTI_ACK_IDLE:
            {
                bool multi_ack_fifo_rok = m_cc_receive_to_multi_ack_fifo.rok();

                // No CC_RECEIVE FSM request and no WRITE FSM request
                if (not multi_ack_fifo_rok and not r_write_to_multi_ack_req.read())
                    break;

                uint8_t updt_index;

                // handling WRITE FSM request to decrement update table response
                // counter if no CC_RECEIVE FSM request
                if (not multi_ack_fifo_rok)
                {
                    updt_index               = r_write_to_multi_ack_upt_index.read();
                    r_write_to_multi_ack_req = false;
                }
                // Handling CC_RECEIVE FSM request
                else
                {
                    uint64_t flit = m_cc_receive_to_multi_ack_fifo.read();
                    updt_index = DspinDhccpParam::dspin_get(flit,
                            DspinDhccpParam::MULTI_ACK_UPDT_INDEX);

                    cc_receive_to_multi_ack_fifo_get = true;
                }

                assert((updt_index < m_upt.size()) and
                        "VCI_MEM_CACHE ERROR in MULTI_ACK_IDLE : "
                        "index too large for UPT");

                r_multi_ack_upt_index = updt_index;
                r_multi_ack_fsm       = MULTI_ACK_UPT_LOCK;

#if DEBUG_MEMC_MULTI_ACK
                if (m_debug)
                {
                    if (multi_ack_fifo_rok)
                    {
                        std::cout << "  <MEMC " << name()
                            << " MULTI_ACK_IDLE> Response for UPT entry "
                            << (size_t) updt_index << std::endl;
                    }
                    else
                    {
                        std::cout << "  <MEMC " << name()
                            << " MULTI_ACK_IDLE> Write FSM request to decrement UPT entry "
                            << updt_index << std::endl;
                    }
                }
#endif
                break;
            }

            ////////////////////////
            case MULTI_ACK_UPT_LOCK:
            {
                // get lock to the UPDATE table
                if (r_alloc_upt_fsm.read() != ALLOC_UPT_MULTI_ACK) break;

                // decrement the number of expected responses
                size_t count = 0;
                bool valid = m_upt.decrement(r_multi_ack_upt_index.read(), count);

                if (not valid)
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name()
                        << " MULTI_ACK_UPT_LOCK state" << std::endl
                        << "unsuccessful access to decrement the UPT" << std::endl;
                    exit(0);
                }

                if (count == 0)
                {
                    r_multi_ack_fsm = MULTI_ACK_UPT_CLEAR;
                }
                else
                {
                    r_multi_ack_fsm = MULTI_ACK_IDLE;
                }

#if DEBUG_MEMC_MULTI_ACK
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " MULTI_ACK_UPT_LOCK> Decrement the responses counter for UPT:"
                        << " entry = "       << r_multi_ack_upt_index.read()
                        << " / rsp_count = " << std::dec << count << std::endl;
                }
#endif
                break;
            }

            /////////////////////////
            case MULTI_ACK_UPT_CLEAR:   // Clear UPT entry / Test if rsp or ack required
            {
                if (r_alloc_upt_fsm.read() != ALLOC_UPT_MULTI_ACK)
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name()
                        << " MULTI_ACK_UPT_CLEAR state"
                        << " bad UPT allocation" << std::endl;
                    exit(0);
                }

                r_multi_ack_srcid = m_upt.srcid(r_multi_ack_upt_index.read());
                r_multi_ack_trdid = m_upt.trdid(r_multi_ack_upt_index.read());
                r_multi_ack_pktid = m_upt.pktid(r_multi_ack_upt_index.read());
                r_multi_ack_nline = m_upt.nline(r_multi_ack_upt_index.read());
                bool need_rsp     = m_upt.need_rsp(r_multi_ack_upt_index.read());

                // clear the UPT entry
                m_upt.clear(r_multi_ack_upt_index.read());

                if (need_rsp) r_multi_ack_fsm = MULTI_ACK_WRITE_RSP;
                else          r_multi_ack_fsm = MULTI_ACK_IDLE;

#if DEBUG_MEMC_MULTI_ACK
                if (m_debug)
                {
                    std::cout <<  "  <MEMC " << name()
                        << " MULTI_ACK_UPT_CLEAR> Clear UPT entry "
                        << std::dec << r_multi_ack_upt_index.read() << std::endl;
                }
#endif
                break;
            }
            /////////////////////////
            case MULTI_ACK_WRITE_RSP:     // Post a response request to TGT_RSP FSM
            // Wait if pending request
            {
                if (r_multi_ack_to_tgt_rsp_req.read()) break;

                r_multi_ack_to_tgt_rsp_req   = true;
                r_multi_ack_to_tgt_rsp_srcid = r_multi_ack_srcid.read();
                r_multi_ack_to_tgt_rsp_trdid = r_multi_ack_trdid.read();
                r_multi_ack_to_tgt_rsp_pktid = r_multi_ack_pktid.read();
                r_multi_ack_fsm              = MULTI_ACK_IDLE;

#if DEBUG_MEMC_MULTI_ACK
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " MULTI_ACK_WRITE_RSP>"
                        << " Request TGT_RSP FSM to send a response to srcid "
                        << std::hex << r_multi_ack_srcid.read() << std::endl;
                }
#endif
                break;
            }
        } // end switch r_multi_ack_fsm

        ////////////////////////////////////////////////////////////////////////////////////
        //    CONFIG FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The CONFIG FSM handles the L2/L3 coherence requests:
        // The target buffer can have any size, and there is one single command for
        // all cache lines covered by the target buffer.
        // An INVAL or SYNC operation is defined by the following registers:
        // - bool      r_config_cmd        : INVAL / SYNC / NOP
        // - uint64_t  r_config_address    : buffer base address
        // - uint32_t  r_config_cmd_lines  : number of lines to be handled
        // - uint32_t  r_config_rsp_lines  : number of lines not completed
        //
        // For both INVAL and SYNC commands, the CONFIG FSM contains the loop handling
        // all cache lines covered by the buffer. The various lines of a given buffer
        // can be pipelined: the CONFIG FSM does not wait the response for line (n) to send
        // the command for line (n+1). It decrements the r_config_cmd_lines counter until
        // the last request has been registered in TRT (for a SYNC), or in IVT (for an INVAL).
        // The r_config_rsp_lines counter contains the number of expected responses from
        // CLEANUP FSM (inval) or from IXR_RSP FSM (sync). This register is incremented by
        // the CONFIG FSM (each time a transaction is requested), and decremented by the
        // CLEANUP or IXR_RSP FSMs(each time a response is received. As this register can
        // be concurently accessed by those three FSMs, it is implemented as an [incr/decr]
        // counter.
        //
        // - INVAL request:
        //   For each line, it access to the DIR.
        //   In case of miss, it does nothing, and a response is requested to TGT_RSP FSM.
        //   In case of hit, with no copies in L1 caches, the line is invalidated and
        //   a response is requested to TGT_RSP FSM.
        //   If there is copies, a multi-inval, or a broadcast-inval coherence transaction
        //   is launched and registered in UPT. The multi-inval transaction completion
        //   is signaled by the CLEANUP FSM by decrementing the r_config_rsp_lines counter.
        //   The CONFIG INVAL response is sent only when the last line has been invalidated.
        //   There is no PUT transaction to XRAM, even if the invalidated line is dirty...
        //   TODO : The target buffer address must be aligned on a cache line boundary.
        //   This constraint can be released, but it requires to make 2 PUT transactions
        //   for the first and the last line...
        //
        // - SYNC request:
        //   For each line, it access to the DIR.
        //   In case of miss, it does nothing, and a response is requested to TGT_RSP FSM.
        //   In case of hit, a PUT transaction is registered in TRT and a request is sent
        //   to IXR_CMD FSM. The IXR_RSP FSM decrements the r_config_rsp_lines counter
        //   when a PUT response is received.
        //   The CONFIG SYNC response is sent only when the last PUT response is received.
        //
        // From the software point of view, a L2/L3 coherence request is a sequence
        // of 4 atomic accesses in an uncached segment:
        // - Write MEMC_ADDR_LO    : Set the buffer address LSB
        // - Write MEMC_ADDR_HI    : Set the buffer address MSB
        // - Write MEMC_BUF_LENGTH : set buffer length (bytes)
        // - Write MEMC_CMD_TYPE   : launch the actual operation
        ////////////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "config_fsm" << std::endl;

        switch (r_config_fsm.read())
        {
            /////////////////
            case CONFIG_IDLE:  // waiting a config request
            {
                if (r_config_cmd.read() != MEMC_CMD_NOP)
                {
                    r_config_fsm = CONFIG_LOOP;

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_IDLE> Config Request received"
          << " / address = " << std::hex << r_config_address.read()
          << " / lines = " << std::dec << r_config_cmd_lines.read()
          << " / type = " << r_config_cmd.read() << std::endl;
#endif
                }
                break;
            }
            /////////////////
            case CONFIG_LOOP:   // test if last line to be handled
            {
                if (r_config_cmd_lines.read() == 0)
                {
                    r_config_cmd = MEMC_CMD_NOP;
                    r_config_fsm = CONFIG_WAIT;
                }
                else
                {
                    r_config_fsm = CONFIG_DIR_REQ;
                }

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_LOOP>"
          << " / address = " << std::hex << r_config_address.read()
          << " / lines not handled = " << std::dec << r_config_cmd_lines.read()
          << " / command = " << r_config_cmd.read() << std::endl;
#endif
                break;
            }
            /////////////////
            case CONFIG_WAIT:   // wait completion (last response)
            {
                if (r_config_rsp_lines.read() == 0)  // last response received
                {
                    r_config_fsm = CONFIG_RSP;
                }

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_WAIT>"
          << " / lines to do = " << std::dec << r_config_rsp_lines.read() << std::endl;
#endif
                break;
            }
            ////////////////
            case CONFIG_RSP:  // request TGT_RSP FSM to return response
            {
                if (not r_config_to_tgt_rsp_req.read())
                {
                    r_config_to_tgt_rsp_srcid = r_config_srcid.read();
                    r_config_to_tgt_rsp_trdid = r_config_trdid.read();
                    r_config_to_tgt_rsp_pktid = r_config_pktid.read();
                    r_config_to_tgt_rsp_error = false;
                    r_config_to_tgt_rsp_req   = true;
                    r_config_fsm              = CONFIG_IDLE;

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_RSP> Request TGT_RSP FSM to send response:"
          << " error = " << r_config_to_tgt_rsp_error.read()
          << " / rsrcid = " << std::hex << r_config_srcid.read()
          << " / rtrdid = " << std::hex << r_config_trdid.read()
          << " / rpktid = " << std::hex << r_config_pktid.read() << std::endl;
#endif
                }
                break;
            }
            ////////////////////
            case CONFIG_DIR_REQ:  // Request directory lock
            {
                if (r_alloc_dir_fsm.read() == ALLOC_DIR_CONFIG)
                {
                    r_config_fsm = CONFIG_DIR_ACCESS;
                }

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_DIR_REQ>"
          << " Request DIR access" << std::endl;
#endif
                break;
            }
            ///////////////////////
            case CONFIG_DIR_ACCESS:   // Access directory and decode config command
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CONFIG) and
                "MEMC ERROR in CONFIG_DIR_ACCESS state: bad DIR allocation");

                size_t way = 0;
                DirectoryEntry entry = m_cache_directory.read(r_config_address.read(), way);

                r_config_dir_way        = way;
                r_config_dir_copy_inst  = entry.owner.inst;
                r_config_dir_copy_srcid = entry.owner.srcid;
                r_config_dir_is_cnt     = entry.is_cnt;
                r_config_dir_lock       = entry.lock;
                r_config_dir_count      = entry.count;
                r_config_dir_ptr        = entry.ptr;

                if (entry.valid and   // hit & inval command
                   (r_config_cmd.read() == MEMC_CMD_INVAL))
                {
                    r_config_fsm = CONFIG_IVT_LOCK;
                }
                else if (entry.valid and  // hit & sync command
                         entry.dirty and
                         (r_config_cmd.read() == MEMC_CMD_SYNC))
                {
                    r_config_fsm = CONFIG_TRT_LOCK;
                }
                else    // miss : return to LOOP
                {
                    r_config_cmd_lines = r_config_cmd_lines.read() - 1;
                    r_config_address   = r_config_address.read() + (m_words << 2);
                    r_config_fsm       = CONFIG_LOOP;
                }

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_DIR_ACCESS> Accessing directory: "
          << " address = " << std::hex << r_config_address.read()
          << " / hit = " << std::dec << entry.valid
          << " / dirty = " << entry.dirty
          << " / count = " << entry.count
          << " / is_cnt = " << entry.is_cnt << std::endl;
#endif
                break;
            }
            /////////////////////
            case CONFIG_TRT_LOCK:      // enter this state in case of SYNC command
                                       // to a dirty cache line
                                       // keep DIR lock, and try to get TRT lock
                                       // return to LOOP state if TRT full
                                       // reset dirty bit in DIR and register a PUT
                                       // transaction in TRT if not full.
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CONFIG) and
                "MEMC ERROR in CONFIG_TRT_LOCK state: bad DIR allocation");

                if (r_alloc_trt_fsm.read() == ALLOC_TRT_CONFIG)
                {
                    size_t index = 0;
                    bool wok = not m_trt.full(index);

                    if (not wok)
                    {
                        r_config_fsm = CONFIG_LOOP;
                    }
                    else
                    {
                        size_t way = r_config_dir_way.read();
                        size_t set = m_y[r_config_address.read()];

                        // reset dirty bit in DIR
                        DirectoryEntry  entry;
                        entry.valid       = true;
                        entry.dirty       = false;
                        entry.tag         = m_z[r_config_address.read()];
                        entry.is_cnt      = r_config_dir_is_cnt.read();
                        entry.lock        = r_config_dir_lock.read();
                        entry.ptr         = r_config_dir_ptr.read();
                        entry.count       = r_config_dir_count.read();
                        entry.owner.inst  = r_config_dir_copy_inst.read();
                        entry.owner.srcid = r_config_dir_copy_srcid.read();
                        m_cache_directory.write(set, way, entry);

                        r_config_trt_index = index;
                        r_config_fsm       = CONFIG_TRT_SET;
                    }

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_TRT_LOCK> Access TRT: "
          << " wok = " << std::dec << wok
          << " index = " << index << std::endl;
#endif
                }
                break;
            }
            ////////////////////
            case CONFIG_TRT_SET:       // read data in cache
                                       // and post a PUT request in TRT
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CONFIG) and
                "MEMC ERROR in CONFIG_TRT_SET state: bad DIR allocation");

                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_CONFIG) and
                "MEMC ERROR in CONFIG_TRT_SET state: bad TRT allocation");

                // read data into cache
                size_t way = r_config_dir_way.read();
                size_t set = m_y[r_config_address.read()];
                std::vector<data_t> data_vector;
                data_vector.clear();
                for (size_t word = 0; word < m_words; word++)
                {
                    uint32_t data = m_cache_data.read(way, set, word);
                    data_vector.push_back(data);
                }

                // post the PUT request in TRT
                m_trt.set(r_config_trt_index.read(),
                          false,                            // PUT transaction
                          m_nline[r_config_address.read()], // line index
                          0,                                // srcid:       unused
                          0,                                // trdid:       unused
                          0,                                // pktid:       unused
                          false,                            // not proc_read
                          0,                                // read_length: unused
                          0,                                // word_index:  unused
                          std::vector<be_t>(m_words, 0xF),  // byte-enable: unused
                          data_vector,                      // data to be written
                          0,                                // ll_key:      unused
                          true);                            // requested by config FSM
                config_rsp_lines_incr = true;
                r_config_fsm          = CONFIG_PUT_REQ;

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_TRT_SET> PUT request in TRT:"
          << " address = " << std::hex << r_config_address.read()
          << " index = " << std::dec << r_config_trt_index.read() << std::endl;
#endif
                break;
            }
            ////////////////////
            case CONFIG_PUT_REQ:       // post PUT request to IXR_CMD_FSM
            {
                if (not r_config_to_ixr_cmd_req.read())
                {
                    r_config_to_ixr_cmd_req   = true;
                    r_config_to_ixr_cmd_index = r_config_trt_index.read();

                    // prepare next iteration
                    r_config_cmd_lines = r_config_cmd_lines.read() - 1;
                    r_config_address   = r_config_address.read() + (m_words << 2);
                    r_config_fsm       = CONFIG_LOOP;

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_PUT_REQ> post PUT request to IXR_CMD_FSM"
          << " / address = " << std::hex << r_config_address.read() << std::endl;
#endif
                }
                break;
            }
            /////////////////////
            case CONFIG_IVT_LOCK:  // enter this state in case of INVAL command
                                   // Keep DIR lock and Try to get IVT lock.
                                   // Return to LOOP state if IVT full.
                                   // Register inval in IVT, and invalidate the
                                   // directory if IVT not full.
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CONFIG) and
                "MEMC ERROR in CONFIG_IVT_LOCK state: bad DIR allocation");

                if (r_alloc_ivt_fsm.read() == ALLOC_IVT_CONFIG)
                {
                    size_t set = m_y[(addr_t) (r_config_address.read())];
                    size_t way = r_config_dir_way.read();

                    if (r_config_dir_count.read() == 0)  // inval DIR and return to LOOP
                    {
                        m_cache_directory.inval(way, set);
                        r_config_cmd_lines = r_config_cmd_lines.read() - 1;
                        r_config_address   = r_config_address.read() + (m_words << 2);
                        r_config_fsm       = CONFIG_LOOP;

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_IVT_LOCK>"
          << " No copies in L1 : inval DIR entry"  << std::endl;
#endif
                    }
                    else    // try to register inval in IVT
                    {
                        bool   wok       = false;
                        size_t index     = 0;
                        bool   broadcast = r_config_dir_is_cnt.read();
                        size_t srcid     = r_config_srcid.read();
                        size_t trdid     = r_config_trdid.read();
                        size_t pktid     = r_config_pktid.read();
                        addr_t nline     = m_nline[(addr_t) (r_config_address.read())];
                        size_t nb_copies = r_config_dir_count.read();

                        wok = m_ivt.set(false,       // it's an inval transaction
                                        broadcast,
                                        false,       // no response required
                                        true,        // acknowledge required
                                        srcid,
                                        trdid,
                                        pktid,
                                        nline,
                                        nb_copies,
                                        index);

                        if (wok)  // IVT success => inval DIR slot
                        {
                            m_cache_directory.inval(way, set);
                            r_config_ivt_index = index;
                            config_rsp_lines_incr = true;
                            if (broadcast)
                            {
                                r_config_fsm = CONFIG_BC_SEND;
                            }
                            else
                            {
                                r_config_fsm = CONFIG_INVAL_SEND;
                            }

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_IVT_LOCK>"
          << " Inval DIR entry and register inval in IVT"
          << " / index = " << std::dec << index
          << " / broadcast = " << broadcast << std::endl;
#endif
                        }
                        else       // IVT full => release both DIR and IVT locks
                        {
                            r_config_fsm = CONFIG_LOOP;

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_IVT_LOCK>"
          << " IVT full : release DIR & IVT locks and retry" << std::endl;
#endif
                        }
                    }
                }
                break;
            }
            ////////////////////
            case CONFIG_BC_SEND:    // Post a broadcast inval request to CC_SEND FSM
            {
                if (not r_config_to_cc_send_multi_req.read() and
                    not r_config_to_cc_send_brdcast_req.read())
                {
                    // post bc inval request
                    r_config_to_cc_send_multi_req = false;
                    r_config_to_cc_send_brdcast_req = true;
                    r_config_to_cc_send_trdid = r_config_ivt_index.read();
                    r_config_to_cc_send_nline = m_nline[(addr_t)(r_config_address.read())];

                    // prepare next iteration
                    r_config_cmd_lines = r_config_cmd_lines.read() - 1;
                    r_config_address   = r_config_address.read() + (m_words << 2);
                    r_config_fsm       = CONFIG_LOOP;

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_BC_SEND>"
          << " Post a broadcast inval request to CC_SEND FSM"
          << " / address = " << r_config_address.read() <<std::endl;
#endif
                }
                break;
            }
            ///////////////////////
            case CONFIG_INVAL_SEND:    // Post a multi inval request to CC_SEND FSM
            {
                if (not r_config_to_cc_send_multi_req.read() and
                    not r_config_to_cc_send_brdcast_req.read())
                {
                    // post first copy into FIFO
                    config_to_cc_send_fifo_srcid = r_config_dir_copy_srcid.read();
                    config_to_cc_send_fifo_inst  = r_config_dir_copy_inst.read();
                    config_to_cc_send_fifo_put   = true;

                    if (r_config_dir_count.read() == 1)  // only one copy
                    {
                        // post multi inval request
                        r_config_to_cc_send_multi_req = true;
                        r_config_to_cc_send_brdcast_req = false;
                        r_config_to_cc_send_trdid = r_config_ivt_index.read();
                        r_config_to_cc_send_nline = m_nline[(addr_t)(r_config_address.read())];

                        // prepare next iteration (next line to be invalidated)
                        r_config_cmd_lines = r_config_cmd_lines.read() - 1;
                        r_config_address = r_config_address.read() + (m_words << 2);
                        r_config_fsm = CONFIG_LOOP;
                    }
                    else                                   // several copies : must use heap
                    {
                        r_config_fsm = CONFIG_HEAP_REQ;
                    }

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_INVAL_SEND>"
          << " Post multi inval request to CC_SEND FSM"
          << " / address = " << std::hex << r_config_address.read()
          << " / copy = " << r_config_dir_copy_srcid.read()
          << " / inst = " << std::dec << r_config_dir_copy_inst.read() << std::endl;
#endif
                }
                break;
            }
            /////////////////////
            case CONFIG_HEAP_REQ:  // Try to get access to Heap
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_CONFIG)
                {
                    r_config_fsm = CONFIG_HEAP_SCAN;
                    r_config_heap_next = r_config_dir_ptr.read();
                }

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_HEAP_REQ>"
          << " Requesting HEAP lock" << std::endl;
#endif
                break;
            }
            //////////////////////
            case CONFIG_HEAP_SCAN: // scan HEAP and send inval to CC_SEND FSM
            {
                HeapEntry entry = m_heap.read(r_config_heap_next.read());
                bool last_copy = (entry.next == r_config_heap_next.read());

                // post one more copy into fifo
                config_to_cc_send_fifo_srcid = entry.owner.srcid;
                config_to_cc_send_fifo_inst = entry.owner.inst;
                config_to_cc_send_fifo_put = true;

                assert ( (m_config_to_cc_send_inst_fifo.wok()) and
                "MEMC ERROR in CONFIG_HEAP_SCAN: The m_config_to_cc_send fifo should never overflow");

                r_config_heap_next = entry.next;
                if (last_copy) 
                {
                    // post multi inval request
                    r_config_to_cc_send_multi_req = true;
                    r_config_to_cc_send_brdcast_req = false;
                    r_config_to_cc_send_trdid = r_config_ivt_index.read();
                    r_config_to_cc_send_nline = m_nline[(addr_t)(r_config_address.read())];

                    // prepare next iteration (next line to be invalidated)
                    r_config_cmd_lines = r_config_cmd_lines.read() - 1;
                    r_config_address = r_config_address.read() + (m_words << 2);
                    r_config_fsm = CONFIG_HEAP_LAST;
                }
#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_HEAP_SCAN>"
          << " Post multi inval request to CC_SEND FSM"
          << " / address = " << std::hex << r_config_address.read()
          << " / copy = " << entry.owner.srcid
          << " / inst = " << std::dec << entry.owner.inst << std::endl;
#endif
                break;
            }
            //////////////////////
            case CONFIG_HEAP_LAST:  // HEAP housekeeping
            {
                size_t free_pointer = m_heap.next_free_ptr();
                HeapEntry last_entry;
                last_entry.owner.srcid = 0;
                last_entry.owner.inst  = false;

                if (m_heap.is_full())
                {
                    last_entry.next = r_config_dir_ptr.read();
                    m_heap.unset_full();
                }
                else
                {
                   last_entry.next = free_pointer;
                }

                m_heap.write_free_ptr(r_config_dir_ptr.read());
                m_heap.write(r_config_heap_next.read(), last_entry);

                r_config_fsm       = CONFIG_LOOP;

#if DEBUG_MEMC_CONFIG
if (m_debug)
std::cout << "  <MEMC " << name() << " CONFIG_HEAP_LAST>"
          << " Heap housekeeping" << std::endl;
#endif
                break;
            }
        }  // end switch r_config_fsm



        ////////////////////////////////////////////////////////////////////////////////////
        //    READ FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The READ FSM controls the VCI read  and ll requests.
        // It takes the lock protecting the cache directory to check the cache line status:
        // - In case of HIT
        //   The fsm copies the data (one line, or one single word)
        //   in the r_read_to_tgt_rsp buffer. It waits if this buffer is not empty.
        //   The requesting initiator is registered in the cache directory.
        //   If the number of copy is larger than 1, the new copy is registered
        //   in the HEAP.
        //   If the number of copy is larger than the threshold, the HEAP is cleared,
        //   and the corresponding line switches to the counter mode.
        // - In case of MISS
        //   The READ fsm takes the lock protecting the transaction tab.
        //   If a read transaction to the XRAM for this line already exists,
        //   or if the transaction tab is full, the fsm is stalled.
        //   If a TRT entry is free, the READ request is registered in TRT,
        //   it is consumed in the request FIFO, and transmited to the IXR_CMD FSM.
        //   The READ FSM returns in the IDLE state as the read transaction will be
        //   completed when the missing line will be received.
        ////////////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "read_fsm" << std::endl;

        switch(r_read_fsm.read())
        {
            ///////////////
            case READ_IDLE:  // waiting a read request
            {
                if (m_cmd_read_addr_fifo.rok())
                {

#if DEBUG_MEMC_READ
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " READ_IDLE> Read request"
                            << " : address = " << std::hex << m_cmd_read_addr_fifo.read()
                            << " / srcid = " << m_cmd_read_srcid_fifo.read()
                            << " / trdid = " << m_cmd_read_trdid_fifo.read()
                            << " / pktid = " << m_cmd_read_pktid_fifo.read()
                            << " / nwords = " << std::dec << m_cmd_read_length_fifo.read() << std::endl;
                    }
#endif
                    r_read_fsm = READ_DIR_REQ;
                }
                break;
            }
            //////////////////
            case READ_DIR_REQ:  // Get the lock to the directory
            {
                if (r_alloc_dir_fsm.read() == ALLOC_DIR_READ)
                {
                    r_read_fsm = READ_DIR_LOCK;
                }

#if DEBUG_MEMC_READ
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " READ_DIR_REQ> Requesting DIR lock " << std::endl;
                }
#endif
                break;
            }

            ///////////////////
            case READ_DIR_LOCK:  // check directory for hit / miss
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_READ) and
                        "MEMC ERROR in READ_DIR_LOCK state: Bad DIR allocation");

                size_t way = 0;
                DirectoryEntry entry = m_cache_directory.read(m_cmd_read_addr_fifo.read(), way);

                // access the global table ONLY when we have an LL cmd
                if ((m_cmd_read_pktid_fifo.read() & 0x7) == TYPE_LL)
                {
                    r_read_ll_key   = m_llsc_table.ll(m_cmd_read_addr_fifo.read());
                }
                r_read_is_cnt    = entry.is_cnt;
                r_read_dirty     = entry.dirty;
                r_read_lock      = entry.lock;
                r_read_tag       = entry.tag;
                r_read_way       = way;
                r_read_count     = entry.count;
                r_read_copy      = entry.owner.srcid;
                r_read_copy_inst = entry.owner.inst;
                r_read_ptr       = entry.ptr; // pointer to the heap

                // check if this is a cached read, this means pktid is either
                // TYPE_READ_DATA_MISS 0bX001 with TSAR encoding
                // TYPE_READ_INS_MISS  0bX011 with TSAR encoding
                bool cached_read = (m_cmd_read_pktid_fifo.read() & 0x1);
                if (entry.valid)    // hit
                {
                    // test if we need to register a new copy in the heap
                    if (entry.is_cnt or (entry.count == 0) or !cached_read)
                    {
                        r_read_fsm = READ_DIR_HIT;
                    }
                    else
                    {
                        r_read_fsm = READ_HEAP_REQ;
                    }
                }
                else      // miss
                {
                    r_read_fsm = READ_TRT_LOCK;
                }

#if DEBUG_MEMC_READ
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " READ_DIR_LOCK> Accessing directory: "
                        << " address = " << std::hex << m_cmd_read_addr_fifo.read()
                        << " / hit = " << std::dec << entry.valid
                        << " / count = " <<std::dec << entry.count
                        << " / is_cnt = " << entry.is_cnt;
                    if ((m_cmd_read_pktid_fifo.read() & 0x7) == TYPE_LL)
                    {
                        std::cout << " / LL access" << std::endl;
                    }
                    else
                    {
                        std::cout << std::endl;
                    }
                }
#endif
                break;
            }
            //////////////////
            case READ_DIR_HIT:    //  read data in cache & update the directory
            //  we enter this state in 3 cases:
            //  - the read request is uncachable
            //  - the cache line is in counter mode
            //  - the cache line is valid but not replicated
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_READ) and
                        "MEMC ERROR in READ_DIR_HIT state: Bad DIR allocation");

                // check if this is an instruction read, this means pktid is either
                // TYPE_READ_INS_UNC   0bX010 with TSAR encoding
                // TYPE_READ_INS_MISS  0bX011 with TSAR encoding
                bool inst_read = ((m_cmd_read_pktid_fifo.read() & 0x2) != 0);
                // check if this is a cached read, this means pktid is either
                // TYPE_READ_DATA_MISS 0bX001 with TSAR encoding
                // TYPE_READ_INS_MISS  0bX011 with TSAR encoding
                bool cached_read = (m_cmd_read_pktid_fifo.read() & 0x1);
                bool is_cnt      = r_read_is_cnt.read();

                // read data in the cache
                size_t set = m_y[(addr_t)(m_cmd_read_addr_fifo.read())];
                size_t way = r_read_way.read();

                m_cache_data.read_line(way, set, r_read_data);

                // update the cache directory
                DirectoryEntry entry;
                entry.valid  = true;
                entry.is_cnt = is_cnt;
                entry.dirty  = r_read_dirty.read();
                entry.tag    = r_read_tag.read();
                entry.lock   = r_read_lock.read();
                entry.ptr    = r_read_ptr.read();

                if (cached_read) // Cached read => we must update the copies
                {
                    if (!is_cnt)  // Not counter mode
                    {
                        entry.owner.srcid = m_cmd_read_srcid_fifo.read();
                        entry.owner.inst  = inst_read;
                        entry.count       = r_read_count.read() + 1;
                    }
                    else  // Counter mode
                    {
                        entry.owner.srcid = 0;
                        entry.owner.inst  = false;
                        entry.count       = r_read_count.read() + 1;
                    }
                }
                else // Uncached read
                {
                    entry.owner.srcid = r_read_copy.read();
                    entry.owner.inst  = r_read_copy_inst.read();
                    entry.count       = r_read_count.read();
                }

#if DEBUG_MEMC_READ
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " READ_DIR_HIT> Update directory entry:"
                        << " addr = " << std::hex << m_cmd_read_addr_fifo.read()
                        << " / set = " << std::dec << set
                        << " / way = " << way
                        << " / owner_id = " << std::hex << entry.owner.srcid
                        << " / owner_ins = " << std::dec << entry.owner.inst
                        << " / count = " << entry.count
                        << " / is_cnt = " << entry.is_cnt << std::endl;
                }
#endif
                m_cache_directory.write(set, way, entry);
                r_read_fsm = READ_RSP;
                break;
            }
            ///////////////////
            case READ_HEAP_REQ:    // Get the lock to the HEAP directory
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_READ)
                {
                    r_read_fsm = READ_HEAP_LOCK;
                }

#if DEBUG_MEMC_READ
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " READ_HEAP_REQ>"
                        << " Requesting HEAP lock " << std::endl;
                }
#endif
                break;
            }

            ////////////////////
            case READ_HEAP_LOCK:   // read data in cache, update the directory
            // and prepare the HEAP update
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_READ)
                {
                    // enter counter mode when we reach the limit of copies or the heap is full
                    bool go_cnt = (r_read_count.read() >= m_max_copies) or m_heap.is_full();

                    // read data in the cache
                    size_t set = m_y[(addr_t) (m_cmd_read_addr_fifo.read())];
                    size_t way = r_read_way.read();

                    m_cache_data.read_line(way, set, r_read_data);

                    // update the cache directory
                    DirectoryEntry entry;
                    entry.valid  = true;
                    entry.is_cnt = go_cnt;
                    entry.dirty  = r_read_dirty.read();
                    entry.tag    = r_read_tag.read();
                    entry.lock   = r_read_lock.read();
                    entry.count  = r_read_count.read() + 1;

                    if (not go_cnt) // Not entering counter mode
                    {
                        entry.owner.srcid = r_read_copy.read();
                        entry.owner.inst  = r_read_copy_inst.read();
                        entry.ptr         = m_heap.next_free_ptr();   // set pointer on the heap
                    }
                    else    // Entering Counter mode
                    {
                        entry.owner.srcid = 0;
                        entry.owner.inst  = false;
                        entry.ptr         = 0;
                    }

                    m_cache_directory.write(set, way, entry);

                    // prepare the heap update (add an entry, or clear the linked list)
                    if (not go_cnt)      // not switching to counter mode
                    {
                        // We test if the next free entry in the heap is the last
                        HeapEntry heap_entry = m_heap.next_free_entry();
                        r_read_next_ptr      = heap_entry.next;
                        r_read_last_free     = (heap_entry.next == m_heap.next_free_ptr());

                        r_read_fsm = READ_HEAP_WRITE; // add an entry in the HEAP
                    }
                    else // switching to counter mode
                    {
                        if (r_read_count.read() > 1) // heap must be cleared
                        {
                            HeapEntry next_entry = m_heap.read(r_read_ptr.read());
                            r_read_next_ptr      = m_heap.next_free_ptr();
                            m_heap.write_free_ptr(r_read_ptr.read());

                            if (next_entry.next == r_read_ptr.read())    // last entry
                            {
                                r_read_fsm = READ_HEAP_LAST;    // erase the entry
                            }
                            else                                        // not the last entry
                            {
                                r_read_ptr = next_entry.next;
                                r_read_fsm = READ_HEAP_ERASE;   // erase the list
                            }
                        }
                        else  // the heap is not used / nothing to do
                        {
                            r_read_fsm = READ_RSP;
                        }
                    }

#if DEBUG_MEMC_READ
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " READ_HEAP_LOCK> Update directory:"
                            << " tag = " << std::hex << entry.tag
                            << " set = " << std::dec << set
                            << " way = " << way
                            << " count = " << entry.count
                            << " is_cnt = " << entry.is_cnt << std::endl;
                    }
#endif
                }
                else
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " READ_HEAP_LOCK"
                        << "Bad HEAP allocation"   << std::endl;
                    exit(0);
                }
                break;
            }
            /////////////////////
            case READ_HEAP_WRITE:       // add an entry in the heap
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_READ)
                {
                    HeapEntry heap_entry;
                    heap_entry.owner.srcid = m_cmd_read_srcid_fifo.read();
                    heap_entry.owner.inst  = ((m_cmd_read_pktid_fifo.read() & 0x2) != 0);

                    if (r_read_count.read() == 1)  // creation of a new linked list
                    {
                        heap_entry.next = m_heap.next_free_ptr();
                    }
                    else                         // head insertion in existing list
                    {
                        heap_entry.next = r_read_ptr.read();
                    }
                    m_heap.write_free_entry(heap_entry);
                    m_heap.write_free_ptr(r_read_next_ptr.read());
                    if (r_read_last_free.read())
                    {
                        m_heap.set_full();
                    }

                    r_read_fsm = READ_RSP;

#if DEBUG_MEMC_READ
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " READ_HEAP_WRITE> Add an entry in the heap:"
                            << " owner_id = " << std::hex << heap_entry.owner.srcid
                            << " owner_ins = " << std::dec << heap_entry.owner.inst << std::endl;
                    }
#endif
                }
                else
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " READ_HEAP_WRITE"
                        << "Bad HEAP allocation" << std::endl;
                    exit(0);
                }
                break;
            }
            /////////////////////
            case READ_HEAP_ERASE:
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_READ)
                {
                    HeapEntry next_entry = m_heap.read(r_read_ptr.read());
                    if (next_entry.next == r_read_ptr.read())
                    {
                        r_read_fsm = READ_HEAP_LAST;
                    }
                    else
                    {
                        r_read_ptr = next_entry.next;
                        r_read_fsm = READ_HEAP_ERASE;
                    }
                }
                else
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " READ_HEAP_ERASE"
                        << "Bad HEAP allocation" << std::endl;
                    exit(0);
                }
                break;
            }

            ////////////////////
            case READ_HEAP_LAST:
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_READ)
                {
                    HeapEntry last_entry;
                    last_entry.owner.srcid = 0;
                    last_entry.owner.inst  = false;

                    if (m_heap.is_full())
                    {
                        last_entry.next = r_read_ptr.read();
                        m_heap.unset_full();
                    }
                    else
                    {
                        last_entry.next = r_read_next_ptr.read();
                    }
                    m_heap.write(r_read_ptr.read(),last_entry);
                    r_read_fsm = READ_RSP;
                }
                else
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " READ_HEAP_LAST"
                        << "Bad HEAP allocation" << std::endl;
                    exit(0);
                }
                break;
            }
            //////////////
            case READ_RSP:    //  request the TGT_RSP FSM to return data
            {
                if (!r_read_to_tgt_rsp_req)
                {
                    for (size_t i = 0; i < m_words; i++)
                    {
                        r_read_to_tgt_rsp_data[i] = r_read_data[i];
                    }
                    r_read_to_tgt_rsp_word   = m_x[(addr_t) m_cmd_read_addr_fifo.read()];
                    r_read_to_tgt_rsp_length = m_cmd_read_length_fifo.read();
                    r_read_to_tgt_rsp_srcid  = m_cmd_read_srcid_fifo.read();
                    r_read_to_tgt_rsp_trdid  = m_cmd_read_trdid_fifo.read();
                    r_read_to_tgt_rsp_pktid  = m_cmd_read_pktid_fifo.read();
                    r_read_to_tgt_rsp_ll_key = r_read_ll_key.read();
                    cmd_read_fifo_get        = true;
                    r_read_to_tgt_rsp_req    = true;
                    r_read_fsm               = READ_IDLE;

#if DEBUG_MEMC_READ
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " READ_RSP> Request TGT_RSP FSM to return data:"
                            << " rsrcid = " << std::hex << m_cmd_read_srcid_fifo.read()
                            << " / address = " << std::hex << m_cmd_read_addr_fifo.read()
                            << " / nwords = " << std::dec << m_cmd_read_length_fifo.read() << std::endl;
                    }
#endif
                }
                break;
            }
            ///////////////////
            case READ_TRT_LOCK: // read miss : check the Transaction Table
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_READ)
                {
                    size_t index     = 0;
                    addr_t addr      = (addr_t) m_cmd_read_addr_fifo.read();
                    bool   hit_read  = m_trt.hit_read(m_nline[addr], index);
                    bool   hit_write = m_trt.hit_write(m_nline[addr]);
                    bool   wok       = not m_trt.full(index);

                    if (hit_read or !wok or hit_write) // line already requested or no space
                    {
                        if (!wok)                  m_cpt_trt_full++;
                        if (hit_read or hit_write) m_cpt_trt_rb++;
                        r_read_fsm = READ_IDLE;
                    }
                    else // missing line is requested to the XRAM
                    {
                        m_cpt_read_miss++;
                        r_read_trt_index = index;
                        r_read_fsm       = READ_TRT_SET;
                    }

#if DEBUG_MEMC_READ
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " READ_TRT_LOCK> Check TRT:"
                            << " hit_read = " << hit_read
                            << " / hit_write = " << hit_write
                            << " / full = " << !wok << std::endl;
                    }
#endif
                }
                break;
            }
            //////////////////
            case READ_TRT_SET: // register get transaction in TRT
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_READ)
                {
                    m_trt.set(r_read_trt_index.read(),
                            true,      // GET
                            m_nline[(addr_t) (m_cmd_read_addr_fifo.read())],
                            m_cmd_read_srcid_fifo.read(),
                            m_cmd_read_trdid_fifo.read(),
                            m_cmd_read_pktid_fifo.read(),
                            true,      // proc read
                            m_cmd_read_length_fifo.read(),
                            m_x[(addr_t) (m_cmd_read_addr_fifo.read())],
                            std::vector<be_t> (m_words, 0),
                            std::vector<data_t> (m_words, 0),
                            r_read_ll_key.read());
#if DEBUG_MEMC_READ
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " READ_TRT_SET> Set a GET in TRT:"
                            << " address = " << std::hex << m_cmd_read_addr_fifo.read()
                            << " / srcid = " << std::hex << m_cmd_read_srcid_fifo.read() << std::endl;
                    }
#endif
                    r_read_fsm = READ_TRT_REQ;
                }
                break;
            }

            //////////////////
            case READ_TRT_REQ:   // consume the read request in FIFO and send it to IXR_CMD_FSM
            {
                if (not r_read_to_ixr_cmd_req)
                {
                    cmd_read_fifo_get       = true;
                    r_read_to_ixr_cmd_req   = true;
                    r_read_to_ixr_cmd_index = r_read_trt_index.read();
                    r_read_fsm              = READ_IDLE;

#if DEBUG_MEMC_READ
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " READ_TRT_REQ> Request GET transaction for address "
                            << std::hex << m_cmd_read_addr_fifo.read() << std::endl;
                    }
#endif
                }
                break;
            }
        } // end switch read_fsm

        ///////////////////////////////////////////////////////////////////////////////////
        //    WRITE FSM
        ///////////////////////////////////////////////////////////////////////////////////
        // The WRITE FSM handles the write bursts and sc requests sent by the processors.
        // All addresses in a burst must be in the same cache line.
        // A complete write burst is consumed in the FIFO & copied to a local buffer.
        // Then the FSM takes the lock protecting the cache directory, to check
        // if the line is in the cache.
        //
        // - In case of HIT, the cache is updated.
        //   If there is no other copy, an acknowledge response is immediately
        //   returned to the writing processor.
        //   If the data is cached by other processors, a coherence transaction must
        //   be launched (sc requests always require a coherence transaction):
        //   It is a multicast update if the line is not in counter mode: the processor
        //   takes the lock protecting the Update Table (UPT) to register this transaction.
        //   If the UPT is full, it releases the lock(s) and retry. Then, it sends
        //   a multi-update request to all owners of the line (but the writer),
        //   through the CC_SEND FSM. In case of coherence transaction, the WRITE FSM
        //   does not respond to the writing processor, as this response will be sent by
        //   the MULTI_ACK FSM when all update responses have been received.
        //   It is a broadcast invalidate if the line is in counter mode: The line
        //   should be erased in memory cache, and written in XRAM with a PUT transaction,
        //   after registration in TRT.
        //
        // - In case of MISS, the WRITE FSM takes the lock protecting the transaction
        //   table (TRT). If a read transaction to the XRAM for this line already exists,
        //   it writes in the TRT (write buffer). Otherwise, if a TRT entry is free,
        //   the WRITE FSM register a new transaction in TRT, and sends a GET request
        //   to the XRAM. If the TRT is full, it releases the lock, and waits.
        //   Finally, the WRITE FSM returns an aknowledge response to the writing processor.
        /////////////////////////////////////////////////////////////////////////////////////

        switch (r_write_fsm.read())
        {
            ////////////////
            case WRITE_IDLE:  // copy first word of a write burst in local buffer
            {
                if (not m_cmd_write_addr_fifo.rok()) break;

                // consume a word in the FIFO & write it in the local buffer
                cmd_write_fifo_get  = true;
                size_t index        = m_x[(addr_t) (m_cmd_write_addr_fifo.read())];

                r_write_address     = (addr_t) (m_cmd_write_addr_fifo.read());
                r_write_word_index  = index;
                r_write_word_count  = 0;
                r_write_data[index] = m_cmd_write_data_fifo.read();
                r_write_srcid       = m_cmd_write_srcid_fifo.read();
                r_write_trdid       = m_cmd_write_trdid_fifo.read();
                r_write_pktid       = m_cmd_write_pktid_fifo.read();

                // if SC command, get the SC key
                if ((m_cmd_write_pktid_fifo.read() & 0x7) == TYPE_SC)
                {
                    assert(not m_cmd_write_eop_fifo.read() &&
                            "MEMC ERROR in WRITE_IDLE state: "
                            "invalid packet format for SC command");

                    r_write_sc_key = m_cmd_write_data_fifo.read();
                }

                // initialize the be field for all words
                for (size_t word = 0; word < m_words; word++)
                {
                    if (word == index) r_write_be[word] = m_cmd_write_be_fifo.read();
                    else               r_write_be[word] = 0x0;
                }

                if (m_cmd_write_eop_fifo.read())
                {
                    r_write_fsm = WRITE_DIR_REQ;
                }
                else
                {
                    r_write_fsm = WRITE_NEXT;
                }

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " WRITE_IDLE> Write request "
                        << " srcid = " << std::hex << m_cmd_write_srcid_fifo.read()
                        << " / address = " << std::hex << m_cmd_write_addr_fifo.read()
                        << " / data = " << m_cmd_write_data_fifo.read() << std::endl;
                }
#endif
                break;
            }
            ////////////////
            case WRITE_NEXT:  // copy next word of a write burst in local buffer
            {
                if (not m_cmd_write_addr_fifo.rok()) break;

                // check that the next word is in the same cache line
                assert((m_nline[(addr_t)(r_write_address.read())] ==
                            m_nline[(addr_t)(m_cmd_write_addr_fifo.read())]) &&
                        "MEMC ERROR in WRITE_NEXT state: Illegal write burst");

                size_t index = m_x[(addr_t)(m_cmd_write_addr_fifo.read())];
                bool   is_sc = ((m_cmd_write_pktid_fifo.read() & 0x7) == TYPE_SC);

                // check that SC command has constant address
                assert((not is_sc or (index == r_write_word_index)) &&
                        "MEMC ERROR in WRITE_NEXT state: "
                        "the address must be constant on a SC command");

                // check that SC command has two flits
                assert((not is_sc or m_cmd_write_eop_fifo.read()) &&
                        "MEMC ERROR in WRITE_NEXT state: "
                        "invalid packet format for SC command");

                // consume a word in the FIFO & write it in the local buffer
                cmd_write_fifo_get  = true;

                r_write_be[index]   = m_cmd_write_be_fifo.read();
                r_write_data[index] = m_cmd_write_data_fifo.read();

                // the first flit of a SC command is the reservation key and
                // therefore it must not be counted as a data to write
                if (not is_sc)
                {
                    r_write_word_count = r_write_word_count.read() + 1;
                }

                if (m_cmd_write_eop_fifo.read())
                {
                    r_write_fsm = WRITE_DIR_REQ;
                }

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " WRITE_NEXT> Write another word in local buffer"
                        << std::endl;
                }
#endif
                break;
            }
            ///////////////////
            case WRITE_DIR_REQ: // Get the lock to the directory
                                // and access the llsc_global_table
            {
                if (r_alloc_dir_fsm.read() != ALLOC_DIR_WRITE) break;

                if ((r_write_pktid.read() & 0x7) == TYPE_SC)
                {
                    // test address and key match of the SC command on the
                    // LL/SC table without removing reservation. The reservation
                    // will be erased after in this FSM.
                    bool sc_success = m_llsc_table.check(r_write_address.read(),
                            r_write_sc_key.read());

                    r_write_sc_fail = not sc_success;

                    if (not sc_success) r_write_fsm = WRITE_RSP;
                    else                r_write_fsm = WRITE_DIR_LOCK;
                }
                else
                {
                    // write burst
#define L2 soclib::common::uint32_log2
                    addr_t min = r_write_address.read();
                    addr_t max = r_write_address.read() +
                        (r_write_word_count.read() << L2(vci_param_int::B));
#undef L2

                    m_llsc_table.sw(min, max);

                    r_write_fsm = WRITE_DIR_LOCK;
                }

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " WRITE_DIR_REQ> Requesting DIR lock "
                        << std::endl;
                }
#endif
                break;
            }
            ////////////////////
            case WRITE_DIR_LOCK:     // access directory to check hit/miss
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_WRITE) and
                        "MEMC ERROR in ALLOC_DIR_LOCK state: Bad DIR allocation");

                size_t way = 0;
                DirectoryEntry entry(m_cache_directory.read(r_write_address.read(), way));

                if (entry.valid)    // hit
                {
                    // copy directory entry in local buffer in case of hit
                    r_write_is_cnt    = entry.is_cnt;
                    r_write_lock      = entry.lock;
                    r_write_tag       = entry.tag;
                    r_write_copy      = entry.owner.srcid;
                    r_write_copy_inst = entry.owner.inst;
                    r_write_count     = entry.count;
                    r_write_ptr       = entry.ptr;
                    r_write_way       = way;

                    if (entry.is_cnt and entry.count) r_write_fsm = WRITE_BC_DIR_READ;
                    else                              r_write_fsm = WRITE_DIR_HIT;
                }
                else  // miss
                {
                    r_write_fsm = WRITE_MISS_TRT_LOCK;
                }

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " WRITE_DIR_LOCK> Check the directory: "
                        << " address = " << std::hex << r_write_address.read()
                        << " / hit = " << std::dec << entry.valid
                        << " / count = " << entry.count
                        << " / is_cnt = " << entry.is_cnt ;
                    if ((r_write_pktid.read() & 0x7) == TYPE_SC)
                    {
                        std::cout << " / SC access" << std::endl;
                    }
                    else
                    {
                        std::cout << " / SW access" << std::endl;
                    }
                }
#endif
                break;
            }
            ///////////////////
            case WRITE_DIR_HIT:    // update the cache directory with Dirty bit
            // and update data cache
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_WRITE) and
                        "MEMC ERROR in ALLOC_DIR_HIT state: Bad DIR allocation");

                DirectoryEntry entry;
                entry.valid       = true;
                entry.dirty       = true;
                entry.tag         = r_write_tag.read();
                entry.is_cnt      = r_write_is_cnt.read();
                entry.lock        = r_write_lock.read();
                entry.owner.srcid = r_write_copy.read();
                entry.owner.inst  = r_write_copy_inst.read();
                entry.count       = r_write_count.read();
                entry.ptr         = r_write_ptr.read();

                size_t set = m_y[(addr_t) (r_write_address.read())];
                size_t way = r_write_way.read();

                // update directory
                m_cache_directory.write(set, way, entry);

                // owner is true when the  the first registered copy is the writer itself
                bool owner = ((r_write_copy.read() == r_write_srcid.read())
                        and not r_write_copy_inst.read());

                // no_update is true when there is no need for coherence transaction
                bool no_update = ((r_write_count.read() == 0) or
                        (owner and (r_write_count.read() == 1) and
                         ((r_write_pktid.read() & 0x7) != TYPE_SC)));

                // write data in the cache if no coherence transaction
                if (no_update)
                {
                    // SC command but zero copies
                    if ((r_write_pktid.read() & 0x7) == TYPE_SC)
                    {
                        m_llsc_table.sc(r_write_address.read(),
                                r_write_sc_key.read());
                    }

                    for (size_t word = 0; word < m_words; word++)
                    {
                        m_cache_data.write(way,
                                set,
                                word,
                                r_write_data[word].read(),
                                r_write_be[word].read());
                    }
                }

                if (owner and not no_update and ((r_write_pktid.read() & 0x7) != TYPE_SC))
                {
                    r_write_count = r_write_count.read() - 1;
                }

                if (no_update) // Write transaction completed
                {
                    r_write_fsm = WRITE_RSP;
                }
                else // coherence update required
                {
                    if (!r_write_to_cc_send_multi_req.read() and
                            !r_write_to_cc_send_brdcast_req.read())
                    {
                        r_write_fsm = WRITE_UPT_LOCK;
                    }
                    else
                    {
                        r_write_fsm = WRITE_WAIT;
                    }
                }

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    if (no_update)
                    {
                        std::cout << "  <MEMC " << name()
                            << " WRITE_DIR_HIT> Write into cache / No coherence transaction" << std::endl;
                    }
                    else
                    {
                        std::cout << "  <MEMC " << name() << " WRITE_DIR_HIT> Coherence update required:"
                            << " is_cnt = " << r_write_is_cnt.read()
                            << " nb_copies = " << std::dec << r_write_count.read() << std::endl;
                        if (owner)
                        {
                            std::cout << "       ... but the first copy is the writer" << std::endl;
                        }
                    }
                }
#endif
                break;
            }
            ////////////////////
            case WRITE_UPT_LOCK:  // Try to register the update request in UPT
            {
                if (r_alloc_upt_fsm.read() == ALLOC_UPT_WRITE)
                {
                    bool   wok       = false;
                    size_t index     = 0;
                    size_t srcid     = r_write_srcid.read();
                    size_t trdid     = r_write_trdid.read();
                    size_t pktid     = r_write_pktid.read();
                    addr_t nline     = m_nline[(addr_t) (r_write_address.read())];
                    size_t nb_copies = r_write_count.read();
                    size_t set       = m_y[(addr_t) (r_write_address.read())];
                    size_t way       = r_write_way.read();

                    wok = m_upt.set(true,  // it's an update transaction
                            false, // it's not a broadcast
                            true,  // response required
                            false, // no acknowledge required
                            srcid,
                            trdid,
                            pktid,
                            nline,
                            nb_copies,
                            index);

                    if (wok) // write data in cache
                    {
                        if ((r_write_pktid.read() & 0x7) == TYPE_SC)
                        {
                            m_llsc_table.sc(r_write_address.read(),
                                    r_write_sc_key.read());
                        }

                        for (size_t word = 0; word < m_words; word++)
                        {
                            m_cache_data.write(way,
                                    set,
                                    word,
                                    r_write_data[word].read(),
                                    r_write_be[word].read());
                        }
                    }

#if DEBUG_MEMC_WRITE
                    if (m_debug and wok)
                    {
                        std::cout << "  <MEMC " << name()
                            << " WRITE_UPT_LOCK> Register the multicast update in UPT / "
                            << " nb_copies = " << r_write_count.read() << std::endl;
                    }
#endif
                    r_write_upt_index = index;
                    // releases the lock protecting UPT and the DIR if no entry...
                    if (wok) r_write_fsm = WRITE_UPT_HEAP_LOCK;
                    else     r_write_fsm = WRITE_WAIT;
                }
                break;
            }

            /////////////////////////
            case WRITE_UPT_HEAP_LOCK:   // get access to heap
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_WRITE)
                {

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " WRITE_UPT_HEAP_LOCK> Get acces to the HEAP" << std::endl;
                    }
#endif
                    r_write_fsm = WRITE_UPT_REQ;
                }
                break;
            }

            //////////////////
            case WRITE_UPT_REQ:    // prepare the coherence transaction for the CC_SEND FSM
            // and write the first copy in the FIFO
            // send the request if only one copy
            {
                assert(not r_write_to_cc_send_multi_req.read() and
                        not r_write_to_cc_send_brdcast_req.read() and
                        "Error in VCI_MEM_CACHE : pending multicast or broadcast\n"
                        "transaction in WRITE_UPT_REQ state");

                r_write_to_cc_send_brdcast_req = false;
                r_write_to_cc_send_trdid = r_write_upt_index.read();
                r_write_to_cc_send_nline = m_nline[(addr_t)(r_write_address.read())];
                r_write_to_cc_send_index = r_write_word_index.read();
                r_write_to_cc_send_count = r_write_word_count.read();

                for (size_t i = 0; i < m_words; i++)
                {
                    r_write_to_cc_send_be[i] = r_write_be[i].read();
                }

                size_t min = r_write_word_index.read();
                size_t max = r_write_word_index.read() + r_write_word_count.read();
                for (size_t i = min; i <= max; i++)
                {
                    r_write_to_cc_send_data[i] = r_write_data[i];
                }

                if ((r_write_copy.read() != r_write_srcid.read()) or
                        ((r_write_pktid.read() & 0x7) == TYPE_SC) or
                        r_write_copy_inst.read())
                {
                    // put the first srcid in the fifo
                    write_to_cc_send_fifo_put   = true;
                    write_to_cc_send_fifo_inst  = r_write_copy_inst.read();
                    write_to_cc_send_fifo_srcid = r_write_copy.read();
                    if (r_write_count.read() == 1)
                    {
                        r_write_fsm = WRITE_IDLE;
                        r_write_to_cc_send_multi_req = true;
                    }
                    else
                    {
                        r_write_fsm = WRITE_UPT_NEXT;
                        r_write_to_dec = false;

                    }
                }
                else
                {
                    r_write_fsm = WRITE_UPT_NEXT;
                    r_write_to_dec = false;
                }

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout
                        << "  <MEMC " << name()
                        << " WRITE_UPT_REQ> Post first request to CC_SEND FSM"
                        << " / srcid = " << std::dec << r_write_copy.read()
                        << " / inst = "  << std::dec << r_write_copy_inst.read() << std::endl;

                    if (r_write_count.read() == 1)
                    {
                        std::cout << "         ... and this is the last" << std::endl;
                    }
                }
#endif
                break;
            }

            ///////////////////
            case WRITE_UPT_NEXT:
            {
                // continue the multi-update request to CC_SEND fsm
                // when there is copies in the heap.
                // if one copy in the heap is the writer itself
                // the corresponding SRCID should not be written in the fifo,
                // but the UPT counter must be decremented.
                // As this decrement is done in the WRITE_UPT_DEC state,
                // after the last copy has been found, the decrement request
                // must be  registered in the r_write_to_dec flip-flop.

                HeapEntry entry = m_heap.read(r_write_ptr.read());

                bool dec_upt_counter;

                // put the next srcid in the fifo
                if ((entry.owner.srcid != r_write_srcid.read()) or
                        ((r_write_pktid.read() & 0x7) == TYPE_SC) or
                        entry.owner.inst)
                {
                    dec_upt_counter             = false;
                    write_to_cc_send_fifo_put   = true;
                    write_to_cc_send_fifo_inst  = entry.owner.inst;
                    write_to_cc_send_fifo_srcid = entry.owner.srcid;

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " WRITE_UPT_NEXT> Post another request to CC_SEND FSM"
                            << " / heap_index = " << std::dec << r_write_ptr.read()
                            << " / srcid = " << std::dec << r_write_copy.read()
                            << " / inst = "  << std::dec << r_write_copy_inst.read() << std::endl;
                        if (entry.next == r_write_ptr.read())
                        {
                            std::cout << "        ... and this is the last" << std::endl;
                        }
                    }
#endif
                }
                else // the UPT counter must be decremented
                {
                    dec_upt_counter = true;
#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " WRITE_UPT_NEXT> Skip one entry in heap matching the writer"
                            << " / heap_index = " << std::dec << r_write_ptr.read()
                            << " / srcid = " << std::dec << r_write_copy.read()
                            << " / inst = "  << std::dec << r_write_copy_inst.read() << std::endl;
                        if (entry.next == r_write_ptr.read())
                        {
                            std::cout << "        ... and this is the last" << std::endl;
                        }
                    }
#endif
                }

                // register the possible UPT decrement request
                r_write_to_dec = dec_upt_counter or r_write_to_dec.read();

                if (not m_write_to_cc_send_inst_fifo.wok())
                {
                    std::cout << "*** VCI_MEM_CACHE ERROR " << name() << " WRITE_UPT_NEXT state" << std::endl
                        << "The write_to_cc_send_fifo should not be full" << std::endl
                        << "as the depth should be larger than the max number of copies" << std::endl;
                    exit(0);
                }

                r_write_ptr = entry.next;

                if (entry.next == r_write_ptr.read()) // last copy
                {
                    r_write_to_cc_send_multi_req = true;
                    if (r_write_to_dec.read() or dec_upt_counter) r_write_fsm = WRITE_UPT_DEC;
                    else                                          r_write_fsm = WRITE_IDLE;
                }
                break;
            }

            //////////////////
            case WRITE_UPT_DEC:
            {
                // If the initial writer has a copy, it should not
                // receive an update request, but the counter in the
                // update table must be decremented by the MULTI_ACK FSM.

                if (!r_write_to_multi_ack_req.read())
                {
                    r_write_to_multi_ack_req = true;
                    r_write_to_multi_ack_upt_index = r_write_upt_index.read();
                    r_write_fsm = WRITE_IDLE;
                }
                break;
            }

            ///////////////
            case WRITE_RSP:  // Post a request to TGT_RSP FSM to acknowledge the write
            // In order to increase the Write requests throughput,
            // we don't wait to return in the IDLE state to consume
            // a new request in the write FIFO
            {
                if (not r_write_to_tgt_rsp_req.read())
                {
                    // post the request to TGT_RSP_FSM
                    r_write_to_tgt_rsp_req     = true;
                    r_write_to_tgt_rsp_srcid   = r_write_srcid.read();
                    r_write_to_tgt_rsp_trdid   = r_write_trdid.read();
                    r_write_to_tgt_rsp_pktid   = r_write_pktid.read();
                    r_write_to_tgt_rsp_sc_fail = r_write_sc_fail.read();

                    // try to get a new write request from the FIFO
                    if (not m_cmd_write_addr_fifo.rok())
                    {
                        r_write_fsm = WRITE_IDLE;
                    }
                    else
                    {
                        // consume a word in the FIFO & write it in the local buffer
                        cmd_write_fifo_get  = true;
                        size_t index        = m_x[(addr_t) (m_cmd_write_addr_fifo.read())];

                        r_write_address     = (addr_t) (m_cmd_write_addr_fifo.read());
                        r_write_word_index  = index;
                        r_write_word_count  = 0;
                        r_write_data[index] = m_cmd_write_data_fifo.read();
                        r_write_srcid       = m_cmd_write_srcid_fifo.read();
                        r_write_trdid       = m_cmd_write_trdid_fifo.read();
                        r_write_pktid       = m_cmd_write_pktid_fifo.read();

                        // if SC command, get the SC key
                        if ((m_cmd_write_pktid_fifo.read() & 0x7) == TYPE_SC)
                        {
                            assert(not m_cmd_write_eop_fifo.read() &&
                                    "MEMC ERROR in WRITE_RSP state: "
                                    "invalid packet format for SC command");

                            r_write_sc_key = m_cmd_write_data_fifo.read();
                        }

                        // initialize the be field for all words
                        for (size_t word = 0; word < m_words; word++)
                        {
                            if (word == index) r_write_be[word] = m_cmd_write_be_fifo.read();
                            else               r_write_be[word] = 0x0;
                        }

                        if (m_cmd_write_eop_fifo.read())
                        {
                            r_write_fsm = WRITE_DIR_REQ;
                        }
                        else
                        {
                            r_write_fsm = WRITE_NEXT;
                        }
                    }

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " WRITE_RSP> Post a request to TGT_RSP FSM"
                            << " : rsrcid = " << std::hex << r_write_srcid.read() << std::endl;
                        if (m_cmd_write_addr_fifo.rok())
                        {
                            std::cout << "                    New Write request: "
                                << " srcid = " << std::hex << m_cmd_write_srcid_fifo.read()
                                << " / address = " << m_cmd_write_addr_fifo.read()
                                << " / data = " << m_cmd_write_data_fifo.read() << std::endl;
                        }
                    }
#endif
                }
                break;
            }

            /////////////////////////
            case WRITE_MISS_TRT_LOCK: // Miss : check Transaction Table
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_WRITE)
                {

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " WRITE_MISS_TRT_LOCK> Check the TRT" << std::endl;
                    }
#endif
                    size_t hit_index = 0;
                    size_t wok_index = 0;
                    addr_t addr = (addr_t) r_write_address.read();
                    bool   hit_read  = m_trt.hit_read(m_nline[addr], hit_index);
                    bool   hit_write = m_trt.hit_write(m_nline[addr]);
                    bool   wok       = not m_trt.full(wok_index);

                    // wait an empty entry in TRT
                    if (not hit_read and (not wok or hit_write))
                    {
                        r_write_fsm = WRITE_WAIT;
                        m_cpt_trt_full++;

                        break;
                    }

                    if ((r_write_pktid.read() & 0x7) == TYPE_SC)
                    {
                        m_llsc_table.sc(r_write_address.read(),
                                r_write_sc_key.read());
                    }

                    // register the modified data in TRT
                    if (hit_read)
                    {
                        r_write_trt_index = hit_index;
                        r_write_fsm       = WRITE_MISS_TRT_DATA;
                        m_cpt_write_miss++;
                        break;
                    }

                    // set a new entry in TRT
                    if (wok and not hit_write)
                    {
                        r_write_trt_index = wok_index;
                        r_write_fsm       = WRITE_MISS_TRT_SET;
                        m_cpt_write_miss++;
                        break;
                    }

                    assert(false && "VCI_MEM_CACHE ERROR: this part must not be reached");
                }
                break;
            }

            ////////////////
            case WRITE_WAIT:  // release the locks protecting the shared ressources
            {

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " WRITE_WAIT> Releases the locks before retry" << std::endl;
                }
#endif
                r_write_fsm = WRITE_DIR_REQ;
                break;
            }

            ////////////////////////
            case WRITE_MISS_TRT_SET:  // register a new transaction in TRT (Write Buffer)
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_WRITE)
                {
                    std::vector<be_t> be_vector;
                    std::vector<data_t> data_vector;
                    be_vector.clear();
                    data_vector.clear();
                    for (size_t i = 0; i < m_words; i++)
                    {
                        be_vector.push_back(r_write_be[i]);
                        data_vector.push_back(r_write_data[i]);
                    }
                    m_trt.set(r_write_trt_index.read(),
                            true,     // read request to XRAM
                            m_nline[(addr_t)(r_write_address.read())],
                            r_write_srcid.read(),
                            r_write_trdid.read(),
                            r_write_pktid.read(),
                            false,      // not a processor read
                            0,        // not a single word
                            0,            // word index
                            be_vector,
                            data_vector);
                    r_write_fsm = WRITE_MISS_XRAM_REQ;

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " WRITE_MISS_TRT_SET> Set a new entry in TRT" << std::endl;
                    }
#endif
                }
                break;
            }

            /////////////////////////
            case WRITE_MISS_TRT_DATA: // update an entry in TRT (used as a Write Buffer)
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_WRITE)
                {
                    std::vector<be_t> be_vector;
                    std::vector<data_t> data_vector;
                    be_vector.clear();
                    data_vector.clear();
                    for (size_t i = 0; i < m_words; i++)
                    {
                        be_vector.push_back(r_write_be[i]);
                        data_vector.push_back(r_write_data[i]);
                    }
                    m_trt.write_data_mask(r_write_trt_index.read(),
                            be_vector,
                            data_vector);
                    r_write_fsm = WRITE_RSP;

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " WRITE_MISS_TRT_DATA> Modify an existing entry in TRT" << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////////////
            case WRITE_MISS_XRAM_REQ: // send a GET request to IXR_CMD FSM
            {
                if (not r_write_to_ixr_cmd_req.read())
                {
                    r_write_to_ixr_cmd_req   = true;
                    r_write_to_ixr_cmd_index = r_write_trt_index.read();
                    r_write_fsm              = WRITE_RSP;

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " WRITE_MISS_XRAM_REQ> Post a GET request to the"
                            << " IXR_CMD FSM" << std::endl;
                    }
#endif
                }
                break;
            }
            ///////////////////////
            case WRITE_BC_DIR_READ:  // enter this state if a broadcast-inval is required
                                     // the cache line must be erased in mem-cache, and written
                                     // into XRAM.
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_WRITE) and
                        "MEMC ERROR in WRITE_BC_DIR_READ state: Bad DIR allocation");

                // write enable signal for data buffer.
                r_write_bc_data_we = true;

                r_write_fsm = WRITE_BC_TRT_LOCK;

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " WRITE_BC_DIR_READ>"
                        << " Read the cache to complete local buffer" << std::endl;
                }
#endif
                break;
            }
            ///////////////////////
            case WRITE_BC_TRT_LOCK:     // get TRT lock to check TRT not full
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_WRITE) and
                        "MEMC ERROR in WRITE_BC_TRT_LOCK state: Bad DIR allocation");

                // We read the cache and complete the buffer. As the DATA cache uses a
                // synchronous RAM, the read DATA request has been performed in the
                // WRITE_BC_DIR_READ state but the data is available in this state.
                if (r_write_bc_data_we.read())
                {
                    size_t set = m_y[(addr_t) (r_write_address.read())];
                    size_t way = r_write_way.read();
                    for (size_t word = 0; word < m_words ; word++)
                    {
                        data_t mask = 0;
                        if (r_write_be[word].read() & 0x1) mask = mask | 0x000000FF;
                        if (r_write_be[word].read() & 0x2) mask = mask | 0x0000FF00;
                        if (r_write_be[word].read() & 0x4) mask = mask | 0x00FF0000;
                        if (r_write_be[word].read() & 0x8) mask = mask | 0xFF000000;

                        // complete only if mask is not null (for energy consumption)
                        r_write_data[word] =
                            (r_write_data[word].read()         &  mask) |
                            (m_cache_data.read(way, set, word) & ~mask);
                    }
#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC "  << name()
                            << " WRITE_BC_TRT_LOCK> Complete data buffer" << std::endl;
                    }
#endif
                }

                if (r_alloc_trt_fsm.read() != ALLOC_TRT_WRITE)
                {
                    // if we loop in this state, the data does not need to be
                    // rewritten (for energy consuption)
                    r_write_bc_data_we = false;
                    break;
                }

                size_t wok_index = 0;
                bool wok = not m_trt.full(wok_index);
                if (wok)
                {
                    r_write_trt_index = wok_index;
                    r_write_fsm       = WRITE_BC_IVT_LOCK;
                }
                else  // wait an empty slot in TRT
                {
                    r_write_fsm = WRITE_WAIT;
                }

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout << "  <MEMC "  << name()
                        << " WRITE_BC_TRT_LOCK> Check TRT : wok = " << wok
                        << " / index = " << wok_index << std::endl;
                }
#endif
                break;
            }
            //////////////////////
            case WRITE_BC_IVT_LOCK:      // get IVT lock and register BC transaction in IVT
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_WRITE) and
                        "MEMC ERROR in WRITE_BC_IVT_LOCK state: Bad DIR allocation");

                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_WRITE) and
                        "MEMC ERROR in WRITE_BC_IVT_LOCK state: Bad TRT allocation");

                if (r_alloc_ivt_fsm.read() == ALLOC_IVT_WRITE)
                {
                    bool   wok       = false;
                    size_t index     = 0;
                    size_t srcid     = r_write_srcid.read();
                    size_t trdid     = r_write_trdid.read();
                    size_t pktid     = r_write_pktid.read();
                    addr_t nline     = m_nline[(addr_t) (r_write_address.read())];
                    size_t nb_copies = r_write_count.read();

                    wok = m_ivt.set(false,  // it's an inval transaction
                            true,   // it's a broadcast
                            true,   // response required
                            false,  // no acknowledge required
                            srcid,
                            trdid,
                            pktid,
                            nline,
                            nb_copies,
                            index);
#if DEBUG_MEMC_WRITE
                    if (m_debug and wok)
                    {
                        std::cout << "  <MEMC " << name() << " WRITE_BC_IVT_LOCK> Register broadcast inval in IVT"
                            << " / nb_copies = " << r_write_count.read() << std::endl;
                    }
#endif
                    r_write_upt_index = index;

                    if (wok) r_write_fsm = WRITE_BC_DIR_INVAL;
                    else     r_write_fsm = WRITE_WAIT;
                }
                break;
            }
            ////////////////////////
            case WRITE_BC_DIR_INVAL:    // Register a put transaction in TRT
            // and invalidate the line in directory
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_WRITE) and
                        "MEMC ERROR in WRITE_BC_DIR_INVAL state: Bad DIR allocation");

                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_WRITE) and
                        "MEMC ERROR in WRITE_BC_DIR_INVAL state: Bad TRT allocation");

                assert((r_alloc_ivt_fsm.read() == ALLOC_IVT_WRITE) and
                        "MEMC ERROR in WRITE_BC_DIR_INVAL state: Bad IVT allocation");

                // register PUT request in TRT
                std::vector<data_t> data_vector;
                data_vector.clear();
                for (size_t i = 0; i < m_words; i++)
                {
                    data_vector.push_back(r_write_data[i].read());
                }
                m_trt.set(r_write_trt_index.read(),
                        false,             // PUT request
                        m_nline[(addr_t) (r_write_address.read())],
                        0,                 // unused
                        0,                 // unused
                        0,                 // unused
                        false,             // not a processor read
                        0,                 // unused
                        0,                 // unused
                        std::vector<be_t> (m_words, 0),
                        data_vector);

                // invalidate directory entry
                DirectoryEntry entry;
                entry.valid       = false;
                entry.dirty       = false;
                entry.tag         = 0;
                entry.is_cnt      = false;
                entry.lock        = false;
                entry.owner.srcid = 0;
                entry.owner.inst  = false;
                entry.ptr         = 0;
                entry.count       = 0;
                size_t set        = m_y[(addr_t) (r_write_address.read())];
                size_t way        = r_write_way.read();

                m_cache_directory.write(set, way, entry);

                if ((r_write_pktid.read() & 0x7) == TYPE_SC)
                {
                    m_llsc_table.sc(r_write_address.read(), r_write_sc_key.read());
                }

#if DEBUG_MEMC_WRITE
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " WRITE_BC_DIR_INVAL> Inval DIR and register in TRT:"
                        << " address = " << std::hex << r_write_address.read() << std::endl;
                }
#endif
                r_write_fsm = WRITE_BC_CC_SEND;
                break;
            }

            //////////////////////
            case WRITE_BC_CC_SEND:    // Post a coherence broadcast request to CC_SEND FSM
            {
                if (!r_write_to_cc_send_multi_req.read() and !r_write_to_cc_send_brdcast_req.read())
                {
                    r_write_to_cc_send_multi_req   = false;
                    r_write_to_cc_send_brdcast_req = true;
                    r_write_to_cc_send_trdid       = r_write_upt_index.read();
                    r_write_to_cc_send_nline       = m_nline[(addr_t) (r_write_address.read())];
                    r_write_to_cc_send_index       = 0;
                    r_write_to_cc_send_count       = 0;

                    for (size_t i = 0; i < m_words; i++)  // à quoi sert ce for? (AG)
                    {
                        r_write_to_cc_send_be[i] = 0;
                        r_write_to_cc_send_data[i] = 0;
                    }
                    r_write_fsm = WRITE_BC_XRAM_REQ;

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " WRITE_BC_CC_SEND> Post a broadcast request to CC_SEND FSM" << std::endl;
                    }
#endif
                }
                break;
            }

            ///////////////////////
            case WRITE_BC_XRAM_REQ:   // Post a PUT request to IXR_CMD FSM
            {
                if (not r_write_to_ixr_cmd_req.read())
                {
                    r_write_to_ixr_cmd_req   = true;
                    r_write_to_ixr_cmd_index = r_write_trt_index.read();
                    r_write_fsm = WRITE_IDLE;

#if DEBUG_MEMC_WRITE
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " WRITE_BC_XRAM_REQ> Post a put request to IXR_CMD FSM" << std::endl;
                    }
#endif
                }
                break;
            }
        } // end switch r_write_fsm

        ///////////////////////////////////////////////////////////////////////
        //    IXR_CMD FSM
        ///////////////////////////////////////////////////////////////////////
        // The IXR_CMD fsm controls the command packets to the XRAM :
        // It handles requests from 5 FSMs with a round-robin priority:
        //  READ > WRITE > CAS > XRAM_RSP > CONFIG
        //
        // - It sends a single flit VCI read to the XRAM in case of
        //   GET request posted by the READ, WRITE or CAS FSMs.
        // - It sends a multi-flit VCI write in case of PUT request posted by
        //   the XRAM_RSP, WRITE, CAS, or CONFIG FSMs.
        //
        // For each client, there is three steps:
        // - IXR_CMD_*_IDLE : round-robin allocation to a client
        // - IXR_CMD_*_TRT  : access to TRT for address and data
        // - IXR_CMD_*_SEND : send the PUT or GET VCI command
        //
        // The address and data to be written (for a PUT) are stored in TRT.
        // The trdid field contains always the TRT entry index.
        ////////////////////////////////////////////////////////////////////////

        switch (r_ixr_cmd_fsm.read())
        {
            ///////////////////////
            case IXR_CMD_READ_IDLE:
            {
                if      (r_write_to_ixr_cmd_req.read())      r_ixr_cmd_fsm = IXR_CMD_WRITE_TRT;
                else if (r_cas_to_ixr_cmd_req.read())        r_ixr_cmd_fsm = IXR_CMD_CAS_TRT;
                else if (r_xram_rsp_to_ixr_cmd_req.read())   r_ixr_cmd_fsm = IXR_CMD_XRAM_TRT;
                else if (r_config_to_ixr_cmd_req.read())     r_ixr_cmd_fsm = IXR_CMD_CONFIG_TRT;
                else if (r_read_to_ixr_cmd_req.read())       r_ixr_cmd_fsm = IXR_CMD_READ_TRT;
                break;
            }
            ////////////////////////
            case IXR_CMD_WRITE_IDLE:
            {
                if      (r_cas_to_ixr_cmd_req.read())        r_ixr_cmd_fsm = IXR_CMD_CAS_TRT;
                else if (r_xram_rsp_to_ixr_cmd_req.read())   r_ixr_cmd_fsm = IXR_CMD_XRAM_TRT;
                else if (r_config_to_ixr_cmd_req.read())     r_ixr_cmd_fsm = IXR_CMD_CONFIG_TRT;
                else if (r_read_to_ixr_cmd_req.read())       r_ixr_cmd_fsm = IXR_CMD_READ_TRT;
                else if (r_write_to_ixr_cmd_req.read())      r_ixr_cmd_fsm = IXR_CMD_WRITE_TRT;
                break;
            }
            //////////////////////
            case IXR_CMD_CAS_IDLE:
            {
                if      (r_xram_rsp_to_ixr_cmd_req.read())   r_ixr_cmd_fsm = IXR_CMD_XRAM_TRT;
                else if (r_config_to_ixr_cmd_req.read())     r_ixr_cmd_fsm = IXR_CMD_CONFIG_TRT;
                else if (r_read_to_ixr_cmd_req.read())       r_ixr_cmd_fsm = IXR_CMD_READ_TRT;
                else if (r_write_to_ixr_cmd_req.read())      r_ixr_cmd_fsm = IXR_CMD_WRITE_TRT;
                else if (r_cas_to_ixr_cmd_req.read())        r_ixr_cmd_fsm = IXR_CMD_CAS_TRT;
                break;
            }
            ///////////////////////
            case IXR_CMD_XRAM_IDLE:
            {
                if      (r_config_to_ixr_cmd_req.read())     r_ixr_cmd_fsm = IXR_CMD_CONFIG_TRT;
                else if (r_read_to_ixr_cmd_req.read())       r_ixr_cmd_fsm = IXR_CMD_READ_TRT;
                else if (r_write_to_ixr_cmd_req.read())      r_ixr_cmd_fsm = IXR_CMD_WRITE_TRT;
                else if (r_cas_to_ixr_cmd_req.read())        r_ixr_cmd_fsm = IXR_CMD_CAS_TRT;
                else if (r_xram_rsp_to_ixr_cmd_req.read())   r_ixr_cmd_fsm = IXR_CMD_XRAM_TRT;
                break;
            }
            /////////////////////////
            case IXR_CMD_CONFIG_IDLE:
            {
                if      (r_read_to_ixr_cmd_req.read())     r_ixr_cmd_fsm = IXR_CMD_READ_TRT;
                else if (r_write_to_ixr_cmd_req.read())    r_ixr_cmd_fsm = IXR_CMD_WRITE_TRT;
                else if (r_cas_to_ixr_cmd_req.read())      r_ixr_cmd_fsm = IXR_CMD_CAS_TRT;
                else if (r_xram_rsp_to_ixr_cmd_req.read()) r_ixr_cmd_fsm = IXR_CMD_XRAM_TRT;
                else if (r_config_to_ixr_cmd_req.read())   r_ixr_cmd_fsm = IXR_CMD_CONFIG_TRT;
                break;
            }

            //////////////////////
            case IXR_CMD_READ_TRT:       // access TRT for a GET
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_IXR_CMD)
                {
                    TransactionTabEntry entry = m_trt.read(r_read_to_ixr_cmd_index.read());
                    r_ixr_cmd_address = entry.nline * (m_words << 2);
                    r_ixr_cmd_trdid   = r_read_to_ixr_cmd_index.read();
                    r_ixr_cmd_get     = true;
                    r_ixr_cmd_word    = 0;
                    r_ixr_cmd_fsm     = IXR_CMD_READ_SEND;

#if DEBUG_MEMC_IXR_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_CMD_READ_TRT> TRT access"
                            << " index = " << std::dec << r_read_to_ixr_cmd_index.read()
                            << " / address = " << std::hex << (entry.nline * (m_words << 2)) << std::endl;
                    }
#endif
                }
                break;
            }
            ///////////////////////
            case IXR_CMD_WRITE_TRT: // access TRT for a PUT or a GET
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_IXR_CMD)
                {
                    TransactionTabEntry entry = m_trt.read(r_write_to_ixr_cmd_index.read());
                    r_ixr_cmd_address = entry.nline * (m_words << 2);
                    r_ixr_cmd_trdid   = r_write_to_ixr_cmd_index.read();
                    r_ixr_cmd_get     = entry.xram_read;
                    r_ixr_cmd_word    = 0;
                    r_ixr_cmd_fsm     = IXR_CMD_WRITE_SEND;

                    // Read data from TRT if PUT transaction
                    if (not entry.xram_read)
                    {
                        for (size_t i = 0; i < m_words; i++)
                        {
                            r_ixr_cmd_wdata[i] = entry.wdata[i];
                        }
                    }

#if DEBUG_MEMC_IXR_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_CMD_WRITE_TRT> TRT access"
                            << " index = " << std::dec << r_write_to_ixr_cmd_index.read()
                            << " / address = " << std::hex << (entry.nline * (m_words << 2)) << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////////
            case IXR_CMD_CAS_TRT:       // access TRT for a PUT or a GET
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_IXR_CMD)
                {
                    TransactionTabEntry entry = m_trt.read(r_cas_to_ixr_cmd_index.read());
                    r_ixr_cmd_address = entry.nline * (m_words << 2);
                    r_ixr_cmd_trdid   = r_cas_to_ixr_cmd_index.read();
                    r_ixr_cmd_get     = entry.xram_read;
                    r_ixr_cmd_word    = 0;
                    r_ixr_cmd_fsm     = IXR_CMD_CAS_SEND;

                    // Read data from TRT if PUT transaction
                    if (not entry.xram_read)
                    {
                        for (size_t i = 0; i < m_words; i++)
                        {
                            r_ixr_cmd_wdata[i] = entry.wdata[i];
                        }
                    }

#if DEBUG_MEMC_IXR_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_CMD_CAS_TRT> TRT access"
                            << " index = " << std::dec << r_cas_to_ixr_cmd_index.read()
                            << " / address = " << std::hex << (entry.nline * (m_words << 2)) << std::endl;
                    }
#endif
                }
                break;
            }
            //////////////////////
            case IXR_CMD_XRAM_TRT:       // access TRT for a PUT
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_IXR_CMD)
                {
                    TransactionTabEntry entry = m_trt.read(r_xram_rsp_to_ixr_cmd_index.read());
                    r_ixr_cmd_address = entry.nline * (m_words << 2);
                    r_ixr_cmd_trdid   = r_xram_rsp_to_ixr_cmd_index.read();
                    r_ixr_cmd_get     = false;
                    r_ixr_cmd_word    = 0;
                    r_ixr_cmd_fsm     = IXR_CMD_XRAM_SEND;
                    for (size_t i = 0; i < m_words; i++)
                    {
                        r_ixr_cmd_wdata[i] = entry.wdata[i];
                    }

#if DEBUG_MEMC_IXR_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_CMD_XRAM_TRT> TRT access"
                            << " index = " << std::dec << r_xram_rsp_to_ixr_cmd_index.read()
                            << " / address = " << std::hex << (entry.nline*(m_words<<2)) << std::endl;
                    }
#endif
                }
                break;
            }
            ////////////////////////
            case IXR_CMD_CONFIG_TRT:       // access TRT for a PUT
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_IXR_CMD)
                {
                    TransactionTabEntry entry = m_trt.read(r_config_to_ixr_cmd_index.read());
                    r_ixr_cmd_address = entry.nline * (m_words << 2);
                    r_ixr_cmd_trdid   = r_config_to_ixr_cmd_index.read();
                    r_ixr_cmd_get     = false;
                    r_ixr_cmd_word    = 0;
                    r_ixr_cmd_fsm     = IXR_CMD_CONFIG_SEND;
                    for (size_t i = 0; i < m_words; i++)
                    {
                        r_ixr_cmd_wdata[i] = entry.wdata[i];
                    }

#if DEBUG_MEMC_IXR_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_CMD_CONFIG_TRT> TRT access"
                            << " index = " << std::dec << r_config_to_ixr_cmd_index.read()
                            << " / address = " << std::hex << (entry.nline*(m_words<<2)) << std::endl;
                    }
#endif
                }
                break;
            }

            ///////////////////////
            case IXR_CMD_READ_SEND:      // send a get from READ FSM
            {
                if (p_vci_ixr.cmdack)
                {
                    r_ixr_cmd_fsm         = IXR_CMD_READ_IDLE;
                    r_read_to_ixr_cmd_req = false;

#if DEBUG_MEMC_IXR_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_CMD_READ_SEND> GET request:" << std::hex
                            << " address = " << r_ixr_cmd_address.read() + (r_ixr_cmd_word.read()<<2) << std::endl;
                    }
#endif
                }
                break;
            }
            ////////////////////////
            case IXR_CMD_WRITE_SEND:     // send a put or get from WRITE FSM
            {
                if (p_vci_ixr.cmdack)
                {
                    if (not r_ixr_cmd_get.read())   // PUT
                    {
                        if (r_ixr_cmd_word.read() == (m_words - 2))
                        {
                            r_ixr_cmd_fsm          = IXR_CMD_WRITE_IDLE;
                            r_write_to_ixr_cmd_req = false;
                        }
                        else
                        {
                            r_ixr_cmd_word = r_ixr_cmd_word.read() + 2;
                        }

#if DEBUG_MEMC_IXR_CMD
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " IXR_CMD_WRITE_SEND> PUT request:" << std::hex
                                << " address = " << r_ixr_cmd_address.read() + (r_ixr_cmd_word.read()<<2) << std::endl;
                        }
#endif
                    }
                    else // GET
                    {
                        r_ixr_cmd_fsm          = IXR_CMD_WRITE_IDLE;
                        r_write_to_ixr_cmd_req = false;

#if DEBUG_MEMC_IXR_CMD
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " IXR_CMD_WRITE_SEND> GET request:" << std::hex
                                << " address = " << r_ixr_cmd_address.read() + (r_ixr_cmd_word.read()<<2) << std::endl;
                        }
#endif
                    }
                }
                break;
            }
            //////////////////////
            case IXR_CMD_CAS_SEND: // send a put or get command from CAS FSM
            {
                if (p_vci_ixr.cmdack)
                {
                    if (not r_ixr_cmd_get.read()) // PUT
                    {
                        if (r_ixr_cmd_word.read() == (m_words - 2))
                        {
                            r_ixr_cmd_fsm        = IXR_CMD_CAS_IDLE;
                            r_cas_to_ixr_cmd_req = false;
                        }
                        else
                        {
                            r_ixr_cmd_word = r_ixr_cmd_word.read() + 2;
                        }

#if DEBUG_MEMC_IXR_CMD
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " IXR_CMD_CAS_SEND> PUT request:" << std::hex
                                << " address = " << r_ixr_cmd_address.read() + (r_ixr_cmd_word.read() << 2) << std::endl;
                        }
#endif
                    }
                    else // GET
                    {
                        r_ixr_cmd_fsm        = IXR_CMD_CAS_IDLE;
                        r_cas_to_ixr_cmd_req = false;

#if DEBUG_MEMC_IXR_CMD
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " IXR_CMD_CAS_SEND> GET request:" << std::hex
                                << " address = " << r_ixr_cmd_address.read() + (r_ixr_cmd_word.read() << 2) << std::endl;
                        }
#endif
                    }
                }
                break;
            }
            ///////////////////////
            case IXR_CMD_XRAM_SEND: // send a put from XRAM_RSP FSM
            {
                if (p_vci_ixr.cmdack.read())
                {
                    if (r_ixr_cmd_word.read() == (m_words - 2))
                    {
                        r_ixr_cmd_fsm = IXR_CMD_XRAM_IDLE;
                        r_xram_rsp_to_ixr_cmd_req = false;
                    }
                    else
                    {
                        r_ixr_cmd_word = r_ixr_cmd_word.read() + 2;
                    }

#if DEBUG_MEMC_IXR_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_CMD_XRAM_SEND> PUT request:" << std::hex
                            << " address = " << r_ixr_cmd_address.read() + (r_ixr_cmd_word.read() << 2) << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////////////
            case IXR_CMD_CONFIG_SEND:     // send a put from CONFIG FSM
            {
                if (p_vci_ixr.cmdack.read())
                {
                    if (r_ixr_cmd_word.read() == (m_words - 2))
                    {
                        r_ixr_cmd_fsm = IXR_CMD_CONFIG_IDLE;
                        r_config_to_ixr_cmd_req = false;
                    }
                    else
                    {
                        r_ixr_cmd_word = r_ixr_cmd_word.read() + 2;
                    }

#if DEBUG_MEMC_IXR_CMD
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_CMD_CONFIG_SEND> PUT request:" << std::hex
                            << " address = " << r_ixr_cmd_address.read() + (r_ixr_cmd_word.read()<<2) << std::endl;
                    }
#endif
                }
                break;
            }
        } // end switch r_ixr_cmd_fsm

        ////////////////////////////////////////////////////////////////////////////
        //                IXR_RSP FSM
        ////////////////////////////////////////////////////////////////////////////
        // The IXR_RSP FSM receives the response packets from the XRAM,
        // for both PUT transaction, and GET transaction.
        //
        // - A response to a PUT request is a single-cell VCI packet.
        // The TRT index is contained in the RTRDID field.
        // The FSM takes the lock protecting the TRT, and the corresponding
        // entry is erased. If an acknowledge was required (in case of software SYNC)
        // the r_config_rsp_lines counter is decremented.
        //
        // - A response to a GET request is a multi-cell VCI packet.
        // The TRT index is contained in the RTRDID field.
        // The N cells contain the N words of the cache line in the RDATA field.
        // The FSM takes the lock protecting the TRT to store the line in the TRT
        // (taking into account the write requests already stored in the TRT).
        // When the line is completely written, the r_ixr_rsp_to_xram_rsp_rok[index]
        // signal is set to inform the XRAM_RSP FSM.
        ///////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "ixr_rsp_fsm" << std::endl;

        switch(r_ixr_rsp_fsm.read())
        {
            //////////////////
            case IXR_RSP_IDLE:  // test transaction type: PUT/GET
            {
                if (p_vci_ixr.rspval.read())
                {
                    r_ixr_rsp_cpt       = 0;
                    r_ixr_rsp_trt_index = p_vci_ixr.rtrdid.read();

                    if (p_vci_ixr.reop.read() and not
                            p_vci_ixr.rerror.read())   // PUT
                    {
                        r_ixr_rsp_fsm = IXR_RSP_TRT_ERASE;

#if DEBUG_MEMC_IXR_RSP
                        if (m_debug)
                            std::cout << "  <MEMC " << name()
                                << " IXR_RSP_IDLE> Response from XRAM to a put transaction" << std::endl;
#endif
                    }
                    else // GET
                    {
                        r_ixr_rsp_fsm = IXR_RSP_TRT_READ;

#if DEBUG_MEMC_IXR_RSP
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name()
                                << " IXR_RSP_IDLE> Response from XRAM to a get transaction" << std::endl;
                        }
#endif
                    }
                }
                break;
            }
            ////////////////////////
            case IXR_RSP_TRT_ERASE:   // erase the entry in the TRT
                                      // decrease the line counter if config request
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_IXR_RSP)
                {
                    size_t index = r_ixr_rsp_trt_index.read();

                    if (m_trt.is_config(index)) // it's a config transaction
                    {
                        config_rsp_lines_ixr_rsp_decr = true;
                    }

                    m_trt.erase(index);
                    r_ixr_rsp_fsm = IXR_RSP_IDLE;

#if DEBUG_MEMC_IXR_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_RSP_TRT_ERASE> Erase TRT entry "
                            << r_ixr_rsp_trt_index.read() << std::endl;
                    }
#endif
                }
                break;
            }
            //////////////////////
            case IXR_RSP_TRT_READ: // write a 64 bits data word in TRT
            {
                if ((r_alloc_trt_fsm.read() == ALLOC_TRT_IXR_RSP) and p_vci_ixr.rspval)
                {
                    size_t      index  = r_ixr_rsp_trt_index.read();
                    size_t      word   = r_ixr_rsp_cpt.read();
                    bool        eop    = p_vci_ixr.reop.read();
                    wide_data_t data   = p_vci_ixr.rdata.read();
                    bool        rerror = ((p_vci_ixr.rerror.read() & 0x1) == 1);

                    assert(((eop == (word == (m_words - 2))) or rerror) and
                    "MEMC ERROR in IXR_RSP_TRT_READ state : invalid response from XRAM");

                    m_trt.write_rsp(index, word, data, rerror);

                    r_ixr_rsp_cpt = word + 2;

                    if (eop)
                    {
                        r_ixr_rsp_to_xram_rsp_rok[r_ixr_rsp_trt_index.read()] = true;
                        r_ixr_rsp_fsm = IXR_RSP_IDLE;
                    }

#if DEBUG_MEMC_IXR_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " IXR_RSP_TRT_READ> Writing 2 words in TRT : "
                            << " index = " << std::dec << index
                            << " / word = " << word
                            << " / data = " << std::hex << data << std::endl;
                    }
#endif
                }
                break;
            }
        } // end swich r_ixr_rsp_fsm

        ////////////////////////////////////////////////////////////////////////////
        //                XRAM_RSP FSM
        ////////////////////////////////////////////////////////////////////////////
        // The XRAM_RSP FSM handles the incoming cache lines after an XRAM GET.
        // The cache line has been written in the TRT by the IXR_CMD_FSM.
        // As the IXR_RSP FSM and the XRAM_RSP FSM are running in parallel,
        // there is as many flip-flops r_ixr_rsp_to_xram_rsp_rok[i] as the number
        // of entries in the TRT, that are handled with a round-robin priority...
        //
        // The FSM takes the lock protecting TRT, and the lock protecting DIR.
        // The selected TRT entry is copied in the local buffer r_xram_rsp_trt_buf.
        // It selects a cache slot and save the victim line in another local buffer
        // r_xram_rsp_victim_***.
        // It writes the line extracted from TRT in the cache.
        // If it was a read MISS, the XRAM_RSP FSM send a request to the TGT_RSP
        // FSM to return the cache line to the registered processor.
        // If there is no empty slot, a victim line is evicted, and
        // invalidate requests are sent to the L1 caches containing copies.
        // If this line is dirty, the XRAM_RSP FSM send a request to the IXR_CMD
        // FSM to save the victim line to the XRAM, and register the write transaction
        // in the TRT (using the entry previously used by the read transaction).
        ///////////////////////////////////////////////////////////////////////////////

        switch(r_xram_rsp_fsm.read())
        {
            ///////////////////
            case XRAM_RSP_IDLE: // scan the XRAM responses / select a TRT index (round robin)
            {
                size_t old = r_xram_rsp_trt_index.read();
                size_t lines = m_trt_lines;
                for (size_t i = 0; i < lines; i++)
                {
                    size_t index = (i + old + 1) % lines;
                    if (r_ixr_rsp_to_xram_rsp_rok[index])
                    {
                        r_xram_rsp_trt_index             = index;
                        r_ixr_rsp_to_xram_rsp_rok[index] = false;
                        r_xram_rsp_fsm                   = XRAM_RSP_DIR_LOCK;

#if DEBUG_MEMC_XRAM_RSP
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " XRAM_RSP_IDLE>"
                                << " Available cache line in TRT:"
                                << " index = " << std::dec << index << std::endl;
                        }
#endif
                        break;
                    }
                }
                break;
            }
            ///////////////////////
            case XRAM_RSP_DIR_LOCK: // Takes the DIR lock and the TRT lock
            // Copy the TRT entry in a local buffer
            {
                if ((r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP) and
                        (r_alloc_trt_fsm.read() == ALLOC_TRT_XRAM_RSP))
                {
                    // copy the TRT entry in the r_xram_rsp_trt_buf local buffer
                    size_t index = r_xram_rsp_trt_index.read();
                    r_xram_rsp_trt_buf.copy(m_trt.read(index));
                    r_xram_rsp_fsm = XRAM_RSP_TRT_COPY;

#if DEBUG_MEMC_XRAM_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " XRAM_RSP_DIR_LOCK>"
                            << " Get access to DIR and TRT" << std::endl;
                    }
#endif
                }
                break;
            }
            ///////////////////////
            case XRAM_RSP_TRT_COPY: // Select a victim cache line
            // and copy it in a local buffer
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP) and
                        "MEMC ERROR in XRAM_RSP_TRT_COPY state: Bad DIR allocation");

                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_XRAM_RSP) and
                        "MEMC ERROR in XRAM_RSP_TRT_COPY state: Bad TRT allocation");

                // selects & extracts a victim line from cache
                size_t way = 0;
                size_t set = m_y[(addr_t) (r_xram_rsp_trt_buf.nline * m_words * 4)];

                DirectoryEntry victim(m_cache_directory.select(set, way));

                bool inval = (victim.count and victim.valid) ;

                // copy the victim line in a local buffer (both data dir)
                m_cache_data.read_line(way, set, r_xram_rsp_victim_data);

                r_xram_rsp_victim_copy      = victim.owner.srcid;
                r_xram_rsp_victim_copy_inst = victim.owner.inst;
                r_xram_rsp_victim_count     = victim.count;
                r_xram_rsp_victim_ptr       = victim.ptr;
                r_xram_rsp_victim_way       = way;
                r_xram_rsp_victim_set       = set;
                r_xram_rsp_victim_nline     = (addr_t)victim.tag*m_sets + set;
                r_xram_rsp_victim_is_cnt    = victim.is_cnt;
                r_xram_rsp_victim_inval     = inval ;
                r_xram_rsp_victim_dirty     = victim.dirty;

                if (not r_xram_rsp_trt_buf.rerror) r_xram_rsp_fsm = XRAM_RSP_IVT_LOCK;
                else                               r_xram_rsp_fsm = XRAM_RSP_ERROR_ERASE;

#if DEBUG_MEMC_XRAM_RSP
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " XRAM_RSP_TRT_COPY>"
                        << " Select a victim slot: "
                        << " way = " << std::dec << way
                        << " / set = " << set
                        << " / inval_required = " << inval << std::endl;
                }
#endif
                break;
            }
            ///////////////////////
            case XRAM_RSP_IVT_LOCK:   // Keep DIR and TRT locks and take the IVT lock
            // to check a possible pending inval
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP) and
                        "MEMC ERROR in XRAM_RSP_IVT_LOCK state: Bad DIR allocation");

                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_XRAM_RSP) and
                        "MEMC ERROR in XRAM_RSP_IVT_LOCK state: Bad TRT allocation");

                if (r_alloc_ivt_fsm == ALLOC_IVT_XRAM_RSP)
                {
                    size_t index = 0;
                    if (m_ivt.search_inval(r_xram_rsp_trt_buf.nline, index))  // pending inval
                    {
                        r_xram_rsp_fsm = XRAM_RSP_INVAL_WAIT;

#if DEBUG_MEMC_XRAM_RSP
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " XRAM_RSP_IVT_LOCK>"
                                << " Get acces to IVT, but line invalidation registered"
                                << " / address = " << std::hex << r_xram_rsp_trt_buf.nline * m_words * 4
                                << " / index = " << std::dec << index << std::endl;
                        }
#endif

                    }
                    else if (m_ivt.is_full() and r_xram_rsp_victim_inval.read()) // IVT full
                    {
                        r_xram_rsp_fsm = XRAM_RSP_INVAL_WAIT;

#if DEBUG_MEMC_XRAM_RSP
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " XRAM_RSP_IVT_LOCK>"
                                << " Get acces to IVT, but inval required and IVT full" << std::endl;
                        }
#endif
                    }
                    else
                    {
                        r_xram_rsp_fsm = XRAM_RSP_DIR_UPDT;

#if DEBUG_MEMC_XRAM_RSP
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " XRAM_RSP_IVT_LOCK>"
                                << " Get acces to IVT / no pending inval request" << std::endl;
                        }
#endif
                    }
                }
                break;
            }
            /////////////////////////
            case XRAM_RSP_INVAL_WAIT: // release all locks and returns to DIR_LOCK to retry
            {

#if DEBUG_MEMC_XRAM_RSP
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " XRAM_RSP_INVAL_WAIT>"
                        << " Release all locks and retry" << std::endl;
                }
#endif
                r_xram_rsp_fsm = XRAM_RSP_DIR_LOCK;
                break;
            }
            ///////////////////////
            case XRAM_RSP_DIR_UPDT:   // updates the cache (both data & directory),
            // erases the TRT entry if victim not dirty,
            // and set inval request in IVT if required
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP) and
                        "MEMC ERROR in XRAM_RSP_DIR_UPDT state: Bad DIR allocation");

                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_XRAM_RSP) and
                        "MEMC ERROR in XRAM_RSP_DIR_UPDT state: Bad TRT allocation");

                assert((r_alloc_ivt_fsm.read() == ALLOC_IVT_XRAM_RSP) and
                        "MEMC ERROR in XRAM_RSP_DIR_UPDT state: Bad IVT allocation");

                // check if this is an instruction read, this means pktid is either
                // TYPE_READ_INS_UNC   0bX010 with TSAR encoding
                // TYPE_READ_INS_MISS  0bX011 with TSAR encoding
                bool inst_read = (r_xram_rsp_trt_buf.pktid & 0x2) and r_xram_rsp_trt_buf.proc_read;

                // check if this is a cached read, this means pktid is either
                // TYPE_READ_DATA_MISS 0bX001 with TSAR encoding
                // TYPE_READ_INS_MISS  0bX011 with TSAR encoding
                bool cached_read = (r_xram_rsp_trt_buf.pktid & 0x1) and r_xram_rsp_trt_buf.proc_read;

                bool dirty = false;

                // update cache data
                size_t set = r_xram_rsp_victim_set.read();
                size_t way = r_xram_rsp_victim_way.read();

                for (size_t word = 0; word < m_words; word++)
                {
                    m_cache_data.write(way, set, word, r_xram_rsp_trt_buf.wdata[word]);
                    dirty = dirty or (r_xram_rsp_trt_buf.wdata_be[word] != 0);
                }

                // update cache directory
                DirectoryEntry entry;
                entry.valid  = true;
                entry.is_cnt = false;
                entry.lock   = false;
                entry.dirty  = dirty;
                entry.tag    = r_xram_rsp_trt_buf.nline / m_sets;
                entry.ptr    = 0;
                if (cached_read)
                {
                    entry.owner.srcid = r_xram_rsp_trt_buf.srcid;
                    entry.owner.inst  = inst_read;
                    entry.count       = 1;
                }
                else
                {
                    entry.owner.srcid = 0;
                    entry.owner.inst  = 0;
                    entry.count       = 0;
                }
                m_cache_directory.write(set, way, entry);

                // register invalid request in IVT for victim line if required
                if (r_xram_rsp_victim_inval.read())
                {
                    bool   broadcast    = r_xram_rsp_victim_is_cnt.read();
                    size_t index        = 0;
                    size_t count_copies = r_xram_rsp_victim_count.read();

                    bool   wok = m_ivt.set(false, // it's an inval transaction
                            broadcast, // set broadcast bit
                            false,     // no response required
                            false,     // no acknowledge required
                            0,         // srcid
                            0,         // trdid
                            0,         // pktid
                            r_xram_rsp_victim_nline.read(),
                            count_copies,
                            index);

                    r_xram_rsp_ivt_index = index;

                    assert(wok and "MEMC ERROR in XRAM_RSP_DIR_UPDT state: IVT should not be full");
                }

#if DEBUG_MEMC_XRAM_RSP
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " XRAM_RSP_DIR_UPDT>"
                        << " Cache update: "
                        << " way = " << std::dec << way
                        << " / set = " << set
                        << " / owner_id = " << std::hex << entry.owner.srcid
                        << " / owner_ins = " << std::dec << entry.owner.inst
                        << " / count = " << entry.count
                        << " / is_cnt = " << entry.is_cnt << std::endl;
                    if (r_xram_rsp_victim_inval.read())
                    {
                        std::cout << "          Invalidation request for address "
                            << std::hex << r_xram_rsp_victim_nline.read() * m_words * 4
                            << " / broadcast = " << r_xram_rsp_victim_is_cnt.read() << std::endl;
                    }
                }
#endif

                // If the victim is not dirty, we don't need to reuse the TRT entry for
                // another PUT transaction, and we can erase the TRT entry
                if (not r_xram_rsp_victim_dirty.read())
                {
                    m_trt.erase(r_xram_rsp_trt_index.read());
                }

                // Next state
                if (r_xram_rsp_victim_dirty.read())      r_xram_rsp_fsm = XRAM_RSP_TRT_DIRTY;
                else if (r_xram_rsp_trt_buf.proc_read)   r_xram_rsp_fsm = XRAM_RSP_DIR_RSP;
                else if (r_xram_rsp_victim_inval.read()) r_xram_rsp_fsm = XRAM_RSP_INVAL;
                else                                     r_xram_rsp_fsm = XRAM_RSP_IDLE;
                break;
            }
            ////////////////////////
            case XRAM_RSP_TRT_DIRTY:  // set the TRT entry (PUT to XRAM) if the victim is dirty
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_XRAM_RSP)
                {
                    std::vector<data_t> data_vector;
                    data_vector.clear();
                    for (size_t i = 0; i < m_words; i++)
                    {
                        data_vector.push_back(r_xram_rsp_victim_data[i].read());
                    }
                    m_trt.set(r_xram_rsp_trt_index.read(),
                            false,                          // PUT
                            r_xram_rsp_victim_nline.read(), // line index
                            0,                              // unused
                            0,                              // unused
                            0,                              // unused
                            false,                          // not proc_read
                            0,                              // unused
                            0,                              // unused
                            std::vector<be_t>(m_words,0xF),
                            data_vector);

#if DEBUG_MEMC_XRAM_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " XRAM_RSP_TRT_DIRTY>"
                            << " Set TRT entry for the put transaction"
                            << " / address = " << (r_xram_rsp_victim_nline.read() * m_words * 4) << std::endl;
                    }
#endif
                    if (r_xram_rsp_trt_buf.proc_read)        r_xram_rsp_fsm = XRAM_RSP_DIR_RSP;
                    else if (r_xram_rsp_victim_inval.read()) r_xram_rsp_fsm = XRAM_RSP_INVAL;
                    else                                     r_xram_rsp_fsm = XRAM_RSP_WRITE_DIRTY;
                }
                break;
            }
            //////////////////////
            case XRAM_RSP_DIR_RSP:     // Request a response to TGT_RSP FSM
            {
                if (not r_xram_rsp_to_tgt_rsp_req.read())
                {
                    r_xram_rsp_to_tgt_rsp_srcid = r_xram_rsp_trt_buf.srcid;
                    r_xram_rsp_to_tgt_rsp_trdid = r_xram_rsp_trt_buf.trdid;
                    r_xram_rsp_to_tgt_rsp_pktid = r_xram_rsp_trt_buf.pktid;
                    for (size_t i = 0; i < m_words; i++)
                    {
                        r_xram_rsp_to_tgt_rsp_data[i] = r_xram_rsp_trt_buf.wdata[i];
                    }
                    r_xram_rsp_to_tgt_rsp_word   = r_xram_rsp_trt_buf.word_index;
                    r_xram_rsp_to_tgt_rsp_length = r_xram_rsp_trt_buf.read_length;
                    r_xram_rsp_to_tgt_rsp_ll_key = r_xram_rsp_trt_buf.ll_key;
                    r_xram_rsp_to_tgt_rsp_rerror = false;
                    r_xram_rsp_to_tgt_rsp_req    = true;

                    if (r_xram_rsp_victim_inval.read())      r_xram_rsp_fsm = XRAM_RSP_INVAL;
                    else if (r_xram_rsp_victim_dirty.read()) r_xram_rsp_fsm = XRAM_RSP_WRITE_DIRTY;
                    else                                     r_xram_rsp_fsm = XRAM_RSP_IDLE;

#if DEBUG_MEMC_XRAM_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " XRAM_RSP_DIR_RSP>"
                            << " Request the TGT_RSP FSM to return data:"
                            << " rsrcid = " << std::hex << r_xram_rsp_trt_buf.srcid
                            << " / address = " << std::hex << r_xram_rsp_trt_buf.nline * m_words * 4
                            << " / nwords = " << std::dec << r_xram_rsp_trt_buf.read_length << std::endl;
                    }
#endif
                }
                break;
            }
            ////////////////////
            case XRAM_RSP_INVAL:  // send invalidate request to CC_SEND FSM
            {
                if (!r_xram_rsp_to_cc_send_multi_req.read() and
                        !r_xram_rsp_to_cc_send_brdcast_req.read())
                {
                    bool multi_req = !r_xram_rsp_victim_is_cnt.read();
                    bool last_multi_req  = multi_req and (r_xram_rsp_victim_count.read() == 1);
                    bool not_last_multi_req = multi_req and (r_xram_rsp_victim_count.read() != 1);

                    r_xram_rsp_to_cc_send_multi_req   = last_multi_req;
                    r_xram_rsp_to_cc_send_brdcast_req = r_xram_rsp_victim_is_cnt.read();
                    r_xram_rsp_to_cc_send_nline       = r_xram_rsp_victim_nline.read();
                    r_xram_rsp_to_cc_send_trdid       = r_xram_rsp_ivt_index;
                    xram_rsp_to_cc_send_fifo_srcid    = r_xram_rsp_victim_copy.read();
                    xram_rsp_to_cc_send_fifo_inst     = r_xram_rsp_victim_copy_inst.read();
                    xram_rsp_to_cc_send_fifo_put      = multi_req;
                    r_xram_rsp_next_ptr               = r_xram_rsp_victim_ptr.read();

                    if (r_xram_rsp_victim_dirty) r_xram_rsp_fsm = XRAM_RSP_WRITE_DIRTY;
                    else if (not_last_multi_req) r_xram_rsp_fsm = XRAM_RSP_HEAP_REQ;
                    else                         r_xram_rsp_fsm = XRAM_RSP_IDLE;

#if DEBUG_MEMC_XRAM_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " XRAM_RSP_INVAL>"
                            << " Send an inval request to CC_SEND FSM"
                            << " / address = " << r_xram_rsp_victim_nline.read()*m_words*4 << std::endl;
                    }
#endif
                }
                break;
            }
            //////////////////////////
            case XRAM_RSP_WRITE_DIRTY:  // send a write request to IXR_CMD FSM
            {
                if (not r_xram_rsp_to_ixr_cmd_req.read())
                {
                    r_xram_rsp_to_ixr_cmd_req = true;
                    r_xram_rsp_to_ixr_cmd_index = r_xram_rsp_trt_index.read();

                    m_cpt_write_dirty++;

                    bool multi_req = not r_xram_rsp_victim_is_cnt.read() and
                        r_xram_rsp_victim_inval.read();
                    bool not_last_multi_req = multi_req and (r_xram_rsp_victim_count.read() != 1);

                    if (not_last_multi_req) r_xram_rsp_fsm = XRAM_RSP_HEAP_REQ;
                    else                    r_xram_rsp_fsm = XRAM_RSP_IDLE;

#if DEBUG_MEMC_XRAM_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " XRAM_RSP_WRITE_DIRTY>"
                            << " Send the put request to IXR_CMD FSM"
                            << " / address = " << r_xram_rsp_victim_nline.read() * m_words * 4 << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////////////
            case XRAM_RSP_HEAP_REQ:    // Get the lock to the HEAP
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_XRAM_RSP)
                {
                    r_xram_rsp_fsm = XRAM_RSP_HEAP_ERASE;
                }

#if DEBUG_MEMC_XRAM_RSP
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " XRAM_RSP_HEAP_REQ>"
                        << " Requesting HEAP lock" << std::endl;
                }
#endif
                break;
            }
            /////////////////////////
            case XRAM_RSP_HEAP_ERASE: // erase the copies and send invalidations
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_XRAM_RSP)
                {
                    HeapEntry entry = m_heap.read(r_xram_rsp_next_ptr.read());

                    xram_rsp_to_cc_send_fifo_srcid = entry.owner.srcid;
                    xram_rsp_to_cc_send_fifo_inst  = entry.owner.inst;
                    xram_rsp_to_cc_send_fifo_put   = true;
                    if (m_xram_rsp_to_cc_send_inst_fifo.wok())
                    {
                        r_xram_rsp_next_ptr = entry.next;
                        if (entry.next == r_xram_rsp_next_ptr.read())   // last copy
                        {
                            r_xram_rsp_to_cc_send_multi_req = true;
                            r_xram_rsp_fsm = XRAM_RSP_HEAP_LAST;
                        }
                        else
                        {
                            r_xram_rsp_fsm = XRAM_RSP_HEAP_ERASE;
                        }
                    }
                    else
                    {
                        r_xram_rsp_fsm = XRAM_RSP_HEAP_ERASE;
                    }

#if DEBUG_MEMC_XRAM_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " XRAM_RSP_HEAP_ERASE>"
                            << " Erase copy:"
                            << " srcid = " << std::hex << entry.owner.srcid
                            << " / inst = " << std::dec << entry.owner.inst << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////////////
            case XRAM_RSP_HEAP_LAST:  // last copy
            {
                if (r_alloc_heap_fsm.read() != ALLOC_HEAP_XRAM_RSP)
                {
                    std::cout << "VCI_MEM_CACHE ERROR " << name() << " XRAM_RSP_HEAP_LAST"
                        << " bad HEAP allocation" << std::endl;
                    exit(0);
                }
                size_t free_pointer = m_heap.next_free_ptr();

                HeapEntry last_entry;
                last_entry.owner.srcid = 0;
                last_entry.owner.inst  = false;
                if (m_heap.is_full())
                {
                    last_entry.next = r_xram_rsp_next_ptr.read();
                    m_heap.unset_full();
                }
                else
                {
                    last_entry.next = free_pointer;
                }

                m_heap.write_free_ptr(r_xram_rsp_victim_ptr.read());
                m_heap.write(r_xram_rsp_next_ptr.read(),last_entry);

                r_xram_rsp_fsm = XRAM_RSP_IDLE;

#if DEBUG_MEMC_XRAM_RSP
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " XRAM_RSP_HEAP_LAST>"
                        << " Heap housekeeping" << std::endl;
                }
#endif
                break;
            }
            //////////////////////////
            case XRAM_RSP_ERROR_ERASE:  // erase TRT entry in case of error
            {
                m_trt.erase(r_xram_rsp_trt_index.read());

                // Next state
                if (r_xram_rsp_trt_buf.proc_read)
                {
                    r_xram_rsp_fsm = XRAM_RSP_ERROR_RSP;
                }
                else
                {
                    // Trigger an interruption to signal a bus error from
                    // the XRAM because a processor WRITE MISS (XRAM GET
                    // transaction and not processor read).
                    //
                    // To avoid deadlocks we do not wait an error to be
                    // acknowledged before signaling another one.
                    // Therefore, when there is an active error, and other
                    // errors arrive, these are not considered

                    if (!r_xram_rsp_rerror_irq.read() && r_xram_rsp_rerror_irq_enable.read()
                            && r_xram_rsp_trt_buf.xram_read)
                    {
                        r_xram_rsp_rerror_irq     = true;
                        r_xram_rsp_rerror_address = r_xram_rsp_trt_buf.nline * m_words * 4;
                        r_xram_rsp_rerror_rsrcid  = r_xram_rsp_trt_buf.srcid;

#if DEBUG_MEMC_XRAM_RSP
                        if (m_debug)
                        {
                            std::cout
                                << "  <MEMC " << name() << " XRAM_RSP_ERROR_ERASE>"
                                << " Triggering interrupt to signal WRITE MISS bus error"
                                << " / irq_enable = " << r_xram_rsp_rerror_irq_enable.read()
                                << " / nline = "      << r_xram_rsp_trt_buf.nline
                                << " / rsrcid = "     << r_xram_rsp_trt_buf.srcid
                                << std::endl;
                        }
#endif
                    }

                    r_xram_rsp_fsm = XRAM_RSP_IDLE;
                }

#if DEBUG_MEMC_XRAM_RSP
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " XRAM_RSP_ERROR_ERASE>"
                        << " Error reported by XRAM / erase the TRT entry" << std::endl;
                }
#endif
                break;
            }
            ////////////////////////
            case XRAM_RSP_ERROR_RSP:     // Request an error response to TGT_RSP FSM
            {
                if (!r_xram_rsp_to_tgt_rsp_req.read())
                {
                    r_xram_rsp_to_tgt_rsp_srcid  = r_xram_rsp_trt_buf.srcid;
                    r_xram_rsp_to_tgt_rsp_trdid  = r_xram_rsp_trt_buf.trdid;
                    r_xram_rsp_to_tgt_rsp_pktid  = r_xram_rsp_trt_buf.pktid;
                    for (size_t i = 0; i < m_words; i++)
                    {
                        r_xram_rsp_to_tgt_rsp_data[i] = r_xram_rsp_trt_buf.wdata[i];
                    }
                    r_xram_rsp_to_tgt_rsp_word   = r_xram_rsp_trt_buf.word_index;
                    r_xram_rsp_to_tgt_rsp_length = r_xram_rsp_trt_buf.read_length;
                    r_xram_rsp_to_tgt_rsp_rerror = true;
                    r_xram_rsp_to_tgt_rsp_req    = true;

                    r_xram_rsp_fsm = XRAM_RSP_IDLE;

#if DEBUG_MEMC_XRAM_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " XRAM_RSP_ERROR_RSP> Request a response error to TGT_RSP FSM:"
                            << " srcid = " << std::dec << r_xram_rsp_trt_buf.srcid << std::endl;
                    }
#endif
                }
                break;
            }
        } // end swich r_xram_rsp_fsm

        ////////////////////////////////////////////////////////////////////////////////////
        //    CLEANUP FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The CLEANUP FSM handles the cleanup request from L1 caches.
        // It accesses the cache directory and the heap to update the list of copies.
        ////////////////////////////////////////////////////////////////////////////////////

        switch(r_cleanup_fsm.read())
        {
            //////////////////
            case CLEANUP_IDLE: // Get first DSPIN flit of the CLEANUP command
            {
                if (not m_cc_receive_to_cleanup_fifo.rok()) break;

                uint64_t flit = m_cc_receive_to_cleanup_fifo.read();

                uint32_t srcid = DspinDhccpParam::dspin_get(flit,
                        DspinDhccpParam::CLEANUP_SRCID);

                uint8_t type = DspinDhccpParam::dspin_get(flit,
                        DspinDhccpParam::P2M_TYPE);

                r_cleanup_way_index = DspinDhccpParam::dspin_get(flit,
                        DspinDhccpParam::CLEANUP_WAY_INDEX);

                r_cleanup_nline = DspinDhccpParam::dspin_get(flit,
                        DspinDhccpParam::CLEANUP_NLINE_MSB) << 32;

                r_cleanup_inst  = (type == DspinDhccpParam::TYPE_CLEANUP_INST);
                r_cleanup_srcid = srcid;

                assert((srcid < m_initiators) and
                        "MEMC ERROR in CLEANUP_IDLE state : illegal SRCID value");

                cc_receive_to_cleanup_fifo_get = true;
                r_cleanup_fsm                  = CLEANUP_GET_NLINE;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name()
          << " CLEANUP_IDLE> Cleanup request:" << std::hex
          << " owner_id = "   << srcid
          << " / owner_ins = "  << (type == DspinDhccpParam::TYPE_CLEANUP_INST) << std::endl;
#endif
                break;
            }
            ///////////////////////
            case CLEANUP_GET_NLINE:  // GET second DSPIN flit of the cleanup command
            {
                if (not m_cc_receive_to_cleanup_fifo.rok()) break;

                uint64_t flit = m_cc_receive_to_cleanup_fifo.read();

                addr_t nline = r_cleanup_nline.read() |
                    DspinDhccpParam::dspin_get(flit, DspinDhccpParam::CLEANUP_NLINE_LSB);

                cc_receive_to_cleanup_fifo_get = true;
                r_cleanup_nline = nline;
                r_cleanup_fsm = CLEANUP_DIR_REQ;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC "         << name()
          << " CLEANUP_GET_NLINE> Cleanup request:"
          << " address = " << std::hex << nline * m_words * 4 << std::endl;
#endif
                break;
            }
            /////////////////////
            case CLEANUP_DIR_REQ:   // Get the lock to the directory
            {
                if (r_alloc_dir_fsm.read() != ALLOC_DIR_CLEANUP) break;

                r_cleanup_fsm = CLEANUP_DIR_LOCK;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name() << " CLEANUP_DIR_REQ> Requesting DIR lock" << std::endl;
#endif
                break;
            }
            //////////////////////
            case CLEANUP_DIR_LOCK:    // test directory status
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CLEANUP) and
                        "MEMC ERROR in CLEANUP_DIR_LOCK: bad DIR allocation");

                // Read the directory
                size_t way = 0;
                addr_t cleanup_address = r_cleanup_nline.read() * m_words * 4;
                DirectoryEntry entry   = m_cache_directory.read(cleanup_address , way);
                r_cleanup_is_cnt       = entry.is_cnt;
                r_cleanup_dirty        = entry.dirty;
                r_cleanup_tag          = entry.tag;
                r_cleanup_lock         = entry.lock;
                r_cleanup_way          = way;
                r_cleanup_count        = entry.count;
                r_cleanup_ptr          = entry.ptr;
                r_cleanup_copy         = entry.owner.srcid;
                r_cleanup_copy_inst    = entry.owner.inst;

                if (entry.valid) // hit : the copy must be cleared
                {
                    assert((entry.count > 0) and
                            "MEMC ERROR in CLEANUP_DIR_LOCK state, CLEANUP on valid entry with no copies");

                    if ((entry.count == 1) or (entry.is_cnt)) // no access to the heap
                    {
                        r_cleanup_fsm = CLEANUP_DIR_WRITE;
                    }
                    else // access to the heap
                    {
                        r_cleanup_fsm = CLEANUP_HEAP_REQ;
                    }
                }
                else // miss : check IVT for a pending inval
                {
                    r_cleanup_fsm = CLEANUP_IVT_LOCK;
                }

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name()
          << " CLEANUP_DIR_LOCK> Test directory status: "
          << std::hex << " address = " << cleanup_address
          << " / hit = "        << entry.valid
          << " / dir_id = "     << entry.owner.srcid
          << " / dir_ins = "    << entry.owner.inst
          << " / search_id = "  << r_cleanup_srcid.read()
          << " / search_ins = " << r_cleanup_inst.read()
          << " / count = "      << entry.count
          << " / is_cnt = "     << entry.is_cnt << std::endl;
#endif
                break;
            }
            ///////////////////////
            case CLEANUP_DIR_WRITE:      // Update the directory entry without heap access
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CLEANUP) and
                        "MEMC ERROR in CLEANUP_DIR_LOCK: bad DIR allocation");

                size_t way       = r_cleanup_way.read();
                size_t set       = m_y[(addr_t) (r_cleanup_nline.read() * m_words * 4)];
                bool match_srcid = (r_cleanup_copy.read() == r_cleanup_srcid.read());
                bool match_inst  = (r_cleanup_copy_inst.read() == r_cleanup_inst.read());
                bool match       = match_srcid and match_inst;

                assert((r_cleanup_is_cnt.read() or match) and
                        "MEMC ERROR in CLEANUP_DIR_LOCK: illegal CLEANUP on valid entry");

                // update the cache directory (for the copies)
                DirectoryEntry entry;
                entry.valid       = true;
                entry.is_cnt      = r_cleanup_is_cnt.read();
                entry.dirty       = r_cleanup_dirty.read();
                entry.tag         = r_cleanup_tag.read();
                entry.lock        = r_cleanup_lock.read();
                entry.ptr         = r_cleanup_ptr.read();
                entry.count       = r_cleanup_count.read() - 1;
                entry.owner.srcid = 0;
                entry.owner.inst  = 0;

                m_cache_directory.write(set, way, entry);

                r_cleanup_fsm = CLEANUP_SEND_CLACK;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name()
          << " CLEANUP_DIR_WRITE> Update directory:"
          << std::hex << " address = "   << r_cleanup_nline.read() * m_words * 4
          << " / dir_id = "  << entry.owner.srcid
          << " / dir_ins = " << entry.owner.inst
          << " / count = "   << entry.count
          << " / is_cnt = "  << entry.is_cnt << std::endl;
#endif

                break;
            }
            //////////////////////
            case CLEANUP_HEAP_REQ:         // get the lock to the HEAP directory
            {
                if (r_alloc_heap_fsm.read() != ALLOC_HEAP_CLEANUP) break;

                r_cleanup_fsm = CLEANUP_HEAP_LOCK;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name()
          << " CLEANUP_HEAP_REQ> HEAP lock acquired " << std::endl;
#endif
                break;
            }
            //////////////////////
            case CLEANUP_HEAP_LOCK:      // two cases are handled in this state :
                                         // 1. the matching copy is directly in the directory
                                         // 2. the matching copy is the first copy in the heap
            {
                assert((r_alloc_heap_fsm.read() == ALLOC_HEAP_CLEANUP) and
                        "MEMC ERROR in CLEANUP_HEAP_LOCK state: bad HEAP allocation");

                size_t way = r_cleanup_way.read();
                size_t set = m_y[(addr_t)(r_cleanup_nline.read() * m_words * 4)];

                HeapEntry heap_entry  = m_heap.read(r_cleanup_ptr.read());
                bool last             = (heap_entry.next == r_cleanup_ptr.read());

                // match_dir computation
                bool match_dir_srcid  = (r_cleanup_copy.read()      == r_cleanup_srcid.read());
                bool match_dir_inst   = (r_cleanup_copy_inst.read() == r_cleanup_inst.read());
                bool match_dir        = match_dir_srcid  and match_dir_inst;

                // match_heap computation
                bool match_heap_srcid = (heap_entry.owner.srcid == r_cleanup_srcid.read());
                bool match_heap_inst  = (heap_entry.owner.inst  == r_cleanup_inst.read());
                bool match_heap       = match_heap_srcid and match_heap_inst;

                r_cleanup_prev_ptr    = r_cleanup_ptr.read();
                r_cleanup_prev_srcid  = heap_entry.owner.srcid;
                r_cleanup_prev_inst   = heap_entry.owner.inst;

                assert((not last or match_dir or match_heap) and
                        "MEMC ERROR in CLEANUP_HEAP_LOCK state: hit but no copy found");

                assert((not match_dir or not match_heap) and
                        "MEMC ERROR in CLEANUP_HEAP_LOCK state: two matching copies found");

                DirectoryEntry dir_entry;
                dir_entry.valid  = true;
                dir_entry.is_cnt = r_cleanup_is_cnt.read();
                dir_entry.dirty  = r_cleanup_dirty.read();
                dir_entry.tag    = r_cleanup_tag.read();
                dir_entry.lock   = r_cleanup_lock.read();
                dir_entry.count  = r_cleanup_count.read() - 1;

                // the matching copy is registered in the directory and
                // it must be replaced by the first copy registered in
                // the heap. The corresponding entry must be freed
                if (match_dir)
                {
                    dir_entry.ptr         = heap_entry.next;
                    dir_entry.owner.srcid = heap_entry.owner.srcid;
                    dir_entry.owner.inst  = heap_entry.owner.inst;
                    r_cleanup_next_ptr    = r_cleanup_ptr.read();
                    r_cleanup_fsm         = CLEANUP_HEAP_FREE;
                }

                // the matching copy is the first copy in the heap
                // It must be freed and the copy registered in directory
                // must point to the next copy in heap
                else if (match_heap)
                {
                    dir_entry.ptr         = heap_entry.next;
                    dir_entry.owner.srcid = r_cleanup_copy.read();
                    dir_entry.owner.inst  = r_cleanup_copy_inst.read();
                    r_cleanup_next_ptr    = r_cleanup_ptr.read();
                    r_cleanup_fsm         = CLEANUP_HEAP_FREE;
                }

                // The matching copy is in the heap, but is not the first copy
                // The directory entry must be modified to decrement count
                else
                {
                    dir_entry.ptr         = r_cleanup_ptr.read();
                    dir_entry.owner.srcid = r_cleanup_copy.read();
                    dir_entry.owner.inst  = r_cleanup_copy_inst.read();
                    r_cleanup_next_ptr    = heap_entry.next;
                    r_cleanup_fsm         = CLEANUP_HEAP_SEARCH;
                }

                m_cache_directory.write(set,way,dir_entry);

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name()
          << " CLEANUP_HEAP_LOCK> Checks matching:"
          << " address = "      << r_cleanup_nline.read() * m_words * 4
          << " / dir_id = "     << r_cleanup_copy.read()
          << " / dir_ins = "    << r_cleanup_copy_inst.read()
          << " / heap_id = "    << heap_entry.owner.srcid
          << " / heap_ins = "   << heap_entry.owner.inst
          << " / search_id = "  << r_cleanup_srcid.read()
          << " / search_ins = " << r_cleanup_inst.read() << std::endl;
#endif
                break;
            }
            ////////////////////////
            case CLEANUP_HEAP_SEARCH:     // This state is handling the case where the copy
                                          // is in the heap, but not the first in linked list
            {
                assert((r_alloc_heap_fsm.read() == ALLOC_HEAP_CLEANUP) and
                        "MEMC ERROR in CLEANUP_HEAP_LOCK state: bad HEAP allocation");

                HeapEntry heap_entry  = m_heap.read(r_cleanup_next_ptr.read());

                bool last             = (heap_entry.next        == r_cleanup_next_ptr.read());
                bool match_heap_srcid = (heap_entry.owner.srcid == r_cleanup_srcid.read());
                bool match_heap_inst  = (heap_entry.owner.inst  == r_cleanup_inst.read());
                bool match_heap       = match_heap_srcid and match_heap_inst;

                assert((not last or match_heap) and
                        "MEMC ERROR in CLEANUP_HEAP_SEARCH state: no copy found");

                // the matching copy must be removed
                if (match_heap)
                {
                    // re-use ressources
                    r_cleanup_ptr = heap_entry.next;
                    r_cleanup_fsm = CLEANUP_HEAP_CLEAN;
                }
                // test the next in the linked list
                else
                {
                    r_cleanup_prev_ptr   = r_cleanup_next_ptr.read();
                    r_cleanup_prev_srcid = heap_entry.owner.srcid;
                    r_cleanup_prev_inst  = heap_entry.owner.inst;
                    r_cleanup_next_ptr   = heap_entry.next;
                    r_cleanup_fsm        = CLEANUP_HEAP_SEARCH;
                }

#if DEBUG_MEMC_CLEANUP
if (m_debug)
{
    if (not match_heap)
    {
    std::cout << "  <MEMC " << name()
              << " CLEANUP_HEAP_SEARCH> Matching copy not found, search next:"
              << std::endl;
    }
    else
    {
    std::cout << "  <MEMC " << name()
              << " CLEANUP_HEAP_SEARCH> Matching copy found:"
              << std::endl;
    }
    std::cout << " address = "      << r_cleanup_nline.read() * m_words * 4
              << " / heap_id = "    << heap_entry.owner.srcid
              << " / heap_ins = "   << heap_entry.owner.inst
              << " / search_id = "  << r_cleanup_srcid.read()
              << " / search_ins = " << r_cleanup_inst.read()
              << " / last = "       << last
              << std::endl;
}
#endif
                break;
            }
            ////////////////////////
            case CLEANUP_HEAP_CLEAN:    // remove a copy in the linked list
            {
                assert((r_alloc_heap_fsm.read() == ALLOC_HEAP_CLEANUP) and
                        "MEMC ERROR in CLEANUP_HEAP_LOCK state: bad HEAP allocation");

                HeapEntry heap_entry;
                heap_entry.owner.srcid = r_cleanup_prev_srcid.read();
                heap_entry.owner.inst  = r_cleanup_prev_inst.read();
                bool last = (r_cleanup_next_ptr.read() == r_cleanup_ptr.read());

                if (last)     // this is the last entry of the list of copies
                {
                    heap_entry.next = r_cleanup_prev_ptr.read();
                }
                else          // this is not the last entry
                {
                    heap_entry.next = r_cleanup_ptr.read();
                }

                m_heap.write(r_cleanup_prev_ptr.read(), heap_entry);

                r_cleanup_fsm = CLEANUP_HEAP_FREE;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name() << " CLEANUP_HEAP_SEARCH>"
          << " Remove the copy in the linked list" << std::endl;
#endif
                break;
            }
            ///////////////////////
            case CLEANUP_HEAP_FREE:   // The heap entry pointed by r_cleanup_next_ptr is freed
            // and becomes the head of the list of free entries
            {
                assert((r_alloc_heap_fsm.read() == ALLOC_HEAP_CLEANUP) and
                        "MEMC ERROR in CLEANUP_HEAP_LOCK state: bad HEAP allocation");

                HeapEntry heap_entry;
                heap_entry.owner.srcid = 0;
                heap_entry.owner.inst  = false;

                if (m_heap.is_full())
                {
                    heap_entry.next = r_cleanup_next_ptr.read();
                }
                else
                {
                    heap_entry.next = m_heap.next_free_ptr();
                }

                m_heap.write(r_cleanup_next_ptr.read(),heap_entry);
                m_heap.write_free_ptr(r_cleanup_next_ptr.read());
                m_heap.unset_full();

                r_cleanup_fsm = CLEANUP_SEND_CLACK;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name() << " CLEANUP_HEAP_FREE>"
          << " Update the list of free entries" << std::endl;
#endif
                break;
            }
            //////////////////////
            case CLEANUP_IVT_LOCK:   // get the lock protecting the IVT to search a pending
                                     // invalidate transaction matching the cleanup
            {
                if (r_alloc_ivt_fsm.read() != ALLOC_IVT_CLEANUP) break;

                size_t index = 0;
                bool match_inval;

                match_inval = m_ivt.search_inval(r_cleanup_nline.read(), index);

                if (not match_inval) // no pending inval in IVT
                {
                    r_cleanup_fsm = CLEANUP_SEND_CLACK;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name() << " CLEANUP_IVT_LOCK>"
          << " Unexpected cleanup with no corresponding IVT entry:"
          << " address = " << std::hex << (r_cleanup_nline.read() * 4 * m_words) << std::endl;
#endif
                }
                else // pending inval in IVT
                {
                    r_cleanup_write_srcid = m_ivt.srcid(index);
                    r_cleanup_write_trdid = m_ivt.trdid(index);
                    r_cleanup_write_pktid = m_ivt.pktid(index);
                    r_cleanup_need_rsp    = m_ivt.need_rsp(index);
                    r_cleanup_need_ack    = m_ivt.need_ack(index);
                    r_cleanup_index       = index;
                    r_cleanup_fsm         = CLEANUP_IVT_DECREMENT;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name() << " CLEANUP_IVT_LOCK>"
          << " Cleanup matching pending invalidate transaction on IVT:"
          << " address = " << std::hex << (r_cleanup_nline.read() * m_words * 4)
          << " / ivt_entry = " << index << std::endl;
#endif
                }
                break;
            }
            ///////////////////////////
            case CLEANUP_IVT_DECREMENT: // decrement response counter in IVT matching entry
            // and test if last
            {
                assert((r_alloc_ivt_fsm.read() == ALLOC_IVT_CLEANUP) and
                        "MEMC ERROR in CLEANUP_IVT_DECREMENT state: Bad IVT allocation");

                size_t count = 0;
                m_ivt.decrement(r_cleanup_index.read(), count);

                if (count == 0) r_cleanup_fsm = CLEANUP_IVT_CLEAR;
                else            r_cleanup_fsm = CLEANUP_SEND_CLACK ;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name() << " CLEANUP_IVT_DECREMENT>"
          << " Decrement response counter in IVT:"
          << " IVT_index = " << r_cleanup_index.read()
          << " / rsp_count = " << count << std::endl;
#endif
                break;
            }
            ///////////////////////
            case CLEANUP_IVT_CLEAR:    // Clear IVT entry
                                       // Acknowledge CONFIG FSM if required
            {
                assert((r_alloc_ivt_fsm.read() == ALLOC_IVT_CLEANUP) and
                "MEMC ERROR in CLEANUP_IVT_CLEAR state : bad IVT allocation");

                m_ivt.clear(r_cleanup_index.read());

                if (r_cleanup_need_ack.read())
                {
                    assert((r_config_rsp_lines.read() > 0) and
                    "MEMC ERROR in CLEANUP_IVT_CLEAR state");

                    config_rsp_lines_cleanup_decr = true;
                }

                if (r_cleanup_need_rsp.read()) r_cleanup_fsm = CLEANUP_WRITE_RSP;
                else                           r_cleanup_fsm = CLEANUP_SEND_CLACK;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name()
          << " CLEANUP_IVT_CLEAR> Clear entry in IVT:"
          << " IVT_index = " << r_cleanup_index.read() << std::endl;
#endif
                break;
            }
            ///////////////////////
            case CLEANUP_WRITE_RSP:    // response to a previous write on the direct network
            // wait if pending request to the TGT_RSP FSM
            {
                if (r_cleanup_to_tgt_rsp_req.read()) break;

                // no pending request
                r_cleanup_to_tgt_rsp_req   = true;
                r_cleanup_to_tgt_rsp_srcid = r_cleanup_write_srcid.read();
                r_cleanup_to_tgt_rsp_trdid = r_cleanup_write_trdid.read();
                r_cleanup_to_tgt_rsp_pktid = r_cleanup_write_pktid.read();
                r_cleanup_fsm              = CLEANUP_SEND_CLACK;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name() << " CLEANUP_WRITE_RSP>"
          << " Send a response to a previous write request: "
          << " rsrcid = "   << std::hex << r_cleanup_write_srcid.read()
          << " / rtrdid = " << r_cleanup_write_trdid.read()
          << " / rpktid = " << r_cleanup_write_pktid.read() << std::endl;
#endif
                break;
            }
            ////////////////////////
            case CLEANUP_SEND_CLACK:  // acknowledgement to a cleanup command
            // on the coherence CLACK network.
            {
                if (not p_dspin_clack.read) break;

                r_cleanup_fsm = CLEANUP_IDLE;

#if DEBUG_MEMC_CLEANUP
if (m_debug)
std::cout << "  <MEMC " << name()
          << " CLEANUP_SEND_CLACK> Send the response to a cleanup request:"
          << " address = "   << std::hex << r_cleanup_nline.read() * m_words * 4
          << " / way = "   << std::dec << r_cleanup_way.read()
          << " / srcid = " << std::hex << r_cleanup_srcid.read()
          << std::endl;
#endif
                break;
            }
        } // end switch cleanup fsm

        ////////////////////////////////////////////////////////////////////////////////////
        //    CAS FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The CAS FSM handles the CAS (Compare And Swap) atomic commands.
        //
        // This command contains two or four flits:
        // - In case of 32 bits atomic access, the first flit contains the value read
        // by a previous READ instruction, the second flit contains the value to be writen.
        // - In case of 64 bits atomic access, the 2 first flits contains the value read
        // by a previous READ instruction, the 2 next flits contains the value to be writen.
        //
        // The target address is cachable. If it is replicated in other L1 caches
        // than the writer, a coherence operation is done.
        //
        // It access the directory to check hit / miss.
        // - In case of miss, the CAS FSM must register a GET transaction in TRT.
        //   If a read transaction to the XRAM for this line already exists,
        //   or if the transaction table is full, it goes to the WAIT state
        //   to release the locks and try again. When the GET transaction has been
        //   launched, it goes to the WAIT state and try again.
        //   The CAS request is not consumed in the FIFO until a HIT is obtained.
        // - In case of hit...
        ///////////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "cas_fsm" << std::endl;

        switch (r_cas_fsm.read())
        {
            ////////////
            case CAS_IDLE:     // fill the local rdata buffers
            {
                if (m_cmd_cas_addr_fifo.rok())
                {

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " CAS_IDLE> CAS command: " << std::hex
                            << " srcid = " <<  std::dec << m_cmd_cas_srcid_fifo.read()
                            << " addr = " << std::hex << m_cmd_cas_addr_fifo.read()
                            << " wdata = " << m_cmd_cas_wdata_fifo.read()
                            << " eop = " << std::dec << m_cmd_cas_eop_fifo.read()
                            << " cpt  = " << std::dec << r_cas_cpt.read() << std::endl;
                    }
#endif
                    if (m_cmd_cas_eop_fifo.read())
                    {
                        r_cas_fsm = CAS_DIR_REQ;
                    }
                    else  // we keep the last word in the FIFO
                    {
                        cmd_cas_fifo_get = true;
                    }

                    // We fill the two buffers
                    if (r_cas_cpt.read() < 2)    // 32 bits access
                    {
                        r_cas_rdata[r_cas_cpt.read()] = m_cmd_cas_wdata_fifo.read();
                    }

                    if ((r_cas_cpt.read() == 1) and m_cmd_cas_eop_fifo.read())
                    {
                        r_cas_wdata = m_cmd_cas_wdata_fifo.read();
                    }

                    assert((r_cas_cpt.read() <= 3) and  // no more than 4 flits...
                            "MEMC ERROR in CAS_IDLE state: illegal CAS command");

                    if (r_cas_cpt.read() == 2)
                    {
                        r_cas_wdata = m_cmd_cas_wdata_fifo.read();
                    }

                    r_cas_cpt = r_cas_cpt.read() + 1;
                }
                break;
            }
            /////////////////
            case CAS_DIR_REQ:
            {
                if (r_alloc_dir_fsm.read() == ALLOC_DIR_CAS)
                {
                    r_cas_fsm = CAS_DIR_LOCK;
                }

#if DEBUG_MEMC_CAS
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " CAS_DIR_REQ> Requesting DIR lock " << std::endl;
                }
#endif
                break;
            }
            /////////////////
            case CAS_DIR_LOCK:  // Read the directory
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CAS) and
                        "MEMC ERROR in CAS_DIR_LOCK: Bad DIR allocation");

                size_t way = 0;
                DirectoryEntry entry(m_cache_directory.read(m_cmd_cas_addr_fifo.read(), way));

                r_cas_is_cnt    = entry.is_cnt;
                r_cas_dirty     = entry.dirty;
                r_cas_tag       = entry.tag;
                r_cas_way       = way;
                r_cas_copy      = entry.owner.srcid;
                r_cas_copy_inst = entry.owner.inst;
                r_cas_ptr       = entry.ptr;
                r_cas_count     = entry.count;

                if (entry.valid)  r_cas_fsm = CAS_DIR_HIT_READ;
                else              r_cas_fsm = CAS_MISS_TRT_LOCK;

#if DEBUG_MEMC_CAS
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " CAS_DIR_LOCK> Directory acces"
                        << " / address = " << std::hex << m_cmd_cas_addr_fifo.read()
                        << " / hit = " << std::dec << entry.valid
                        << " / count = " << entry.count
                        << " / is_cnt = " << entry.is_cnt << std::endl;
                }
#endif

                break;
            }
            /////////////////////
            case CAS_DIR_HIT_READ:  // update directory for lock and dirty bit
            // and check data change in cache
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CAS) and
                        "MEMC ERROR in CAS_DIR_HIT_READ: Bad DIR allocation");

                size_t way = r_cas_way.read();
                size_t set = m_y[(addr_t)(m_cmd_cas_addr_fifo.read())];

                // update directory (lock & dirty bits)
                DirectoryEntry entry;
                entry.valid       = true;
                entry.is_cnt      = r_cas_is_cnt.read();
                entry.dirty       = true;
                entry.lock        = true;
                entry.tag         = r_cas_tag.read();
                entry.owner.srcid = r_cas_copy.read();
                entry.owner.inst  = r_cas_copy_inst.read();
                entry.count       = r_cas_count.read();
                entry.ptr         = r_cas_ptr.read();

                m_cache_directory.write(set, way, entry);

                // Store data from cache in buffer to do the comparison in next state
                m_cache_data.read_line(way, set, r_cas_data);

                r_cas_fsm = CAS_DIR_HIT_COMPARE;

#if DEBUG_MEMC_CAS
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " CAS_DIR_HIT_READ> Read data from "
                        << " cache and store it in buffer" << std::endl;
                }
#endif
                break;
            }
            ////////////////////////
            case CAS_DIR_HIT_COMPARE:
            {
                size_t word = m_x[(addr_t)(m_cmd_cas_addr_fifo.read())];

                // check data change
                bool ok = (r_cas_rdata[0].read() == r_cas_data[word].read());

                if (r_cas_cpt.read() == 4)     // 64 bits CAS
                    ok &= (r_cas_rdata[1] == r_cas_data[word+1]);

                // to avoid livelock, force the atomic access to fail pseudo-randomly
                bool forced_fail = ((r_cas_lfsr % (64) == 0) and RANDOMIZE_CAS);
                r_cas_lfsr = (r_cas_lfsr >> 1) ^ ((- (r_cas_lfsr & 1)) & 0xd0000001);

                if (ok and not forced_fail) r_cas_fsm = CAS_DIR_HIT_WRITE;
                else                        r_cas_fsm = CAS_RSP_FAIL;

#if DEBUG_MEMC_CAS
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " CAS_DIR_HIT_COMPARE> Compare old and new data"
                        << " / expected value = " << std::hex << r_cas_rdata[0].read()
                        << " / actual value = "   << std::hex << r_cas_data[word].read()
                        << " / forced_fail = "    << std::dec << forced_fail << std::endl;
                }
#endif
                break;
            }
            //////////////////////
            case CAS_DIR_HIT_WRITE:    // test if a CC transaction is required
            // write data in cache if no CC request
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CAS) and
                        "MEMC ERROR in CAS_DIR_HIT_WRITE: Bad DIR allocation");

                // The CAS is a success => sw access to the llsc_global_table
                m_llsc_table.sw(m_cmd_cas_addr_fifo.read(), m_cmd_cas_addr_fifo.read());

                // test coherence request
                if (r_cas_count.read())   // replicated line
                {
                    if (r_cas_is_cnt.read())
                    {
                        r_cas_fsm = CAS_BC_TRT_LOCK;    // broadcast invalidate required

#if DEBUG_MEMC_CAS
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " CAS_DIR_HIT_WRITE>"
                                << " Broacast Inval required"
                                << " / copies = " << r_cas_count.read() << std::endl;
                        }
#endif
                    }
                    else if (not r_cas_to_cc_send_multi_req.read() and
                            not r_cas_to_cc_send_brdcast_req.read())
                    {
                        r_cas_fsm = CAS_UPT_LOCK;       // multi update required

#if DEBUG_MEMC_CAS
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " CAS_DIR_HIT_WRITE>"
                                << " Multi Inval required"
                                << " / copies = " << r_cas_count.read() << std::endl;
                        }
#endif
                    }
                    else
                    {
                        r_cas_fsm = CAS_WAIT;

#if DEBUG_MEMC_CAS
                        if (m_debug)
                        {
                            std::cout << "  <MEMC " << name() << " CAS_DIR_HIT_WRITE>"
                                << " CC_SEND FSM busy: release all locks and retry" << std::endl;
                        }
#endif
                    }
                }
                else                    // no copies
                {
                    size_t way  = r_cas_way.read();
                    size_t set  = m_y[(addr_t) (m_cmd_cas_addr_fifo.read())];
                    size_t word = m_x[(addr_t) (m_cmd_cas_addr_fifo.read())];

                    // cache update
                    m_cache_data.write(way, set, word, r_cas_wdata.read());
                    if (r_cas_cpt.read() == 4)
                    {
                        m_cache_data.write(way, set, word + 1, m_cmd_cas_wdata_fifo.read());
                    }

                    r_cas_fsm = CAS_RSP_SUCCESS;

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " CAS_DIR_HIT_WRITE> Update cache:"
                            << " way = " << std::dec << way
                            << " / set = " << set
                            << " / word = " << word
                            << " / value = " << r_cas_wdata.read()
                            << " / count = " << r_cas_count.read()
                            << " / global_llsc_table access" << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////
            case CAS_UPT_LOCK:  // try to register the transaction in UPT
            // and write data in cache if successful registration
            // releases locks to retry later if UPT full
            {
                if (r_alloc_upt_fsm.read() == ALLOC_UPT_CAS)
                {
                    bool        wok        = false;
                    size_t      index      = 0;
                    size_t      srcid      = m_cmd_cas_srcid_fifo.read();
                    size_t      trdid      = m_cmd_cas_trdid_fifo.read();
                    size_t      pktid      = m_cmd_cas_pktid_fifo.read();
                    addr_t      nline      = m_nline[(addr_t)(m_cmd_cas_addr_fifo.read())];
                    size_t      nb_copies  = r_cas_count.read();

                    wok = m_upt.set(true,    // it's an update transaction
                            false,   // it's not a broadcast
                            true,    // response required
                            false,   // no acknowledge required
                            srcid,
                            trdid,
                            pktid,
                            nline,
                            nb_copies,
                            index);
                    if (wok)   // coherence transaction registered in UPT
                    {
                        // cache update
                        size_t way  = r_cas_way.read();
                        size_t set  = m_y[(addr_t) (m_cmd_cas_addr_fifo.read())];
                        size_t word = m_x[(addr_t) (m_cmd_cas_addr_fifo.read())];

                        m_cache_data.write(way, set, word, r_cas_wdata.read());
                        if (r_cas_cpt.read() == 4)
                        {
                            m_cache_data.write(way, set, word + 1, m_cmd_cas_wdata_fifo.read());
                        }

                        r_cas_upt_index = index;
                        r_cas_fsm = CAS_UPT_HEAP_LOCK;
                    }
                    else       //  releases the locks protecting UPT and DIR UPT full
                    {
                        r_cas_fsm = CAS_WAIT;
                    }

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " CAS_UPT_LOCK> Register multi-update transaction in UPT"
                            << " / wok = " << wok
                            << " / address  = " << std::hex << nline * m_words * 4
                            << " / count = " << nb_copies << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////
            case CAS_WAIT:   // release all locks and retry from beginning
            {

#if DEBUG_MEMC_CAS
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " CAS_WAIT> Release all locks" << std::endl;
                }
#endif
                r_cas_fsm = CAS_DIR_REQ;
                break;
            }
            //////////////////////
            case CAS_UPT_HEAP_LOCK:  // lock the heap
            {
                if (r_alloc_heap_fsm.read() == ALLOC_HEAP_CAS)
                {

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " CAS_UPT_HEAP_LOCK> Get access to the heap" << std::endl;
                    }
#endif
                    r_cas_fsm = CAS_UPT_REQ;
                }
                break;
            }
            ////////////////
            case CAS_UPT_REQ:  // send a first update request to CC_SEND FSM
            {
                assert((r_alloc_heap_fsm.read() == ALLOC_HEAP_CAS) and
                        "VCI_MEM_CACHE ERROR : bad HEAP allocation");

                if (!r_cas_to_cc_send_multi_req.read() and !r_cas_to_cc_send_brdcast_req.read())
                {
                    r_cas_to_cc_send_brdcast_req = false;
                    r_cas_to_cc_send_trdid       = r_cas_upt_index.read();
                    r_cas_to_cc_send_nline       = m_nline[(addr_t) (m_cmd_cas_addr_fifo.read())];
                    r_cas_to_cc_send_index       = m_x[(addr_t) (m_cmd_cas_addr_fifo.read())];
                    r_cas_to_cc_send_wdata       = r_cas_wdata.read();

                    if (r_cas_cpt.read() == 4)
                    {
                        r_cas_to_cc_send_is_long    = true;
                        r_cas_to_cc_send_wdata_high = m_cmd_cas_wdata_fifo.read();
                    }
                    else
                    {
                        r_cas_to_cc_send_is_long    = false;
                        r_cas_to_cc_send_wdata_high = 0;
                    }

                    // We put the first copy in the fifo
                    cas_to_cc_send_fifo_put   = true;
                    cas_to_cc_send_fifo_inst  = r_cas_copy_inst.read();
                    cas_to_cc_send_fifo_srcid = r_cas_copy.read();
                    if (r_cas_count.read() == 1)  // one single copy
                    {
                        r_cas_fsm = CAS_IDLE;   // Response will be sent after receiving
                        // update responses
                        cmd_cas_fifo_get = true;
                        r_cas_to_cc_send_multi_req = true;
                        r_cas_cpt = 0;
                    }
                    else      // several copies
                    {
                        r_cas_fsm = CAS_UPT_NEXT;
                    }

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " CAS_UPT_REQ> Send the first update request to CC_SEND FSM "
                            << " / address = " << std::hex << m_cmd_cas_addr_fifo.read()
                            << " / wdata = " << std::hex << r_cas_wdata.read()
                            << " / srcid = " << std::dec << r_cas_copy.read()
                            << " / inst = " << std::dec << r_cas_copy_inst.read() << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////
            case CAS_UPT_NEXT:     // send a multi-update request to CC_SEND FSM
            {
                assert((r_alloc_heap_fsm.read() == ALLOC_HEAP_CAS)
                        and "VCI_MEM_CACHE ERROR : bad HEAP allocation");

                HeapEntry entry = m_heap.read(r_cas_ptr.read());
                cas_to_cc_send_fifo_srcid = entry.owner.srcid;
                cas_to_cc_send_fifo_inst  = entry.owner.inst;
                cas_to_cc_send_fifo_put = true;

                if (m_cas_to_cc_send_inst_fifo.wok())   // request accepted by CC_SEND FSM
                {
                    r_cas_ptr = entry.next;
                    if (entry.next == r_cas_ptr.read())    // last copy
                    {
                        r_cas_to_cc_send_multi_req = true;
                        r_cas_fsm = CAS_IDLE;   // Response will be sent after receiving
                        // all update responses
                        cmd_cas_fifo_get = true;
                        r_cas_cpt        = 0;
                    }
                }

#if DEBUG_MEMC_CAS
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " CAS_UPT_NEXT> Send the next update request to CC_SEND FSM "
                        << " / address = " << std::hex << m_cmd_cas_addr_fifo.read()
                        << " / wdata = " << std::hex << r_cas_wdata.read()
                        << " / srcid = " << std::dec << entry.owner.srcid
                        << " / inst = " << std::dec << entry.owner.inst << std::endl;
                }
#endif
                break;
            }
            /////////////////////
            case CAS_BC_TRT_LOCK:      // get TRT lock to check TRT not full
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CAS) and
                        "MEMC ERROR in CAS_BC_TRT_LOCK state: Bas DIR allocation");

                if (r_alloc_trt_fsm.read() == ALLOC_TRT_CAS)
                {
                    size_t wok_index = 0;
                    bool   wok       = !m_trt.full(wok_index);
                    if (wok)
                    {
                        r_cas_trt_index = wok_index;
                        r_cas_fsm = CAS_BC_IVT_LOCK;
                    }
                    else
                    {
                        r_cas_fsm = CAS_WAIT;
                    }

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " CAS_BC_TRT_LOCK> Check TRT"
                            << " : wok = " << wok << " / index = " << wok_index << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////////
            case CAS_BC_IVT_LOCK:  // get IVT lock and register BC transaction in IVT
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CAS) and
                        "MEMC ERROR in CAS_BC_IVT_LOCK state: Bas DIR allocation");

                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_CAS) and
                        "MEMC ERROR in CAS_BC_IVT_LOCK state: Bas TRT allocation");

                if (r_alloc_ivt_fsm.read() == ALLOC_IVT_CAS)
                {
                    // register broadcast inval transaction in IVT
                    bool        wok       = false;
                    size_t      index     = 0;
                    size_t      srcid     = m_cmd_cas_srcid_fifo.read();
                    size_t      trdid     = m_cmd_cas_trdid_fifo.read();
                    size_t      pktid     = m_cmd_cas_pktid_fifo.read();
                    addr_t      nline     = m_nline[(addr_t)(m_cmd_cas_addr_fifo.read())];
                    size_t      nb_copies = r_cas_count.read();

                    wok = m_ivt.set(false,  // it's an inval transaction
                            true,   // it's a broadcast
                            true,   // response required
                            false,  // no acknowledge required
                            srcid,
                            trdid,
                            pktid,
                            nline,
                            nb_copies,
                            index);
#if DEBUG_MEMC_CAS
                    if (m_debug and wok)
                    {
                        std::cout << "  <MEMC " << name() << " CAS_BC_IVT_LOCK> Register broadcast inval in IVT"
                            << " / copies = " << r_cas_count.read() << std::endl;
                    }
#endif
                    r_cas_upt_index = index;
                    if (wok) r_cas_fsm = CAS_BC_DIR_INVAL;
                    else     r_cas_fsm = CAS_WAIT;
                }
                break;
            }
            //////////////////////
            case CAS_BC_DIR_INVAL:  // Register PUT transaction in TRT,
            // and inval the DIR entry
            {
                assert((r_alloc_dir_fsm.read() == ALLOC_DIR_CAS) and
                        "MEMC ERROR in CAS_BC_DIR_INVAL state: Bad DIR allocation");

                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_CAS) and
                        "MEMC ERROR in CAS_BC_DIR_INVAL state: Bad TRT allocation");

                assert((r_alloc_ivt_fsm.read() == ALLOC_IVT_CAS) and
                        "MEMC ERROR in CAS_BC_DIR_INVAL state: Bad IVT allocation");

                // set TRT
                std::vector<data_t> data_vector;
                data_vector.clear();
                size_t word = m_x[(addr_t)(m_cmd_cas_addr_fifo.read())];
                for (size_t i = 0; i < m_words; i++)
                {
                    if (i == word)
                    {
                        // first modified word
                        data_vector.push_back(r_cas_wdata.read());
                    }
                    else if ((i == word + 1) and (r_cas_cpt.read() == 4))
                    {
                        // second modified word
                        data_vector.push_back(m_cmd_cas_wdata_fifo.read());
                    }
                    else
                    {
                        // unmodified words
                        data_vector.push_back(r_cas_data[i].read());
                    }
                }
                m_trt.set(r_cas_trt_index.read(),
                        false,    // PUT request
                        m_nline[(addr_t)(m_cmd_cas_addr_fifo.read())],
                        0,
                        0,
                        0,
                        false,    // not a processor read
                        0,
                        0,
                        std::vector<be_t> (m_words,0),
                        data_vector);

                // invalidate directory entry
                DirectoryEntry entry;
                entry.valid       = false;
                entry.dirty       = false;
                entry.tag         = 0;
                entry.is_cnt      = false;
                entry.lock        = false;
                entry.count       = 0;
                entry.owner.srcid = 0;
                entry.owner.inst  = false;
                entry.ptr         = 0;
                size_t set        = m_y[(addr_t) (m_cmd_cas_addr_fifo.read())];
                size_t way        = r_cas_way.read();

                m_cache_directory.write(set, way, entry);

                r_cas_fsm = CAS_BC_CC_SEND;

#if DEBUG_MEMC_CAS
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " CAS_BC_DIR_INVAL> Inval DIR & register in TRT:"
                        << " address = " << m_cmd_cas_addr_fifo.read() << std::endl;
                }
#endif
                break;
            }
            ///////////////////
            case CAS_BC_CC_SEND:  // Request the broadcast inval to CC_SEND FSM
            {
                if (not r_cas_to_cc_send_multi_req.read() and
                        not r_cas_to_cc_send_brdcast_req.read())
                {
                    r_cas_to_cc_send_multi_req   = false;
                    r_cas_to_cc_send_brdcast_req = true;
                    r_cas_to_cc_send_trdid       = r_cas_upt_index.read();
                    r_cas_to_cc_send_nline       = m_nline[(addr_t)(m_cmd_cas_addr_fifo.read())];
                    r_cas_to_cc_send_index       = 0;
                    r_cas_to_cc_send_wdata       = 0;

                    r_cas_fsm = CAS_BC_XRAM_REQ;

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " CAS_BC_CC_SEND> Post a broadcast request to CC_SEND FSM" << std::endl;
                    }
#endif
                }
                break;
            }
            ////////////////////
            case CAS_BC_XRAM_REQ: // request the IXR FSM to start a PUT transaction
            {
                if (not r_cas_to_ixr_cmd_req.read())
                {
                    r_cas_to_ixr_cmd_req   = true;
                    r_cas_to_ixr_cmd_index = r_cas_trt_index.read();
                    r_cas_fsm              = CAS_IDLE;
                    cmd_cas_fifo_get       = true;
                    r_cas_cpt              = 0;

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " CAS_BC_XRAM_REQ> Request a PUT transaction to IXR_CMD FSM" << std::hex
                            << " / address = " << (addr_t) m_cmd_cas_addr_fifo.read()
                            << " / trt_index = " << r_cas_trt_index.read() << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////
            case CAS_RSP_FAIL:  // request TGT_RSP FSM to send a failure response
            {
                if (not r_cas_to_tgt_rsp_req.read())
                {
                    cmd_cas_fifo_get       = true;
                    r_cas_cpt              = 0;
                    r_cas_to_tgt_rsp_req   = true;
                    r_cas_to_tgt_rsp_data  = 1;
                    r_cas_to_tgt_rsp_srcid = m_cmd_cas_srcid_fifo.read();
                    r_cas_to_tgt_rsp_trdid = m_cmd_cas_trdid_fifo.read();
                    r_cas_to_tgt_rsp_pktid = m_cmd_cas_pktid_fifo.read();
                    r_cas_fsm              = CAS_IDLE;

#if DEBUG_MEMC_CAS
                    if (m_debug)
                        std::cout << "  <MEMC " << name()
                            << " CAS_RSP_FAIL> Request TGT_RSP to send a failure response" << std::endl;
#endif
                }
                break;
            }
            ////////////////////
            case CAS_RSP_SUCCESS:  // request TGT_RSP FSM to send a success response
            {
                if (not r_cas_to_tgt_rsp_req.read())
                {
                    cmd_cas_fifo_get       = true;
                    r_cas_cpt              = 0;
                    r_cas_to_tgt_rsp_req   = true;
                    r_cas_to_tgt_rsp_data  = 0;
                    r_cas_to_tgt_rsp_srcid = m_cmd_cas_srcid_fifo.read();
                    r_cas_to_tgt_rsp_trdid = m_cmd_cas_trdid_fifo.read();
                    r_cas_to_tgt_rsp_pktid = m_cmd_cas_pktid_fifo.read();
                    r_cas_fsm              = CAS_IDLE;

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name()
                            << " CAS_RSP_SUCCESS> Request TGT_RSP to send a success response" << std::endl;
                    }
#endif
                }
                break;
            }
            ///////////////////////
            case CAS_MISS_TRT_LOCK: // cache miss : request access to transaction Table
            {
                if (r_alloc_trt_fsm.read() == ALLOC_TRT_CAS)
                {
                    size_t index = 0;
                    bool hit_read = m_trt.hit_read(
                            m_nline[(addr_t) m_cmd_cas_addr_fifo.read()],index);
                    bool hit_write = m_trt.hit_write(
                            m_nline[(addr_t) m_cmd_cas_addr_fifo.read()]);
                    bool wok = not m_trt.full(index);

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " CAS_MISS_TRT_LOCK> Check TRT state"
                            << " / hit_read = "  << hit_read
                            << " / hit_write = " << hit_write
                            << " / wok = " << wok
                            << " / index = " << index << std::endl;
                    }
#endif

                    if (hit_read or !wok or hit_write)    // missing line already requested or TRT full
                    {
                        r_cas_fsm = CAS_WAIT;
                    }
                    else
                    {
                        r_cas_trt_index = index;
                        r_cas_fsm       = CAS_MISS_TRT_SET;
                    }
                }
                break;
            }
            //////////////////////
            case CAS_MISS_TRT_SET: // register the GET transaction in TRT
            {
                assert((r_alloc_trt_fsm.read() == ALLOC_TRT_CAS) and
                        "MEMC ERROR in CAS_MISS_TRT_SET state: Bad TRT allocation");

                std::vector<be_t> be_vector;
                std::vector<data_t> data_vector;
                be_vector.clear();
                data_vector.clear();
                for (size_t i = 0; i < m_words; i++)
                {
                    be_vector.push_back(0);
                    data_vector.push_back(0);
                }

                m_trt.set(r_cas_trt_index.read(),
                        true,     // GET
                        m_nline[(addr_t) m_cmd_cas_addr_fifo.read()],
                        m_cmd_cas_srcid_fifo.read(),
                        m_cmd_cas_trdid_fifo.read(),
                        m_cmd_cas_pktid_fifo.read(),
                        false,    // write request from processor
                        0,
                        0,
                        std::vector<be_t>(m_words, 0),
                        std::vector<data_t>(m_words, 0));

                r_cas_fsm = CAS_MISS_XRAM_REQ;

#if DEBUG_MEMC_CAS
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name() << " CAS_MISS_TRT_SET> Register GET transaction in TRT"
                        << " / address = " << std::hex << (addr_t)m_cmd_cas_addr_fifo.read()
                        << " / trt_index = " << std::dec << r_cas_trt_index.read() << std::endl;
                }
#endif
                break;
            }
            //////////////////////
            case CAS_MISS_XRAM_REQ:  // request the IXR_CMD FSM a GET request
            {
                if (not r_cas_to_ixr_cmd_req.read())
                {
                    r_cas_to_ixr_cmd_req   = true;
                    r_cas_to_ixr_cmd_index = r_cas_trt_index.read();
                    r_cas_fsm              = CAS_WAIT;

#if DEBUG_MEMC_CAS
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " CAS_MISS_XRAM_REQ> Request a GET transaction"
                            << " / address = " << std::hex << (addr_t) m_cmd_cas_addr_fifo.read()
                            << " / trt_index = " << std::dec << r_cas_trt_index.read() << std::endl;
                    }
#endif
                }
                break;
            }
        } // end switch r_cas_fsm


        //////////////////////////////////////////////////////////////////////////////
        //    CC_SEND FSM
        //////////////////////////////////////////////////////////////////////////////
        // The CC_SEND fsm controls the DSPIN initiator port on the coherence
        // network, used to update or invalidate cache lines in L1 caches.
        //
        // It implements a round-robin priority between the four possible client FSMs
        //     XRAM_RSP > CAS > WRITE > CONFIG
        //
        // Each FSM can request the next services:
        // - r_xram_rsp_to_cc_send_multi_req : multi-inval
        //   r_xram_rsp_to_cc_send_brdcast_req : broadcast-inval
        // - r_write_to_cc_send_multi_req : multi-update
        //   r_write_to_cc_send_brdcast_req : broadcast-inval
        // - r_cas_to_cc_send_multi_req : multi-update
        //   r_cas_to_cc_send_brdcast_req : broadcast-inval
        // - r_config_to_cc_send_multi_req : multi-inval
        //   r_config_to_cc_send_brdcast_req : broadcast-inval
        //
        // An inval request is a double DSPIN flit command containing:
        // 1. the index of the line to be invalidated.
        //
        // An update request is a multi-flit DSPIN command containing:
        // 1. the index of the cache line to be updated.
        // 2. the index of the first modified word in the line.
        // 3. the data to update
        ///////////////////////////////////////////////////////////////////////////////

        switch (r_cc_send_fsm.read())
        {
            /////////////////////////
            case CC_SEND_CONFIG_IDLE:    // XRAM_RSP FSM has highest priority
            {
                // XRAM_RSP
                if (m_xram_rsp_to_cc_send_inst_fifo.rok() or
                        r_xram_rsp_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_INVAL_HEADER;
                    break;
                }
                if (r_xram_rsp_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_BRDCAST_HEADER;
                    break;
                }
                // CAS
                if (m_cas_to_cc_send_inst_fifo.rok() or
                        r_cas_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_UPDT_HEADER;
                    break;
                }
                if (r_cas_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_BRDCAST_HEADER;
                    break;
                }
                // WRITE
                if (m_write_to_cc_send_inst_fifo.rok() or
                        r_write_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_WRITE_UPDT_HEADER;
                    break;
                }
                if (r_write_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_WRITE_BRDCAST_HEADER;
                    break;
                }
                // CONFIG
                if (r_config_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CONFIG_INVAL_HEADER;
                    break;
                }
                if (r_config_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CONFIG_BRDCAST_HEADER;
                    break;
                }
                break;
            }
            ////////////////////////
            case CC_SEND_WRITE_IDLE:     // CONFIG FSM has highest priority
            {
                // CONFIG
                if (r_config_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CONFIG_INVAL_HEADER;
                    break;
                }
                if (r_config_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CONFIG_BRDCAST_HEADER;
                    break;
                }
                // XRAM_RSP
                if (m_xram_rsp_to_cc_send_inst_fifo.rok() or
                        r_xram_rsp_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_INVAL_HEADER;
                    break;
                }
                if (r_xram_rsp_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_BRDCAST_HEADER;
                    break;
                }
                // CAS
                if (m_cas_to_cc_send_inst_fifo.rok() or
                        r_cas_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_UPDT_HEADER;
                    break;
                }
                if (r_cas_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_BRDCAST_HEADER;
                    break;
                }
                // WRITE
                if (m_write_to_cc_send_inst_fifo.rok() or
                        r_write_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_WRITE_UPDT_HEADER;
                    break;
                }
                if (r_write_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_WRITE_BRDCAST_HEADER;
                    break;
                }
                break;
            }
            ///////////////////////////
            case CC_SEND_XRAM_RSP_IDLE:   // CAS FSM has highest priority
            {
                // CAS
                if (m_cas_to_cc_send_inst_fifo.rok() or
                        r_cas_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_UPDT_HEADER;
                    break;
                }
                if (r_cas_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_BRDCAST_HEADER;
                    break;
                }
                // WRITE
                if (m_write_to_cc_send_inst_fifo.rok() or
                        r_write_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_WRITE_UPDT_HEADER;
                    break;
                }

                if (r_write_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_WRITE_BRDCAST_HEADER;
                    break;
                }
                // CONFIG
                if (r_config_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CONFIG_INVAL_HEADER;
                    break;
                }
                if (r_config_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CONFIG_BRDCAST_HEADER;
                    break;
                }
                // XRAM_RSP
                if (m_xram_rsp_to_cc_send_inst_fifo.rok() or
                        r_xram_rsp_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_INVAL_HEADER;
                    break;
                }
                if (r_xram_rsp_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_BRDCAST_HEADER;
                    break;
                }
                break;
            }
            //////////////////////
            case CC_SEND_CAS_IDLE:   // CLEANUP FSM has highest priority
            {
                if (m_write_to_cc_send_inst_fifo.rok() or
                        r_write_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_WRITE_UPDT_HEADER;
                    break;
                }
                if (r_write_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_WRITE_BRDCAST_HEADER;
                    break;
                }
                // CONFIG
                if (r_config_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CONFIG_INVAL_HEADER;
                    break;
                }
                if (r_config_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CONFIG_BRDCAST_HEADER;
                    break;
                }
                if (m_xram_rsp_to_cc_send_inst_fifo.rok() or
                        r_xram_rsp_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_INVAL_HEADER;
                    break;
                }
                if (r_xram_rsp_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_BRDCAST_HEADER;
                    break;
                }
                if (m_cas_to_cc_send_inst_fifo.rok() or
                        r_cas_to_cc_send_multi_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_UPDT_HEADER;
                    break;
                }
                if (r_cas_to_cc_send_brdcast_req.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_BRDCAST_HEADER;
                    break;
                }
                break;
            }
            /////////////////////////////////
            case CC_SEND_CONFIG_INVAL_HEADER:   // send first flit multi-inval (from CONFIG FSM)
            {
                if (m_config_to_cc_send_inst_fifo.rok())
                {
                    if (not p_dspin_m2p.read) break;
                    // <Activity Counters>
                    if (is_local_req(m_config_to_cc_send_srcid_fifo.read()))
                    {
                        m_cpt_minval_local++;
                    }
                    else
                    {
                        m_cpt_minval_remote++;
                    }
                    // 2 flits for multi inval
                    m_cpt_minval_cost += 2 * req_distance(m_config_to_cc_send_srcid_fifo.read());
                    // </Activity Counters>
                    r_cc_send_fsm = CC_SEND_CONFIG_INVAL_NLINE;
                    break;
                }
                if (r_config_to_cc_send_multi_req.read()) r_config_to_cc_send_multi_req = false;
                // <Activity Counters>
                m_cpt_minval++;
                // </Activity Counters>
                r_cc_send_fsm = CC_SEND_CONFIG_IDLE;
                break;
            }
            ////////////////////////////////
            case CC_SEND_CONFIG_INVAL_NLINE:    // send second flit multi-inval (from CONFIG FSM)
            {
                if (not p_dspin_m2p.read) break;
                config_to_cc_send_fifo_get = true;
                r_cc_send_fsm = CC_SEND_CONFIG_INVAL_HEADER;

#if DEBUG_MEMC_CC_SEND
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " CC_SEND_CONFIG_INVAL_NLINE> multi-inval for line "
                        << std::hex << r_config_to_cc_send_nline.read() << std::endl;
                }
#endif
                break;
            }
            ///////////////////////////////////
            case CC_SEND_CONFIG_BRDCAST_HEADER:   // send first flit BC-inval (from CONFIG FSM)
            {
                if (not p_dspin_m2p.read) break;
                r_cc_send_fsm = CC_SEND_CONFIG_BRDCAST_NLINE;
                break;
            }
            //////////////////////////////////
            case CC_SEND_CONFIG_BRDCAST_NLINE:    // send second flit BC-inval (from CONFIG FSM)
            {
                if (not p_dspin_m2p.read) break;
                // <Activity Counters>
                m_cpt_binval++;
                // </Activity Counters>
                r_config_to_cc_send_brdcast_req = false;
                r_cc_send_fsm = CC_SEND_CONFIG_IDLE;

#if DEBUG_MEMC_CC_SEND
                if (m_debug)
                    std::cout << "  <MEMC " << name()
                        << " CC_SEND_CONFIG_BRDCAST_NLINE> BC-Inval for line "
                        << std::hex << r_config_to_cc_send_nline.read() << std::endl;
#endif
                break;
            }
            ///////////////////////////////////
            case CC_SEND_XRAM_RSP_INVAL_HEADER:   // send first flit multi-inval (from XRAM_RSP FSM)
            {
                if (m_xram_rsp_to_cc_send_inst_fifo.rok())
                {
                    if (not p_dspin_m2p.read) break;
                    // <Activity Counters>
                    if (is_local_req(m_xram_rsp_to_cc_send_srcid_fifo.read()))
                    {
                        m_cpt_minval_local++;
                    }
                    else
                    {
                        m_cpt_minval_remote++;
                    }
                    // 2 flits for multi inval
                    m_cpt_minval_cost += 2 * req_distance(m_xram_rsp_to_cc_send_srcid_fifo.read());
                    // </Activity Counters>
                    r_cc_send_fsm = CC_SEND_XRAM_RSP_INVAL_NLINE;
                    break;
                }
                if (r_xram_rsp_to_cc_send_multi_req.read()) r_xram_rsp_to_cc_send_multi_req = false;
                // <Activity Counters>
                m_cpt_minval++;
                // </Activity Counters>
                r_cc_send_fsm = CC_SEND_XRAM_RSP_IDLE;
                break;
            }
            //////////////////////////////////
            case CC_SEND_XRAM_RSP_INVAL_NLINE:   // send second flit multi-inval (from XRAM_RSP FSM)
            {
                if (not p_dspin_m2p.read) break;
                xram_rsp_to_cc_send_fifo_get = true;
                r_cc_send_fsm = CC_SEND_XRAM_RSP_INVAL_HEADER;

#if DEBUG_MEMC_CC_SEND
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " CC_SEND_XRAM_RSP_INVAL_NLINE> Multicast-Inval for line "
                        << std::hex << r_xram_rsp_to_cc_send_nline.read() << std::endl;
                }
#endif
                break;
            }
            /////////////////////////////////////
            case CC_SEND_XRAM_RSP_BRDCAST_HEADER:  // send first flit broadcast-inval (from XRAM_RSP FSM)
            {
                if (not p_dspin_m2p.read) break;
                r_cc_send_fsm = CC_SEND_XRAM_RSP_BRDCAST_NLINE;
                break;
            }
            ////////////////////////////////////
            case CC_SEND_XRAM_RSP_BRDCAST_NLINE:   // send second flit broadcast-inval (from XRAM_RSP FSM)
            {
                if (not p_dspin_m2p.read) break;
                // <Activity Counters>
                m_cpt_binval++;
                // </Activity Counters>
                r_xram_rsp_to_cc_send_brdcast_req = false;
                r_cc_send_fsm = CC_SEND_XRAM_RSP_IDLE;

#if DEBUG_MEMC_CC_SEND
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " CC_SEND_XRAM_RSP_BRDCAST_NLINE> BC-Inval for line "
                        << std::hex << r_xram_rsp_to_cc_send_nline.read() << std::endl;
                }
#endif
                break;
            }
            //////////////////////////////////
            case CC_SEND_WRITE_BRDCAST_HEADER:   // send first flit broadcast-inval (from WRITE FSM)
            {
                if (not p_dspin_m2p.read) break;
                r_cc_send_fsm = CC_SEND_WRITE_BRDCAST_NLINE;
                break;
            }
            /////////////////////////////////
            case CC_SEND_WRITE_BRDCAST_NLINE:   // send second flit broadcast-inval (from WRITE FSM)
            {
                if (not p_dspin_m2p.read) break;

                // <Activity Counters>
                m_cpt_binval++;
                m_cpt_write_broadcast++;
                // </Activity Counters>

                r_write_to_cc_send_brdcast_req = false;
                r_cc_send_fsm = CC_SEND_WRITE_IDLE;

#if DEBUG_MEMC_CC_SEND
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " CC_SEND_WRITE_BRDCAST_NLINE> BC-Inval for line "
                        << std::hex << r_write_to_cc_send_nline.read() << std::endl;
                }
#endif
                break;
            }
            ///////////////////////////////
            case CC_SEND_WRITE_UPDT_HEADER:   // send first flit for a multi-update (from WRITE FSM)
            {
                if (m_write_to_cc_send_inst_fifo.rok())
                {
                    if (not p_dspin_m2p.read) break;
                    // <Activity Counters>
                    if (is_local_req(m_write_to_cc_send_srcid_fifo.read()))
                    {
                        m_cpt_update_local++;
                    }
                    else
                    {
                        m_cpt_update_remote++;
                    }
                    // 2 flits for multi update
                    m_cpt_update_cost += 2 * req_distance(m_write_to_cc_send_srcid_fifo.read());
                    // </Activity Counters>

                    r_cc_send_fsm = CC_SEND_WRITE_UPDT_NLINE;
                    break;
                }

                if (r_write_to_cc_send_multi_req.read())
                {
                    r_write_to_cc_send_multi_req = false;
                }

                // <Activity Counters>
                m_cpt_update++;
                // </Activity Counters>
                r_cc_send_fsm = CC_SEND_WRITE_IDLE;
                break;
            }
            //////////////////////////////
            case CC_SEND_WRITE_UPDT_NLINE:   // send second flit for a multi-update (from WRITE FSM)
            {
                if (not p_dspin_m2p.read) break;

                r_cc_send_cpt = 0;
                r_cc_send_fsm = CC_SEND_WRITE_UPDT_DATA;

#if DEBUG_MEMC_CC_SEND
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " CC_SEND_WRITE_UPDT_NLINE> Multicast-Update for address "
                        << r_write_to_cc_send_nline.read() * m_words * 4 << std::endl;
                }
#endif
                break;
            }
            /////////////////////////////
            case CC_SEND_WRITE_UPDT_DATA:   // send data flits for multi-update (from WRITE FSM)
            {
                if (not p_dspin_m2p.read) break;
                if (r_cc_send_cpt.read() == r_write_to_cc_send_count.read())
                {
                    write_to_cc_send_fifo_get = true;
                    r_cc_send_fsm = CC_SEND_WRITE_UPDT_HEADER;
                    break;
                }

                r_cc_send_cpt = r_cc_send_cpt.read() + 1;
                break;
            }
            ////////////////////////////////
            case CC_SEND_CAS_BRDCAST_HEADER:   // send first flit  broadcast-inval (from CAS FSM)
            {
                if (not p_dspin_m2p.read) break;
                r_cc_send_fsm = CC_SEND_CAS_BRDCAST_NLINE;
                break;
            }
            ///////////////////////////////
            case CC_SEND_CAS_BRDCAST_NLINE:   // send second flit broadcast-inval (from CAS FSM)
            {
                if (not p_dspin_m2p.read) break;
                // <Activity Counters>
                m_cpt_binval++;
                // </Activity Counters>

                r_cas_to_cc_send_brdcast_req = false;
                r_cc_send_fsm = CC_SEND_CAS_IDLE;

#if DEBUG_MEMC_CC_SEND
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " CC_SEND_CAS_BRDCAST_NLINE> Broadcast-Inval for address: "
                        << r_cas_to_cc_send_nline.read() * m_words * 4 << std::endl;
                }
#endif
                break;
            }
            /////////////////////////////
            case CC_SEND_CAS_UPDT_HEADER:   // send first flit for a multi-update (from CAS FSM)
            {
                if (m_cas_to_cc_send_inst_fifo.rok())
                {
                    if (not p_dspin_m2p.read) break;
                    // <Activity Counters>
                    if (is_local_req(m_cas_to_cc_send_srcid_fifo.read()))
                    {
                        m_cpt_update_local++;
                    }
                    else
                    {
                        m_cpt_update_remote++;
                    }
                    // 2 flits for multi update
                    m_cpt_update_cost += 2 * req_distance(m_cas_to_cc_send_srcid_fifo.read());
                    // </Activity Counters>
                    r_cc_send_fsm = CC_SEND_CAS_UPDT_NLINE;
                    break;
                }

                // no more packets to send for the multi-update
                if (r_cas_to_cc_send_multi_req.read())
                {
                    r_cas_to_cc_send_multi_req = false;
                }

                // <Activity Counters>
                m_cpt_update++;
                // </Activity Counters>
                r_cc_send_fsm = CC_SEND_CAS_IDLE;
                break;
            }
            ////////////////////////////
            case CC_SEND_CAS_UPDT_NLINE:   // send second flit for a multi-update (from CAS FSM)
            {
                if (not p_dspin_m2p.read) break;
                r_cc_send_cpt = 0;
                r_cc_send_fsm = CC_SEND_CAS_UPDT_DATA;

#if DEBUG_MEMC_CC_SEND
                if (m_debug)
                {
                    std::cout << "  <MEMC " << name()
                        << " CC_SEND_CAS_UPDT_NLINE> Multicast-Update for address "
                        << r_cas_to_cc_send_nline.read() * m_words * 4 << std::endl;
                }
#endif
                break;
            }
            ///////////////////////////
            case CC_SEND_CAS_UPDT_DATA:   // send first data for a multi-update (from CAS FSM)
            {
                if (not p_dspin_m2p.read) break;

                if (r_cas_to_cc_send_is_long.read())
                {
                    r_cc_send_fsm = CC_SEND_CAS_UPDT_DATA_HIGH;
                    break;
                }

                cas_to_cc_send_fifo_get = true;
                r_cc_send_fsm = CC_SEND_CAS_UPDT_HEADER;
                break;
            }
            ////////////////////////////////
            case CC_SEND_CAS_UPDT_DATA_HIGH:   // send second data for multi-update (from CAS FSM)
            {
                if (not p_dspin_m2p.read) break;
                cas_to_cc_send_fifo_get = true;
                r_cc_send_fsm = CC_SEND_CAS_UPDT_HEADER;
                break;
            }
        }
        // end switch r_cc_send_fsm

        //////////////////////////////////////////////////////////////////////////////
        //    CC_RECEIVE FSM
        //////////////////////////////////////////////////////////////////////////////
        // The CC_RECEIVE fsm controls the DSPIN target port on the coherence
        // network.
        //////////////////////////////////////////////////////////////////////////////

        switch (r_cc_receive_fsm.read())
        {
            /////////////////////
            case CC_RECEIVE_IDLE:
            {
                if (not p_dspin_p2m.write) break;

                uint8_t type =
                    DspinDhccpParam::dspin_get(
                            p_dspin_p2m.data.read(),
                            DspinDhccpParam::P2M_TYPE);

                if ((type == DspinDhccpParam::TYPE_CLEANUP_DATA) or
                        (type == DspinDhccpParam::TYPE_CLEANUP_INST))
                {
                    r_cc_receive_fsm = CC_RECEIVE_CLEANUP;
                    break;
                }

                if (type == DspinDhccpParam::TYPE_MULTI_ACK)
                {
                    r_cc_receive_fsm = CC_RECEIVE_MULTI_ACK;
                    break;
                }

                assert(false and
                        "VCI_MEM_CACHE ERROR in CC_RECEIVE : "
                        "Illegal type in coherence request");

                break;
            }
            ////////////////////////
            case CC_RECEIVE_CLEANUP:
            {
                // write first CLEANUP flit in CC_RECEIVE to CLEANUP fifo

                if (not p_dspin_p2m.write or not m_cc_receive_to_cleanup_fifo.wok())
                    break;

                assert(not p_dspin_p2m.eop.read() and
                        "VCI_MEM_CACHE ERROR in CC_RECEIVE : "
                        "CLEANUP command must have two flits");

                cc_receive_to_cleanup_fifo_put = true;
                r_cc_receive_fsm               = CC_RECEIVE_CLEANUP_EOP;

                // <Activity Counters>
                uint32_t srcid = DspinDhccpParam::dspin_get(
                        p_dspin_p2m.data.read(),
                        DspinDhccpParam::CLEANUP_SRCID);

                if (is_local_req(srcid))
                {
                    m_cpt_cleanup_local++;
                }
                else
                {
                    m_cpt_cleanup_remote++;
                }
                // 2 flits for cleanup without data
                m_cpt_cleanup_cost += 2 * req_distance(srcid);
                // </Activity Counters>

                break;
            }
            ////////////////////////////
            case CC_RECEIVE_CLEANUP_EOP:
            {
                // write second CLEANUP flit in CC_RECEIVE to CLEANUP fifo

                if (not p_dspin_p2m.write or not m_cc_receive_to_cleanup_fifo.wok())
                    break;

                assert(p_dspin_p2m.eop.read() and
                        "VCI_MEM_CACHE ERROR in CC_RECEIVE : "
                        "CLEANUP command must have two flits");

                cc_receive_to_cleanup_fifo_put = true;
                r_cc_receive_fsm = CC_RECEIVE_IDLE;

                break;
            }

            //////////////////////////
            case CC_RECEIVE_MULTI_ACK:
            {
                // write MULTI_ACK flit in CC_RECEIVE to MULTI_ACK fifo

                // wait for a WOK in the CC_RECEIVE to MULTI_ACK fifo
                if (not p_dspin_p2m.write or not m_cc_receive_to_multi_ack_fifo.wok())
                    break;

                assert(p_dspin_p2m.eop.read() and
                        "VCI_MEM_CACHE ERROR in CC_RECEIVE : "
                        "MULTI_ACK command must have one flit");

                cc_receive_to_multi_ack_fifo_put = true;
                r_cc_receive_fsm = CC_RECEIVE_IDLE;
                break;
            }
        }

        //////////////////////////////////////////////////////////////////////////
        //    TGT_RSP FSM
        //////////////////////////////////////////////////////////////////////////
        // The TGT_RSP fsm sends the responses on the VCI target port
        // with a round robin priority between eigth requests :
        // - r_config_to_tgt_rsp_req
        // - r_tgt_cmd_to_tgt_rsp_req
        // - r_read_to_tgt_rsp_req
        // - r_write_to_tgt_rsp_req
        // - r_cas_to_tgt_rsp_req
        // - r_cleanup_to_tgt_rsp_req
        // - r_xram_rsp_to_tgt_rsp_req
        // - r_multi_ack_to_tgt_rsp_req
        //
        // The ordering is :
        //   config >tgt_cmd > read > write > cas > xram > multi_ack > cleanup
        //////////////////////////////////////////////////////////////////////////

        switch (r_tgt_rsp_fsm.read())
        {
            /////////////////////////
            case TGT_RSP_CONFIG_IDLE:  // tgt_cmd requests have the highest priority
            {
                if (r_tgt_cmd_to_tgt_rsp_req) r_tgt_rsp_fsm = TGT_RSP_TGT_CMD;
                else if (r_read_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_READ;
                    r_tgt_rsp_cpt = r_read_to_tgt_rsp_word.read();
                }
                else if (r_write_to_tgt_rsp_req)     r_tgt_rsp_fsm = TGT_RSP_WRITE;
                else if (r_cas_to_tgt_rsp_req)       r_tgt_rsp_fsm = TGT_RSP_CAS;
                else if (r_xram_rsp_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_XRAM;
                    r_tgt_rsp_cpt = r_xram_rsp_to_tgt_rsp_word.read();
                }
                else if (r_multi_ack_to_tgt_rsp_req) r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK;
                else if (r_cleanup_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_CLEANUP;
                else if (r_config_to_tgt_rsp_req)    r_tgt_rsp_fsm = TGT_RSP_CONFIG;
                break;
            }
            //////////////////////////
            case TGT_RSP_TGT_CMD_IDLE: // read requests have the highest priority
            {
                if (r_read_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_READ;
                    r_tgt_rsp_cpt = r_read_to_tgt_rsp_word.read();
                }
                else if (r_write_to_tgt_rsp_req)     r_tgt_rsp_fsm = TGT_RSP_WRITE;
                else if (r_cas_to_tgt_rsp_req)       r_tgt_rsp_fsm = TGT_RSP_CAS;
                else if (r_xram_rsp_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_XRAM;
                    r_tgt_rsp_cpt = r_xram_rsp_to_tgt_rsp_word.read();
                }
                else if (r_multi_ack_to_tgt_rsp_req) r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK;
                else if (r_cleanup_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_CLEANUP;
                else if (r_config_to_tgt_rsp_req)    r_tgt_rsp_fsm = TGT_RSP_CONFIG;
                else if (r_tgt_cmd_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_TGT_CMD;
                break;
            }
            ///////////////////////
            case TGT_RSP_READ_IDLE: // write requests have the highest priority
            {
                if (r_write_to_tgt_rsp_req)          r_tgt_rsp_fsm = TGT_RSP_WRITE;
                else if (r_cas_to_tgt_rsp_req)       r_tgt_rsp_fsm = TGT_RSP_CAS;
                else if (r_xram_rsp_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_XRAM;
                    r_tgt_rsp_cpt = r_xram_rsp_to_tgt_rsp_word.read();
                }
                else if (r_multi_ack_to_tgt_rsp_req) r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK;
                else if (r_cleanup_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_CLEANUP;
                else if (r_config_to_tgt_rsp_req)    r_tgt_rsp_fsm = TGT_RSP_CONFIG;
                else if (r_tgt_cmd_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_TGT_CMD;
                else if (r_read_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_READ;
                    r_tgt_rsp_cpt = r_read_to_tgt_rsp_word.read();
                }
                break;
            }
            ////////////////////////
            case TGT_RSP_WRITE_IDLE: // cas requests have the highest priority
            {
                if (r_cas_to_tgt_rsp_req)            r_tgt_rsp_fsm = TGT_RSP_CAS;
                else if (r_xram_rsp_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_XRAM;
                    r_tgt_rsp_cpt = r_xram_rsp_to_tgt_rsp_word.read();
                }
                else if (r_multi_ack_to_tgt_rsp_req) r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK;
                else if (r_cleanup_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_CLEANUP;
                else if (r_config_to_tgt_rsp_req)    r_tgt_rsp_fsm = TGT_RSP_CONFIG;
                else if (r_tgt_cmd_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_TGT_CMD;
                else if (r_read_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_READ;
                    r_tgt_rsp_cpt = r_read_to_tgt_rsp_word.read();
                }
                else if (r_write_to_tgt_rsp_req)     r_tgt_rsp_fsm = TGT_RSP_WRITE;
                break;
            }
            ///////////////////////
            case TGT_RSP_CAS_IDLE: // xram_rsp requests have the highest priority
            {
                if (r_xram_rsp_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_XRAM;
                    r_tgt_rsp_cpt = r_xram_rsp_to_tgt_rsp_word.read();
                }
                else if (r_multi_ack_to_tgt_rsp_req) r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK   ;
                else if (r_cleanup_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_CLEANUP;
                else if (r_config_to_tgt_rsp_req)    r_tgt_rsp_fsm = TGT_RSP_CONFIG;
                else if (r_tgt_cmd_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_TGT_CMD;
                else if (r_read_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_READ;
                    r_tgt_rsp_cpt = r_read_to_tgt_rsp_word.read();
                }
                else if (r_write_to_tgt_rsp_req)     r_tgt_rsp_fsm = TGT_RSP_WRITE;
                else if (r_cas_to_tgt_rsp_req)       r_tgt_rsp_fsm = TGT_RSP_CAS  ;
                break;
            }
            ///////////////////////
            case TGT_RSP_XRAM_IDLE: // multi ack requests have the highest priority
            {

                if (r_multi_ack_to_tgt_rsp_req)      r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK   ;
                else if (r_cleanup_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_CLEANUP;
                else if (r_config_to_tgt_rsp_req)    r_tgt_rsp_fsm = TGT_RSP_CONFIG;
                else if (r_tgt_cmd_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_TGT_CMD;
                else if (r_read_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_READ;
                    r_tgt_rsp_cpt = r_read_to_tgt_rsp_word.read();
                }
                else if (r_write_to_tgt_rsp_req)     r_tgt_rsp_fsm = TGT_RSP_WRITE;
                else if (r_cas_to_tgt_rsp_req)       r_tgt_rsp_fsm = TGT_RSP_CAS  ;
                else if (r_xram_rsp_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_XRAM;
                    r_tgt_rsp_cpt = r_xram_rsp_to_tgt_rsp_word.read();
                }
                break;
            }
            ////////////////////////////
            case TGT_RSP_MULTI_ACK_IDLE: // cleanup requests have the highest priority
            {
                if (r_cleanup_to_tgt_rsp_req)        r_tgt_rsp_fsm = TGT_RSP_CLEANUP;
                else if (r_config_to_tgt_rsp_req)    r_tgt_rsp_fsm = TGT_RSP_CONFIG;
                else if (r_tgt_cmd_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_TGT_CMD;
                else if (r_read_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_READ;
                    r_tgt_rsp_cpt = r_read_to_tgt_rsp_word.read();
                }
                else if (r_write_to_tgt_rsp_req)     r_tgt_rsp_fsm = TGT_RSP_WRITE;
                else if (r_cas_to_tgt_rsp_req)       r_tgt_rsp_fsm = TGT_RSP_CAS  ;
                else if (r_xram_rsp_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_XRAM;
                    r_tgt_rsp_cpt = r_xram_rsp_to_tgt_rsp_word.read();
                }
                else if (r_multi_ack_to_tgt_rsp_req) r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK;
                break;
            }
            //////////////////////////
            case TGT_RSP_CLEANUP_IDLE: // tgt cmd requests have the highest priority
            {
                if (r_config_to_tgt_rsp_req)         r_tgt_rsp_fsm = TGT_RSP_CONFIG;
                else if (r_tgt_cmd_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_TGT_CMD;
                else if (r_read_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_READ;
                    r_tgt_rsp_cpt = r_read_to_tgt_rsp_word.read();
                }
                else if (r_write_to_tgt_rsp_req)     r_tgt_rsp_fsm = TGT_RSP_WRITE;
                else if (r_cas_to_tgt_rsp_req)       r_tgt_rsp_fsm = TGT_RSP_CAS  ;
                else if (r_xram_rsp_to_tgt_rsp_req)
                {
                    r_tgt_rsp_fsm = TGT_RSP_XRAM;
                    r_tgt_rsp_cpt = r_xram_rsp_to_tgt_rsp_word.read();
                }
                else if (r_multi_ack_to_tgt_rsp_req) r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK   ;
                else if (r_cleanup_to_tgt_rsp_req)   r_tgt_rsp_fsm = TGT_RSP_CLEANUP;
                break;
            }
            ////////////////////
            case TGT_RSP_CONFIG:  // send the response for a config transaction
            {
                if (p_vci_tgt.rspack)
                {
                    r_config_to_tgt_rsp_req = false;
                    r_tgt_rsp_fsm           = TGT_RSP_CONFIG_IDLE;

#if DEBUG_MEMC_TGT_RSP
                    if (m_debug)
                    {
                        std::cout
                            << "  <MEMC " << name()
                            << " TGT_RSP_CONFIG>  Config transaction completed response"
                            << " / rsrcid = " << std::hex << r_config_to_tgt_rsp_srcid.read()
                            << " / rtrdid = " << r_config_to_tgt_rsp_trdid.read()
                            << " / rpktid = " << r_config_to_tgt_rsp_pktid.read()
                            << std::endl;
                    }
#endif
                }
                break;
            }
            /////////////////////
            case TGT_RSP_TGT_CMD: // send the response for a configuration access
            {
                if (p_vci_tgt.rspack)
                {
                    r_tgt_cmd_to_tgt_rsp_req = false;
                    r_tgt_rsp_fsm            = TGT_RSP_TGT_CMD_IDLE;

#if DEBUG_MEMC_TGT_RSP
                    if (m_debug)
                    {
                        std::cout
                            << "  <MEMC " << name()
                            << " TGT_RSP_TGT_CMD> Send response for a configuration access"
                            << " / rsrcid = " << std::hex << r_tgt_cmd_to_tgt_rsp_srcid.read()
                            << " / rtrdid = " << r_tgt_cmd_to_tgt_rsp_trdid.read()
                            << " / rpktid = " << r_tgt_cmd_to_tgt_rsp_pktid.read()
                            << " / error = " << r_tgt_cmd_to_tgt_rsp_error.read()
                            << std::endl;
                    }
#endif
                }
                break;
            }
            //////////////////
            case TGT_RSP_READ:    // send the response to a read
            {
                if (p_vci_tgt.rspack)
                {

#if DEBUG_MEMC_TGT_RSP
                    if (m_debug)
                    {
                        std::cout
                            << "  <MEMC " << name() << " TGT_RSP_READ> Read response"
                            << " / rsrcid = " << std::hex << r_read_to_tgt_rsp_srcid.read()
                            << " / rtrdid = " << r_read_to_tgt_rsp_trdid.read()
                            << " / rpktid = " << r_read_to_tgt_rsp_pktid.read()
                            << " / rdata = " << r_read_to_tgt_rsp_data[r_tgt_rsp_cpt.read()].read()
                            << " / cpt = " << std::dec << r_tgt_rsp_cpt.read() << std::endl;
                    }
#endif

                    uint32_t last_word_idx = r_read_to_tgt_rsp_word.read() +
                        r_read_to_tgt_rsp_length.read() - 1;
                    bool     is_last_word  = (r_tgt_rsp_cpt.read() == last_word_idx);
                    bool     is_ll         = ((r_read_to_tgt_rsp_pktid.read() & 0x7) == TYPE_LL);

                    if ((is_last_word and not is_ll) or
                       (r_tgt_rsp_key_sent.read() and is_ll))
                    {
                        // Last word in case of READ or second flit in case if LL
                        r_tgt_rsp_key_sent    = false;
                        r_read_to_tgt_rsp_req = false;
                        r_tgt_rsp_fsm         = TGT_RSP_READ_IDLE;
                    }
                    else
                    {
                        if (is_ll)
                        {
                            r_tgt_rsp_key_sent = true; // Send second flit of ll
                        }
                        else
                        {
                            r_tgt_rsp_cpt = r_tgt_rsp_cpt.read() + 1; // Send next word of read
                        }
                    }
                }
                break;
            }
            //////////////////
            case TGT_RSP_WRITE:   // send the write acknowledge
            {
                if (p_vci_tgt.rspack)
                {

#if DEBUG_MEMC_TGT_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " TGT_RSP_WRITE> Write response"
                            << " / rsrcid = " << std::hex << r_write_to_tgt_rsp_srcid.read()
                            << " / rtrdid = " << r_write_to_tgt_rsp_trdid.read()
                            << " / rpktid = " << r_write_to_tgt_rsp_pktid.read() << std::endl;
                    }
#endif
                    r_tgt_rsp_fsm = TGT_RSP_WRITE_IDLE;
                    r_write_to_tgt_rsp_req = false;
                }
                break;
            }
            /////////////////////
            case TGT_RSP_CLEANUP:   // pas clair pour moi (AG)
            {
                if (p_vci_tgt.rspack)
                {

#if DEBUG_MEMC_TGT_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " TGT_RSP_CLEANUP> Cleanup response"
                            << " / rsrcid = " << std::hex << r_cleanup_to_tgt_rsp_srcid.read()
                            << " / rtrdid = " << r_cleanup_to_tgt_rsp_trdid.read()
                            << " / rpktid = " << r_cleanup_to_tgt_rsp_pktid.read() << std::endl;
                    }
#endif
                    r_tgt_rsp_fsm = TGT_RSP_CLEANUP_IDLE;
                    r_cleanup_to_tgt_rsp_req = false;
                }
                break;
            }
            /////////////////
            case TGT_RSP_CAS:    // send one atomic word response
            {
                if (p_vci_tgt.rspack)
                {

#if DEBUG_MEMC_TGT_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " TGT_RSP_CAS> CAS response"
                            << " / rsrcid = " << std::hex << r_cas_to_tgt_rsp_srcid.read()
                            << " / rtrdid = " << r_cas_to_tgt_rsp_trdid.read()
                            << " / rpktid = " << r_cas_to_tgt_rsp_pktid.read() << std::endl;
                    }
#endif
                    r_tgt_rsp_fsm = TGT_RSP_CAS_IDLE;
                    r_cas_to_tgt_rsp_req = false;
                }
                break;
            }
            //////////////////
            case TGT_RSP_XRAM:    // send the response after XRAM access
            {
                if (p_vci_tgt.rspack)
                {

#if DEBUG_MEMC_TGT_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " TGT_RSP_XRAM> Response following XRAM access"
                            << " / rsrcid = " << std::hex << r_xram_rsp_to_tgt_rsp_srcid.read()
                            << " / rtrdid = " << r_xram_rsp_to_tgt_rsp_trdid.read()
                            << " / rpktid = " << r_xram_rsp_to_tgt_rsp_pktid.read()
                            << " / rdata = " << r_xram_rsp_to_tgt_rsp_data[r_tgt_rsp_cpt.read()].read()
                            << " / cpt = " << std::dec << r_tgt_rsp_cpt.read() << std::endl;
                    }
#endif
                    uint32_t last_word_idx = r_xram_rsp_to_tgt_rsp_word.read() +
                        r_xram_rsp_to_tgt_rsp_length.read() - 1;
                    bool is_last_word = (r_tgt_rsp_cpt.read() == last_word_idx);
                    bool is_ll        = ((r_xram_rsp_to_tgt_rsp_pktid.read() & 0x7) == TYPE_LL);
                    bool is_error     = r_xram_rsp_to_tgt_rsp_rerror.read();

                    if (((is_last_word or is_error) and not is_ll) or
                            (r_tgt_rsp_key_sent.read() and is_ll))
                    {
                        // Last word sent in case of READ or second flit sent in case if LL
                        r_tgt_rsp_key_sent        = false;
                        r_xram_rsp_to_tgt_rsp_req = false;
                        r_tgt_rsp_fsm             = TGT_RSP_XRAM_IDLE;
                    }
                    else
                    {
                        if (is_ll)
                        {
                            r_tgt_rsp_key_sent = true; // Send second flit of ll
                        }
                        else
                        {
                            r_tgt_rsp_cpt = r_tgt_rsp_cpt.read() + 1; // Send next word of read
                        }
                    }
                }
                break;
            }
            ///////////////////////
            case TGT_RSP_MULTI_ACK:    // send the write response after coherence transaction
            {
                if (p_vci_tgt.rspack)
                {

#if DEBUG_MEMC_TGT_RSP
                    if (m_debug)
                    {
                        std::cout << "  <MEMC " << name() << " TGT_RSP_MULTI_ACK> Write response after coherence transaction"
                            << " / rsrcid = " << std::hex << r_multi_ack_to_tgt_rsp_srcid.read()
                            << " / rtrdid = " << r_multi_ack_to_tgt_rsp_trdid.read()
                            << " / rpktid = " << r_multi_ack_to_tgt_rsp_pktid.read() << std::endl;
                    }
#endif
                    r_tgt_rsp_fsm = TGT_RSP_MULTI_ACK_IDLE;
                    r_multi_ack_to_tgt_rsp_req = false;
                }
                break;
            }
        } // end switch tgt_rsp_fsm

        ////////////////////////////////////////////////////////////////////////////////////
        //    ALLOC_UPT FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The ALLOC_UPT FSM allocates the access to the Update Table (UPT),
        // with a round robin priority between three FSMs, with the following order:
        //  WRITE -> CAS -> MULTI_ACK
        // - The WRITE FSM initiates update transaction and sets a new entry in UPT.
        // - The CAS FSM does the same thing as the WRITE FSM.
        // - The MULTI_ACK FSM complete those trasactions and erase the UPT entry.
        // The resource is always allocated.
        /////////////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "alloc_upt_fsm" << std::endl;

        switch(r_alloc_upt_fsm.read())
        {
            /////////////////////////
            case ALLOC_UPT_WRITE:         // allocated to WRITE FSM
                if (r_write_fsm.read() != WRITE_UPT_LOCK)
                {
                    if (r_cas_fsm.read() == CAS_UPT_LOCK)
                        r_alloc_upt_fsm = ALLOC_UPT_CAS;

                    else if (r_multi_ack_fsm.read() == MULTI_ACK_UPT_LOCK)
                        r_alloc_upt_fsm = ALLOC_UPT_MULTI_ACK;
                }
                break;

                /////////////////////////
            case ALLOC_UPT_CAS:           // allocated to CAS FSM
                if (r_cas_fsm.read() != CAS_UPT_LOCK)
                {
                    if (r_multi_ack_fsm.read() == MULTI_ACK_UPT_LOCK)
                        r_alloc_upt_fsm = ALLOC_UPT_MULTI_ACK;

                    else if (r_write_fsm.read() == WRITE_UPT_LOCK)
                        r_alloc_upt_fsm = ALLOC_UPT_WRITE;
                }
                break;

                /////////////////////////
            case ALLOC_UPT_MULTI_ACK:     // allocated to MULTI_ACK FSM
                if (r_multi_ack_fsm.read() != MULTI_ACK_UPT_LOCK)
                {
                    if (r_write_fsm.read() == WRITE_UPT_LOCK)
                        r_alloc_upt_fsm = ALLOC_UPT_WRITE;

                    else if (r_cas_fsm.read() == CAS_UPT_LOCK)
                        r_alloc_upt_fsm = ALLOC_UPT_CAS;
                }
                break;
        } // end switch r_alloc_upt_fsm

        ////////////////////////////////////////////////////////////////////////////////////
        //    ALLOC_IVT FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The ALLOC_IVT FSM allocates the access to the Invalidate Table (IVT),
        // with a round robin priority between five FSMs, with the following order:
        //  WRITE -> XRAM_RSP -> CLEANUP -> CAS -> CONFIG
        // - The WRITE FSM initiates broadcast invalidate transactions and sets a new entry
        //   in IVT.
        // - The CAS FSM does the same thing as the WRITE FSM.
        // - The XRAM_RSP FSM initiates broadcast/multicast invalidate transaction and sets
        //   a new entry in the IVT
        // - The CONFIG FSM does the same thing as the XRAM_RSP FSM
        // - The CLEANUP FSM complete those trasactions and erase the IVT entry.
        // The resource is always allocated.
        /////////////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "alloc_ivt_fsm" << std::endl;

        switch(r_alloc_ivt_fsm.read())
        {
            /////////////////////
            case ALLOC_IVT_WRITE:            // allocated to WRITE FSM
                if (r_write_fsm.read() != WRITE_BC_IVT_LOCK)
                {
                    if (r_xram_rsp_fsm.read() == XRAM_RSP_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_XRAM_RSP;

                    else if (r_cleanup_fsm.read() == CLEANUP_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CLEANUP;

                    else if (r_cas_fsm.read() == CAS_BC_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CAS;

                    else if (r_config_fsm.read() == CONFIG_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CONFIG;
                }
                break;

                ////////////////////////
            case ALLOC_IVT_XRAM_RSP:         // allocated to XRAM_RSP FSM
                if (r_xram_rsp_fsm.read() != XRAM_RSP_IVT_LOCK)
                {
                    if (r_cleanup_fsm.read() == CLEANUP_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CLEANUP;

                    else if (r_cas_fsm.read() == CAS_BC_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CAS;

                    else if (r_config_fsm.read() == CONFIG_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CONFIG;

                    else if (r_write_fsm.read() == WRITE_BC_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_WRITE;
                }
                break;

                ///////////////////////
            case ALLOC_IVT_CLEANUP:          // allocated to CLEANUP FSM
                if ((r_cleanup_fsm.read() != CLEANUP_IVT_LOCK) and
                        (r_cleanup_fsm.read() != CLEANUP_IVT_DECREMENT))
                {
                    if (r_cas_fsm.read() == CAS_BC_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CAS;

                    else if (r_config_fsm.read() == CONFIG_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CONFIG;

                    else if (r_write_fsm.read() == WRITE_BC_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_WRITE;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_XRAM_RSP;
                }
                break;

                //////////////////////////
            case ALLOC_IVT_CAS:              // allocated to CAS FSM
                if (r_cas_fsm.read() != CAS_BC_IVT_LOCK)
                {
                    if (r_config_fsm.read() == CONFIG_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CONFIG;

                    else if (r_write_fsm.read() == WRITE_BC_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_WRITE;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_XRAM_RSP;

                    else if (r_cleanup_fsm.read() == CLEANUP_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CLEANUP;
                }
                break;

                //////////////////////////
            case ALLOC_IVT_CONFIG:           // allocated to CONFIG FSM
                if (r_config_fsm.read() != CONFIG_IVT_LOCK)
                {
                    if (r_write_fsm.read() == WRITE_BC_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_WRITE;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_XRAM_RSP;

                    else if (r_cleanup_fsm.read() == CLEANUP_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CLEANUP;

                    else if (r_cas_fsm.read() == CAS_BC_IVT_LOCK)
                        r_alloc_ivt_fsm = ALLOC_IVT_CAS;
                }
                break;

        } // end switch r_alloc_ivt_fsm

        ////////////////////////////////////////////////////////////////////////////////////
        //    ALLOC_DIR FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The ALLOC_DIR FSM allocates the access to the directory and
        // the data cache with a round robin priority between 6 user FSMs :
        // The cyclic ordering is CONFIG > READ > WRITE > CAS > CLEANUP > XRAM_RSP
        // The ressource is always allocated.
        /////////////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "alloc_dir_fsm" << std::endl;

        switch(r_alloc_dir_fsm.read())
        {
            /////////////////////
            case ALLOC_DIR_RESET: // Initializes the directory one SET per cycle.
                // All the WAYS of a SET initialized in parallel

                r_alloc_dir_reset_cpt.write(r_alloc_dir_reset_cpt.read() + 1);

                if (r_alloc_dir_reset_cpt.read() == (m_sets - 1))
                {
                    m_cache_directory.init();
                    r_alloc_dir_fsm = ALLOC_DIR_READ;
                }
                break;

                //////////////////////
            case ALLOC_DIR_CONFIG:    // allocated to CONFIG FSM
                if ((r_config_fsm.read() != CONFIG_DIR_REQ) and
                    (r_config_fsm.read() != CONFIG_DIR_ACCESS) and
                    (r_config_fsm.read() != CONFIG_TRT_LOCK) and
                    (r_config_fsm.read() != CONFIG_TRT_SET) and
                    (r_config_fsm.read() != CONFIG_IVT_LOCK))
                {
                    if (r_read_fsm.read() == READ_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_READ;

                    else if (r_write_fsm.read() == WRITE_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_WRITE;

                    else if (r_cas_fsm.read() == CAS_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CAS;

                    else if (r_cleanup_fsm.read() == CLEANUP_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CLEANUP;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_DIR_LOCK)
                        r_alloc_dir_fsm = ALLOC_DIR_XRAM_RSP;
                }
                break;

                ////////////////////
            case ALLOC_DIR_READ:    // allocated to READ FSM
                if (((r_read_fsm.read() != READ_DIR_REQ) and
                     (r_read_fsm.read() != READ_DIR_LOCK) and
                     (r_read_fsm.read() != READ_TRT_LOCK) and
                     (r_read_fsm.read() != READ_HEAP_REQ))
                    or
                     ((r_read_fsm.read() == READ_TRT_LOCK) and
                     (r_alloc_trt_fsm.read() == ALLOC_TRT_READ)))
                {
                    if (r_write_fsm.read() == WRITE_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_WRITE;

                    else if (r_cas_fsm.read() == CAS_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CAS;

                    else if (r_cleanup_fsm.read() == CLEANUP_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CLEANUP;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_DIR_LOCK)
                        r_alloc_dir_fsm = ALLOC_DIR_XRAM_RSP;

                    else if (r_config_fsm.read() == CONFIG_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CONFIG;
                }
                break;

                /////////////////////
            case ALLOC_DIR_WRITE:    // allocated to WRITE FSM
                if (((r_write_fsm.read() != WRITE_DIR_REQ) and
                     (r_write_fsm.read() != WRITE_DIR_LOCK) and
                     (r_write_fsm.read() != WRITE_BC_DIR_READ) and
                     (r_write_fsm.read() != WRITE_DIR_HIT) and
                     (r_write_fsm.read() != WRITE_BC_TRT_LOCK) and
                     (r_write_fsm.read() != WRITE_BC_IVT_LOCK) and
                     (r_write_fsm.read() != WRITE_MISS_TRT_LOCK) and
                     (r_write_fsm.read() != WRITE_UPT_LOCK) and
                     (r_write_fsm.read() != WRITE_UPT_HEAP_LOCK))
                    or
                     ((r_write_fsm.read()     == WRITE_UPT_HEAP_LOCK) and
                     (r_alloc_heap_fsm.read() == ALLOC_HEAP_WRITE))
                    or
                     ((r_write_fsm.read()     == WRITE_MISS_TRT_LOCK) and
                     (r_alloc_trt_fsm.read()  == ALLOC_TRT_WRITE)))
                {
                    if (r_cas_fsm.read() == CAS_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CAS;

                    else if (r_cleanup_fsm.read() == CLEANUP_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CLEANUP;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_DIR_LOCK)
                        r_alloc_dir_fsm = ALLOC_DIR_XRAM_RSP;

                    else if (r_config_fsm.read() == CONFIG_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CONFIG;

                    else if (r_read_fsm.read() == READ_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_READ;
                }
                break;

                ///////////////////
            case ALLOC_DIR_CAS:    // allocated to CAS FSM
                if (((r_cas_fsm.read() != CAS_DIR_REQ) and
                     (r_cas_fsm.read() != CAS_DIR_LOCK) and
                     (r_cas_fsm.read() != CAS_DIR_HIT_READ) and
                     (r_cas_fsm.read() != CAS_DIR_HIT_COMPARE) and
                     (r_cas_fsm.read() != CAS_DIR_HIT_WRITE) and
                     (r_cas_fsm.read() != CAS_BC_TRT_LOCK) and
                     (r_cas_fsm.read() != CAS_BC_IVT_LOCK) and
                     (r_cas_fsm.read() != CAS_MISS_TRT_LOCK) and
                     (r_cas_fsm.read() != CAS_UPT_LOCK) and
                     (r_cas_fsm.read() != CAS_UPT_HEAP_LOCK))
                    or
                     ((r_cas_fsm.read()       == CAS_UPT_HEAP_LOCK) and
                     (r_alloc_heap_fsm.read() == ALLOC_HEAP_CAS))
                    or
                     ((r_cas_fsm.read()       == CAS_MISS_TRT_LOCK) and
                      (r_alloc_trt_fsm.read() == ALLOC_TRT_CAS)))
                {
                    if (r_cleanup_fsm.read() == CLEANUP_DIR_REQ)
                       r_alloc_dir_fsm = ALLOC_DIR_CLEANUP;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_DIR_LOCK)
                        r_alloc_dir_fsm = ALLOC_DIR_XRAM_RSP;

                    else if (r_config_fsm.read() == CONFIG_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CONFIG;

                    else if (r_read_fsm.read() == READ_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_READ;

                    else if (r_write_fsm.read() == WRITE_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_WRITE;
                }
                break;

                ///////////////////////
            case ALLOC_DIR_CLEANUP:    // allocated to CLEANUP FSM
                if ((r_cleanup_fsm.read() != CLEANUP_DIR_REQ) and
                        (r_cleanup_fsm.read() != CLEANUP_DIR_LOCK) and
                        (r_cleanup_fsm.read() != CLEANUP_HEAP_REQ) and
                        (r_cleanup_fsm.read() != CLEANUP_HEAP_LOCK))
                {
                    if (r_xram_rsp_fsm.read() == XRAM_RSP_DIR_LOCK)
                        r_alloc_dir_fsm = ALLOC_DIR_XRAM_RSP;

                    else if (r_config_fsm.read() == CONFIG_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CONFIG;

                    else if (r_read_fsm.read() == READ_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_READ;

                    else if (r_write_fsm.read() == WRITE_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_WRITE;

                    else if (r_cas_fsm.read() == CAS_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CAS;
                }
                break;

                ////////////////////////
            case ALLOC_DIR_XRAM_RSP:    // allocated to XRAM_RSP FSM
                if ((r_xram_rsp_fsm.read() != XRAM_RSP_DIR_LOCK) and
                        (r_xram_rsp_fsm.read() != XRAM_RSP_TRT_COPY) and
                        (r_xram_rsp_fsm.read() != XRAM_RSP_IVT_LOCK))
                {
                    if (r_config_fsm.read() == CONFIG_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CONFIG;

                    else if (r_read_fsm.read() == READ_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_READ;

                    else if (r_write_fsm.read() == WRITE_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_WRITE;

                    else if (r_cas_fsm.read() == CAS_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CAS;

                    else if (r_cleanup_fsm.read() == CLEANUP_DIR_REQ)
                        r_alloc_dir_fsm = ALLOC_DIR_CLEANUP;
                }
                break;

        } // end switch alloc_dir_fsm

        ////////////////////////////////////////////////////////////////////////////////////
        //    ALLOC_TRT FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The ALLOC_TRT fsm allocates the access to the Transaction Table (write buffer)
        // with a round robin priority between 7 user FSMs :
        // The priority is READ > WRITE > CAS > IXR_CMD > XRAM_RSP > IXR_RSP > CONFIG
        // The ressource is always allocated.
        ///////////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "alloc_trt_fsm" << std::endl;

        switch(r_alloc_trt_fsm.read())
        {
            ////////////////////
            case ALLOC_TRT_READ:
                if (r_read_fsm.read() != READ_TRT_LOCK)
                {
                    if ((r_write_fsm.read() == WRITE_MISS_TRT_LOCK) or
                            (r_write_fsm.read() == WRITE_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_WRITE;

                    else if ((r_cas_fsm.read() == CAS_MISS_TRT_LOCK) or
                            (r_cas_fsm.read() == CAS_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_CAS;

                    else if ((r_ixr_cmd_fsm.read() == IXR_CMD_READ_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_WRITE_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CAS_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_XRAM_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CONFIG_TRT))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_CMD;

                    else if ((r_xram_rsp_fsm.read()  == XRAM_RSP_DIR_LOCK) and
                            (r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP))
                        r_alloc_trt_fsm = ALLOC_TRT_XRAM_RSP;

                    else if ((r_ixr_rsp_fsm.read() == IXR_RSP_TRT_ERASE) or
                            (r_ixr_rsp_fsm.read() == IXR_RSP_TRT_READ))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_RSP;

                    else if (r_config_fsm.read() == CONFIG_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_CONFIG;
                }
                break;

                /////////////////////
            case ALLOC_TRT_WRITE:
                if ((r_write_fsm.read() != WRITE_MISS_TRT_LOCK) and
                        (r_write_fsm.read() != WRITE_BC_TRT_LOCK) and
                        (r_write_fsm.read() != WRITE_BC_IVT_LOCK))
                {
                    if ((r_cas_fsm.read() == CAS_MISS_TRT_LOCK) or
                            (r_cas_fsm.read() == CAS_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_CAS;

                    else if ((r_ixr_cmd_fsm.read() == IXR_CMD_READ_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_WRITE_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CAS_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_XRAM_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CONFIG_TRT))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_CMD;

                    else if ((r_xram_rsp_fsm.read()  == XRAM_RSP_DIR_LOCK) and
                            (r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP))
                        r_alloc_trt_fsm = ALLOC_TRT_XRAM_RSP;

                    else if ((r_ixr_rsp_fsm.read() == IXR_RSP_TRT_ERASE) or
                            (r_ixr_rsp_fsm.read() == IXR_RSP_TRT_READ))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_RSP;

                    else if (r_config_fsm.read() == CONFIG_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_CONFIG;

                    else if (r_read_fsm.read() == READ_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_READ;
                }
                break;

                ///////////////////
            case ALLOC_TRT_CAS:
                if ((r_cas_fsm.read() != CAS_MISS_TRT_LOCK) and
                        (r_cas_fsm.read() != CAS_BC_TRT_LOCK) and
                        (r_cas_fsm.read() != CAS_BC_IVT_LOCK))
                {
                    if ((r_ixr_cmd_fsm.read() == IXR_CMD_READ_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_WRITE_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CAS_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_XRAM_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CONFIG_TRT))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_CMD;

                    else if ((r_xram_rsp_fsm.read()  == XRAM_RSP_DIR_LOCK) and
                             (r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP))
                        r_alloc_trt_fsm = ALLOC_TRT_XRAM_RSP;

                    else if ((r_ixr_rsp_fsm.read() == IXR_RSP_TRT_ERASE) or
                            (r_ixr_rsp_fsm.read() == IXR_RSP_TRT_READ))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_RSP;

                    else if (r_config_fsm.read() == CONFIG_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_CONFIG;

                    else if (r_read_fsm.read() == READ_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_READ;

                    else if ((r_write_fsm.read() == WRITE_MISS_TRT_LOCK) or
                            (r_write_fsm.read() == WRITE_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_WRITE;
                }
                break;

                ///////////////////////
            case ALLOC_TRT_IXR_CMD:
                if ((r_ixr_cmd_fsm.read() != IXR_CMD_READ_TRT) and
                        (r_ixr_cmd_fsm.read() != IXR_CMD_WRITE_TRT) and
                        (r_ixr_cmd_fsm.read() != IXR_CMD_CAS_TRT) and
                        (r_ixr_cmd_fsm.read() != IXR_CMD_XRAM_TRT) and
                        (r_ixr_cmd_fsm.read() != IXR_CMD_CONFIG_TRT))
                {
                    if ((r_xram_rsp_fsm.read()  == XRAM_RSP_DIR_LOCK) and
                            (r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP))
                        r_alloc_trt_fsm = ALLOC_TRT_XRAM_RSP;

                    else if ((r_ixr_rsp_fsm.read() == IXR_RSP_TRT_ERASE) or
                            (r_ixr_rsp_fsm.read() == IXR_RSP_TRT_READ))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_RSP;

                    else if (r_config_fsm.read() == CONFIG_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_CONFIG;

                    else if (r_read_fsm.read() == READ_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_READ;

                    else if ((r_write_fsm.read() == WRITE_MISS_TRT_LOCK) or
                            (r_write_fsm.read() == WRITE_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_WRITE;

                    else if ((r_cas_fsm.read() == CAS_MISS_TRT_LOCK) or
                            (r_cas_fsm.read() == CAS_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_CAS;
                }
                break;

                ////////////////////////
            case ALLOC_TRT_XRAM_RSP:
                if (((r_xram_rsp_fsm.read()  != XRAM_RSP_DIR_LOCK)  or
                            (r_alloc_dir_fsm.read() != ALLOC_DIR_XRAM_RSP)) and
                        (r_xram_rsp_fsm.read()  != XRAM_RSP_TRT_COPY)  and
                        (r_xram_rsp_fsm.read()  != XRAM_RSP_DIR_UPDT)  and
                        (r_xram_rsp_fsm.read()  != XRAM_RSP_IVT_LOCK))
                {
                    if ((r_ixr_rsp_fsm.read() == IXR_RSP_TRT_ERASE) or
                            (r_ixr_rsp_fsm.read() == IXR_RSP_TRT_READ))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_RSP;

                    else if (r_config_fsm.read() == CONFIG_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_CONFIG;

                    else if (r_read_fsm.read() == READ_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_READ;

                    else if ((r_write_fsm.read() == WRITE_MISS_TRT_LOCK) or
                            (r_write_fsm.read() == WRITE_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_WRITE;

                    else if ((r_cas_fsm.read() == CAS_MISS_TRT_LOCK) or
                            (r_cas_fsm.read() == CAS_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_CAS;

                    else if ((r_ixr_cmd_fsm.read() == IXR_CMD_READ_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_WRITE_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CAS_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_XRAM_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CONFIG_TRT))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_CMD;

                }
                break;

                ///////////////////////
            case ALLOC_TRT_IXR_RSP:
                if ((r_ixr_rsp_fsm.read() != IXR_RSP_TRT_ERASE) and
                        (r_ixr_rsp_fsm.read() != IXR_RSP_TRT_READ))
                {
                    if (r_config_fsm.read() == CONFIG_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_CONFIG;

                    else if (r_read_fsm.read() == READ_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_READ;

                    else if ((r_write_fsm.read() == WRITE_MISS_TRT_LOCK) or
                            (r_write_fsm.read() == WRITE_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_WRITE;

                    else if ((r_cas_fsm.read() == CAS_MISS_TRT_LOCK) or
                            (r_cas_fsm.read() == CAS_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_CAS;

                    else if ((r_ixr_cmd_fsm.read() == IXR_CMD_READ_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_WRITE_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CAS_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_XRAM_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CONFIG_TRT))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_CMD;

                    else if ((r_xram_rsp_fsm.read()  == XRAM_RSP_DIR_LOCK) and
                            (r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP))
                        r_alloc_trt_fsm = ALLOC_TRT_XRAM_RSP;
                }
                break;

                //////////////////////
            case ALLOC_TRT_CONFIG:
                if ((r_config_fsm.read() != CONFIG_TRT_LOCK) and
                        (r_config_fsm.read() != CONFIG_TRT_SET))
                {
                    if (r_read_fsm.read() == READ_TRT_LOCK)
                        r_alloc_trt_fsm = ALLOC_TRT_READ;

                    else if ((r_write_fsm.read() == WRITE_MISS_TRT_LOCK) or
                            (r_write_fsm.read() == WRITE_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_WRITE;

                    else if ((r_cas_fsm.read() == CAS_MISS_TRT_LOCK) or
                            (r_cas_fsm.read() == CAS_BC_TRT_LOCK))
                        r_alloc_trt_fsm = ALLOC_TRT_CAS;

                    else if ((r_ixr_cmd_fsm.read() == IXR_CMD_READ_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_WRITE_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CAS_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_XRAM_TRT) or
                            (r_ixr_cmd_fsm.read() == IXR_CMD_CONFIG_TRT))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_CMD;

                    else if ((r_xram_rsp_fsm.read()  == XRAM_RSP_DIR_LOCK) and
                            (r_alloc_dir_fsm.read() == ALLOC_DIR_XRAM_RSP))
                        r_alloc_trt_fsm = ALLOC_TRT_XRAM_RSP;

                    else if ((r_ixr_rsp_fsm.read() == IXR_RSP_TRT_ERASE) or
                            (r_ixr_rsp_fsm.read() == IXR_RSP_TRT_READ))
                        r_alloc_trt_fsm = ALLOC_TRT_IXR_RSP;
                }
                break;

        } // end switch alloc_trt_fsm

        ////////////////////////////////////////////////////////////////////////////////////
        //    ALLOC_HEAP FSM
        ////////////////////////////////////////////////////////////////////////////////////
        // The ALLOC_HEAP FSM allocates the access to the heap
        // with a round robin priority between 6 user FSMs :
        // The cyclic ordering is READ > WRITE > CAS > CLEANUP > XRAM_RSP > CONFIG
        // The ressource is always allocated.
        /////////////////////////////////////////////////////////////////////////////////////

        //std::cout << std::endl << "alloc_heap_fsm" << std::endl;

        switch (r_alloc_heap_fsm.read())
        {
            ////////////////////
            case ALLOC_HEAP_RESET:
                // Initializes the heap one ENTRY each cycle.

                r_alloc_heap_reset_cpt.write(r_alloc_heap_reset_cpt.read() + 1);

                if (r_alloc_heap_reset_cpt.read() == (m_heap_size - 1))
                {
                    m_heap.init();

                    r_alloc_heap_fsm = ALLOC_HEAP_READ;
                }
                break;

                ////////////////////
            case ALLOC_HEAP_READ:
                if ((r_read_fsm.read() != READ_HEAP_REQ) and
                        (r_read_fsm.read() != READ_HEAP_LOCK) and
                        (r_read_fsm.read() != READ_HEAP_ERASE))
                {
                    if (r_write_fsm.read() == WRITE_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_WRITE;

                    else if (r_cas_fsm.read() == CAS_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_CAS;

                    else if (r_cleanup_fsm.read() == CLEANUP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CLEANUP;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_XRAM_RSP;

                    else if (r_config_fsm.read() == CONFIG_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CONFIG;
                }
                break;

                /////////////////////
            case ALLOC_HEAP_WRITE:
                if ((r_write_fsm.read() != WRITE_UPT_HEAP_LOCK) and
                        (r_write_fsm.read() != WRITE_UPT_REQ) and
                        (r_write_fsm.read() != WRITE_UPT_NEXT))
                {
                    if (r_cas_fsm.read() == CAS_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_CAS;

                    else if (r_cleanup_fsm.read() == CLEANUP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CLEANUP;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_XRAM_RSP;

                    else if (r_config_fsm.read() == CONFIG_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CONFIG;

                    else if (r_read_fsm.read() == READ_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_READ;
                }
                break;

                ////////////////////
            case ALLOC_HEAP_CAS:
                if ((r_cas_fsm.read() != CAS_UPT_HEAP_LOCK) and
                        (r_cas_fsm.read() != CAS_UPT_REQ) and
                        (r_cas_fsm.read() != CAS_UPT_NEXT))
                {
                    if (r_cleanup_fsm.read() == CLEANUP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CLEANUP;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_XRAM_RSP;

                    else if (r_config_fsm.read() == CONFIG_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CONFIG;

                    else if (r_read_fsm.read() == READ_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_READ;

                    else if (r_write_fsm.read() == WRITE_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_WRITE;
                }
                break;

                ///////////////////////
            case ALLOC_HEAP_CLEANUP:
                if ((r_cleanup_fsm.read() != CLEANUP_HEAP_REQ) and
                        (r_cleanup_fsm.read() != CLEANUP_HEAP_LOCK) and
                        (r_cleanup_fsm.read() != CLEANUP_HEAP_SEARCH) and
                        (r_cleanup_fsm.read() != CLEANUP_HEAP_CLEAN))
                {
                    if (r_xram_rsp_fsm.read() == XRAM_RSP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_XRAM_RSP;

                    else if (r_config_fsm.read() == CONFIG_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CONFIG;

                    else if (r_read_fsm.read() == READ_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_READ;

                    else if (r_write_fsm.read() == WRITE_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_WRITE;

                    else if (r_cas_fsm.read() == CAS_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_CAS;
                }
                break;

                ////////////////////////
            case ALLOC_HEAP_XRAM_RSP:
                if ((r_xram_rsp_fsm.read() != XRAM_RSP_HEAP_REQ) and
                        (r_xram_rsp_fsm.read() != XRAM_RSP_HEAP_ERASE))
                {
                    if (r_config_fsm.read() == CONFIG_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CONFIG;

                    else if (r_read_fsm.read() == READ_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_READ;

                    else if (r_write_fsm.read() == WRITE_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_WRITE;

                    else if (r_cas_fsm.read() == CAS_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_CAS;

                    else if (r_cleanup_fsm.read() == CLEANUP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CLEANUP;

                }
                break;

                ///////////////////////
            case ALLOC_HEAP_CONFIG:
                if ((r_config_fsm.read() != CONFIG_HEAP_REQ) and
                        (r_config_fsm.read() != CONFIG_HEAP_SCAN))
                {
                    if (r_read_fsm.read() == READ_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_READ;

                    else if (r_write_fsm.read() == WRITE_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_WRITE;

                    else if (r_cas_fsm.read() == CAS_UPT_HEAP_LOCK)
                        r_alloc_heap_fsm = ALLOC_HEAP_CAS;

                    else if (r_cleanup_fsm.read() == CLEANUP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_CLEANUP;

                    else if (r_xram_rsp_fsm.read() == XRAM_RSP_HEAP_REQ)
                        r_alloc_heap_fsm = ALLOC_HEAP_XRAM_RSP;
                }
                break;

        } // end switch alloc_heap_fsm

        //std::cout << std::endl << "fifo_update" << std::endl;

        /////////////////////////////////////////////////////////////////////
        //    TGT_CMD to READ FIFO
        /////////////////////////////////////////////////////////////////////

        m_cmd_read_addr_fifo.update(cmd_read_fifo_get, cmd_read_fifo_put,
                p_vci_tgt.address.read());
        m_cmd_read_length_fifo.update(cmd_read_fifo_get, cmd_read_fifo_put,
                p_vci_tgt.plen.read() >> 2);
        m_cmd_read_srcid_fifo.update(cmd_read_fifo_get, cmd_read_fifo_put,
                p_vci_tgt.srcid.read());
        m_cmd_read_trdid_fifo.update(cmd_read_fifo_get, cmd_read_fifo_put,
                p_vci_tgt.trdid.read());
        m_cmd_read_pktid_fifo.update(cmd_read_fifo_get, cmd_read_fifo_put,
                p_vci_tgt.pktid.read());

        /////////////////////////////////////////////////////////////////////
        //    TGT_CMD to WRITE FIFO
        /////////////////////////////////////////////////////////////////////

        m_cmd_write_addr_fifo.update(cmd_write_fifo_get, cmd_write_fifo_put,
                (addr_t)p_vci_tgt.address.read());
        m_cmd_write_eop_fifo.update(cmd_write_fifo_get, cmd_write_fifo_put,
                p_vci_tgt.eop.read());
        m_cmd_write_srcid_fifo.update(cmd_write_fifo_get, cmd_write_fifo_put,
                p_vci_tgt.srcid.read());
        m_cmd_write_trdid_fifo.update(cmd_write_fifo_get, cmd_write_fifo_put,
                p_vci_tgt.trdid.read());
        m_cmd_write_pktid_fifo.update(cmd_write_fifo_get, cmd_write_fifo_put,
                p_vci_tgt.pktid.read());
        m_cmd_write_data_fifo.update(cmd_write_fifo_get, cmd_write_fifo_put,
                p_vci_tgt.wdata.read());
        m_cmd_write_be_fifo.update(cmd_write_fifo_get, cmd_write_fifo_put,
                p_vci_tgt.be.read());

        ////////////////////////////////////////////////////////////////////////////////////
        //    TGT_CMD to CAS FIFO
        ////////////////////////////////////////////////////////////////////////////////////

        m_cmd_cas_addr_fifo.update(cmd_cas_fifo_get, cmd_cas_fifo_put,
                (addr_t)p_vci_tgt.address.read());
        m_cmd_cas_eop_fifo.update(cmd_cas_fifo_get, cmd_cas_fifo_put,
                p_vci_tgt.eop.read());
        m_cmd_cas_srcid_fifo.update(cmd_cas_fifo_get, cmd_cas_fifo_put,
                p_vci_tgt.srcid.read());
        m_cmd_cas_trdid_fifo.update(cmd_cas_fifo_get, cmd_cas_fifo_put,
                p_vci_tgt.trdid.read());
        m_cmd_cas_pktid_fifo.update(cmd_cas_fifo_get, cmd_cas_fifo_put,
                p_vci_tgt.pktid.read());
        m_cmd_cas_wdata_fifo.update(cmd_cas_fifo_get, cmd_cas_fifo_put,
                p_vci_tgt.wdata.read());

        ////////////////////////////////////////////////////////////////////////////////////
        //    CC_RECEIVE to CLEANUP FIFO
        ////////////////////////////////////////////////////////////////////////////////////

        m_cc_receive_to_cleanup_fifo.update(cc_receive_to_cleanup_fifo_get,
                cc_receive_to_cleanup_fifo_put,
                p_dspin_p2m.data.read());

        ////////////////////////////////////////////////////////////////////////////////////
        //    CC_RECEIVE to MULTI_ACK FIFO
        ////////////////////////////////////////////////////////////////////////////////////

        m_cc_receive_to_multi_ack_fifo.update(cc_receive_to_multi_ack_fifo_get,
                cc_receive_to_multi_ack_fifo_put,
                p_dspin_p2m.data.read());

        ////////////////////////////////////////////////////////////////////////////////////
        //    WRITE to CC_SEND FIFO
        ////////////////////////////////////////////////////////////////////////////////////

        m_write_to_cc_send_inst_fifo.update(write_to_cc_send_fifo_get,
                write_to_cc_send_fifo_put,
                write_to_cc_send_fifo_inst);
        m_write_to_cc_send_srcid_fifo.update(write_to_cc_send_fifo_get,
                write_to_cc_send_fifo_put,
                write_to_cc_send_fifo_srcid);

        ////////////////////////////////////////////////////////////////////////////////////
        //    CONFIG to CC_SEND FIFO
        ////////////////////////////////////////////////////////////////////////////////////

        m_config_to_cc_send_inst_fifo.update(config_to_cc_send_fifo_get,
                config_to_cc_send_fifo_put,
                config_to_cc_send_fifo_inst);
        m_config_to_cc_send_srcid_fifo.update(config_to_cc_send_fifo_get,
                config_to_cc_send_fifo_put,
                config_to_cc_send_fifo_srcid);

        ////////////////////////////////////////////////////////////////////////////////////
        //    XRAM_RSP to CC_SEND FIFO
        ////////////////////////////////////////////////////////////////////////////////////

        m_xram_rsp_to_cc_send_inst_fifo.update(xram_rsp_to_cc_send_fifo_get,
                xram_rsp_to_cc_send_fifo_put,
                xram_rsp_to_cc_send_fifo_inst);
        m_xram_rsp_to_cc_send_srcid_fifo.update(xram_rsp_to_cc_send_fifo_get,
                xram_rsp_to_cc_send_fifo_put,
                xram_rsp_to_cc_send_fifo_srcid);

        ////////////////////////////////////////////////////////////////////////////////////
        //    CAS to CC_SEND FIFO
        ////////////////////////////////////////////////////////////////////////////////////

        m_cas_to_cc_send_inst_fifo.update(cas_to_cc_send_fifo_get,
                cas_to_cc_send_fifo_put,
                cas_to_cc_send_fifo_inst);
        m_cas_to_cc_send_srcid_fifo.update(cas_to_cc_send_fifo_get,
                cas_to_cc_send_fifo_put,
                cas_to_cc_send_fifo_srcid);
        m_cpt_cycles++;

        ////////////////////////////////////////////////////////////////////////////////////
        //            Update r_config_rsp_lines counter.
        // The three sources of (increment / decrement) are CONFIG / CLEANUP / IXR_RSP FSMs
        ////////////////////////////////////////////////////////////////////////////////////
        if (config_rsp_lines_incr and not
             (config_rsp_lines_cleanup_decr or config_rsp_lines_ixr_rsp_decr))
        {
            r_config_rsp_lines = r_config_rsp_lines.read() + 1;
        }
        if (not config_rsp_lines_incr and
             (config_rsp_lines_cleanup_decr or config_rsp_lines_ixr_rsp_decr))
        {
            r_config_rsp_lines = r_config_rsp_lines.read() - 1;
        }

    } // end transition()

    /////////////////////////////
    tmpl(void)::genMoore()
        /////////////////////////////
    {
#if MONITOR_MEMCACHE_FSM == 1
        p_read_fsm.write      (r_read_fsm.read());
        p_write_fsm.write     (r_write_fsm.read());
        p_xram_rsp_fsm.write  (r_xram_rsp_fsm.read());
        p_cas_fsm.write       (r_cas_fsm.read());
        p_cleanup_fsm.write   (r_cleanup_fsm.read());
        p_config_fsm.write    (r_config_fsm.read());
        p_alloc_heap_fsm.write(r_alloc_heap_fsm.read());
        p_alloc_dir_fsm.write (r_alloc_dir_fsm.read());
        p_alloc_trt_fsm.write (r_alloc_trt_fsm.read());
        p_alloc_upt_fsm.write (r_alloc_upt_fsm.read());
        p_alloc_ivt_fsm.write (r_alloc_ivt_fsm.read());
        p_tgt_cmd_fsm.write   (r_tgt_cmd_fsm.read());
        p_tgt_rsp_fsm.write   (r_tgt_rsp_fsm.read());
        p_ixr_cmd_fsm.write   (r_ixr_cmd_fsm.read());
        p_ixr_rsp_fsm.write   (r_ixr_rsp_fsm.read());
        p_cc_send_fsm.write   (r_cc_send_fsm.read());
        p_cc_receive_fsm.write(r_cc_receive_fsm.read());
        p_multi_ack_fsm.write (r_multi_ack_fsm.read());
#endif

        ////////////////////////////////////////////////////////////
        // Command signals on the p_vci_ixr port
        ////////////////////////////////////////////////////////////

        // DATA width is 8 bytes
        // The following values are not transmitted to XRAM
        //   p_vci_ixr.be
        //   p_vci_ixr.pktid
        //   p_vci_ixr.cons
        //   p_vci_ixr.wrap
        //   p_vci_ixr.contig
        //   p_vci_ixr.clen
        //   p_vci_ixr.cfixed

        p_vci_ixr.plen    = 64;
        p_vci_ixr.srcid   = m_srcid_x;
        p_vci_ixr.trdid   = r_ixr_cmd_trdid.read();
        p_vci_ixr.address = (addr_t)r_ixr_cmd_address.read() + (r_ixr_cmd_word.read() << 2);
        p_vci_ixr.be      = 0xFF;
        p_vci_ixr.pktid   = 0;
        p_vci_ixr.cons    = false;
        p_vci_ixr.wrap    = false;
        p_vci_ixr.contig  = true;
        p_vci_ixr.clen    = 0;
        p_vci_ixr.cfixed  = false;

        if ((r_ixr_cmd_fsm.read() == IXR_CMD_READ_SEND) or
                (r_ixr_cmd_fsm.read() == IXR_CMD_WRITE_SEND) or
                (r_ixr_cmd_fsm.read() == IXR_CMD_CAS_SEND) or
                (r_ixr_cmd_fsm.read() == IXR_CMD_XRAM_SEND) or
                (r_ixr_cmd_fsm.read() == IXR_CMD_CONFIG_SEND))
        {
            p_vci_ixr.cmdval  = true;

            if (r_ixr_cmd_get.read())  // GET
            {
                p_vci_ixr.cmd   = vci_param_ext::CMD_READ;
                p_vci_ixr.wdata = 0;
                p_vci_ixr.eop   = true;
            }
            else                         // PUT
            {
                size_t word     = r_ixr_cmd_word.read();
                p_vci_ixr.cmd   = vci_param_ext::CMD_WRITE;
                p_vci_ixr.wdata = ((wide_data_t)(r_ixr_cmd_wdata[word].read()))  |
                    ((wide_data_t) (r_ixr_cmd_wdata[word + 1].read()) << 32);
                p_vci_ixr.eop   = (word == (m_words - 2));
            }
        }
        else
        {
            p_vci_ixr.cmdval = false;
        }

        ////////////////////////////////////////////////////
        // Response signals on the p_vci_ixr port
        ////////////////////////////////////////////////////

        if ((r_ixr_rsp_fsm.read() == IXR_RSP_TRT_READ) or
                (r_ixr_rsp_fsm.read() == IXR_RSP_TRT_ERASE))
        {
            p_vci_ixr.rspack = (r_alloc_trt_fsm.read() == ALLOC_TRT_IXR_RSP);
        }
        else // r_ixr_rsp_fsm == IXR_RSP_IDLE
        {
            p_vci_ixr.rspack = false;
        }

        ////////////////////////////////////////////////////
        // Command signals on the p_vci_tgt port
        ////////////////////////////////////////////////////

        switch ((tgt_cmd_fsm_state_e) r_tgt_cmd_fsm.read())
        {
            case TGT_CMD_IDLE:
                p_vci_tgt.cmdack = false;
                break;

            case TGT_CMD_CONFIG:
            {
                addr_t addr_lsb = p_vci_tgt.address.read() &
                                  m_config_addr_mask;

                addr_t cell = (addr_lsb / vci_param_int::B);

                size_t regr = cell & m_config_regr_idx_mask;

                size_t func = (cell >> m_config_regr_width) &
                              m_config_func_idx_mask;

                switch (func)
                {
                    case MEMC_CONFIG:
                        if ((p_vci_tgt.cmd.read() == vci_param_int::CMD_WRITE)
                                and (regr == MEMC_CMD_TYPE))
                        {
                            p_vci_tgt.cmdack = true;
                        }
                        else
                        {
                            p_vci_tgt.cmdack = not r_tgt_cmd_to_tgt_rsp_req.read();
                        }
                        break;

                    default:
                        p_vci_tgt.cmdack = not r_tgt_cmd_to_tgt_rsp_req.read();
                        break;
                }

                break;
            }
            case TGT_CMD_ERROR:
                p_vci_tgt.cmdack = not r_tgt_cmd_to_tgt_rsp_req.read();
                break;

            case TGT_CMD_READ:
                p_vci_tgt.cmdack = m_cmd_read_addr_fifo.wok();
                break;

            case TGT_CMD_WRITE:
                p_vci_tgt.cmdack = m_cmd_write_addr_fifo.wok();
                break;

            case TGT_CMD_CAS:
                p_vci_tgt.cmdack = m_cmd_cas_addr_fifo.wok();
                break;
        }

        ////////////////////////////////////////////////////
        // Response signals on the p_vci_tgt port
        ////////////////////////////////////////////////////

        switch (r_tgt_rsp_fsm.read())
        {
            case TGT_RSP_CONFIG_IDLE:
            case TGT_RSP_TGT_CMD_IDLE:
            case TGT_RSP_READ_IDLE:
            case TGT_RSP_WRITE_IDLE:
            case TGT_RSP_CAS_IDLE:
            case TGT_RSP_XRAM_IDLE:
            case TGT_RSP_MULTI_ACK_IDLE:
            case TGT_RSP_CLEANUP_IDLE:
            {
                p_vci_tgt.rspval = false;
                p_vci_tgt.rsrcid = 0;
                p_vci_tgt.rdata  = 0;
                p_vci_tgt.rpktid = 0;
                p_vci_tgt.rtrdid = 0;
                p_vci_tgt.rerror = 0;
                p_vci_tgt.reop   = false;
                break;
            }
            case TGT_RSP_CONFIG:
            {
                p_vci_tgt.rspval = true;
                p_vci_tgt.rdata  = 0;
                p_vci_tgt.rsrcid = r_config_to_tgt_rsp_srcid.read();
                p_vci_tgt.rtrdid = r_config_to_tgt_rsp_trdid.read();
                p_vci_tgt.rpktid = r_config_to_tgt_rsp_pktid.read();
                p_vci_tgt.rerror = r_config_to_tgt_rsp_error.read();
                p_vci_tgt.reop   = true;
                break;
            }
            case TGT_RSP_TGT_CMD:
            {
                p_vci_tgt.rspval = true;
                p_vci_tgt.rdata  = r_tgt_cmd_to_tgt_rsp_rdata.read();
                p_vci_tgt.rsrcid = r_tgt_cmd_to_tgt_rsp_srcid.read();
                p_vci_tgt.rtrdid = r_tgt_cmd_to_tgt_rsp_trdid.read();
                p_vci_tgt.rpktid = r_tgt_cmd_to_tgt_rsp_pktid.read();
                p_vci_tgt.rerror = r_tgt_cmd_to_tgt_rsp_error.read();
                p_vci_tgt.reop   = true;
                break;
            }
            case TGT_RSP_READ:
            {
                uint32_t last_word_idx = r_read_to_tgt_rsp_word.read() + r_read_to_tgt_rsp_length - 1;
                bool is_last_word = (r_tgt_rsp_cpt.read() == last_word_idx);
                bool is_ll = ((r_read_to_tgt_rsp_pktid.read() & 0x7) == TYPE_LL);

                p_vci_tgt.rspval  = true;

                if (is_ll and not r_tgt_rsp_key_sent.read())
                {
                    // LL response first flit
                    p_vci_tgt.rdata = r_read_to_tgt_rsp_ll_key.read();
                }
                else
                {
                    // LL response second flit or READ response
                    p_vci_tgt.rdata = r_read_to_tgt_rsp_data[r_tgt_rsp_cpt.read()].read();
                }

                p_vci_tgt.rsrcid = r_read_to_tgt_rsp_srcid.read();
                p_vci_tgt.rtrdid = r_read_to_tgt_rsp_trdid.read();
                p_vci_tgt.rpktid = r_read_to_tgt_rsp_pktid.read();
                p_vci_tgt.rerror = 0;
                p_vci_tgt.reop   = (is_last_word and not is_ll) or (r_tgt_rsp_key_sent.read() and is_ll);
                break;
            }

            case TGT_RSP_WRITE:
                p_vci_tgt.rspval = true;
                if (((r_write_to_tgt_rsp_pktid.read() & 0x7) == TYPE_SC) and r_write_to_tgt_rsp_sc_fail.read())
                    p_vci_tgt.rdata = 1;
                else
                    p_vci_tgt.rdata = 0;
                p_vci_tgt.rsrcid = r_write_to_tgt_rsp_srcid.read();
                p_vci_tgt.rtrdid = r_write_to_tgt_rsp_trdid.read();
                p_vci_tgt.rpktid = r_write_to_tgt_rsp_pktid.read();
                p_vci_tgt.rerror = 0;
                p_vci_tgt.reop   = true;
                break;

            case TGT_RSP_CLEANUP:
                p_vci_tgt.rspval = true;
                p_vci_tgt.rdata  = 0;
                p_vci_tgt.rsrcid = r_cleanup_to_tgt_rsp_srcid.read();
                p_vci_tgt.rtrdid = r_cleanup_to_tgt_rsp_trdid.read();
                p_vci_tgt.rpktid = r_cleanup_to_tgt_rsp_pktid.read();
                p_vci_tgt.rerror = 0; // Can be a CAS rsp
                p_vci_tgt.reop   = true;
                break;

            case TGT_RSP_CAS:
                p_vci_tgt.rspval = true;
                p_vci_tgt.rdata  = r_cas_to_tgt_rsp_data.read();
                p_vci_tgt.rsrcid = r_cas_to_tgt_rsp_srcid.read();
                p_vci_tgt.rtrdid = r_cas_to_tgt_rsp_trdid.read();
                p_vci_tgt.rpktid = r_cas_to_tgt_rsp_pktid.read();
                p_vci_tgt.rerror = 0;
                p_vci_tgt.reop   = true;
                break;

            case TGT_RSP_XRAM:
                {
                    uint32_t last_word_idx = r_xram_rsp_to_tgt_rsp_word.read() + r_xram_rsp_to_tgt_rsp_length.read() - 1;
                    bool     is_last_word  = (r_tgt_rsp_cpt.read() == last_word_idx);
                    bool     is_ll         = ((r_xram_rsp_to_tgt_rsp_pktid.read() & 0x7) == TYPE_LL);
                    bool     is_error      = r_xram_rsp_to_tgt_rsp_rerror.read();

                    p_vci_tgt.rspval = true;

                    if (is_ll and not r_tgt_rsp_key_sent.read())
                    {
                        // LL response first flit
                        p_vci_tgt.rdata = r_xram_rsp_to_tgt_rsp_ll_key.read();
                    }
                    else
                    {
                        // LL response second flit or READ response
                        p_vci_tgt.rdata = r_xram_rsp_to_tgt_rsp_data[r_tgt_rsp_cpt.read()].read();
                    }

                    p_vci_tgt.rsrcid = r_xram_rsp_to_tgt_rsp_srcid.read();
                    p_vci_tgt.rtrdid = r_xram_rsp_to_tgt_rsp_trdid.read();
                    p_vci_tgt.rpktid = r_xram_rsp_to_tgt_rsp_pktid.read();
                    p_vci_tgt.rerror = is_error;
                    p_vci_tgt.reop   = (((is_last_word or is_error) and not is_ll) or
                            (r_tgt_rsp_key_sent.read() and     is_ll));
                    break;
                }

            case TGT_RSP_MULTI_ACK:
                p_vci_tgt.rspval = true;
                p_vci_tgt.rdata  = 0; // Can be a CAS or SC rsp
                p_vci_tgt.rsrcid = r_multi_ack_to_tgt_rsp_srcid.read();
                p_vci_tgt.rtrdid = r_multi_ack_to_tgt_rsp_trdid.read();
                p_vci_tgt.rpktid = r_multi_ack_to_tgt_rsp_pktid.read();
                p_vci_tgt.rerror = 0;
                p_vci_tgt.reop   = true;
                break;
        } // end switch r_tgt_rsp_fsm

        ////////////////////////////////////////////////////////////////////
        //  p_irq port
        //
        //  WRITE MISS response error signaling
        ////////////////////////////////////////////////////////////////////

        p_irq =
            r_xram_rsp_rerror_irq.read() &&
            r_xram_rsp_rerror_irq_enable.read();

        ////////////////////////////////////////////////////////////////////
        //  p_dspin_m2p port (CC_SEND FSM)
        ////////////////////////////////////////////////////////////////////

        p_dspin_m2p.write = false;
        p_dspin_m2p.eop   = false;
        p_dspin_m2p.data  = 0;

        switch (r_cc_send_fsm.read())
        {
            ///////////////////////////
            case CC_SEND_CONFIG_IDLE:
            case CC_SEND_XRAM_RSP_IDLE:
            case CC_SEND_WRITE_IDLE:
            case CC_SEND_CAS_IDLE:
            {
                break;
            }
            ////////////////////////////////
            case CC_SEND_CONFIG_INVAL_HEADER:
            {
                uint8_t multi_inval_type;
                if (m_config_to_cc_send_inst_fifo.read())
                {
                    multi_inval_type = DspinDhccpParam::TYPE_MULTI_INVAL_INST;
                }
                else
                {
                    multi_inval_type = DspinDhccpParam::TYPE_MULTI_INVAL_DATA;
                }

                uint64_t flit = 0;
                uint64_t dest = m_config_to_cc_send_srcid_fifo.read() <<
                    (DspinDhccpParam::SRCID_WIDTH - vci_param_int::S);

                DspinDhccpParam::dspin_set(flit,
                        dest,
                        DspinDhccpParam::MULTI_INVAL_DEST);

                DspinDhccpParam::dspin_set(flit,
                        r_config_to_cc_send_trdid.read(),
                        DspinDhccpParam::MULTI_INVAL_UPDT_INDEX);

                DspinDhccpParam::dspin_set(flit,
                        multi_inval_type,
                        DspinDhccpParam::M2P_TYPE);
                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;
                break;
            }
            ////////////////////////////////
            case CC_SEND_CONFIG_INVAL_NLINE:
            {
                uint64_t flit = 0;
                DspinDhccpParam::dspin_set(flit,
                        r_config_to_cc_send_nline.read(),
                        DspinDhccpParam::MULTI_INVAL_NLINE);
                p_dspin_m2p.eop   = true;
                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;
                break;
            }
            ///////////////////////////////////
            case CC_SEND_XRAM_RSP_INVAL_HEADER:
            {
                if (not m_xram_rsp_to_cc_send_inst_fifo.rok()) break;

                uint8_t multi_inval_type;
                if (m_xram_rsp_to_cc_send_inst_fifo.read())
                {
                    multi_inval_type = DspinDhccpParam::TYPE_MULTI_INVAL_INST;
                }
                else
                {
                    multi_inval_type = DspinDhccpParam::TYPE_MULTI_INVAL_DATA;
                }

                uint64_t flit = 0;
                uint64_t dest = m_xram_rsp_to_cc_send_srcid_fifo.read() <<
                    (DspinDhccpParam::SRCID_WIDTH - vci_param_int::S);

                DspinDhccpParam::dspin_set(flit,
                        dest,
                        DspinDhccpParam::MULTI_INVAL_DEST);

                DspinDhccpParam::dspin_set(flit,
                        r_xram_rsp_to_cc_send_trdid.read(),
                        DspinDhccpParam::MULTI_INVAL_UPDT_INDEX);

                DspinDhccpParam::dspin_set(flit,
                        multi_inval_type,
                        DspinDhccpParam::M2P_TYPE);
                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;
                break;
            }

            //////////////////////////////////
            case CC_SEND_XRAM_RSP_INVAL_NLINE:
            {
                uint64_t flit = 0;

                DspinDhccpParam::dspin_set(flit,
                        r_xram_rsp_to_cc_send_nline.read(),
                        DspinDhccpParam::MULTI_INVAL_NLINE);
                p_dspin_m2p.eop   = true;
                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;
                break;
            }

            /////////////////////////////////////
            case CC_SEND_CONFIG_BRDCAST_HEADER:
            case CC_SEND_XRAM_RSP_BRDCAST_HEADER:
            case CC_SEND_WRITE_BRDCAST_HEADER:
            case CC_SEND_CAS_BRDCAST_HEADER:
            {
                uint64_t flit = 0;

                DspinDhccpParam::dspin_set(flit,
                        m_broadcast_boundaries,
                        DspinDhccpParam::BROADCAST_BOX);

                DspinDhccpParam::dspin_set(flit,
                        1ULL,
                        DspinDhccpParam::M2P_BC);
                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;
                break;
            }
            ////////////////////////////////////
            case CC_SEND_XRAM_RSP_BRDCAST_NLINE:
            {
                uint64_t flit = 0;
                DspinDhccpParam::dspin_set(flit,
                        r_xram_rsp_to_cc_send_nline.read(),
                        DspinDhccpParam::BROADCAST_NLINE);
                p_dspin_m2p.write = true;
                p_dspin_m2p.eop   = true;
                p_dspin_m2p.data  = flit;
                break;
            }
            //////////////////////////////////
            case CC_SEND_CONFIG_BRDCAST_NLINE:
            {
                uint64_t flit = 0;
                DspinDhccpParam::dspin_set(flit,
                        r_config_to_cc_send_nline.read(),
                        DspinDhccpParam::BROADCAST_NLINE);
                p_dspin_m2p.write = true;
                p_dspin_m2p.eop   = true;
                p_dspin_m2p.data  = flit;
                break;
            }
            /////////////////////////////////
            case CC_SEND_WRITE_BRDCAST_NLINE:
            {
                uint64_t flit = 0;
                DspinDhccpParam::dspin_set(flit,
                        r_write_to_cc_send_nline.read(),
                        DspinDhccpParam::BROADCAST_NLINE);
                p_dspin_m2p.write = true;
                p_dspin_m2p.eop   = true;
                p_dspin_m2p.data  = flit;
                break;
            }
            ///////////////////////////////
            case CC_SEND_CAS_BRDCAST_NLINE:
            {
                uint64_t flit = 0;
                DspinDhccpParam::dspin_set(flit,
                        r_cas_to_cc_send_nline.read(),
                        DspinDhccpParam::BROADCAST_NLINE);
                p_dspin_m2p.write = true;
                p_dspin_m2p.eop   = true;
                p_dspin_m2p.data  = flit;
                break;
            }
            ///////////////////////////////
            case CC_SEND_WRITE_UPDT_HEADER:
            {
                if (not m_write_to_cc_send_inst_fifo.rok()) break;

                uint8_t multi_updt_type;
                if (m_write_to_cc_send_inst_fifo.read())
                {
                    multi_updt_type = DspinDhccpParam::TYPE_MULTI_UPDT_INST;
                }
                else
                {
                    multi_updt_type = DspinDhccpParam::TYPE_MULTI_UPDT_DATA;
                }

                uint64_t flit = 0;
                uint64_t dest =
                    m_write_to_cc_send_srcid_fifo.read() <<
                    (DspinDhccpParam::SRCID_WIDTH - vci_param_int::S);

                DspinDhccpParam::dspin_set(
                        flit,
                        dest,
                        DspinDhccpParam::MULTI_UPDT_DEST);

                DspinDhccpParam::dspin_set(
                        flit,
                        r_write_to_cc_send_trdid.read(),
                        DspinDhccpParam::MULTI_UPDT_UPDT_INDEX);

                DspinDhccpParam::dspin_set(
                        flit,
                        multi_updt_type,
                        DspinDhccpParam::M2P_TYPE);

                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;

                break;
            }
            //////////////////////////////
            case CC_SEND_WRITE_UPDT_NLINE:
            {
                uint64_t flit = 0;

                DspinDhccpParam::dspin_set(
                        flit,
                        r_write_to_cc_send_index.read(),
                        DspinDhccpParam::MULTI_UPDT_WORD_INDEX);

                DspinDhccpParam::dspin_set(
                        flit,
                        r_write_to_cc_send_nline.read(),
                        DspinDhccpParam::MULTI_UPDT_NLINE);

                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;

                break;
            }
            /////////////////////////////
            case CC_SEND_WRITE_UPDT_DATA:
            {

                uint8_t multi_updt_cpt = r_cc_send_cpt.read() + r_write_to_cc_send_index.read();
                uint8_t multi_updt_be    = r_write_to_cc_send_be[multi_updt_cpt].read();
                uint32_t multi_updt_data = r_write_to_cc_send_data[multi_updt_cpt].read();

                uint64_t flit = 0;

                DspinDhccpParam::dspin_set(
                        flit,
                        multi_updt_be,
                        DspinDhccpParam::MULTI_UPDT_BE);

                DspinDhccpParam::dspin_set(
                        flit,
                        multi_updt_data,
                        DspinDhccpParam::MULTI_UPDT_DATA);

                p_dspin_m2p.write = true;
                p_dspin_m2p.eop   = (r_cc_send_cpt.read() == r_write_to_cc_send_count.read());
                p_dspin_m2p.data  = flit;

                break;
            }
            ////////////////////////////
            case CC_SEND_CAS_UPDT_HEADER:
            {
                if (not m_cas_to_cc_send_inst_fifo.rok()) break;

                uint8_t multi_updt_type;
                if (m_cas_to_cc_send_inst_fifo.read())
                {
                    multi_updt_type = DspinDhccpParam::TYPE_MULTI_UPDT_INST;
                }
                else
                {
                    multi_updt_type = DspinDhccpParam::TYPE_MULTI_UPDT_DATA;
                }

                uint64_t flit = 0;
                uint64_t dest =
                    m_cas_to_cc_send_srcid_fifo.read() <<
                    (DspinDhccpParam::SRCID_WIDTH - vci_param_int::S);

                DspinDhccpParam::dspin_set(
                        flit,
                        dest,
                        DspinDhccpParam::MULTI_UPDT_DEST);

                DspinDhccpParam::dspin_set(
                        flit,
                        r_cas_to_cc_send_trdid.read(),
                        DspinDhccpParam::MULTI_UPDT_UPDT_INDEX);

                DspinDhccpParam::dspin_set(
                        flit,
                        multi_updt_type,
                        DspinDhccpParam::M2P_TYPE);

                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;

                break;
            }
            ////////////////////////////
            case CC_SEND_CAS_UPDT_NLINE:
            {
                uint64_t flit = 0;

                DspinDhccpParam::dspin_set(
                        flit,
                        r_cas_to_cc_send_index.read(),
                        DspinDhccpParam::MULTI_UPDT_WORD_INDEX);

                DspinDhccpParam::dspin_set(
                        flit,
                        r_cas_to_cc_send_nline.read(),
                        DspinDhccpParam::MULTI_UPDT_NLINE);

                p_dspin_m2p.write = true;
                p_dspin_m2p.data  = flit;

                break;
            }
            ///////////////////////////
            case CC_SEND_CAS_UPDT_DATA:
            {
                uint64_t flit = 0;

                DspinDhccpParam::dspin_set(
                        flit,
                        0xF,
                        DspinDhccpParam::MULTI_UPDT_BE);

                DspinDhccpParam::dspin_set(
                        flit,
                        r_cas_to_cc_send_wdata.read(),
                        DspinDhccpParam::MULTI_UPDT_DATA);

                p_dspin_m2p.write = true;
                p_dspin_m2p.eop   = not r_cas_to_cc_send_is_long.read();
                p_dspin_m2p.data  = flit;

                break;
            }
            ////////////////////////////////
            case CC_SEND_CAS_UPDT_DATA_HIGH:
            {
                uint64_t flit = 0;

                DspinDhccpParam::dspin_set(
                        flit,
                        0xF,
                        DspinDhccpParam::MULTI_UPDT_BE);

                DspinDhccpParam::dspin_set(
                        flit,
                        r_cas_to_cc_send_wdata_high.read(),
                        DspinDhccpParam::MULTI_UPDT_DATA);

                p_dspin_m2p.write = true;
                p_dspin_m2p.eop   = true;
                p_dspin_m2p.data  = flit;

                break;
            }
        }

        ////////////////////////////////////////////////////////////////////
        //  p_dspin_clack port (CLEANUP FSM)
        ////////////////////////////////////////////////////////////////////

        if (r_cleanup_fsm.read() == CLEANUP_SEND_CLACK)
        {
            uint8_t cleanup_ack_type;
            if (r_cleanup_inst.read())
            {
                cleanup_ack_type = DspinDhccpParam::TYPE_CLACK_INST;
            }
            else
            {
                cleanup_ack_type = DspinDhccpParam::TYPE_CLACK_DATA;
            }

            uint64_t flit = 0;
            uint64_t dest = r_cleanup_srcid.read() <<
                (DspinDhccpParam::SRCID_WIDTH - vci_param_int::S);

            DspinDhccpParam::dspin_set(
                    flit,
                    dest,
                    DspinDhccpParam::CLACK_DEST);

            DspinDhccpParam::dspin_set(
                    flit,
                    r_cleanup_nline.read(),
                    DspinDhccpParam::CLACK_SET);

            DspinDhccpParam::dspin_set(
                    flit,
                    r_cleanup_way_index.read(),
                    DspinDhccpParam::CLACK_WAY);

            DspinDhccpParam::dspin_set(
                    flit,
                    cleanup_ack_type,
                    DspinDhccpParam::CLACK_TYPE);

            p_dspin_clack.eop   = true;
            p_dspin_clack.write = true;
            p_dspin_clack.data  = flit;
        }
        else
        {
            p_dspin_clack.write = false;
            p_dspin_clack.eop   = false;
            p_dspin_clack.data  = 0;
        }

        ///////////////////////////////////////////////////////////////////
        //  p_dspin_p2m port (CC_RECEIVE FSM)
        ///////////////////////////////////////////////////////////////////
        //
        switch (r_cc_receive_fsm.read())
        {
            case CC_RECEIVE_IDLE:
            {
                p_dspin_p2m.read = false;
                break;
            }
            case CC_RECEIVE_CLEANUP:
            case CC_RECEIVE_CLEANUP_EOP:
            {
                p_dspin_p2m.read = m_cc_receive_to_cleanup_fifo.wok();
                break;
            }
            case CC_RECEIVE_MULTI_ACK:
            {
                p_dspin_p2m.read = m_cc_receive_to_multi_ack_fifo.wok();
                break;
            }
        }
        // end switch r_cc_send_fsm
    } // end genMoore()

}
} // end name space

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
