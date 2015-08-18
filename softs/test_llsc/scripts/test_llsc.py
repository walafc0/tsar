#!/usr/bin/env python

import sys
import dsx
from tsarch import TSArch
from dsx.mapper.mapper import Mapper
from dsx import *



if len(sys.argv) < 4 or sys.argv[1] == '' or sys.argv[2] == '':
   print "Usage: ", sys.argv[0], "<nb_clusters_x> <nb_clusters_y> <memspace_size>"
   exit()

cluster_x = int(sys.argv[1])
cluster_y = int(sys.argv[2])
memspace_size = int(sys.argv[3]) * 4

nb_procs = 4
nb_total_procs = nb_procs * cluster_x * cluster_y

hd = TSArch(cluster_x = cluster_x, cluster_y = cluster_y, nb_proc = nb_procs, nb_tty = 4)

test_llsc = TaskModel(
        'test_llsc_main',
        ports = {
            'table':   MemspacePort(),
            'barrier': BarrierPort(),
            'barrier2': BarrierPort(),
        },
        impls = [
                SwTask('test_llsc_main_func',
                           stack_size = 2048,
                           sources = ['test_llsc_main.c', 'functions.c'],
                           headers = ['functions.h'],
                           defines = [])
        ], 
        uses = ['tty']
        )


test_llsc_no_tty = TaskModel(
        'test_llsc_no_tty',
        ports = {
            'table':   MemspacePort(),
            'barrier': BarrierPort(),
            'barrier2': BarrierPort(),
            'id' : ConstPort(),
        },
        impls = [
                SwTask('test_llsc_no_tty_func',
                           stack_size = 2048,
                           sources = ['test_llsc_no_tty.c', 'functions.c'],
                           headers = ['functions.h'],
                           defines = [])
        ],
        )
 

barrier = Barrier('barrier')
barrier2 = Barrier('barrier2')
memspace = Memspace('memspace', memspace_size)

tasks = ()


tasks += Task('task_llsc_main', 'test_llsc_main',
            {
               'table'   : memspace,
               'barrier' : barrier,
               'barrier2' : barrier2,
            },
            defines = {} ),


for i in range(1, nb_total_procs):
   tasks += Task('task_llsc_no_tty_%d' % i, 'test_llsc_no_tty',
            {
               'table'   : memspace,
               'barrier' : barrier,
               'barrier2' : barrier2,
               'id' : i,
            },
            defines = {} ),

tcg = dsx.Tcg('test_llsc', *tasks)

mpr = Mapper(hd, tcg) 

mpr.map('task_llsc_main', cluster = 0, proc = 0, stack = "PSEG_RAM_0")

for i in range(1, nb_total_procs):
   #print "cluster = %d - proc = %d" % (int(i) / 4, i % 4)
   mpr.map('task_llsc_no_tty_%d' % i, cluster = int(i) / 4, proc = int(i) % 4, stack = "PSEG_RAM_%d" % (int(i) / 4))

for const in tcg.nodesOfType('const'):
   mpr.map(const, pseg = 'PSEG_RAM_0')


mpr.map('memspace', pseg = "PSEG_RAM_0")
mpr.map('barrier', pseg = "PSEG_RAM_0")
mpr.map('barrier2', pseg = "PSEG_RAM_0")

mpr.map(tcg, code = 'PSEG_RAM_0', data = 'PSEG_RAM_0', ptab = "PSEG_RAM_0")
mpr.map('system', boot = 'PSEG_RAM_0', kernel = 'PSEG_RAM_0', scheduler = True)

mpr.generate(dsx.Giet(outdir = '.', vaddr_replicated_peri_inc = 0x2000, debug = False))

