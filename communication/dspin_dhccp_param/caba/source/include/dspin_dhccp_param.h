/* -*- c++ -*-
 * File         : dspin_dhccp_param.h
 * Date         : 01/03/2013
 * Copyright    : UPMC / LIP6
 * Authors      : Cesar Fuguet
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
 */


#ifndef DSPIN_DHCCP_PARAMS_H
#define DSPIN_DHCCP_PARAMS_H

#include <inttypes.h>
#include <assert.h>

namespace soclib { namespace caba {

/*
 * L1 cache to Memory Cache command packets
 *
 * CLEANUP
 *
 * flit 1
 * ----------------------------------------------------------------------------------------------
 * EOP:0 |    DEST    |   SRCID   | NLINE MSB(2 bits)| X | WAY_INDEX(2 bits) | TYPE:0b1X | BC:0
 *       |  (10 bits) | (14 bits) |                  |   |                   |           |
 * ----------------------------------------------------------------------------------------------
 *                                                                                 | X: 0 DATA  |
 *                                                                                 |    1 INST  |
 * flit 2
 * ----------------------------------------------------------------------------------------------
 * EOP:1 |                                                                         NLINE(32 bits)
 * ----------------------------------------------------------------------------------------------
 *
 * MULTICAST ACKNOWLEDGEMENT
 *
 * flit 1
 * ----------------------------------------------------------------------------------------------
 * EOP:1 |     DEST(10 bits)     |       X(15 bits)       | UPDT_INDEX(4 bits) | TYPE:0b00 | BC:0
 * ----------------------------------------------------------------------------------------------
 */

/*
 * M2P command packets
 *
 * MULTICAST UPDATE
 *
 * flit 1
 * ----------------------------------------------------------------------------------------------
 * EOP:0 | DEST(14 bits) | X(4 bits) | MEMC_ID(14 bits) | UPDT_INDEX(4 bits) | TYPE:0b0X   | BC:0
 * ----------------------------------------------------------------------------------------------
 *                                                                           | X: 0 DATA   |
 *                                                                           |    1 INST   |
 * flit 2
 * ----------------------------------------------------------------------------------------------
 * EOP:0 | X | WORD_INDEX(4 bits)  |                                             NLINE (34 bits)
 * ----------------------------------------------------------------------------------------------
 *
 * flit 3
 * ----------------------------------------------------------------------------------------------
 * EOP:0 | X(3 bits)  | BE(4 bits) |                                              WDATA(32 bits)
 * ----------------------------------------------------------------------------------------------
 *
 * flit N
 * ----------------------------------------------------------------------------------------------
 * EOP:1 | X(3 bits)  | BE(4 bits) |                                              WDATA(32 bits)
 * ----------------------------------------------------------------------------------------------
 *
 * MULTICAST INVALIDATE
 *
 * flit 1
 * ----------------------------------------------------------------------------------------------
 * EOP:0 | DEST(14 bits) | X(4 bits) | MEMC_ID(14 bits) | UPDT_INDEX(4 bits) | TYPE:0b1X  | BC:0
 * ----------------------------------------------------------------------------------------------
 *                                                                           | X: 0 DATA  |
 *                                                                           |    1 INST  |
 * flit 2
 * ----------------------------------------------------------------------------------------------
 * EOP:1 | X(5 bits) |                                                           NLINE (34 bits)
 * ----------------------------------------------------------------------------------------------
 *
 * BROADCAST INVALIDATE
 *
 * flit 1
 *       | BOUNDING BOX(20 bits)            |
 * ----------------------------------------------------------------------------------------------
 * EOP:0 | XMIN   | XMAX   | YMIN   | YMAX  | MEMC_ID(14 bits) | NETWORK_RESERVED(4 bits) | BC:1
 * ----------------------------------------------------------------------------------------------
 *
 * flit 2
 * ----------------------------------------------------------------------------------------------
 * EOP:1 |        X(5 bits)       |                                              NLINE (34 bits)
 * ----------------------------------------------------------------------------------------------
 *
 * M2P clack commands
 *
 * CLEANUP ACKNOWLEDGEMENT
 *
 * flit 1
 * ----------------------------------------------------------------------------------------------
 * EOP:1 | DEST(14 bits) | X(5 bits) | SET_INDEX(16 bits) | WAY_INDEX(2 bits) | TYPE:0bX   | BC:0
 * ----------------------------------------------------------------------------------------------
 *                                                                            | X: 0 CLACK |
 *                                                                            |      DATA  |
 *                                                                            |    1 CLACK |
 *                                                                            |      INST  |
 *
 */

/*
 * Utility MACROS
 */
#define GET_FIELD(x,y)\
    case y: return ((x >> y##_SHIFT) & y##_MASK)

#define SET_FIELD(x,y,z)\
    case z: x |= ((y & z##_MASK) << z##_SHIFT);break

class DspinDhccpParam
{
  public:

    static const uint8_t  m2p_flit_width               = 40;
    static const uint8_t  p2m_flit_width               = 33;
    static const uint8_t  clack_flit_width             = 40;

    static const uint8_t  UPDT_INDEX_WIDTH             = 4;
    static const uint8_t  NLINE_WIDTH                  = 34;
    static const uint8_t  SRCID_WIDTH                  = 14;
    static const uint8_t  GLOBALID_WIDTH               = 10;
    static const uint8_t  WORD_INDEX_WIDTH             = 4;
    static const uint8_t  BE_WIDTH                     = 4;
    static const uint8_t  DATA_WIDTH                   = 32;
    static const uint8_t  SET_INDEX_WIDTH              = 6;
    static const uint8_t  WAY_INDEX_WIDTH              = 2;
    static const uint8_t  BROADCAST_BOX_WIDTH          = 20;
    static const uint8_t  M2P_TYPE_WIDTH               = 2;
    static const uint8_t  P2M_TYPE_WIDTH               = 2;
    static const uint8_t  CLACK_TYPE_WIDTH             = 1;

    static const uint8_t  P2M_TYPE_SHIFT               = 1;
    static const uint64_t P2M_TYPE_MASK                = ((1ULL<<P2M_TYPE_WIDTH)-1);
    static const uint8_t  P2M_EOP_SHIFT                = 32;
    static const uint64_t P2M_EOP_MASK                 = 1;
    static const uint8_t  P2M_BC_SHIFT                 = 0;
    static const uint64_t P2M_BC_MASK                  = 1;

    static const uint8_t  CLEANUP_DEST_SHIFT           = 22;
    static const uint64_t CLEANUP_DEST_MASK            = ((1ULL<<GLOBALID_WIDTH)-1);
    static const uint8_t  CLEANUP_SRCID_SHIFT          = 8;
    static const uint64_t CLEANUP_SRCID_MASK           = ((1ULL<<SRCID_WIDTH)-1);
    static const uint8_t  CLEANUP_NLINE_MSB_SHIFT      = 6;
    static const uint64_t CLEANUP_NLINE_MSB_MASK       = ((1ULL<< 2)-1);
    static const uint8_t  CLEANUP_WAY_INDEX_SHIFT      = 3;
    static const uint64_t CLEANUP_WAY_INDEX_MASK       = ((1ULL<<WAY_INDEX_WIDTH)-1);
    static const uint8_t  CLEANUP_NLINE_LSB_SHIFT      = 0;
    static const uint64_t CLEANUP_NLINE_LSB_MASK       = ((1ULL<<32)-1);

    static const uint8_t  MULTI_ACK_DEST_SHIFT         = CLEANUP_DEST_SHIFT;
    static const uint64_t MULTI_ACK_DEST_MASK          = CLEANUP_DEST_MASK;
    static const uint8_t  MULTI_ACK_UPDT_INDEX_SHIFT   = 3;
    static const uint64_t MULTI_ACK_UPDT_INDEX_MASK    = ((1ULL<<UPDT_INDEX_WIDTH)-1);

    static const uint8_t  M2P_TYPE_SHIFT               = 1;
    static const uint64_t M2P_TYPE_MASK                = ((1ULL<<M2P_TYPE_WIDTH)-1);
    static const uint8_t  M2P_EOP_SHIFT                = 39;
    static const uint64_t M2P_EOP_MASK                 = 1;
    static const uint8_t  M2P_BC_SHIFT                 = 0;
    static const uint64_t M2P_BC_MASK                  = 1;

    static const uint8_t  MULTI_INVAL_DEST_SHIFT       = 25;
    static const uint64_t MULTI_INVAL_DEST_MASK        = ((1ULL<<SRCID_WIDTH)-1);
    static const uint8_t  MULTI_INVAL_SRCID_SHIFT      = 7;
    static const uint64_t MULTI_INVAL_SRCID_MASK       = ((1ULL<<SRCID_WIDTH)-1);
    static const uint8_t  MULTI_INVAL_UPDT_INDEX_SHIFT = 3;
    static const uint64_t MULTI_INVAL_UPDT_INDEX_MASK  = ((1ULL<<UPDT_INDEX_WIDTH)-1);
    static const uint8_t  MULTI_INVAL_NLINE_SHIFT      = 0;
    static const uint64_t MULTI_INVAL_NLINE_MASK       = ((1ULL<<NLINE_WIDTH)-1);

    static const uint8_t  MULTI_UPDT_DEST_SHIFT        = MULTI_INVAL_DEST_SHIFT;
    static const uint64_t MULTI_UPDT_DEST_MASK         = MULTI_INVAL_DEST_MASK;
    static const uint8_t  MULTI_UPDT_SRCID_SHIFT       = MULTI_INVAL_SRCID_SHIFT;
    static const uint64_t MULTI_UPDT_SRCID_MASK        = MULTI_INVAL_SRCID_MASK;
    static const uint8_t  MULTI_UPDT_UPDT_INDEX_SHIFT  = MULTI_INVAL_UPDT_INDEX_SHIFT;
    static const uint64_t MULTI_UPDT_UPDT_INDEX_MASK   = MULTI_INVAL_UPDT_INDEX_MASK;
    static const uint8_t  MULTI_UPDT_WORD_INDEX_SHIFT  = 34;
    static const uint64_t MULTI_UPDT_WORD_INDEX_MASK   = ((1ULL<<WORD_INDEX_WIDTH)-1);
    static const uint8_t  MULTI_UPDT_NLINE_SHIFT       = MULTI_INVAL_NLINE_SHIFT;
    static const uint64_t MULTI_UPDT_NLINE_MASK        = MULTI_INVAL_NLINE_MASK;
    static const uint8_t  MULTI_UPDT_BE_SHIFT          = 32;
    static const uint64_t MULTI_UPDT_BE_MASK           = ((1ULL<<BE_WIDTH)-1);
    static const uint8_t  MULTI_UPDT_DATA_SHIFT        = 0;
    static const uint64_t MULTI_UPDT_DATA_MASK         = ((1ULL<<DATA_WIDTH)-1);

    static const uint8_t  BROADCAST_BOX_SHIFT          = 19;
    static const uint64_t BROADCAST_BOX_MASK           = ((1ULL<<BROADCAST_BOX_WIDTH)-1);
    static const uint8_t  BROADCAST_SRCID_SHIFT        = 5;
    static const uint64_t BROADCAST_SRCID_MASK         = MULTI_INVAL_SRCID_MASK;
    static const uint8_t  BROADCAST_NLINE_SHIFT        = 0;
    static const uint64_t BROADCAST_NLINE_MASK         = MULTI_INVAL_NLINE_MASK;

    static const uint8_t  CLACK_TYPE_SHIFT             = 1;
    static const uint64_t CLACK_TYPE_MASK              = ((1ULL<<CLACK_TYPE_WIDTH)-1);
    static const uint8_t  CLACK_EOP_SHIFT              = 39;
    static const uint64_t CLACK_EOP_MASK               = 1;
    static const uint8_t  CLACK_BC_SHIFT               = 0;
    static const uint64_t CLACK_BC_MASK                = 1;
    static const uint8_t  CLACK_DEST_SHIFT             = 25;
    static const uint64_t CLACK_DEST_MASK              = ((1ULL<<SRCID_WIDTH)-1);
    static const uint8_t  CLACK_SET_SHIFT              = 4;
    static const uint64_t CLACK_SET_MASK               = ((1ULL<<SET_INDEX_WIDTH)-1);
    static const uint8_t  CLACK_WAY_SHIFT              = 2;
    static const uint64_t CLACK_WAY_MASK               = ((1ULL<<WAY_INDEX_WIDTH)-1);

    /*
     * P2M command types
     */
    enum
    {
      TYPE_MULTI_ACK    = 0,
      TYPE_CLEANUP      = 2,
      TYPE_CLEANUP_DATA = TYPE_CLEANUP,
      TYPE_CLEANUP_INST = 3
    };

    /*
     * M2P command types
     */
    enum
    {
      TYPE_MULTI_UPDT       = 0,
      TYPE_MULTI_UPDT_DATA  = TYPE_MULTI_UPDT,
      TYPE_MULTI_UPDT_INST  = 1,
      TYPE_MULTI_INVAL      = 2,
      TYPE_MULTI_INVAL_DATA = TYPE_MULTI_INVAL,
      TYPE_MULTI_INVAL_INST = 3
    };

    /*
     * CLACK command types
     */
    enum
    {
      TYPE_CLACK      = 0,
      TYPE_CLACK_DATA = TYPE_CLACK,
      TYPE_CLACK_INST = 1
    };

    enum flit_field_e
    {
      P2M_TYPE,
      P2M_EOP,
      P2M_BC,

      CLEANUP_DEST,
      CLEANUP_SRCID,
      CLEANUP_NLINE_MSB,
      CLEANUP_WAY_INDEX,
      CLEANUP_NLINE_LSB,

      MULTI_ACK_DEST,
      MULTI_ACK_UPDT_INDEX,

      M2P_TYPE,
      M2P_EOP,
      M2P_BC,

      MULTI_INVAL_DEST,
      MULTI_INVAL_SRCID,
      MULTI_INVAL_UPDT_INDEX,
      MULTI_INVAL_NLINE,

      MULTI_UPDT_DEST,
      MULTI_UPDT_SRCID,
      MULTI_UPDT_UPDT_INDEX,
      MULTI_UPDT_WORD_INDEX,
      MULTI_UPDT_NLINE,
      MULTI_UPDT_BE,
      MULTI_UPDT_DATA,

      CLACK_TYPE,

      CLACK_DEST,
      CLACK_SET,
      CLACK_WAY,

      BROADCAST_BOX,
      BROADCAST_SRCID,
      BROADCAST_NLINE
    };

    static uint64_t dspin_get(uint64_t flit, flit_field_e field)
    {
      switch(field)
      {
        GET_FIELD(flit,P2M_TYPE);
        GET_FIELD(flit,P2M_EOP);
        GET_FIELD(flit,P2M_BC);
        GET_FIELD(flit,CLEANUP_DEST);
        GET_FIELD(flit,CLEANUP_SRCID);
        GET_FIELD(flit,CLEANUP_NLINE_MSB);
        GET_FIELD(flit,CLEANUP_WAY_INDEX);
        GET_FIELD(flit,CLEANUP_NLINE_LSB);
        GET_FIELD(flit,MULTI_ACK_DEST);
        GET_FIELD(flit,MULTI_ACK_UPDT_INDEX);
        GET_FIELD(flit,M2P_TYPE);
        GET_FIELD(flit,M2P_EOP);
        GET_FIELD(flit,M2P_BC);
        GET_FIELD(flit,MULTI_INVAL_DEST);
        GET_FIELD(flit,MULTI_INVAL_SRCID);
        GET_FIELD(flit,MULTI_INVAL_UPDT_INDEX);
        GET_FIELD(flit,MULTI_INVAL_NLINE);
        GET_FIELD(flit,MULTI_UPDT_DEST);
        GET_FIELD(flit,MULTI_UPDT_SRCID);
        GET_FIELD(flit,MULTI_UPDT_UPDT_INDEX);
        GET_FIELD(flit,MULTI_UPDT_WORD_INDEX);
        GET_FIELD(flit,MULTI_UPDT_NLINE);
        GET_FIELD(flit,MULTI_UPDT_BE);
        GET_FIELD(flit,MULTI_UPDT_DATA);
        GET_FIELD(flit,CLACK_TYPE);
        GET_FIELD(flit,CLACK_DEST);
        GET_FIELD(flit,CLACK_SET);
        GET_FIELD(flit,CLACK_WAY);
        GET_FIELD(flit,BROADCAST_BOX);
        GET_FIELD(flit,BROADCAST_SRCID);
        GET_FIELD(flit,BROADCAST_NLINE);

        default: assert(false && "Incorrect DHCCP DSPIN field");
      }
    }

    static void dspin_set(uint64_t &flit, uint64_t value, flit_field_e field)
    {
      switch(field)
      {
        SET_FIELD(flit,value,P2M_TYPE);
        SET_FIELD(flit,value,P2M_EOP);
        SET_FIELD(flit,value,P2M_BC);
        SET_FIELD(flit,value,CLEANUP_DEST);
        SET_FIELD(flit,value,CLEANUP_SRCID);
        SET_FIELD(flit,value,CLEANUP_NLINE_MSB);
        SET_FIELD(flit,value,CLEANUP_WAY_INDEX);
        SET_FIELD(flit,value,CLEANUP_NLINE_LSB);
        SET_FIELD(flit,value,MULTI_ACK_DEST);
        SET_FIELD(flit,value,MULTI_ACK_UPDT_INDEX);
        SET_FIELD(flit,value,M2P_TYPE);
        SET_FIELD(flit,value,M2P_EOP);
        SET_FIELD(flit,value,M2P_BC);
        SET_FIELD(flit,value,MULTI_INVAL_DEST);
        SET_FIELD(flit,value,MULTI_INVAL_SRCID);
        SET_FIELD(flit,value,MULTI_INVAL_UPDT_INDEX);
        SET_FIELD(flit,value,MULTI_INVAL_NLINE);
        SET_FIELD(flit,value,MULTI_UPDT_DEST);
        SET_FIELD(flit,value,MULTI_UPDT_SRCID);
        SET_FIELD(flit,value,MULTI_UPDT_UPDT_INDEX);
        SET_FIELD(flit,value,MULTI_UPDT_WORD_INDEX);
        SET_FIELD(flit,value,MULTI_UPDT_NLINE);
        SET_FIELD(flit,value,MULTI_UPDT_BE);
        SET_FIELD(flit,value,MULTI_UPDT_DATA);
        SET_FIELD(flit,value,CLACK_TYPE);
        SET_FIELD(flit,value,CLACK_DEST);
        SET_FIELD(flit,value,CLACK_SET);
        SET_FIELD(flit,value,CLACK_WAY);
        SET_FIELD(flit,value,BROADCAST_BOX);
        SET_FIELD(flit,value,BROADCAST_SRCID);
        SET_FIELD(flit,value,BROADCAST_NLINE);

        default: assert(false && "Incorrect DHCCP DSPIN field");
      }
    }
};

#undef GET_FIELD
#undef SET_FIELD

}} // end namespace soclib::caba

#endif
// Local Variables:
// tab-width: 2
// c-basic-offset: 2
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=2:tabstop=2:softtabstop=2
