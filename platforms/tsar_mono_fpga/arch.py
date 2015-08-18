#!/usr/bin/env python

from math import log, ceil
from mapping import *

###############################################################################
#   file   : arch.py  (for the tsar_monocluster_fpga architecture)
#   date   : March 2015
#   author : Cesar Fuguet
###############################################################################
#  This file contains a mapping generator for the "tsar_mono_fpga"
#  platform.
#  This includes both the hardware architecture (clusters, processors,
#  peripherals, physical space segmentation) and the mapping of all boot
#  and kernel objects (global vsegs).
#
#  It is inspired on the tsar_mono_fpga platform but includes a ROM in the
#  cluster.
#
#  The others hardware parameters are:
#  - fbf_width      : frame_buffer width = frame_buffer heigth
#  - nb_ttys        : number of TTY channels
#  - nb_nics        : number of NIC channels
#  - nb_cmas        : number of CMA channels
#  - irq_per_proc   : number of input IRQs per processor
#  - use_ramdisk    : use a RAMDISK when True
#  - peri_increment : address increment for replicated peripherals
###############################################################################

########################
def arch( x_size    = 1,
          y_size    = 1,
          nb_procs  = 2,
          nb_ttys   = 1,
          fbf_width = 480,
          ioc_type  = 'BDV' ):

    ### define architecture constants

    x_io            = 0
    y_io            = 0
    x_width         = 4
    y_width         = 4
    p_width         = 2
    paddr_width     = 40
    irq_per_proc    = 4
    use_ramdisk     = False
    peri_increment  = 0x10000     # distributed peripherals vbase increment
    reset_address   = 0xFF000000  # wired preloader pbase address
    use_backup_peri = True

    ### parameters checking

    assert( nb_procs <= (1 << p_width) )
    assert( (x_size == 1) and (y_size == 1) );

    ### define type and name

    platform_type  = 'tsar_fpga'
    platform_name  = '%s_%d' % (platform_type, nb_procs )

    ### define physical segments replicated in all clusters
    ### the base address is extended by the cluster_xy (8 bits)

    ram_base = 0x00000000
    ram_size = 0x8000000                   # 128 Mbytes

    xcu_base = 0xF0000000
    xcu_size = 0x1000                      # 4 Kbytes

    mmc_base = 0xF1000000
    mmc_size = 0x1000                      # 4 Kbytes

    rom_base = 0xFF000000
    rom_size = 0x10000                     # 64 Kbytes

    bdv_base = 0xF2000000
    bdv_size = 0x1000                     # 4kbytes

    tty_base = 0xF4000000
    tty_size = 0x4000                     # 16 Kbytes

    fbf_base = 0xF3000000
    fbf_size = fbf_width * fbf_width      # fbf_width * fbf_width bytes

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
                       use_ramdisk    = use_ramdisk,
                       x_io           = x_io,
                       y_io           = y_io,
                       peri_increment = peri_increment,
                       reset_address  = reset_address,
                       ram_base       = ram_base,
                       ram_size       = ram_size )

    ###########################
    ### Hardware Description
    ###########################

    ### components replicated in all clusters but the upper row
    ram = mapping.addRam( 'RAM', base = ram_base, size = ram_size )

    xcu = mapping.addPeriph( 'XCU', base = xcu_base, size = xcu_size,
                             ptype = 'XCU', channels = nb_procs * irq_per_proc,
                             arg0 = 16, arg1 = 16, arg2 = 16 )

    mmc = mapping.addPeriph( 'MMC', base = mmc_base, size = mmc_size,
                             ptype = 'MMC' )

    mapping.addIrq( xcu, index = 8 , src = mmc, isrtype = 'ISR_MMC' )

    for p in xrange ( nb_procs ): mapping.addProc( 0, 0, p )

    bdv = mapping.addPeriph( 'BDV0', base = bdv_base, size = bdv_size,
                             ptype = 'IOC', subtype = 'BDV' )

    mapping.addIrq( xcu, index = 9 , src = bdv, isrtype = 'ISR_BDV' )

    tty = mapping.addPeriph( 'TTY0', base = tty_base, size = tty_size,
                             ptype = 'TTY', channels = nb_ttys )

    mapping.addIrq( xcu, index = 10, src = tty, isrtype = 'ISR_TTY_RX' )

    rom = mapping.addPeriph( 'ROM', base = rom_base, size = rom_size,
                             ptype = 'ROM' )

    fbf = mapping.addPeriph( 'FBF', base = fbf_base, size = fbf_size,
                             ptype = 'FBF', arg0 = fbf_width , arg1 = fbf_width)

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
    if use_ramdisk:

        mapping.addGlobal( 'seg_ramdisk', ramdisk_vbase, ramdisk_size,
                           'C_W_', vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM',
                           identity = True, local = True, big = True )

    ### global vsegs kernel_code, kernel_init : local / big page
    ### replicated in all clusters containing processors
    ### same content => same name / same vbase
    mapping.addGlobal( 'seg_kernel_code',
                       kernel_code_vbase, kernel_code_size,
                       'CXW_', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM',
                       binpath = 'build/kernel/kernel.elf',
                       local = True, big = True )

    mapping.addGlobal( 'seg_kernel_init',
                       kernel_init_vbase, kernel_init_size,
                       'CXW_', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM',
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
    mapping.addGlobal( 'seg_kernel_ptab',
                       kernel_ptab_vbase, kernel_ptab_size,
                       'CXW_', vtype = 'PTAB', x = 0, y = 0, pseg = 'RAM',
                       local = False, big = True )

    ### global vsegs kernel_heap_x_y : non local / big pages
    ### distributed in all clusters containing processors
    ### different content => name & vbase indexed by (x,y)
    mapping.addGlobal( 'seg_kernel_heap',
                       kernel_heap_vbase, kernel_heap_size,
                       'C_W_', vtype = 'HEAP', x = 0 , y = 0 , pseg = 'RAM',
                       local = False, big = True )

    ### global vsegs for external peripherals: non local / big page
    ### only mapped in cluster_io
    mapping.addGlobal( 'seg_bdv', bdv_base, bdv_size,
                       '__W_', vtype = 'PERI', x = 0, y = 0, pseg = 'BDV',
                       local = False, big = True )

    mapping.addGlobal( 'seg_tty', tty_base, tty_size,
                       '__W_', vtype = 'PERI', x = 0, y = 0, pseg = 'TTY',
                       local = False, big = True )

    mapping.addGlobal( 'seg_fbf', fbf_base, fbf_size,
                       '__W_', vtype = 'PERI', x = 0, y = 0, pseg = 'FBF',
                       local = False, big = True )

    ### global vsegs for internal peripherals : non local / small pages
    ### allocated in all clusters containing processors
    ### name and vbase indexed by (x,y)
    mapping.addGlobal( 'seg_xcu',
                       xcu_base, xcu_size,
                       '__W_', vtype = 'PERI' , x = 0 , y = 0 , pseg = 'XCU',
                       local = False, big = False )

    mapping.addGlobal( 'seg_mmc',
                       mmc_base, mmc_size,
                       '__W_', vtype = 'PERI' , x = 0 , y = 0 , pseg = 'MMC',
                       local = False, big = False )

    ### global vsegs kernel_sched : non local / small pages
    ### allocated in all clusters containing processors
    ### different content => name & vbase indexed by (x,y)
    mapping.addGlobal( 'seg_kernel_sched',
                       kernel_sched_vbase, kernel_sched_size,
                       'C_W_', vtype = 'SCHED', x = 0, y = 0, pseg = 'RAM',
                       local = False, big = False )

    return mapping

########################## platform test #############################################

if __name__ == '__main__':
    mapping = arch( x_size = 1,
                    y_size = 1,
                    nb_procs = 2 )

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

