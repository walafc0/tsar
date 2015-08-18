#!/usr/bin/env python

from dsx.hard.hard import *


def TSArch(cluster_x, cluster_y, nb_proc = 1, nb_tty = 8, wcoproc = False):
 
    nb_cluster = cluster_x * cluster_y

    hd = Hardware(cluster_x, cluster_y , addr_size = 40, nb_proc = nb_proc, ccoherence = True) #nb_proc : proc by cluster


    ######### peripherals ##########
    hd.add(Tty('PSEG_TTY', pbase = 0xB4000000, channel_size = 16, nb_channel = nb_tty))
    hd.add(Fbf('PSEG_FBF', pbase = 0xB2000000, channel_size = 352 * 288 * 2, nb_channel = 1))
    hd.add(Ioc('PSEG_IOC', pbase = 0xB3000000, channel_size = 32, nb_channel = 1))
    hd.add(SimH('PSEG_SIMH', pbase = 0xB7000000, channel_size = 32, nb_channel = 1)) # 6 mapped registers

    hd.add(Xicu('PSEG_XICU', pbase = 0xB0000000, channel_size = 32, nb_channel = nb_proc, replicated = True)) # name suffixed with "_<num_cluster>"
    hd.add(Dma('PSEG_DMA', pbase = 0xB1000000, channel_size = 32, nb_channel = nb_proc, replicated = True))
    
    ############## MEMORY ###########
    for cl in range(nb_cluster):
        hd.add(RAM('PSEG_RAM_%d'%cl, pbase = 0x00000000 + (cl * hd.cluster_span), size = 0x00C00000))

    ############## IRQ ############
    hd.add(Irq(cluster_id = 0, proc_id = 0, icu_irq_id = 31, peri = Ioc, channel_id = 0))
    for j in range(16, 31):
        hd.add(Irq(cluster_id = 0, proc_id = 0, icu_irq_id = j, peri = Tty, channel_id = j - 16))

    for cl in range(nb_cluster):
        for p in xrange(nb_proc):
            hd.add(Irq(cluster_id = cl, proc_id = p, icu_irq_id = p + 8, peri = Dma,  channel_id = p))
            hd.add(Irq(cluster_id = cl, proc_id = p, icu_irq_id = p,     peri = Xicu, channel_id = p)) 


    ############# ROM ############
    hd.add(ROM("PSEG_ROM", pbase = 0xbfc00000, size = 0x00100000)) 

    return hd

