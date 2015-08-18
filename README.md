# The TSAR hardware architecture

The TSAR shared memory architecture is a scalable, cache coherent,
    general-purpose multicore architecture. It is intended to support commodity
    applications and operating systems running on standard PCs, such as LINUX or
    NetBSD. Therefore, the cache coherence must be entirely guaranteed by the
    hardware. Moreover, the TSAR architecture provides a paginated virtual
    memory and efficient atomic operations for synchronization.

The main technical issue is the scalability, as this architecture is intended to
integrate up to 4096 cores (even if the first prototype will contain only 128
    cores). The second technical issue is the power consumption, and all
technical choices described below are driven by these two goals.

## 1/ Processor core

In order to obtain the best MIPS/MicroWatt ratio, the TSAR processor core is a
simple 32 bits, single instruction issue RISC processor, with no superscalar
features, no out of order execution, no branch prediction, no speculative
execution. In order to avoid the enormous effort to develop a brand new
compiler, TSAR uses an existing processor core: The first TSAR prototype
contains a MIPS32 processor core. This choice is not important : It could be as
well a PPC405, a SPARC V8, or an ARM7 core, as all these processor cores have
similar performances.

## 2/ Memory layout

The physical address space size is a parameter. The maximal value is 1 Tbytes
(40 bits physical address). For scalability reasons, the TSAR physical memory is
logically shared, but physically distributed : The architecture is clusterized ,
          and has a 2D mesh topology. Each cluster contains up to 4 processors,
          a local interconnect and one physical memory bank. The architecture is
          NUMA (Non Uniform Memory Access) : All processors can access all
                                             memory banks, but the access time,
                                             and the power consumption depend on
                                             the distance between the processor
                                             and the memory bank.

Each memory bank is actually implemented as a memory cache : A memory cache is
not a classical level 2 cache. The physical address space is statically split
into fixed size segments, and each memory cache is responsible for one segment,
     defined by the physical address MSB bits.  With this principle, it is
     possible to control the physical placement of the data by controlling the
     address values.

## 3/ Virtual memory support

The TSAR architecture implements a paginated virtual memory. It defines a
generic MMU (Memory Management Unit), physically implemented in the L1 cache
controller. This generic MMU is independent on the processor core, and can be
used with any 32 bits, single instruction issue RISC processor. To be
independent from the processor core, the TLB MISS are handled by an hardwired
FSM, and do not use any specific instructions.

The virtual address is 32 bits, and the physical address has up to 40 bits. It
defines two types of pages (4 Kbytes pages, and 2 Mbytes pages).  The page
tables are mapped in memory and have a classical two level hierarchical
structure. There is of course two separated TLB (Translation Look-aside Buffers)
  for instruction addresses and data addresses.

In order to help the operating system to implement efficient page replacement
policies, each entry in the page table contains three bits that are updated by
the hardware MMU : a dirty bit to indicate modifications, and two separated
access bits for “local access” (processor and memory cache located in the same
    cluster), and “remote access” (processor and memory cache located in
      different clusters).


## 4/ DHCCP cache coherence protocol

The shared memory TSAR architecture implements the DHCCP protocol (Distributed
    Hybrid Cache Coherence Protocol). As it is not possible to monitor all
simultaneous transaction in a distributed network on chip, the DHCCP protocol is
based on the global directory paradigm.

To simplify the scalability problem, the TSAR architecture benefits from the
scalable bandwidth provided by the NoC technology and implements a WRITE-THROUGH
policy between the distributed L1 caches and the distributed memory caches. This
WRITE-THROUGH policy provides a tremendous simplification on the cache coherence
protocol, as the memory is always up to date, and there is no exclusive
ownership state for a modified cache line. In this approach, the memory
controller (actually the memory caches) must register all cache line replicated
in the various L1 caches, and send update or invalidate requests to all L1
caches that have a copy when a shared cache line is written. This choice
increases the number of write transactions, and enforces the importance of a
proper placement of the data on this NUMA architecture.  This is the price to
pay for the scalability.

Finally, the DHCCP protocol is called “hybrid”, as it uses a multicast/update
policy when the number of copies is lower than a given threshold, and
automatically switches to a broadcast/invalidate policy when this number of
copies exceeds this threshold.

## 5/ Interconnection networks

The TSAR architecture requires a hierarchical two levels interconnect : each
cluster must contain a local interconnect, and the communications between
clusters relies on a global interconnect.

As described in the cache coherence section, the DHCCP protocol defines three
classes of transactions that must use three separated interconnection networks :
the D_network, used for the direct read/write transactions; the C_network, used
for coherence transactions; the X_network, used to access the external memory in
case of Miss on the memory cache.

The DSPIN network on chip (developed by the LIP6 laboratory) implements the
D_network and the C_network. It has the requested 2D mesh topology, and provides
the shared memory TSAR architecture a truly scalable bandwidth. It supports the
VCI/OCP standard, and implements a logically “flat” address space. It is well
suited to power consumption management, as it relies on the GALS (Globally
    Asynchronous, Locally Synchronous) approach : Both the voltage & the clock
frequency can be independently adjusted in each cluster. It provides two fully
separated virtual channels for the direct traffic and for the coherence traffic.
It provides the broadcast service requested by the DHCCP protocol.

The X_network between the memory cache and the external memory controller is
implemented by a physically separated network, because it has a very different
N-to-1 tree topology.

## 6/ Atomic operations

Any multi-processor architecture must provide an hardware support for atomic
operations. These “read-then-write” atomic operations are used by the software
for synchronization.

In a distributed architecture using a NoC, these atomic operations must be
implemented in both the memory controller (in our case, the memory caches), and
the L1 cache controller.

Each processor instruction set defines a different set of atomic instruction.
The TSAR architecture implements the LL/SC mechanism, that are natively defined
by the MIPS32 & PPC405 processors, and are directly supported by the VCI/OCP
standard. Other atomic instructions, such as the SWAP, or LDSTUB instructions
defined by the SPARC processor can be emulated using the LL/SC instructions.

With this mechanism, the TSAR architecture allows the software developers to use
cachable spin-locks. 
