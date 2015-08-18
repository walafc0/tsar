#!/usr/bin/python

import subprocess
import os
import random

data_dir = 'data'
gen_dir = 'generated'
test_gen_tool_dir = 'LLSCTestGenerator'

test_gen_binary = 'generate_test'

log_init_name = 'log_init_'
log_term_name = 'log_term_'

generated_test = 'test_llsc.c'
main_task = 'test_llsc_main.c'
task_no_tty = 'test_llsc_no_tty.c'

res_natif = 'res_natif.txt'

# Parametres des tests
nb_locks = 20
nb_max_incr = 2000
nb_procs = 64


os.chdir(os.path.dirname(__file__))

scripts_path = os.path.abspath(".")
top_path = os.path.abspath("../")

topcell_name = "top.cpp"


random.seed()


def get_x_y(nb_procs):
   x = 1
   y = 1
   to_x = True
   while (x * y * 4 < nb_procs):
      if to_x:
         x = x * 2
      else:
         y = y * 2
      to_x = not to_x
   return x, y


print "make -C", test_gen_tool_dir
subprocess.call([ 'make', '-C', test_gen_tool_dir ])

print "cp", os.path.join(test_gen_tool_dir, test_gen_binary), os.path.join(scripts_path, test_gen_binary)
subprocess.call([ 'cp', os.path.join(test_gen_tool_dir, test_gen_binary), os.path.join(scripts_path, test_gen_binary)])

print "mkdir -p", os.path.join(scripts_path, data_dir)
subprocess.call([ 'mkdir', '-p', os.path.join(scripts_path, data_dir) ])

while True:
   x, y = get_x_y(nb_procs)
   
   b0 = random.randint(0, 1)
   b1 = random.randint(0, 1)

   print test_gen_binary, nb_procs, nb_max_incr, nb_locks, b0, b1, generated_test, main_task, task_no_tty
   tab_size = subprocess.Popen([ os.path.join(scripts_path, test_gen_binary), str(nb_procs), str(nb_max_incr), str(nb_locks), str(b0), str(b1), generated_test, main_task, task_no_tty ], stdout = subprocess.PIPE).communicate()[0]
   
   print "make -f Makefile.nat"
   subprocess.call([ 'make', '-f', 'Makefile.nat' ])

   print "./test_natif >", os.path.join(data_dir, res_natif)
   output = subprocess.Popen([ './test_natif' ], stdout = subprocess.PIPE).communicate()[0]
   file = open(os.path.join(data_dir, res_natif), 'w')
   file.write(output)
   file.close()

   print "./test_llsc.py", str(x), str(y), tab_size
   subprocess.call([ './test_llsc.py', str(x), str(y), tab_size ])
   
   print "cd", top_path
   os.chdir(top_path)
   print "touch", topcell_name
   subprocess.call([ 'touch', topcell_name ])
   print "make"
   subprocess.call([ 'make' ])
   
   # Launch simulation
   print "./simul.x >", os.path.join(scripts_path, data_dir, log_init_name + str(nb_procs))
   output = subprocess.Popen([ './simul.x' ], stdout = subprocess.PIPE).communicate()[0]

   # Write simulation results to data directory
   print "cd", scripts_path
   os.chdir(scripts_path)
   filename = os.path.join(data_dir, log_init_name + str(nb_procs))
   file = open(filename, 'w')
   file.write(output)
   file.close()

   term_filename = os.path.join(scripts_path, data_dir, log_term_name + str(nb_procs))
   print "mv", os.path.join(top_path, 'term1'), term_filename
   subprocess.call([ 'mv', os.path.join(top_path, 'term1'), term_filename ])
   
   # Quit if results obtained by simulation are incorrect
   print "diff", term_filename, os.path.join(data_dir, res_natif)
   output = subprocess.Popen([ 'diff', term_filename, os.path.join(data_dir, res_natif) ], stdout = subprocess.PIPE).communicate()[0]
   if output != "":
      break;


## Enf of simulations









