#ifndef XRAM_TRANSACTION_H_
#define XRAM_TRANSACTION_H_

#include <inttypes.h>
#include <systemc>
#include <cassert>
#include "arithmetics.h"

#define DEBUG_XRAM_TRANSACTION 0

////////////////////////////////////////////////////////////////////////
//                  A transaction tab entry         
////////////////////////////////////////////////////////////////////////

class TransactionTabEntry 
{
    typedef sc_dt::sc_uint<64> wide_data_t;
    typedef sc_dt::sc_uint<40> addr_t;
    typedef uint32_t data_t;
    typedef uint32_t be_t;

    public:
    bool   valid;                 // entry valid 
    bool   xram_read;             // read request to XRAM
    addr_t nline;                 // index (zy) of the requested line
    size_t srcid;                 // processor requesting the transaction
    size_t trdid;                 // processor requesting the transaction
    size_t pktid;                 // processor requesting the transaction
    bool   proc_read;             // read request from processor
    size_t read_length;           // length of the read (for the response)
    size_t word_index;            // index of the first read word (for response)
    std::vector<data_t> wdata;    // write buffer (one cache line)
    std::vector<be_t>   wdata_be; // be for each data in the write buffer
    bool    rerror;               // error returned by xram
    data_t  ll_key;               // LL key returned by the llsc_global_table
    bool    config;               // transaction required by CONFIG FSM

    /////////////////////////////////////////////////////////////////////
    // The init() function initializes the entry 
    /////////////////////////////////////////////////////////////////////
    void init()
    {
        valid  = false;
        rerror = false;
        config = false;
    }

    /////////////////////////////////////////////////////////////////////
    // The alloc() function initializes the vectors of an entry
    // The "n_words" argument is the number of words in a cache line.
    /////////////////////////////////////////////////////////////////////
    void alloc(size_t n_words)
    {
        wdata_be.reserve((int) n_words);
        wdata.reserve((int) n_words);
        for (size_t i = 0; i < n_words; i++)
        {
            wdata_be.push_back(0);
            wdata.push_back(0);
        }
    }

    ////////////////////////////////////////////////////////////////////
    // The copy() function copies an existing entry
    ////////////////////////////////////////////////////////////////////
    void copy(const TransactionTabEntry &source)
    {
        valid       = source.valid;
        xram_read   = source.xram_read;
        nline       = source.nline;
        srcid       = source.srcid;
        trdid       = source.trdid;
        pktid       = source.pktid;
        proc_read   = source.proc_read;
        read_length = source.read_length;
        word_index  = source.word_index;
        wdata_be.assign(source.wdata_be.begin(),source.wdata_be.end());
        wdata.assign(source.wdata.begin(),source.wdata.end());
        rerror      = source.rerror;
        ll_key      = source.ll_key;
        config      = source.config;
    }

    ////////////////////////////////////////////////////////////////////
    // The print() function prints the entry identified by "index". 
    ////////////////////////////////////////////////////////////////////
    void print(size_t index, size_t mode)
    {
        std::cout << "  TRT[" << std::dec << index << "] "
            << " valid = " << valid
            << " / error = " << rerror 
            << " / get = " << xram_read 
            << " / config = " << config << std::hex
            << " / address = " << nline*4*wdata.size()
            << " / srcid = " << srcid << std::endl;
        if (mode)
        {
            std::cout << "        trdid = " << trdid
                << " / pktid = " << pktid << std::dec 
                << " / proc_read = " << proc_read 
                << " / read_length = " << read_length
                << " / word_index  = " << word_index << std::hex 
                << " / ll_key = " << ll_key << std::endl;
            std::cout << "        wdata = ";
            for (size_t i = 0; i < wdata.size(); i++)
            {
                std::cout << std::hex << wdata[i] << " / ";
            }
            std::cout << std::endl;
        }
    }

    /////////////////////////////////////////////////////////////////////
    //         Constructors
    /////////////////////////////////////////////////////////////////////

    TransactionTabEntry()
    {
        wdata_be.clear();
        wdata.clear();
        valid  = false;
        rerror = false;
        config = false;
    }

    TransactionTabEntry(const TransactionTabEntry &source)
    {
        valid       = source.valid;
        xram_read   = source.xram_read;
        nline       = source.nline;
        srcid       = source.srcid;
        trdid       = source.trdid;
        pktid       = source.pktid;
        proc_read   = source.proc_read;
        read_length = source.read_length;
        word_index  = source.word_index;
        wdata_be.assign(source.wdata_be.begin(), source.wdata_be.end());
        wdata.assign(source.wdata.begin(), source.wdata.end());    
        rerror      = source.rerror;
        ll_key      = source.ll_key;
        config      = source.config;
    }

}; // end class TransactionTabEntry

////////////////////////////////////////////////////////////////////////
//                  The transaction tab                              
////////////////////////////////////////////////////////////////////////
class TransactionTab
{
    typedef sc_dt::sc_uint<64> wide_data_t;
    typedef sc_dt::sc_uint<40> addr_t;
    typedef uint32_t           data_t;
    typedef uint32_t           be_t;

    private:
    const std::string tab_name; // the name for logs
    size_t size_tab; // the size of the tab

    data_t be_to_mask(be_t be)
    {
        data_t ret = 0;
        if (be & 0x1) {
            ret = ret | 0x000000FF;
        }
        if (be & 0x2) {
            ret = ret | 0x0000FF00;
        }
        if (be & 0x4) {
            ret = ret | 0x00FF0000;
        }
        if (be & 0x8) {
            ret = ret | 0xFF000000;
        }
        return ret;
    }

    public:
    TransactionTabEntry * tab; // The transaction tab

    ////////////////////////////////////////////////////////////////////
    //        Constructors
    ////////////////////////////////////////////////////////////////////
    TransactionTab()
    {
        size_tab = 0;
        tab = NULL;
    }

    TransactionTab(const std::string &name,
            size_t n_entries, 
            size_t n_words)
        : tab_name(name),
        size_tab(n_entries) 
    {
        tab = new TransactionTabEntry[size_tab];
        for (size_t i = 0; i < size_tab; i++) 
        {
            tab[i].alloc(n_words);
        }
    }

    ~TransactionTab()
    {
        delete [] tab;
    }
    /////////////////////////////////////////////////////////////////////
    // The size() function returns the size of the tab
    /////////////////////////////////////////////////////////////////////
    size_t size()
    {
        return size_tab;
    }
    /////////////////////////////////////////////////////////////////////
    // The init() function initializes the transaction tab entries
    /////////////////////////////////////////////////////////////////////
    void init()
    {
        for (size_t i = 0; i < size_tab; i++) 
        {
            tab[i].init();
        }
    }
    /////////////////////////////////////////////////////////////////////
    // The print() function prints TRT content.
    // Detailed content if detailed argument is non zero.
    /////////////////////////////////////////////////////////////////////
    void print(size_t detailed = 0)
    {
        std::cout << "  < TRT content in " <<  tab_name << " >" << std::endl;
        for (size_t id = 0; id < size_tab; id++)
        {
            tab[id].print( id , detailed );
        }
    }
    /////////////////////////////////////////////////////////////////////
    // The read() function returns a transaction tab entry.
    // Arguments :
    // - index : the index of the entry to read
    /////////////////////////////////////////////////////////////////////
    TransactionTabEntry read(const size_t index)
    {
        assert((index < size_tab) and "MEMC ERROR: Invalid Transaction Tab Entry");

        return tab[index];
    }
    /////////////////////////////////////////////////////////////////////
    // The full() function returns the state of the transaction tab
    // Arguments :
    // - index : (return argument) the index of an empty entry 
    // The function returns true if the transaction tab is full
    /////////////////////////////////////////////////////////////////////
    bool full(size_t & index)
    {
        for (size_t i = 0; i < size_tab; i++)
        {
            if (!tab[i].valid)
            {
                index = i;
                return false;    
            }
        }
        return true;
    }
    /////////////////////////////////////////////////////////////////////
    // The hit_read() function checks if an XRAM read transaction exists 
    // for a given cache line.
    // Arguments :
    // - index : (return argument) the index of the hit entry, if there is 
    // - nline : the index (zy) of the requested line
    // The function returns true if a read request has already been sent
    //////////////////////////////////////////////////////////////////////
    bool hit_read(const addr_t nline,size_t &index)
    {
        for (size_t i = 0; i < size_tab; i++)
        {
            if ((tab[i].valid && (nline == tab[i].nline)) && (tab[i].xram_read)) 
            {
                index = i;
                return true;    
            }
        }
        return false;
    }
    ///////////////////////////////////////////////////////////////////////
    // The hit_write() function looks if an XRAM write transaction exists 
    // for a given line.
    // Arguments :
    // - nline : the index (zy) of the requested line
    // The function returns true if a write request has already been sent
    ///////////////////////////////////////////////////////////////////////
    bool hit_write(const addr_t nline)
    {
        for (size_t i = 0; i < size_tab; i++)
        {
            if (tab[i].valid && (nline == tab[i].nline) && !(tab[i].xram_read)) 
            {
                return true;    
            }
        }
        return false;
    }
    /////////////////////////////////////////////////////////////////////
    // The write_data_mask() function writes a vector of data (a line).
    // The data is written only if the corresponding bits are set
    // in the be vector. 
    // Arguments :
    // - index : the index of the request in the transaction tab
    // - be   : vector of be 
    // - data : vector of data
    /////////////////////////////////////////////////////////////////////
    void write_data_mask(const size_t index, 
            const std::vector<be_t> & be, 
            const std::vector<data_t> & data) 
    {
        assert( (index < size_tab) and
                "MEMC ERROR: The selected entry is out of range in TRT write_data_mask()");

        assert( (be.size() == tab[index].wdata_be.size()) and
                "MEMC ERROR: Bad be size in TRT write_data_mask()");

        assert( (data.size() == tab[index].wdata.size()) and
                "MEMC ERROR: Bad data size in TRT write_data_mask()");

        for (size_t i = 0; i < tab[index].wdata_be.size(); i++) 
        {
            tab[index].wdata_be[i] = tab[index].wdata_be[i] | be[i];
            data_t mask = be_to_mask(be[i]);
            tab[index].wdata[i] = (tab[index].wdata[i] & ~mask) | (data[i] & mask);
        }
    }
    /////////////////////////////////////////////////////////////////////
    // The set() function registers a transaction (read or write)
    // to the XRAM in the transaction tab.
    // Arguments :
    // - index : index in the transaction tab
    // - xram_read : transaction type (read or write a cache line)
    // - nline : the index (zy) of the cache line
    // - srcid : srcid of the initiator that caused the transaction
    // - trdid : trdid of the initiator that caused the transaction
    // - pktid : pktid of the initiator that caused the transaction
    // - proc_read : does the initiator want a copy
    // - read_length : length of read (in case of processor read)
    // - word_index : index in the line (in case of single word read)
    // - data : the data to write (in case of write)
    // - data_be : the mask of the data to write (in case of write)
    // - ll_key  : the ll key (if any) returned by the llsc_global_table
    // - config  : transaction required by config FSM
    /////////////////////////////////////////////////////////////////////
    void set(const size_t index,
            const bool xram_read,
            const addr_t nline,
            const size_t srcid,
            const size_t trdid,
            const size_t pktid,
            const bool proc_read,
            const size_t read_length,
            const size_t word_index,
            const std::vector<be_t> & data_be,
            const std::vector<data_t> & data, 
            const data_t ll_key = 0,
            const bool config = false) 
    {
        assert((index < size_tab) and
                "MEMC ERROR: The selected entry is out of range in TRT set()");

        assert((data_be.size()==tab[index].wdata_be.size()) and 
                "MEMC ERROR: Bad data_be argument in TRT set()");

        assert((data.size()==tab[index].wdata.size()) and 
                "MEMC ERROR: Bad data argument in TRT set()");

        tab[index].valid       = true;
        tab[index].xram_read   = xram_read;
        tab[index].nline       = nline;
        tab[index].srcid       = srcid;
        tab[index].trdid       = trdid;
        tab[index].pktid       = pktid;
        tab[index].proc_read   = proc_read;
        tab[index].read_length = read_length;
        tab[index].word_index  = word_index;
        tab[index].ll_key      = ll_key;
        tab[index].config      = config;
        for (size_t i = 0; i < tab[index].wdata.size(); i++) 
        {
            tab[index].wdata_be[i] = data_be[i];
            tab[index].wdata[i]    = data[i];
        }
    }

    /////////////////////////////////////////////////////////////////////
    // The write_rsp() function writes two 32 bits words of the response 
    // to a XRAM read transaction.
    // The BE field in TRT is taken into account.
    // Arguments :
    // - index : index of the entry in TRT
    // - word  : index of the 32 bits word in the line
    // - data  : 64 bits value (first data right)
    /////////////////////////////////////////////////////////////////////
    void write_rsp(const size_t index,
            const size_t        word,
            const wide_data_t   data,
            const bool          rerror)
    {
        data_t value;
        data_t mask;

        assert((index < size_tab) and
                "MEMC ERROR: The selected entry is out of range in TRT write_rsp()");

        assert((word < tab[index].wdata_be.size()) and 
                "MEMC ERROR: Bad word index in TRT write_rsp()");

        assert((tab[index].valid) and
                "MEMC ERROR: TRT entry not valid in TRT write_rsp()");

        assert((tab[index].xram_read ) and
                "MEMC ERROR: TRT entry is not a GET in TRT write_rsp()");

        if (rerror)
        {
            tab[index].rerror = true;
            return;
        }

        // first 32 bits word
        value = (data_t) data;
        mask  = be_to_mask(tab[index].wdata_be[word]);
        tab[index].wdata[word] = (tab[index].wdata[word] & mask) | (value & ~mask);

        // second 32 bits word
        value = (data_t) (data >> 32);
        mask  = be_to_mask(tab[index].wdata_be[word + 1]);
        tab[index].wdata[word + 1] = (tab[index].wdata[word + 1] & mask) | (value & ~mask);
    }
    /////////////////////////////////////////////////////////////////////
    // The erase() function erases an entry in the transaction tab.
    // Arguments :
    // - index : the index of the request in the transaction tab
    /////////////////////////////////////////////////////////////////////
    void erase(const size_t index)
    {
        assert( (index < size_tab) and 
                "MEMC ERROR: The selected entry is out of range in TRT erase()");

        tab[index].valid  = false;
        tab[index].rerror = false;
    }
    /////////////////////////////////////////////////////////////////////
    // The is_config() function returns the config flag value.
    // Arguments :
    // - index : the index of the entry in the transaction tab
    /////////////////////////////////////////////////////////////////////
    bool is_config(const size_t index)
    {
        assert( (index < size_tab) and
                "MEMC ERROR: The selected entry is out of range in TRT is_config()");

        return tab[index].config;
    }
}; // end class TransactionTab

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

