#ifndef IOB_TRANSACTION_H_
#define IOB_TRANSACTION_H_
 
#include <inttypes.h>
#include <systemc>
#include <cassert>
#include "arithmetics.h"

#define DEBUG_IOB_TRANSACTION 0

////////////////////////////////////////////////////////////////////////
//                  A transaction tab entry         
////////////////////////////////////////////////////////////////////////

class TransactionTabIOEntry {
  typedef uint32_t size_t;

 public:
  bool   valid;          // valid entry
  size_t srcid;          // initiator requesting the transaction
  size_t trdid;          // thread ID of transaction

  /////////////////////////////////////////////////////////////////////
  // The init() function initializes the entry 
  /////////////////////////////////////////////////////////////////////
  void init()
  {
    valid = false;
  }

  ////////////////////////////////////////////////////////////////////
  // The copy() function copies an existing entry
  // Its arguments are :
  // - source : the transaction tab entry to copy
  ////////////////////////////////////////////////////////////////////
  void copy(const TransactionTabIOEntry &source)
  {
    valid = source.valid;
    srcid = source.srcid;
    trdid = source.trdid;
  }

  ////////////////////////////////////////////////////////////////////
  // The print() function prints the entry 
  ////////////////////////////////////////////////////////////////////
  void print(){
    std::cout << "   valid = " << valid << std::hex
              << " / srcid = " << srcid
              << " / trdid = " << trdid << std::dec
              << std::endl;
  }

  /////////////////////////////////////////////////////////////////////
  //        Constructors
  /////////////////////////////////////////////////////////////////////

  TransactionTabIOEntry()
  {
    valid = false;
  }

  TransactionTabIOEntry(const TransactionTabIOEntry &source){
    valid = source.valid;
    srcid = source.srcid;
    trdid = source.trdid;
  }

}; // end class TransactionTabIOEntry

////////////////////////////////////////////////////////////////////////
//                  The transaction tab                              
////////////////////////////////////////////////////////////////////////
class TransactionTabIO{
 private:
  const size_t size_tab;          // The size of the tab

 public:
  TransactionTabIOEntry *tab;     // The transaction tab

  ////////////////////////////////////////////////////////////////////
  //        Constructors
  ////////////////////////////////////////////////////////////////////
  TransactionTabIO(size_t n_entries) : size_tab(n_entries)
  {
    tab = new TransactionTabIOEntry[size_tab];
  }

  ~TransactionTabIO()
  {
    delete [] tab;
  }

  /////////////////////////////////////////////////////////////////////
  // The size() function returns the size of the tab
  /////////////////////////////////////////////////////////////////////
  const size_t& size()
  {
    return size_tab;
  }

  /////////////////////////////////////////////////////////////////////
  // The init() function initializes the transaction tab entries
  /////////////////////////////////////////////////////////////////////
  void init()
  {
    for ( size_t index = 0; index < size_tab; index++)
    {
      tab[index].init();
    }
  }

  /////////////////////////////////////////////////////////////////////
  // The print() function prints a transaction tab entry
  // Arguments :
  // - index : the index of the entry to print
  /////////////////////////////////////////////////////////////////////
  void print(const size_t index)
  {
    assert( (index < size_tab) && "Invalid Transaction Tab Entry");
    tab[index].print();
    return;
  }

  /////////////////////////////////////////////////////////////////////
  // The printTrace() function prints all transaction tab entries
  /////////////////////////////////////////////////////////////////////
  void printTrace()
  {
    for (size_t index = 0; index < size_tab; index++)
    {
      tab[index].print();
    }
  }

  /////////////////////////////////////////////////////////////////////
  // The read() function returns a transaction tab entry.
  // Arguments :
  // - index : the index of the entry to read
  /////////////////////////////////////////////////////////////////////
  TransactionTabIOEntry& read(const size_t index)
  {
    assert( (index < size_tab) && "Invalid Transaction Tab Entry");
    return tab[index];
  }
  
  /////////////////////////////////////////////////////////////////////
  // The readSrcid() function returns the srcid field of a transaction tab entry.
  // Arguments :
  // - index : the index of the entry to read
  /////////////////////////////////////////////////////////////////////
  size_t readSrcid(const size_t index)
  {
    assert( (index < size_tab) && "Invalid Transaction Tab Entry");
    return tab[index].srcid;
  }

  /////////////////////////////////////////////////////////////////////
  // The readTrdid() function returns the trdid field of a transaction tab entry.
  // Arguments :
  // - index : the index of the entry to read
  /////////////////////////////////////////////////////////////////////
  size_t readTrdid(const size_t index)
  {
    assert( (index < size_tab) && "Invalid Transaction Tab Entry");
    return tab[index].trdid;
  }

  /////////////////////////////////////////////////////////////////////
  // The full() function returns the state of the transaction tab
  // Arguments :
  // - index : (return argument) the index of an empty entry 
  // The function returns true if the transaction tab is full
  /////////////////////////////////////////////////////////////////////
  bool full(size_t &index)
  {
    for(size_t i=0; i<size_tab; i++)
    {
      if(!tab[i].valid)
      {
        index = i;
        return false;   
      }
    }
    return true;
  }

  /////////////////////////////////////////////////////////////////////
  // The set() function registers a transaction (read or write)
  // to the XRAM in the transaction tab.
  // Arguments :
  // - index : index in the transaction tab
  // - srcid : srcid of the initiator that caused the transaction
  // - trdid : trdid of the initiator that caused the transaction
  /////////////////////////////////////////////////////////////////////
  void set(const size_t index,
           const size_t srcid,
           const size_t trdid) 
  {
    assert( (index < size_tab) && "Invalid Transaction Tab Entry");
    tab[index].valid = true;
    tab[index].srcid = srcid;
    tab[index].trdid = trdid;
  }

  /////////////////////////////////////////////////////////////////////
  // The erase() function erases an entry in the transaction tab.
  // Arguments :
  // - index : the index of the request in the transaction tab
  /////////////////////////////////////////////////////////////////////
  void erase(const size_t index)
  {
    assert( (index < size_tab) && "Invalid Transaction Tab Entry");
    tab[index].valid = false;
  }
}; // end class TransactionTabIO

#endif

// Local Variables:
// tab-width: 2
// c-basic-offset: 2
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=2:tabstop=2:softtabstop=2

