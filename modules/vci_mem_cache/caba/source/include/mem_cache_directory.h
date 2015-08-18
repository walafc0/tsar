#ifndef SOCLIB_CABA_MEM_CACHE_DIRECTORY_H
#define SOCLIB_CABA_MEM_CACHE_DIRECTORY_H 

#include <inttypes.h>
#include <systemc>
#include <cassert>
#include <cstring>
#include "arithmetics.h"

//#define RANDOM_EVICTION

namespace soclib { namespace caba {

using namespace sc_core;

////////////////////////////////////////////////////////////////////////
//                    A LRU entry 
////////////////////////////////////////////////////////////////////////
class LruEntry {

    public:

        bool recent;            

        void init()
        {
            recent = false;
        }

}; // end class LruEntry

////////////////////////////////////////////////////////////////////////
//                    An Owner
////////////////////////////////////////////////////////////////////////
class Owner{

    public:
        // Fields
        bool   inst;  // Is the owner an ICache ?
        size_t srcid; // The SRCID of the owner

        ////////////////////////
        // Constructors
        ////////////////////////
        Owner(bool i_inst,
                size_t i_srcid)
        {
            inst  = i_inst;
            srcid = i_srcid;
        }

        Owner(const Owner &a)
        {
            inst  = a.inst;
            srcid = a.srcid;
        }

        Owner()
        {
            inst  = false;
            srcid = 0;
        }
        // end constructors

}; // end class Owner


////////////////////////////////////////////////////////////////////////
//                    A directory entry                               
////////////////////////////////////////////////////////////////////////
class DirectoryEntry {

    typedef uint32_t tag_t;

    public:

    bool    valid;  // entry valid
    bool    is_cnt; // directory entry is in counter mode
    bool    dirty;  // entry dirty
    bool    lock;   // entry locked
    tag_t   tag;    // tag of the entry
    size_t  count;  // number of copies
    Owner   owner;  // an owner of the line 
    size_t  ptr;    // pointer to the next owner

    DirectoryEntry()
    {
        valid       = false;
        is_cnt      = false;
        dirty       = false;
        lock        = false;
        tag         = 0;
        count       = 0;
        owner.inst  = 0;
        owner.srcid = 0;
        ptr         = 0;
    }

    DirectoryEntry(const DirectoryEntry &source)
    {
        valid  = source.valid;
        is_cnt = source.is_cnt;
        dirty  = source.dirty;
        lock   = source.lock;
        tag    = source.tag;
        count  = source.count;
        owner  = source.owner;
        ptr    = source.ptr;
    }          

    /////////////////////////////////////////////////////////////////////
    // The init() function initializes the entry 
    /////////////////////////////////////////////////////////////////////
    void init()
    {
        valid  = false;
        is_cnt = false;
        dirty  = false;
        lock   = false;
        count  = 0;
    }

    /////////////////////////////////////////////////////////////////////
    // The copy() function copies an existing source entry to a target 
    /////////////////////////////////////////////////////////////////////
    void copy(const DirectoryEntry &source)
    {
        valid  = source.valid;
        is_cnt = source.is_cnt;
        dirty  = source.dirty;
        lock   = source.lock;
        tag    = source.tag;
        count  = source.count;
        owner  = source.owner;
        ptr    = source.ptr;
    }

    ////////////////////////////////////////////////////////////////////
    // The print() function prints the entry 
    ////////////////////////////////////////////////////////////////////
    void print()
    {
        std::cout << "Valid = " << valid 
            << " ; IS COUNT = " << is_cnt 
            << " ; Dirty = " << dirty 
            << " ; Lock = " << lock 
            << " ; Tag = " << std::hex << tag << std::dec 
            << " ; Count = " << count 
            << " ; Owner = " << owner.srcid 
            << " " << owner.inst 
            << " ; Pointer = " << ptr << std::endl;
    }

}; // end class DirectoryEntry

////////////////////////////////////////////////////////////////////////
//                       The directory  
////////////////////////////////////////////////////////////////////////
class CacheDirectory {

    typedef sc_dt::sc_uint<40> addr_t;
    typedef uint32_t data_t;
    typedef uint32_t tag_t;

    private:

    // Directory constants
    size_t   m_ways;
    size_t   m_sets;
    size_t   m_words;
    size_t   m_width;
    uint32_t lfsr;

    // the directory & lru tables
    DirectoryEntry ** m_dir_tab;
    LruEntry       ** m_lru_tab;

    public:

    ////////////////////////
    // Constructor
    ////////////////////////
    CacheDirectory( size_t ways, size_t sets, size_t words, size_t address_width)     
    {
        m_ways  = ways; 
        m_sets  = sets;
        m_words = words;
        m_width = address_width;
        lfsr = -1;

        m_dir_tab = new DirectoryEntry*[sets];
        for (size_t i = 0; i < sets; i++ ) {
            m_dir_tab[i] = new DirectoryEntry[ways];
            for (size_t j = 0; j < ways; j++) m_dir_tab[i][j].init();
        }
        m_lru_tab = new LruEntry*[sets];
        for (size_t i = 0; i < sets; i++) {
            m_lru_tab[i] = new LruEntry[ways];
            for (size_t j = 0; j < ways; j++) m_lru_tab[i][j].init();
        }
    } // end constructor

    /////////////////
    // Destructor
    /////////////////
    ~CacheDirectory()
    {
        for(size_t i = 0; i < m_sets; i++){
            delete [] m_dir_tab[i];
            delete [] m_lru_tab[i];
        }
        delete [] m_dir_tab;
        delete [] m_lru_tab;
    } // end destructor

    /////////////////////////////////////////////////////////////////////
    // The read() function reads a directory entry. In case of hit, the
    // LRU is updated.
    // Arguments :
    // - address : the address of the entry 
    // - way : (return argument) the way of the entry in case of hit
    // The function returns a copy of a (valid or invalid) entry  
    /////////////////////////////////////////////////////////////////////
    DirectoryEntry read(const addr_t &address, size_t &way)
    {

#define L2 soclib::common::uint32_log2
        const size_t set = (size_t)(address >> (L2(m_words) + 2)) & (m_sets - 1);
        const tag_t  tag = (tag_t)(address >> (L2(m_sets) + L2(m_words) + 2));
#undef L2

        bool hit = false;
        for (size_t i = 0; i < m_ways; i++ ) 
        {
            bool equal = (m_dir_tab[set][i].tag == tag);
            bool valid = m_dir_tab[set][i].valid;
            hit        = equal && valid;
            if (hit) 
            {            
                way = i;
                break;
            } 
        }
        if (hit) 
        {
            m_lru_tab[set][way].recent = true;
            return DirectoryEntry(m_dir_tab[set][way]);
        } 
        else 
        {
            return DirectoryEntry();
        }
    } // end read()

    /////////////////////////////////////////////////////////////////////
    // The inval function invalidate an entry defined by the set and
    // way arguments. 
    /////////////////////////////////////////////////////////////////////
    void inval(const size_t &way, const size_t &set)
    {
        m_dir_tab[set][way].init();
    }

    /////////////////////////////////////////////////////////////////////
    // The read_neutral() function reads a directory entry, without
    // changing the LRU
    // Arguments :
    // - address : the address of the entry 
    // The function returns a copy of a (valid or invalid) entry  
    /////////////////////////////////////////////////////////////////////
    DirectoryEntry read_neutral(const addr_t &address, 
            size_t * ret_way,
            size_t * ret_set )
    {

#define L2 soclib::common::uint32_log2
        size_t set = (size_t) (address >> (L2(m_words) + 2)) & (m_sets - 1);
        tag_t  tag = (tag_t) (address >> (L2(m_sets) + L2(m_words) + 2));
#undef L2

        for (size_t way = 0; way < m_ways; way++)
        {
            bool equal = (m_dir_tab[set][way].tag == tag);
            bool valid = m_dir_tab[set][way].valid;
            if (equal and valid)
            {
                *ret_set = set;
                *ret_way = way; 
                return DirectoryEntry(m_dir_tab[set][way]);
            }
        } 
        return DirectoryEntry();
    } // end read_neutral()

    /////////////////////////////////////////////////////////////////////
    // The write function writes a new entry, 
    // and updates the LRU bits if necessary.
    // Arguments :
    // - set : the set of the entry
    // - way : the way of the entry
    // - entry : the entry value
    /////////////////////////////////////////////////////////////////////
    void write(const size_t &set, 
               const size_t &way, 
               const DirectoryEntry &entry)
    {
        assert((set < m_sets) && "Cache Directory write : The set index is invalid");
        assert((way < m_ways) && "Cache Directory write : The way index is invalid");

        // update Directory
        m_dir_tab[set][way].copy(entry);

        // update LRU bits
        bool all_recent = true;
        for (size_t i = 0; i < m_ways; i++) 
        {
            if (i != way) all_recent = m_lru_tab[set][i].recent && all_recent;
        }
        if (all_recent) 
        {
            for (size_t i = 0; i < m_ways; i++) m_lru_tab[set][i].recent = false;
        } 
        else 
        {
            m_lru_tab[set][way].recent = true;
        }
    } // end write()

    /////////////////////////////////////////////////////////////////////
    // The print() function prints a selected directory entry
    // Arguments :
    // - set : the set of the entry to print
    // - way : the way of the entry to print
    /////////////////////////////////////////////////////////////////////
    void print(const size_t &set, const size_t &way)
    {
        std::cout << std::dec << " set : " << set << " ; way : " << way << " ; " ;
        m_dir_tab[set][way].print();
    } // end print()

    /////////////////////////////////////////////////////////////////////
    // The select() function selects a directory entry to evince.
    // Arguments :
    // - set   : (input argument) the set to modify
    // - way   : (return argument) the way to evince
    /////////////////////////////////////////////////////////////////////
    DirectoryEntry select(const size_t &set, size_t &way)
    {
        assert((set < m_sets) 
                && "Cache Directory : (select) The set index is invalid");

        // looking for an empty slot
        for (size_t i = 0; i < m_ways; i++)
        {
            if (not m_dir_tab[set][i].valid)
            {
                way = i;
                return DirectoryEntry(m_dir_tab[set][way]);
            }
        }

#ifdef RANDOM_EVICTION
        lfsr = (lfsr >> 1) ^ ((-(lfsr & 1)) & 0xd0000001);
        way = lfsr % m_ways;
        return DirectoryEntry(m_dir_tab[set][way]);
#endif

        // looking for a not locked and not recently used entry
        for (size_t i = 0; i < m_ways; i++)
        {
            if ((not m_lru_tab[set][i].recent) && (not m_dir_tab[set][i].lock))
            {
                way = i;
                return DirectoryEntry(m_dir_tab[set][way]);
            }
        }

        // looking for a locked not recently used entry
        for (size_t i = 0; i < m_ways; i++)
        {
            if ((not m_lru_tab[set][i].recent) && (m_dir_tab[set][i].lock))
            {
                way = i;
                return DirectoryEntry(m_dir_tab[set][way]);
            }
        }

        // looking for a recently used entry not locked
        for (size_t i = 0; i < m_ways; i++)
        {
            if ((m_lru_tab[set][i].recent) && (not m_dir_tab[set][i].lock))
            {
                way = i;
                return DirectoryEntry(m_dir_tab[set][way]);
            }
        }

        // select way 0 (even if entry is locked and recently used)
        way = 0;
        return DirectoryEntry(m_dir_tab[set][0]);
    } // end select()

    /////////////////////////////////////////////////////////////////////
    //         Global initialisation function
    /////////////////////////////////////////////////////////////////////
    void init()
    {
        for (size_t set = 0; set < m_sets; set++) 
        {
            for (size_t way = 0; way < m_ways; way++) 
            {
                m_dir_tab[set][way].init();
                m_lru_tab[set][way].init();
            }
        }
    } // end init()

}; // end class CacheDirectory

///////////////////////////////////////////////////////////////////////
//                    A Heap Entry
///////////////////////////////////////////////////////////////////////
class HeapEntry{

    public:
        // Fields of the entry
        Owner  owner;
        size_t next;

        ////////////////////////
        // Constructor
        ////////////////////////
        HeapEntry()
            :owner(false, 0)
        {
            next = 0;
        } // end constructor

        ////////////////////////
        // Constructor
        ////////////////////////
        HeapEntry(const HeapEntry &entry)
        {
            owner.inst  = entry.owner.inst;
            owner.srcid = entry.owner.srcid;
            next        = entry.next;
        } // end constructor

        /////////////////////////////////////////////////////////////////////
        // The copy() function copies an existing source entry to a target 
        /////////////////////////////////////////////////////////////////////
        void copy(const HeapEntry &entry)
        {
            owner.inst     = entry.owner.inst;
            owner.srcid    = entry.owner.srcid;
            next           = entry.next;
        } // end copy()

        ////////////////////////////////////////////////////////////////////
        // The print() function prints the entry 
        ////////////////////////////////////////////////////////////////////
        void print() {
            std::cout 
                << " -- owner.inst     : " << std::dec << owner.inst << std::endl
                << " -- owner.srcid    : " << std::dec << owner.srcid << std::endl
                << " -- next           : " << std::dec << next << std::endl;

        } // end print()

}; // end class HeapEntry

////////////////////////////////////////////////////////////////////////
//                        The Heap 
////////////////////////////////////////////////////////////////////////
class HeapDirectory{

    private:
        // Registers and the heap
        size_t ptr_free;
        bool   full;
        HeapEntry * m_heap_tab;

        // Constants for debugging purpose
        size_t    tab_size;

    public:
        ////////////////////////
        // Constructor
        ////////////////////////
        HeapDirectory(uint32_t size) {
            assert(size > 0 && "Memory Cache, HeapDirectory constructor : invalid size");
            ptr_free    = 0;
            full        = false;
            m_heap_tab  = new HeapEntry[size];
            tab_size    = size;
        } // end constructor

        /////////////////
        // Destructor
        /////////////////
        ~HeapDirectory() {
            delete [] m_heap_tab;
        } // end destructor

        /////////////////////////////////////////////////////////////////////
        //         Global initialisation function
        /////////////////////////////////////////////////////////////////////
        void init() {
            ptr_free = 0;
            full = false;
            for (size_t i = 0; i< tab_size - 1; i++){
                m_heap_tab[i].next = i + 1;
            }
            m_heap_tab[tab_size - 1].next = tab_size - 1;
            return;
        }

        /////////////////////////////////////////////////////////////////////
        // The print() function prints a selected directory entry
        // Arguments :
        // - ptr : the pointer to the entry to print
        /////////////////////////////////////////////////////////////////////
        void print(const size_t &ptr) {
            std::cout << "Heap, printing the entry : " << std::dec << ptr << std::endl;
            m_heap_tab[ptr].print();
        } // end print()

        /////////////////////////////////////////////////////////////////////
        // The print_list() function prints a list from selected directory entry
        // Arguments :
        // - ptr : the pointer to the first entry to print
        /////////////////////////////////////////////////////////////////////
        void print_list(const size_t &ptr) {
            bool end = false;
            size_t ptr_temp = ptr;
            std::cout << "Heap, printing the list from : " << std::dec << ptr << std::endl;
            while (!end){
                m_heap_tab[ptr_temp].print();
                if (ptr_temp == m_heap_tab[ptr_temp].next) {
                    end = true;
                }
                ptr_temp = m_heap_tab[ptr_temp].next;
            } 
        } // end print_list()

        /////////////////////////////////////////////////////////////////////
        // The is_full() function return true if the heap is full.
        /////////////////////////////////////////////////////////////////////
        bool is_full() {
            return full;
        } // end is_full()

        /////////////////////////////////////////////////////////////////////
        // The next_free_ptr() function returns the pointer 
        // to the next free entry.
        /////////////////////////////////////////////////////////////////////
        size_t next_free_ptr() {
            return ptr_free;
        } // end next_free_ptr()

        /////////////////////////////////////////////////////////////////////
        // The next_free_entry() function returns 
        // a copy of the next free entry.
        /////////////////////////////////////////////////////////////////////
        HeapEntry next_free_entry() {
            return HeapEntry(m_heap_tab[ptr_free]);
        } // end next_free_entry()

        /////////////////////////////////////////////////////////////////////
        // The write_free_entry() function modify the next free entry.
        // Arguments :
        // - entry : the entry to write
        /////////////////////////////////////////////////////////////////////
        void write_free_entry(const HeapEntry &entry){
            m_heap_tab[ptr_free].copy(entry);
        } // end write_free_entry()

        /////////////////////////////////////////////////////////////////////
        // The write_free_ptr() function writes the pointer
        // to the next free entry
        /////////////////////////////////////////////////////////////////////
        void write_free_ptr(const size_t &ptr){
            assert((ptr < tab_size) && "HeapDirectory error : try to write a wrong free pointer");
            ptr_free = ptr;
        } // end write_free_ptr()

        /////////////////////////////////////////////////////////////////////
        // The set_full() function sets the full bit (to true).
        /////////////////////////////////////////////////////////////////////
        void set_full() {
            full = true;
        } // end set_full()

        /////////////////////////////////////////////////////////////////////
        // The unset_full() function unsets the full bit (to false).
        /////////////////////////////////////////////////////////////////////
        void unset_full() {
            full = false;
        } // end unset_full()

        /////////////////////////////////////////////////////////////////////
        // The read() function returns a copy of
        // the entry pointed by the argument
        // Arguments :
        //  - ptr : the pointer to the entry to read
        /////////////////////////////////////////////////////////////////////
        HeapEntry read(const size_t &ptr) {
            assert((ptr < tab_size) && "HeapDirectory error : try to write a wrong free pointer");
            return HeapEntry(m_heap_tab[ptr]);
        } // end read()

        /////////////////////////////////////////////////////////////////////
        // The write() function writes an entry in the heap
        // Arguments :
        //  - ptr : the pointer to the entry to replace
        //  - entry : the entry to write
        /////////////////////////////////////////////////////////////////////
        void write(const size_t &ptr, const HeapEntry &entry) {
            assert((ptr < tab_size) && "HeapDirectory error : try to write a wrong free pointer");
            m_heap_tab[ptr].copy(entry);
        } // end write()

}; // end class HeapDirectory

////////////////////////////////////////////////////////////////////////
//                        Cache Data 
////////////////////////////////////////////////////////////////////////
class CacheData 
{
    private:
        const uint32_t m_sets;
        const uint32_t m_ways;
        const uint32_t m_words;

        uint32_t *** m_cache_data;

    public:

        ///////////////////////////////////////////////////////
        CacheData(uint32_t ways, uint32_t sets, uint32_t words)
            : m_sets(sets), m_ways(ways), m_words(words) 
        {
            m_cache_data = new uint32_t ** [ways];
            for (size_t i = 0; i < ways; i++) 
            {
                m_cache_data[i] = new uint32_t * [sets];
            }
            for (size_t i = 0; i < ways; i++) 
            {
                for (size_t j = 0; j < sets; j++) 
                {
                    m_cache_data[i][j] = new uint32_t[words];
                    // Init to avoid potential errors from memory checkers
                    std::memset(m_cache_data[i][j], 0, sizeof(uint32_t) * words);
                }
            }
        }
        ////////////
        ~CacheData() 
        {
            for (size_t i = 0; i < m_ways; i++)
            {
                for (size_t j = 0; j < m_sets; j++)
                {
                    delete [] m_cache_data[i][j];
                }
            }
            for (size_t i = 0; i < m_ways; i++)
            {
                delete [] m_cache_data[i];
            }
            delete [] m_cache_data;
        }
        //////////////////////////////////////////
        uint32_t read (const uint32_t &way,
                const uint32_t &set,
                const uint32_t &word) const 
        {
            assert((set  < m_sets)  && "Cache data error: Trying to read a wrong set" );
            assert((way  < m_ways)  && "Cache data error: Trying to read a wrong way" );
            assert((word < m_words) && "Cache data error: Trying to read a wrong word");

            return m_cache_data[way][set][word];
        }
        //////////////////////////////////////////
        void read_line(const uint32_t &way,
                const uint32_t &set,
                sc_core::sc_signal<uint32_t> * cache_line)
        {
            assert((set < m_sets) && "Cache data error: Trying to read a wrong set" );
            assert((way < m_ways) && "Cache data error: Trying to read a wrong way" );

            for (uint32_t word = 0; word < m_words; word++) {
                cache_line[word].write(m_cache_data[way][set][word]);
            }
        }
        /////////////////////////////////////////
        void write (const uint32_t &way,
                const uint32_t &set,
                const uint32_t &word,
                const uint32_t &data,
                const uint32_t &be = 0xF) 
        {

            assert((set  < m_sets)  && "Cache data error: Trying to write a wrong set" );
            assert((way  < m_ways)  && "Cache data error: Trying to write a wrong way" );
            assert((word < m_words) && "Cache data error: Trying to write a wrong word");
            assert((be  <= 0xF)     && "Cache data error: Trying to write a wrong be");

            if (be == 0x0) return;

            if (be == 0xF)
            {
                m_cache_data[way][set][word] = data; 
                return;
            }

            uint32_t mask = 0;
            if (be & 0x1) mask = mask | 0x000000FF;
            if (be & 0x2) mask = mask | 0x0000FF00;
            if (be & 0x4) mask = mask | 0x00FF0000;
            if (be & 0x8) mask = mask | 0xFF000000;

            m_cache_data[way][set][word] = 
                (data & mask) | (m_cache_data[way][set][word] & ~mask);
        }
}; // end class CacheData

}} // end namespaces

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

