/*
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
 * Copyright (c) UPMC, Lip6, Asim
 *         alain greiner
 *
 * Maintainers: alain
 */
#ifndef MEM_CACHE_REGS_H
#define MEM_CACHE_REGS_H

enum SoclibMemCacheFunc
{
    MEMC_CONFIG = 0,
    MEMC_INSTRM = 1,
    MEMC_RERROR = 2,

    MEMC_FUNC_SPAN = 0x200
};

enum SoclibMemCacheConfigRegs
{
    MEMC_ADDR_LO,
    MEMC_ADDR_HI,
    MEMC_BUF_LENGTH,
    MEMC_CMD_TYPE
};

enum SoclibMemCacheConfigCmd
{
    MEMC_CMD_NOP,
    MEMC_CMD_INVAL,
    MEMC_CMD_SYNC
};

///////////////////////////////////////////////////////////
//  Decoding CONFIG interface commands                   //
//                                                       //
//  VCI ADDRESS                                          //
//  ================================================     //
//  GLOBAL | LOCAL | ... | FUNC_IDX | REGS_IDX | 00      //
//   IDX   |  IDX  |     | (3 bits) | (7 bits) |         //
//  ================================================     //
//                                                       //
//  For instrumentation: FUNC_IDX = 0b001                //
//                                                       //
//  REGS_IDX                                             //
//  ============================================         //
//       Z     |    Y      |    X     |   W              //
//    (1 bit)  | (2 bits)  | (3 bits) | (1 bit)          //
//  ============================================         //
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

enum SoclibMemCacheInstrRegs {
    ///////////////////////////////////////////////////////
    //          DIRECT instrumentation registers         //
    ///////////////////////////////////////////////////////

    // LOCAL

    MEMC_LOCAL_READ_LO   = 0x00,
    MEMC_LOCAL_READ_HI   = 0x01,
    MEMC_LOCAL_WRITE_LO  = 0x02,
    MEMC_LOCAL_WRITE_HI  = 0x03,
    MEMC_LOCAL_LL_LO     = 0x04,
    MEMC_LOCAL_LL_HI     = 0x05,
    MEMC_LOCAL_SC_LO     = 0x06,
    MEMC_LOCAL_SC_HI     = 0x07,
    MEMC_LOCAL_CAS_LO    = 0x08,
    MEMC_LOCAL_CAS_HI    = 0x09,

    // REMOTE

    MEMC_REMOTE_READ_LO  = 0x10,
    MEMC_REMOTE_READ_HI  = 0x11,
    MEMC_REMOTE_WRITE_LO = 0x12,
    MEMC_REMOTE_WRITE_HI = 0x13,
    MEMC_REMOTE_LL_LO    = 0x14,
    MEMC_REMOTE_LL_HI    = 0x15,
    MEMC_REMOTE_SC_LO    = 0x16,
    MEMC_REMOTE_SC_HI    = 0x17,
    MEMC_REMOTE_CAS_LO   = 0x18,
    MEMC_REMOTE_CAS_HI   = 0x19,

    // COST

    MEMC_COST_READ_LO    = 0x20,
    MEMC_COST_READ_HI    = 0x21,
    MEMC_COST_WRITE_LO   = 0x22,
    MEMC_COST_WRITE_HI   = 0x23,
    MEMC_COST_LL_LO      = 0x24,
    MEMC_COST_LL_HI      = 0x25,
    MEMC_COST_SC_LO      = 0x26,
    MEMC_COST_SC_HI      = 0x27,
    MEMC_COST_CAS_LO     = 0x28,
    MEMC_COST_CAS_HI     = 0x29,

    ///////////////////////////////////////////////////////
    //       COHERENCE instrumentation registers         //
    ///////////////////////////////////////////////////////

    // LOCAL

    MEMC_LOCAL_MUPDATE_LO  = 0x40,
    MEMC_LOCAL_MUPDATE_HI  = 0x41,
    MEMC_LOCAL_MINVAL_LO   = 0x42,
    MEMC_LOCAL_MINVAL_HI   = 0x43,
    MEMC_LOCAL_CLEANUP_LO  = 0x44,
    MEMC_LOCAL_CLEANUP_HI  = 0x45,

    // REMOTE

    MEMC_REMOTE_MUPDATE_LO = 0x50,
    MEMC_REMOTE_MUPDATE_HI = 0x51,
    MEMC_REMOTE_MINVAL_LO  = 0x52,
    MEMC_REMOTE_MINVAL_HI  = 0x53,
    MEMC_REMOTE_CLEANUP_LO = 0x54,
    MEMC_REMOTE_CLEANUP_HI = 0x55,

    // COST

    MEMC_COST_MUPDATE_LO   = 0x60,
    MEMC_COST_MUPDATE_HI   = 0x61,
    MEMC_COST_MINVAL_LO    = 0x62,
    MEMC_COST_MINVAL_HI    = 0x63,
    MEMC_COST_CLEANUP_LO   = 0x64,
    MEMC_COST_CLEANUP_HI   = 0x65,

    // TOTAL

    MEMC_TOTAL_MUPDATE_LO  = 0x68,
    MEMC_TOTAL_MUPDATE_HI  = 0x69,
    MEMC_TOTAL_MINVAL_LO   = 0x6A,
    MEMC_TOTAL_MINVAL_HI   = 0x6B,
    MEMC_TOTAL_BINVAL_LO   = 0x6C,
    MEMC_TOTAL_BINVAL_HI   = 0x6D,
};

enum SoclibMemCacheRerrorRegs
{
    MEMC_RERROR_ADDR_LO = 0,
    MEMC_RERROR_ADDR_HI,
    MEMC_RERROR_SRCID, 
    MEMC_RERROR_IRQ_RESET,
    MEMC_RERROR_IRQ_ENABLE
};

#define MEMC_REG(func,idx) ((func<<7)|idx) 

#endif /* MEM_CACHE_REGS_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

