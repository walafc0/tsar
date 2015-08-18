#!/usr/bin/env python

from math import log, ceil
from mapping import *

###############################################################################
#   file   : arch.py  (for the tsar_generic_leti architecture)
#   date   : may 2014
#   author : Alain Greiner
###############################################################################
#  This file contains a mapping generator for the "tsar_generic_leti" platform.
#  This includes both the hardware architecture (clusters, processors,
#  peripherals, physical space segmentation) and the mapping of all boot 
#  and kernel objects (global vsegs).
#
#  This platform includes 6 external peripherals controllers located 
#  in cluster[x_size-1][y_size-1]: TTY, IOC, FBF, NIC, CMA, PIC.
#  It does not use the IOB component.
#  It does not use an external ROM, as the preloader code is (pre)loaded
#  at address 0x0, in the physical memory of cluster[0][0].
#  It can use an - optional - RAMDISK located in cluster[0][0].
#  The upper row (y = y_size-1) does not contain processors or memory.
#
#  The "constructor" parameters (defined in Makefile) are:
#  - x_size         : number of clusters in a row
#  - y_size         : number of clusters in a column
#  - nb_procs       : number of processors per cluster
#  - nb_ttys        : number of TTY channels
#  - fbf_width      : frame_buffer width = frame_buffer heigth
#  - ioc_type       : can be 'BDV','HBA','SDC','RDK' 
#
#  The others hardware parameters (defined below) are:
#  - nb_nics        : number of NIC channels
#  - nb_cmas        : number of CMA channels
#  - x_io           : cluster_io x coordinate
#  - y_io           : cluster_io y coordinate
#  - x_width        : number of bits for x coordinate
#  - y_width        : number of bits for y coordinate
#  - paddr_width    : number of bits for physical address
#  - irq_per_proc   : number of input IRQs per processor
#  - use_ramdisk    : use a RAMDISK when True
#  - peri_increment : address increment for replicated vsegs
#
#  Regarding the boot and kernel vsegs mapping :
#  - We use one big physical page (2 Mbytes) for the preloader and the four
#    boot vsegs, all allocated in cluster[0,0].
#  - We use the 16 next big pages in cluster[0][0] to implement the RAMDISK.
#  - We use one big page per cluster for the replicated kernel code vsegs.
#  - We use one big page in cluster[0][0] for the kernel data vseg.
#  - We use one big page per cluster for the distributed kernel heap vsegs.
#  - We use one big page per cluster for the distributed ptab vsegs.
#  - We use small physical pages (4 Kbytes) per cluster for the schedulers.
#  - We use one big page for each external peripheral in IO cluster,
#  - We use one small page per cluster for each internal peripheral.
###############################################################################

########################
def arch( x_size    = 2,
          y_size    = 2,
          nb_procs  = 4,
          nb_ttys   = 1,
          fbf_width = 128,
          ioc_type  = 'HBA' ):

    ### define architecture constants

    nb_nics         = 1
    nb_cmas         = 2
    x_io            = x_size - 1  # LETI constraint
    y_io            = y_size - 1  # LETI constraint
    x_width         = 4
    y_width         = 4
    p_width         = 2           # LETI constraint
    paddr_width     = 40
    irq_per_proc    = 4           # NetBSD constraint
    peri_increment  = 0x10000 
    reset_address   = 0x00000000  # LETI constraint

    ### parameters checking

    assert( nb_procs <= 4 )

    assert( x_size <= (1 << x_width) )

    assert( (y_size > 1) and (y_size <= (1 << y_width)) )

    assert( ioc_type in [ 'BDV' , 'HBA' , 'SDC' , 'RDK' ] )
  
    ### define type and name 

    platform_name  = 'tsar_leti_%d_%d_%d' % ( x_size, y_size, nb_procs )
    platform_name  += '_%d_%d_%s' % ( fbf_width , nb_ttys , ioc_type )

    ### define physical segments replicated in all clusters
    ### the base address is extended by the cluster_xy (8 bits)

    ram_base = 0x00000000
    ram_size = 0x4000000                   # 64 Mbytes

    xcu_base = 0xF0000000
    xcu_size = 0x1000                      # 4 Kbytes

    mmc_base = 0xF1000000
    mmc_size = 0x1000                      # 4 Kbytes

    ### define physical segments for external peripherals
    ## These segments are only defined in cluster_io

    ioc_base  = 0xF2000000
    ioc_size  = 0x1000                     # 4kbytes

    tty_base  = 0xF4000000
    tty_size  = 0x4000                     # 16 Kbytes

    nic_base  = 0xF7000000
    nic_size  = 0x80000                    # 512 kbytes

    cma_base  = 0xF8000000
    cma_size  = 0x1000 * 2 * nb_nics       # 4 kbytes * 2 * nb_nics

    pic_base  = 0xF9000000
    pic_size  = 0x1000                     # 4 Kbytes

    fbf_base  = 0xF3000000
    fbf_size  = fbf_width * fbf_width      # fbf_width * fbf_width bytes


    ### define preloader & bootloader vsegs base addresses and sizes
    ### We want to pack these 5 vsegs in the same big page
    ### => boot cost is one BPP in cluster[0][0]

    preloader_vbase      = reset_address   # ident
    preloader_size       = 0x00010000      # 64 Kbytes

    boot_mapping_vbase   = 0x00010000      # ident
    boot_mapping_size    = 0x00080000      # 512 Kbytes

    boot_code_vbase      = 0x00090000      # ident
    boot_code_size       = 0x00040000      # 256 Kbytes

    boot_data_vbase      = 0x000D0000      # ident
    boot_data_size       = 0x000B0000      # 704 Kbytes

    boot_stack_vbase     = 0x00180000      # ident
    boot_stack_size      = 0x00080000      # 512 Kbytes

    ### define ramdisk vseg / must be identity mapping in cluster[0][0]
    ### occupies 15 BPP after the boot  
    ramdisk_vbase        = 0x00200000
    ramdisk_size         = 0x02000000      # 32 Mbytes

    ### define kernel vsegs base addresses and sizes
    ### code, init, ptab, heap & sched vsegs are replicated in all clusters.
    ### data & uncdata vsegs are only mapped in cluster[0][0].

    kernel_code_vbase    = 0x80000000
    kernel_code_size     = 0x00100000      # 1 Mbytes per cluster

    kernel_init_vbase    = 0x80100000
    kernel_init_size     = 0x00100000      # 1 Mbytes per cluster

    kernel_data_vbase    = 0x90000000
    kernel_data_size     = 0x00200000      # 2 Mbytes in cluster[0][0]

    kernel_ptab_vbase    = 0xE0000000
    kernel_ptab_size     = 0x00200000      # 2 Mbytes per cluster

    kernel_heap_vbase    = 0xD0000000
    kernel_heap_size     = 0x00200000      # 2 Mbytes per cluster

    kernel_sched_vbase   = 0xA0000000
    kernel_sched_size    = 0x00002000 * nb_procs # 8 kbytes per proc per cluster

    #####################
    ### create mapping
    #####################

    mapping = Mapping( name           = platform_name,
                       x_size         = x_size,
                       y_size         = y_size,
                       nprocs         = nb_procs,
                       x_width        = x_width,
                       y_width        = y_width,
                       p_width        = p_width,
                       paddr_width    = paddr_width,
                       coherence      = True,
                       irq_per_proc   = irq_per_proc,
                       use_ramdisk    = (ioc_type == 'RDK'),
                       x_io           = x_io,
                       y_io           = y_io,
                       peri_increment = peri_increment,
                       reset_address  = reset_address,
                       ram_base       = ram_base,
                       ram_size       = ram_size )

    ###########################
    ### Hardware Description
    ###########################

    for x in xrange( x_size ):
        for y in xrange( y_size ):
            cluster_xy = (x << y_width) + y;
            offset     = cluster_xy << (paddr_width - x_width - y_width)
 
            ### components replicated in all clusters but the upper row
            if ( y < (y_size - 1) ):

                ram = mapping.addRam( 'RAM', base = ram_base + offset, 
                                      size = ram_size )

                mmc = mapping.addPeriph( 'MMC', base = mmc_base + offset, 
                                         size = mmc_size, ptype = 'MMC' )

                xcu = mapping.addPeriph( 'XCU', base = xcu_base + offset, 
                                         size = xcu_size, ptype = 'XCU', 
                                         channels = nb_procs * irq_per_proc, 
                                         arg0 = 16, arg1 = 16, arg2 = 16 )

                mapping.addIrq( xcu, index = 8, src = mmc, isrtype = 'ISR_MMC' )

                for p in xrange ( nb_procs ):
                    mapping.addProc( x, y, p )

            ###  external peripherals in cluster_io 
            if ( (x==x_io) and (y==y_io) ):

                if ( ioc_type != 'RDK' ):
                    ioc = mapping.addPeriph( 'IOC', base = ioc_base + offset, size = ioc_size, 
                                             ptype = 'IOC', subtype = ioc_type )

                tty = mapping.addPeriph( 'TTY', base = tty_base + offset, size = tty_size, 
                                         ptype = 'TTY', channels = nb_ttys )

                nic = mapping.addPeriph( 'NIC', base = nic_base + offset, size = nic_size, 
                                         ptype = 'NIC', channels = nb_nics )

                cma = mapping.addPeriph( 'CMA', base = cma_base + offset, size = cma_size, 
                                         ptype = 'CMA', channels = nb_cmas )

                fbf = mapping.addPeriph( 'FBF', base = fbf_base + offset, size = fbf_size, 
                                         ptype = 'FBF', arg0 = fbf_width, arg1 = fbf_width )

                pic = mapping.addPeriph( 'PIC', base = pic_base + offset, size = pic_size, 
                                         ptype = 'PIC', channels = 32 )

                mapping.addIrq( pic, index = 0, src = nic,
                                isrtype = 'ISR_NIC_RX', channel = 0 )
                mapping.addIrq( pic, index = 1, src = nic,
                                isrtype = 'ISR_NIC_RX', channel = 1 )
                mapping.addIrq( pic, index = 2, src = nic,
                                isrtype = 'ISR_NIC_TX', channel = 0 )
                mapping.addIrq( pic, index = 3, src = nic,
                                isrtype = 'ISR_NIC_TX', channel = 1 )
                mapping.addIrq( pic, index = 4, src = cma,
                                isrtype = 'ISR_CMA', channel = 0 )
                mapping.addIrq( pic, index = 5, src = cma,
                                isrtype = 'ISR_CMA', channel = 1 )
                mapping.addIrq( pic, index = 6, src = cma,
                                isrtype = 'ISR_CMA', channel = 2 )
                mapping.addIrq( pic, index = 7, src = cma,
                                isrtype = 'ISR_CMA', channel = 3 )

                if   ( ioc_type == 'BDV' ): isr_type = 'ISR_BDV'
                elif ( ioc_type == 'HBA' ): isr_type = 'ISR_HBA'
                elif ( ioc_type == 'SDC' ): isr_type = 'ISR_SDC'

                if ( ioc_type != 'RDK' ):
                    mapping.addIrq( pic, index = 8, src = ioc,
                                    isrtype = isr_type, channel = 0 )

                mapping.addIrq( pic, index = 16, src = tty,
                                isrtype = 'ISR_TTY_RX', channel = 0 )
                mapping.addIrq( pic, index = 17, src = tty,
                                isrtype = 'ISR_TTY_RX', channel = 1 )
                mapping.addIrq( pic, index = 18, src = tty,
                                isrtype = 'ISR_TTY_RX', channel = 2 )
                mapping.addIrq( pic, index = 19, src = tty,
                                isrtype = 'ISR_TTY_RX', channel = 3 )
                mapping.addIrq( pic, index = 20, src = tty,
                                isrtype = 'ISR_TTY_RX', channel = 4 )
                mapping.addIrq( pic, index = 21, src = tty,
                                isrtype = 'ISR_TTY_RX', channel = 5 )
                mapping.addIrq( pic, index = 22, src = tty,
                                isrtype = 'ISR_TTY_RX', channel = 6 )
                mapping.addIrq( pic, index = 23, src = tty,
                                isrtype = 'ISR_TTY_RX', channel = 7 )

    ###################################
    ### boot & kernel vsegs mapping
    ###################################

    ### global vsegs for preloader & boot_loader are mapped in cluster[0][0]
    ### => same flags CXW_ / identity mapping / non local / big page

    mapping.addGlobal( 'seg_preloader', preloader_vbase, preloader_size,
                       'CXW_', vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM',
                       identity = True, local = False, big = True )

    mapping.addGlobal( 'seg_boot_mapping', boot_mapping_vbase, boot_mapping_size,
                       'CXW_', vtype = 'BLOB'  , x = 0, y = 0, pseg = 'RAM',
                       identity = True, local = False, big = True )

    mapping.addGlobal( 'seg_boot_code', boot_code_vbase, boot_code_size,
                       'CXW_', vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM',
                       identity = True, local = False, big = True )

    mapping.addGlobal( 'seg_boot_data', boot_data_vbase, boot_data_size,
                       'CXW_', vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM',
                       identity = True, local = False, big = True )

    mapping.addGlobal( 'seg_boot_stack', boot_stack_vbase, boot_stack_size,
                       'CXW_', vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM',
                       identity = True, local = False, big = True )

    ### global vseg for RAM-DISK in cluster[0][0]
    ### identity mapping / non local / big pages
    if (ioc_type == 'RDK'):

        mapping.addGlobal( 'seg_ramdisk', ramdisk_vbase, ramdisk_size,
                           'C_W_', vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM',
                           identity = True, local = True, big = True )

    ### global vsegs kernel_code, kernel_init : local / big page
    ### replicated in all clusters containing processors
    ### same content => same name / same vbase
    for x in xrange( x_size ):
        for y in xrange( y_size - 1 ):

            mapping.addGlobal( 'seg_kernel_code', 
                               kernel_code_vbase, kernel_code_size,
                               'CXW_', vtype = 'ELF', x = x, y = y, pseg = 'RAM',
                               binpath = 'build/kernel/kernel.elf',
                               local = True, big = True )

            mapping.addGlobal( 'seg_kernel_init', 
                               kernel_init_vbase, kernel_init_size,
                               'CXW_', vtype = 'ELF', x = x, y = y, pseg = 'RAM',
                               binpath = 'build/kernel/kernel.elf',
                               local = True, big = True )

    ### global vseg kernel_data: non local / big page
    ### Only mapped in cluster[0][0]
    mapping.addGlobal( 'seg_kernel_data', 
                       kernel_data_vbase, kernel_data_size,
                       'C_W_', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM',
                       binpath = 'build/kernel/kernel.elf', 
                       local = False, big = True )

    ### Global vsegs kernel_ptab_x_y: non local / big page
    ### replicated in all clusters containing processors
    ### different content => name & vbase indexed by (x,y)
    for x in xrange( x_size ):
        for y in xrange( y_size - 1 ):
            offset = ((x << y_width) + y) * kernel_ptab_size

            mapping.addGlobal( 'seg_kernel_ptab_%d_%d' %(x,y), 
                               kernel_ptab_vbase + offset, kernel_ptab_size,
                               'CXW_', vtype = 'PTAB', x = x, y = y, pseg = 'RAM',
                               local = False, big = True )

    ### global vsegs kernel_heap_x_y : non local / big pages
    ### distributed in all clusters containing processors
    ### different content => name & vbase indexed by (x,y) 
    for x in xrange( x_size ):
        for y in xrange( y_size - 1 ):
            offset = ((x << y_width) + y) * kernel_heap_size

            mapping.addGlobal( 'seg_kernel_heap_%d_%d' %(x,y), 
                               kernel_heap_vbase + offset , kernel_heap_size,
                               'C_W_', vtype = 'HEAP', x = x , y = y , pseg = 'RAM',
                               local = False, big = True )

    ### global vsegs for external peripherals: non local / big page
    ### only mapped in cluster_io
    mapping.addGlobal( 'seg_ioc', ioc_base, ioc_size,
                       '__W_', vtype = 'PERI', x = x_io, y = y_io, pseg = 'IOC',
                       local = False, big = True )

    mapping.addGlobal( 'seg_tty', tty_base, tty_size, 
                       '__W_', vtype = 'PERI', x = x_io, y = y_io, pseg = 'TTY',
                       local = False, big = True )

    mapping.addGlobal( 'seg_nic', nic_base, nic_size,
                       '__W_', vtype = 'PERI', x = x_io, y = y_io, pseg = 'NIC',
                       local = False, big = True )

    mapping.addGlobal( 'seg_cma', cma_base, cma_size,
                       '__W_', vtype = 'PERI', x = x_io, y = y_io, pseg = 'CMA',
                       local = False, big = True )

    mapping.addGlobal( 'seg_fbf', fbf_base, fbf_size,
                       '__W_', vtype = 'PERI', x = x_io, y = y_io, pseg = 'FBF',
                       local = False, big = True )

    mapping.addGlobal( 'seg_pic', pic_base, pic_size,
                       '__W_', vtype = 'PERI', x = x_io, y = y_io, pseg = 'PIC',
                       local = False, big = True )

    ### global vsegs for internal peripherals : non local / small pages
    ### allocated in all clusters containing processors
    ### name and vbase indexed by (x,y)
    for x in xrange( x_size ):
        for y in xrange( y_size - 1 ):
            offset = ((x << y_width) + y) * peri_increment

            mapping.addGlobal( 'seg_xcu_%d_%d' %(x,y), 
                               xcu_base + offset, xcu_size,
                               '__W_', vtype = 'PERI' , x = x , y = y , pseg = 'XCU',
                               local = False, big = False )

            mapping.addGlobal( 'seg_mmc_%d_%d' %(x,y), 
                               mmc_base + offset, mmc_size,
                               '__W_', vtype = 'PERI' , x = x , y = y , pseg = 'MMC',
                               local = False, big = False )

    ### global vsegs kernel_sched : non local / small pages
    ### allocated in all clusters containing processors
    ### different content => name & vbase indexed by (x,y)
    for x in xrange( x_size ):
        for y in xrange( y_size - 1 ):
            offset = ((x << y_width) + y) * kernel_ptab_size

            mapping.addGlobal( 'seg_kernel_sched_%d_%d' %(x,y), 
                               kernel_sched_vbase + offset , kernel_sched_size,
                               'C_W_', vtype = 'SCHED', x = x, y = y, pseg = 'RAM',
                               local = False, big = False )

    return mapping

########################## platform test #############################################

if __name__ == '__main__':

    mapping = arch( x_size    = 2,
                    y_size    = 2,
                    nb_procs  = 2 )

#   print mapping.netbsd_dts()

    print mapping.xml()

#   print mapping.giet_vsegs()


# Local Variables:
# tab-width: 4;
# c-basic-offset: 4;
# c-file-offsets:((innamespace . 0)(inline-open . 0));
# indent-tabs-mode: nil;
# End:
#
# vim: filetype=python:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

