#!/usr/bin/env python

from math import log, ceil
from mapping import *

#######################################################################################
#   file   : arch.py  (for the tsar_generic_iob architecture)
#   date   : may 2014
#   author : Alain Greiner
#######################################################################################
#  This file contains a mapping generator for the "tsar_generic_iob" platform.
#  This includes both the hardware architecture (clusters, processors, peripherals,
#  physical space segmentation) and the mapping of all kernel objects (global vsegs).
#  This platform includes 6 external peripherals, accessible through two IO_Bridge
# components located in cluster [0,0] and cluster [x_size-1, y_size-1].
# Available peripherals are: TTY, BDV, FBF, ROM, NIC, CMA.
#
#  The "constructor" parameters are:
#  - x_size         : number of clusters in a row
#  - y_size         : number of clusters in a column
#  - nb_procs       : number of processors per cluster
#
#  The "hidden" platform parameters are:
#  - nb_ttys        : number of TTY channels
#  - nb_nics        : number of NIC channels
#  - fbf_width      : frame_buffer width = frame_buffer heigth
#  - x_io           : cluster_io x coordinate
#  - y_io           : cluster_io y coordinate
#  - x_width        : number of bits for x coordinate
#  - y_width        : number of bits for y coordinate
#  - paddr_width    : number of bits for physical address
#  - irq_per_proc   : number of input IRQs per processor
#  - use_ramdisk    : use a ramdisk when True
#  - peri_increment : address increment for replicated peripherals
####################################################################################

########################
def arch( x_size    = 2,
          y_size    = 2,
          nb_procs  = 2 ):

    ### define architecture constants

    nb_ttys        = 1
    nb_nics        = 2
    fbf_width      = 1024
    x_io           = 0
    y_io           = 0
    p_width        = int(ceil(log(nb_procs, 2)))
    x_width        = 4
    y_width        = 4
    paddr_width    = 40
    irq_per_proc   = 4
    use_ramdisk    = False
    peri_increment = 0x10000

    ### parameters checking

    assert( nb_procs <= (1 << p_width) )

    assert( (x_size == 1) or (x_size == 2) or (x_size == 4)
             or (y_size == 8) or (x_size == 16) )

    assert( (y_size == 1) or (y_size == 2) or (y_size == 4)
             or (y_size == 8) or (y_size == 16) )

    assert( nb_ttys == 1 )

    assert( ((x_io == 0) and (y_io == 0)) or
            ((x_io == x_size-1) and (y_io == y_size-1)) )

    platform_name  = 'tsar_iob_%d_%d_%d' % ( x_size, y_size, nb_procs )

    ### define physical segments

    ram_base = 0x0000000000
    ram_size = 0x4000000                   # 64 Mbytes

    xcu_base = 0x00B0000000
    xcu_size = 0x1000                      # 4 Kbytes

    dma_base = 0x00B1000000
    dma_size = 0x1000 * nb_procs           # 4 Kbytes * nb_procs

    mmc_base = 0x00B2000000
    mmc_size = 0x1000                      # 4 Kbytes

    offset_io = ((x_io << y_width) + y_io) << (paddr_width - x_width - y_width)

    bdv_base  = 0x00B3000000 + offset_io
    bdv_size  = 0x1000                     # 4kbytes

    tty_base  = 0x00B4000000 + offset_io
    tty_size  = 0x4000                     # 16 Kbytes

    nic_base  = 0x00B5000000 + offset_io
    nic_size  = 0x80000                    # 512 kbytes

    cma_base  = 0x00B6000000 + offset_io
    cma_size  = 0x1000 * 2 * nb_nics       # 4 kbytes * 2 * nb_nics

    fbf_base  = 0x00B7000000 + offset_io
    fbf_size  = fbf_width * fbf_width      # fbf_width * fbf_width bytes

    pic_base  = 0x00B8000000 + offset_io
    pic_size  = 0x1000                     # 4 Kbytes

    iob_base  = 0x00BE000000 + offset_io
    iob_size  = 0x1000                     # 4kbytes

    rom_base  = 0x00BFC00000 + offset_io
    rom_size  = 0x4000                     # 16 Kbytes

    ### define  bootloader vsegs base addresses

    boot_mapping_vbase   = 0x00000000      # ident
    boot_mapping_size    = 0x00010000      # 64 Kbytes

    boot_code_vbase      = 0x00010000      # ident
    boot_code_size       = 0x00020000      # 128 Kbytes

    boot_data_vbase      = 0x00030000      # ident
    boot_data_size       = 0x00010000      # 64 Kbytes

    boot_buffer_vbase    = 0x00040000      # ident
    boot_buffer_size     = 0x00060000      # 384 Kbytes

    boot_stack_vbase     = 0x000A0000      # ident
    boot_stack_size      = 0x00050000      # 320 Kbytes

    ### define kernel vsegs base addresses

    kernel_code_vbase    = 0x80000000
    kernel_code_size     = 0x00020000      # 128 Kbytes

    kernel_data_vbase    = 0x80020000
    kernel_data_size     = 0x00060000      # 384 Kbytes

    kernel_uncdata_vbase = 0x80080000
    kernel_uncdata_size  = 0x00040000      # 256 Kbytes

    kernel_init_vbase    = 0x800C0000
    kernel_init_size     = 0x00010000      # 64 Kbytes

    kernel_sched_vbase   = 0xF0000000            # distributed in all clusters
    kernel_sched_size    = 0x1000 * nb_procs     # 4 kbytes per processor

    ### create mapping

    mapping = Mapping( name           = platform_name,
                       x_size         = x_size,
                       y_size         = y_size,
                       procs_max      = nb_procs,
                       x_width        = x_width,
                       y_width        = y_width,
                       p_width        = p_width,
                       paddr_width    = paddr_width,
                       coherence      = True,
                       irq_per_proc   = irq_per_proc,
                       use_ramdisk    = use_ramdisk,
                       x_io           = x_io,
                       y_io           = y_io,
                       peri_increment = peri_increment )

    ###  external peripherals (accessible in cluster[0,0] only for this mapping)

    iob = mapping.addPeriph( 'IOB', base = iob_base, size = iob_size, ptype = 'IOB' )

    bdv = mapping.addPeriph( 'BDV', base = bdv_base, size = bdv_size, ptype = 'IOC', subtype = 'BDV' )

    tty = mapping.addPeriph( 'TTY', base = tty_base, size = tty_size, ptype = 'TTY', channels = nb_ttys )

    nic = mapping.addPeriph( 'NIC', base = nic_base, size = nic_size, ptype = 'NIC', channels = nb_nics )

    cma = mapping.addPeriph( 'CMA', base = cma_base, size = cma_size, ptype = 'CMA', channels = 2*nb_nics )

    fbf = mapping.addPeriph( 'FBF', base = fbf_base, size = fbf_size, ptype = 'FBF', arg = fbf_width )

    rom = mapping.addPeriph( 'ROM', base = rom_base, size = rom_size, ptype = 'ROM' )

    pic = mapping.addPeriph( 'PIC', base = pic_base, size = pic_size, ptype = 'PIC', channels = 32 )

    mapping.addIrq( pic, index = 0, isrtype = 'ISR_NIC_RX', channel = 0 )
    mapping.addIrq( pic, index = 1, isrtype = 'ISR_NIC_RX', channel = 1 )

    mapping.addIrq( pic, index = 2, isrtype = 'ISR_NIC_TX', channel = 0 )
    mapping.addIrq( pic, index = 3, isrtype = 'ISR_NIC_TX', channel = 1 )

    mapping.addIrq( pic, index = 4, isrtype = 'ISR_CMA'   , channel = 0 )
    mapping.addIrq( pic, index = 5, isrtype = 'ISR_CMA'   , channel = 1 )
    mapping.addIrq( pic, index = 6, isrtype = 'ISR_CMA'   , channel = 2 )
    mapping.addIrq( pic, index = 7, isrtype = 'ISR_CMA'   , channel = 3 )

    mapping.addIrq( pic, index = 8, isrtype = 'ISR_BDV'   , channel = 0 )

    mapping.addIrq( pic, index = 9, isrtype = 'ISR_TTY_RX', channel = 0 )

    ### hardware components replicated in all clusters

    for x in xrange( x_size ):
        for y in xrange( y_size ):
            cluster_xy = (x << y_width) + y;
            offset     = cluster_xy << (paddr_width - x_width - y_width)

            ram = mapping.addRam( 'RAM', base = ram_base + offset, size = ram_size )

            mmc = mapping.addPeriph( 'MMC', base = mmc_base + offset, size = mmc_size,
                                     ptype = 'MMC' )

            dma = mapping.addPeriph( 'DMA', base = dma_base + offset, size = dma_size,
                                     ptype = 'DMA', channels = nb_procs )

            xcu = mapping.addPeriph( 'XCU', base = xcu_base + offset, size = xcu_size,
                                     ptype = 'XCU', channels = nb_procs * irq_per_proc, arg = 16 )

            # MMC IRQ replicated in all clusters
            mapping.addIrq( xcu, index = 0, isrtype = 'ISR_MMC' )

            # processors
            for p in xrange ( nb_procs ):
                mapping.addProc( x, y, p )

    ### global vsegs for boot_loader / identity mapping

    mapping.addGlobal( 'seg_boot_mapping'  , boot_mapping_vbase  , boot_mapping_size  , 'C_W_',
                       vtype = 'BLOB'  , x = 0, y = 0, pseg = 'RAM', identity = True )

    mapping.addGlobal( 'seg_boot_code'     , boot_code_vbase     , boot_code_size     , 'CXW_',
                       vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM', identity = True )

    mapping.addGlobal( 'seg_boot_data'     , boot_data_vbase     , boot_data_size     , 'C_W_',
                       vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM', identity = True )

    mapping.addGlobal( 'seg_boot_buffer'   , boot_buffer_vbase   , boot_buffer_size   , 'C_W_',
                       vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM', identity = True )

    mapping.addGlobal( 'seg_boot_stack'    , boot_stack_vbase    , boot_stack_size    , 'C_W_',
                       vtype = 'BUFFER', x = 0, y = 0, pseg = 'RAM', identity = True )

    ### global vsegs for kernel

    mapping.addGlobal( 'seg_kernel_code'   , kernel_code_vbase   , kernel_code_size   , 'CXW_',
                       vtype = 'ELF'   , x = 0, y = 0, pseg = 'RAM', binpath = 'build/kernel/kernel.elf' )

    mapping.addGlobal( 'seg_kernel_data'   , kernel_data_vbase   , kernel_data_size   , 'C_W_',
                       vtype = 'ELF'   , x = 0, y = 0, pseg = 'RAM', binpath = 'build/kernel/kernel.elf' )

    mapping.addGlobal( 'seg_kernel_uncdata', kernel_uncdata_vbase, kernel_uncdata_size, '__W_',
                       vtype = 'ELF'   , x = 0, y = 0, pseg = 'RAM', binpath = 'build/kernel/kernel.elf' )

    mapping.addGlobal( 'seg_kernel_init'   , kernel_init_vbase   , kernel_init_size   , 'CXW_',
                       vtype = 'ELF'   , x = 0, y = 0, pseg = 'RAM', binpath = 'build/kernel/kernel.elf' )

    ### global vsegs for external peripherals / identity mapping

    mapping.addGlobal( 'seg_iob', iob_base, iob_size, '__W_',
                       vtype = 'PERI', x = 0, y = 0, pseg = 'IOB', identity = True )

    mapping.addGlobal( 'seg_bdv', bdv_base, bdv_size, '__W_',
                       vtype = 'PERI', x = 0, y = 0, pseg = 'BDV', identity = True )

    mapping.addGlobal( 'seg_tty', tty_base, tty_size, '__W_',
                       vtype = 'PERI', x = 0, y = 0, pseg = 'TTY', identity = True )

    mapping.addGlobal( 'seg_nic', nic_base, nic_size, '__W_',
                       vtype = 'PERI', x = 0, y = 0, pseg = 'NIC', identity = True )

    mapping.addGlobal( 'seg_cma', cma_base, cma_size, '__W_',
                       vtype = 'PERI', x = 0, y = 0, pseg = 'CMA', identity = True )

    mapping.addGlobal( 'seg_fbf', fbf_base, fbf_size, '__W_',
                       vtype = 'PERI', x = 0, y = 0, pseg = 'FBF', identity = True )

    mapping.addGlobal( 'seg_pic', pic_base, pic_size, '__W_',
                       vtype = 'PERI', x = 0, y = 0, pseg = 'PIC', identity = True )

    mapping.addGlobal( 'seg_rom', rom_base, rom_size, 'CXW_',
                       vtype = 'PERI', x = 0, y = 0, pseg = 'ROM', identity = True )

    ### Global vsegs for replicated peripherals, and for schedulers
    ### name is indexed by (x,y), base address is incremented by (cluster_xy * peri_increment)

    for x in xrange( x_size ):
        for y in xrange( y_size ):
            cluster_xy = (x << y_width) + y;
            offset     = cluster_xy * peri_increment

            mapping.addGlobal( 'seg_xcu_%d_%d' %(x,y), xcu_base + offset, xcu_size,
                               '__W_', vtype = 'PERI' , x = x , y = y , pseg = 'XCU' )

            mapping.addGlobal( 'seg_dma_%d_%d' %(x,y), dma_base + offset, dma_size,
                               '__W_', vtype = 'PERI' , x = x , y = y , pseg = 'DMA' )

            mapping.addGlobal( 'seg_mmc_%d_%d' %(x,y), mmc_base + offset, mmc_size,
                               '__W_', vtype = 'PERI' , x = x , y = y , pseg = 'MMC' )

            mapping.addGlobal( 'seg_sched_%d_%d' %(x,y), kernel_sched_vbase + offset, kernel_sched_size,
                               'C_W_', vtype = 'SCHED', x = x , y = y , pseg = 'RAM' )

    ### return mapping ###

    return mapping

################################# platform test #######################################################

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

