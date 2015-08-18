/* -*- c++ -*-
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
 * Alexandre JOANNOU <alexandre.joannou@lip6.fr>
 *
 */

#ifndef SOCLIB_GENERIC_LLSC_GLOBAL_TABLE_H
#define SOCLIB_GENERIC_LLSC_GLOBAL_TABLE_H

#include <cassert>
#include <cstring>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdint.h>

#define RESERVATION_LIFE_SPAN 1

namespace soclib
{

//////////////////////////
//TODO switch to this
/*
template
<
size_t          nb_slots,   // max number of concerned shared resources
typename        key_t,      // key type => max number of key; TODO wich one ?
unsigned int    t_network,  // max number of cycle spent in the network when responding to a client (or more)
unsigned int    t_inter_op, // min number of cycle between 2 reservation operation (or less but > 0)
typename        addr_t      // ressource identifier type
>
*/
template
<
size_t          nb_slots,   // desired number of slots
unsigned int    nb_procs,   // number of processors in the system
unsigned int    life_span,  // registratioçn life span (in # of LL operations)
typename        addr_t      // address type
>
class GenericLLSCGlobalTable
////////////////////////////
{
    private :

    const std::string           name              ; // component name

    uint32_t                    r_key  [nb_slots] ; // array of key
    addr_t                      r_addr [nb_slots] ; // array of addresses
    bool                        r_val  [nb_slots] ; // array of valid bits

    uint32_t                    r_next_key        ; // value of the next key
    uint64_t                    r_block_mask      ; // mask for the slots blocks
    uint64_t                    r_last_counter    ; // mask for the slots blocks
    size_t                      r_write_ptr       ; // index of next slot to replace
    size_t                      r_last_empty      ; // index of last empty slot used

    mutable uint32_t            m_cpt_evic        ; // number of eviction in the table
    mutable uint32_t            m_cpt_ll          ; // number of ll accesses to the table
    mutable uint32_t            m_cpt_ll_update   ; // number of ll accesses to the table that trigger an update TODO check that
    mutable uint32_t            m_cpt_sc          ; // number of sc accesses to the table
    mutable uint32_t            m_cpt_sc_success  ; // number of sc accesses to the table that are successful
    mutable uint32_t            m_cpt_check       ; // number of check accesses to the table
    mutable uint32_t            m_cpt_sw          ; // number of sw accesses to the table

    ////////////////////////////////////////////////////////////////////////////
    inline void upNextKey()
    //  This function generates a new value for the next key
    {
        // generating a new key in r_next_key
        r_next_key++;
    }

    ////////////////////////////////////////////////////////////////////////////
    /*
    inline void updateVictimSlot()
    //  This function selects the next slot to be evicted
    //  This is done by updating the value of r_write_ptr
    {
        // updates the position of the next slot to be replaced

        static unsigned int count = 0;


        // for each slot, check if it actually is the slot to replace
        // this is done by checking count % 2^(i+1) == (2^i)-1
        // 2^(i+1) being the period
        // (2^i)-1 being the first apparition
        // NB : the -1 in (2^i)-1 is here because of the 0 indexed array

        for (size_t i = 0 ; i < nb_slots; i++)
            if (count % (int)pow(2,i+1) == pow(2,i)-1)
                r_write_ptr = i;

        count = (count + 1) % (int) pow(2,nb_slots);    // mustn't go further than 2^nb_slots
                                                        // or (2^nb_slots)+1 for a 1 indexed array
                                                        // 2^32 = periodicity of slot #31
    }
    */
    ////////////////////////////////////////////////////////////////////////////
    inline void updateVictimSlot()
    //  This function selects the next slot to be evicted
    //  This is done by updating the value of r_write_ptr
    {
        uint64_t new_counter;
        uint64_t xor_counter;

        new_counter = newCounter(r_block_mask, r_last_counter);
        xor_counter = new_counter ^ r_last_counter;

        for (size_t i = nb_slots - 1; i >= 0; --i)
        {
            if(xor_counter & (1 << i))
            {
                r_write_ptr = i;
                break;
            }
        }

        r_last_counter = new_counter;
    }

    ////////////////////////////////////////////////////////////////////////////
    inline uint64_t newCounter(const uint64_t& mask,
                               const uint64_t& counter) const
    // This function generates the new counter //TODO comment more
    {
        return ((((~counter) & (counter << 1)) & mask) | (counter + 1));
    }

    ////////////////////////////////////////////////////////////////////////////
    inline void init_block_mask()
    //TODO
    //This function selects the block mask to be used
    //Need to provide another way to do that ?
    {
        /*
        //try to dynamically compute the block mask ...
        #define L2 soclib::common::uint32_log2
        unsigned int budget = nb_slots - (L2(nb_procs) + 1); //TODO +1?
        #undef L2
        */

        switch(nb_slots)
        {
            case 12:
            r_block_mask = (uint64_t)0x000ULL;
            break;
            case 16 :
            r_block_mask = (uint64_t)0xA800ULL;
            break;
            case 20 :
            r_block_mask = (uint64_t)0xD5500ULL;
            break;
            case 24 :
            r_block_mask = (uint64_t)0xDB5540ULL;
            break;
            case 28 :
            r_block_mask = (uint64_t)0xEEDAAA0ULL;
            break;
            case 32 :
            r_block_mask = (uint64_t)0xF776D550ULL;
            break;
            case 36 :
            r_block_mask = (uint64_t)0xFBDDDB550ULL;
            break;
            case 40 :
            r_block_mask = (uint64_t)0xFDF7BB6D50ULL;
            break;
            case 44 :
            r_block_mask = (uint64_t)0xFEFBDEEDAA8ULL;
            break;
            case 48 :
            r_block_mask = (uint64_t)0xFF7EFBDDDAA8ULL;
            break;
            case 52 :
            r_block_mask = (uint64_t)0xFFBFBF7BBB6A8ULL;
            break;
            case 56 :
            r_block_mask = (uint64_t)0xFFDFEFDF7BB6A8ULL;
            break;
            case 60 :
            r_block_mask = (uint64_t)0xFFF7FDFDF7BB6A8ULL;
            break;
            case 64 :
            r_block_mask = (uint64_t)0xFFFBFF7FBF7BB6A8ULL;
            break;
            default:
            assert(false && "nb_slots must be either 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60 or 64");
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    inline int nextEmptySlot() const
    //  This function returns :
    //  - the position of the first next empty slot in the table
    //    (starting from the last empty slot used)
    //    and updates the r_last_empty_slot register
    //  - -1 if the table is full
    {
        uint64_t i; 
        for(i = 0; i < nb_slots; i++)
        {
            if (!r_val[i]) return i;
        }

        return -1;
    }

    ////////////////////////////////////////////////////////////////////////////
    inline int hitAddr(const addr_t ad) const
    //  HIT on the address only
    //  This function takes an addr_t ad
    //  It returns :
    //  - the position of the first HIT in the table
    //  - -1 in case of MISS
    //  NB : HIT = (slot addr == ad) AND (slot is valid)
    {
        // checking all slots
        for (size_t i = 0; i < nb_slots; i++)
        {
            // if HIT, returning its position
            if(ad == r_addr[i] && r_val[i]) return i;
        }

        // MISS
        return -1;
    }

    ////////////////////////////////////////////////////////////////////////////
    inline int hitAddrKey(const addr_t ad, const uint32_t key) const
    //  HIT on the address AND the on the signature
    //  This function takes an addr_t ad and a uint32_t key
    //  It returns :
    //  - the position of the first HIT in the table
    //  - -1 in case of MISS
    //  NB : HIT = (slot addr == ad) AND (slot key == key)
    //                               AND (slot is valid)
    {
        // checking all slots
        for (size_t i = 0; i < nb_slots; i++)
        {
            // if HIT, returning its position
            if(ad == r_addr[i] && key == r_key[i] && r_val[i]) return i;
        }

        // MISS
        return -1;
    }

    ////////////////////////////////////////////////////////////////////////////
    inline void reset()
    {
        // init the table
        init();

        // init stat counters
        m_cpt_evic          = 0;
        m_cpt_ll            = 0;
        m_cpt_ll_update     = 0;
        m_cpt_sc            = 0;
        m_cpt_sc_success    = 0;
        m_cpt_check         = 0;
        m_cpt_sw            = 0;
    }


    public:

    ////////////////////////////////////////////////////////////////////////////
    GenericLLSCGlobalTable( const std::string   &n = "llsc_global_table" )
    :   name(n)
    {
        assert(nb_procs > 1); 
        init();
        init_block_mask();
    }

    ////////////////////////////////////////////////////////////////////////////
    ~GenericLLSCGlobalTable()
    {
    }

    ////////////////////////////////////////////////////////////////////////////
    //  This function initializes the table (all slots empty)
    inline void init()
    {
        // making all slots available by reseting all valid bits
        std::memset(r_val,  0, sizeof(*r_val) * nb_slots);
        std::memset(r_addr, 0, sizeof(*r_addr) * nb_slots);
        std::memset(r_key,  0, sizeof(*r_key) * nb_slots);

        // init registers
        r_next_key          = 0;
        r_last_counter      = 0; //TODO static in updateVictimSlot() ?
        r_write_ptr         = 0;
        r_last_empty        = 0;
    }

    ////////////////////////////////////////////////////////////////////////////
    inline uint32_t ll(const addr_t ad)
    //  This method registers an LL in the table and returns the key associated
    //  with the registration
    {
        // increment the ll access counter (for stats)
        m_cpt_ll++;

        // hit addr ?
        // YES
        //      enough time left ?
        //      YES
        //          use this registration, return key
        //      NO
        //          update this registration with a new key, return new key
        // NO
        //      table has an empty slot ?
        //      YES
        //              select empty slot
        //      NO
        //              select victim slot (r_write_ptr)
        //              update next victim
        //      open registration on selected slot
        //      update next key
        //      return the registration key

        //  Is the address found in the table ?
        int pos = hitAddr(ad);

        //  Yes, then return the associated key
        if (pos >= 0)
        {
#if RESERVATION_LIFE_SPAN == 0
            return r_key[pos];
#else
            uint32_t absdiff = ( r_key[pos] > r_next_key) ?
                                 r_key[pos] - r_next_key  :
                                 r_next_key - r_key[pos];

            if(absdiff < life_span) return r_key[pos];

            r_key[pos] = r_next_key;
            upNextKey();
            m_cpt_ll_update++;

            return r_key[pos];
#endif
        }

        //  No, then try to find an empty slot
        pos = nextEmptySlot();

        //  If there is no empty slot,
        //  evict an existing registration
        if (pos == -1)
        {
            //  update the victim slot for the next eviction
            updateVictimSlot();

            //  get the position of the evicted registration
            pos = r_write_ptr;

            // increment the eviction counter (for stats)
            m_cpt_evic++;
        }

        // get the key for the new registration
        uint32_t key    = r_next_key;
        //  update the registration slot
        r_key[pos]      = key   ;
        r_addr[pos]     = ad    ;
        r_val[pos]      = true  ;
        //  compute the next key
        upNextKey();

        // return the key of the new registration
        return key;

    }

    ////////////////////////////////////////////////////////////////////////////
    inline bool sc(const addr_t ad, const uint32_t key)
    //  This method checks if there is a valid registration for the SC (ad &&
    //  key) and, in case of hit,invalidates the registration and returns true
    //  (returns false otherwise)
    //
    //  The return value can be used to tell if the SC is atomic
    {
        // increment the sc access counter (for stats)
        m_cpt_sc++;
        // hit addr && hit key ?
        // NO
        //      return miss
        // YES
        //      inval registration and return hit

        //  Is there a valid registration in the table ?
        int pos = hitAddrKey(ad, key);
        if(pos >= 0)
        {
            // increment the sc success counter (for stats)
            m_cpt_sc_success++;
            // invalidate the registration
            r_val[pos] = false;
            // return the success of the sc operation
            return true;
        }
        else
        {
            // return the failure of the sc operation
            return false;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    inline bool check(const addr_t ad, const uint32_t key) const
    //  This method checks if there is a valid registration for the SC (ad &&
    //  key)
    //  The return value can be used to tell if the SC is atomic
    {
        // increment the check access counter (for stats)
        m_cpt_check++;

        return (hitAddrKey(ad, key) >= 0);
    }

    ////////////////////////////////////////////////////////////////////////////
    /*
    inline void sw(const addr_t ad)
    //  This method checks if there is a valid registration for the given
    //  address and, in case of hit, invalidates the registration
    {
        // increment the sw access counter (for stats)
        m_cpt_sw++;
        // hit addr ?
        // YES
        //      inval registration
        // NO
        //      nothing

        //  Is there a registration for the given address ?
        int pos = hitAddr(ad);
        //  If there is one, invalidate it
        if(pos >= 0) r_val[pos] = false;
    }
    */
    inline void sw(const addr_t ad_min, const addr_t ad_max)
    //  This method checks if there is / are valid registration(s) for the given
    //  range and, in case of hit(s), invalidates the registration(s)
    {
        // increment the sw access counter (for stats)
        m_cpt_sw++;
        // hit range ?
        // YES
        //      inval registration(s)
        // NO
        //      nothing

        // for every address in the given range ...
        for (addr_t i = ad_min; i <= ad_max; i+=4)
        {
            //  Is there a registration for the given address ?
            int pos = hitAddr(i);

            //  If there is one, invalidate it
            if (pos >= 0)
            {
                r_val[pos] = false;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    /*
    void fileTrace(FILE* file)
    {
    }
    */

    ////////////////////////////////////////////////////////////////////////////
    inline void print_trace(std::ostream& out = std::cout)
    {
        for ( size_t i = 0; i < nb_slots ; i++ )
        {
            out << std::setw(3)   << std::setfill(' ') << std::dec << i
                << std::noshowbase
                << " VLD_RX = "   << r_val[i]
                << std::uppercase
                << " ADR_RX = 0x" << std::setw(8) << std::setfill('0') << std::hex << (r_addr[i] >> 2)
                << " SGN_RX = 0x" << std::setw(8) << std::setfill('0') << std::hex << r_key[i]
                << std::endl;
        }
        out << "NEXT_SGN_RX = 0x" << std::setw(8) << std::setfill('0') << std::hex << r_next_key     << std::endl
            << "CNT_RX = 0x"      << std::setw(8) << std::setfill('0') << std::hex << r_last_counter << std::endl;
    }

    ////////////////////////////////////////////////////////////////////////////
    inline void print_stats(std::ostream& out = std::cout)
    {
        out << "# of ll accesses : " << m_cpt_ll            << std::endl
            << "# of ll updates  : " << m_cpt_ll_update     << std::endl
            << "# of sc accesses : " << m_cpt_sc            << std::endl
            << "# of sc success  : " << m_cpt_sc_success    << std::endl
            << "# of sw accesses : " << m_cpt_sw            << std::endl
            << "# of evictions   : " << m_cpt_evic          << std::endl ;
    }

};

} // end namespace soclib

#endif

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
