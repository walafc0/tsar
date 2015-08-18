#!/usr/bin/python

import subprocess
import os
import sys
import shutil

#TODO (?): recopier les fichiers d'entrees dans le script en fonction de l'appli selectionnee
# Par exemple, tk14.O pour LU, img.raw pour ep_filter, etc.


# User parameters
nb_procs = [ 4 ]
#nb_procs = [ 1, 4, 8, 16, 32, 64, 128, 256 ]
rerun_stats = False
use_omp = False
protocol = 'wtidl'

#apps = [ 'histogram', 'mandel', 'filter', 'radix_ga', 'fft_ga', 'kmeans' ]
#apps = [ 'histogram', 'mandel', 'filter', 'radix_ga', 'fft_ga' ]
apps = [ 'radix' ]


# Variables which could be changed but ought not to because they are reflected in the create_graphs.py script
data_dir = 'data'
log_init_name = protocol + '_stdo_'
log_term_name = protocol + '_term_'

# Global Variables

all_apps = [ 'cholesky', 'fft', 'fft_ga', 'filter', 'filt_ga', 'histogram', 'kmeans', 'lu', 'mandel', 'mat_mult', 'pca', 'radix', 'radix_ga', 'showimg', ]
# to come: 'barnes', 'fmm', 'ocean', 'raytrace', 'radiosity', 'waters', 'watern'

all_protocols = [ 'dhccp', 'rwt', 'hmesi', 'wtidl' ]

top_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "..")
config_name = os.path.join(os.path.dirname(os.path.realpath(__file__)), "config.py")

scripts_path       = os.path.join(top_path, 'scripts')
almos_path         = os.path.join(top_path, 'almos')
soclib_conf_name   = os.path.join(top_path, "soclib.conf")
topcell_name       = os.path.join(top_path, "top.cpp")
arch_info_name     = os.path.join(almos_path, "arch-info-gen.info")
arch_info_bib_name = os.path.join(almos_path, 'arch-info.bib')
hdd_img_file_name  = os.path.join(almos_path, "hdd-img.bin")
shrc_file_name     = os.path.join(almos_path, "shrc")
hard_config_name   = os.path.join(almos_path, "hard_config.h")


# Checks
if protocol not in all_protocols:
    help_str = '''
*** Error: variable protocol has an unsupported value
'''
    print help_str
    sys.exit()

for the_app in apps:
    if the_app not in all_apps:
        print "*** Error: application %s is not defined" % (the_app)
        sys.exit()

if not os.path.isfile(config_name):
    help_str = '''
You should create a file named config.py in this directory with the following definitions:
 - apps_dir:      path to almos-tsar-mipsel/apps directory
 - almos_src_dir: path to almos source directory (for kernel and bootloader binaries)
 - hdd_img_name:  path to the hdd image to use (will be copied but not modified)
 - tsar_dir:      path to tsar repository
Optional definitions (necessary if you want to use alternative protocols):
 - rwt_dir:       path to the RWT repository
 - hmesi_dir:     path to HMESI directory
 - wtidl_dir:     path to the ideal write-through protocol directory
*** Stopping execution
'''
    print help_str
    sys.exit()

# Loading config
exec(file(config_name))

# Check that variables and paths exist
for var in [ 'apps_dir', 'almos_src_dir', 'hdd_img_name', 'tsar_dir' ]:
    if eval(var) == "":
        print "*** Error: variable %s not defined in config file" % (var)
        sys.exit()
    if not os.path.exists(eval(var)):
        print "*** Error: variable %s does not define a valid path" % (var)
        sys.exit()

if protocol == "rwt":
    if rwt_dir == "":
        print "*** Error: variable rwt_dir not defined in config file"
        sys.exit()
    if not os.path.exists(rwt_dir):
        print "*** Error: variable rwt_dir does not define a valid path"
        sys.exit()

if protocol == "hmesi":
    if hmesi_dir == "":
        print "*** Error: variable hmesi_dir not defined in config file"
        sys.exit()
    if not os.path.exists(hmesi_dir):
        print "*** Error: variable hmesi_dir does not define a valid path"
        sys.exit()

if protocol == "wtidl":
    if wtidl_dir == "":
        print "*** Error: variable wtidl_dir not defined in config file"
        sys.exit()
    if not os.path.exists(wtidl_dir):
        print "*** Error: variable wtidl_dir does not define a valid path"
        sys.exit()




#splash_app_dir = {}
#splash_app_dir['barnes'] = 'apps/barnes'
#splash_app_dir['fmm'] = 'apps/fmm'
#splash_app_dir['ocean'] = 'apps/ocean/contiguous_partitions'
#splash_app_dir['raytrace'] = 'apps/raytrace'
#splash_app_dir['radiosity'] = 'apps/radiosity'
#splash_app_dir['volrend'] = 'apps/volrend'
#splash_app_dir['watern'] = 'apps/water-nsquared'
#splash_app_dir['waters'] = 'apps/water-spatial'


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


def gen_soclib_conf():

    if os.path.isfile(soclib_conf_name):
        print "Updating file %s" % (soclib_conf_name)
        # First, remove lines containing "addDescPath"
        f = open(soclib_conf_name, "r")
        lines = f.readlines()
        f.close()

        f = open(soclib_conf_name, "w")

        for line in lines:
            if not ("addDescPath" in line):
                f.write(line)
        f.close()
    else:
        print "Creating file %s" % (soclib_conf_name)
        f = open(soclib_conf_name, "w")
        f.close()

    # Defining common and specific modules 
    common_modules = [
            'lib/generic_llsc_global_table',
            'modules/dspin_router_tsar', 
            'modules/sdmmc',
            'modules/vci_block_device_tsar',
            'modules/vci_ethernet_tsar',
            'modules/vci_io_bridge',
            'modules/vci_iox_network',
            'modules/vci_spi',
            'platforms/tsar_generic_xbar/tsar_xbar_cluster'
    ]

    specific_modules = [
            'communication',
            'lib/generic_cache_tsar',
            'modules/vci_cc_vcache_wrapper',
            'modules/vci_mem_cache',
    ]

    f = open(soclib_conf_name, "a")
    # Adding common modules
    for common_module in common_modules:
        f.write("config.addDescPath(\"%s/%s\")\n" % (tsar_dir, common_module))
    #f.write("\n")

    if protocol == "dhccp":
        arch_dir = tsar_dir
    elif protocol == "rwt":
        arch_dir = rwt_dir
    elif protocol == "hmesi":
        arch_dir = hmesi_dir
    elif protocol == "wtidl":
        arch_dir = wtidl_dir
    else:
        assert(False)

    for specific_module in specific_modules:
        f.write("config.addDescPath(\"%s/%s\")\n" % (arch_dir, specific_module))

    #f.write("\n")
    f.close()


def gen_hard_config(x, y, hard_config):
   header = '''
/* Generated from run_simus.py */

#ifndef _HD_CONFIG_H
#define _HD_CONFIG_H

#define X_SIZE              %(x)d
#define Y_SIZE              %(y)d
#define NB_CLUSTERS         %(nb_clus)d
#define NB_PROCS_MAX        4
#define NB_TASKS_MAX        8

#define NB_TIM_CHANNELS     32
#define NB_DMA_CHANNELS     1

#define NB_TTY_CHANNELS     4
#define NB_IOC_CHANNELS     1
#define NB_NIC_CHANNELS     0
#define NB_CMA_CHANNELS     0

#define USE_XICU            1
#define IOMMU_ACTIVE        0

#define IRQ_PER_PROCESSOR   1
''' % dict(x = x, y = y, nb_clus = x * y)

   if protocol == 'wtidl':
       header += '#define WT_IDL\n'

   header += '#endif //_HD_CONFIG_H\n'

   file = open(hard_config, 'w')
   file.write(header)
   file.close()



def gen_arch_info(x, y, arch_info, arch_info_bib):
   old_path = os.getcwd()
   print "cd", scripts_path
   os.chdir(scripts_path)
   print "./gen_arch_info_large.sh", str(x), str(y), ">", arch_info
   output = subprocess.Popen([ './gen_arch_info_large.sh', str(x), str(y) ], stdout = subprocess.PIPE).communicate()[0]
   os.chdir(almos_path)
   file = open(arch_info, 'w')
   file.write(output)
   file.close()
   print "./info2bib -i", arch_info, "-o", arch_info_bib
   subprocess.call([ './info2bib', '-i', arch_info, '-o', arch_info_bib ])
   print "cd", old_path
   os.chdir(old_path)
   

def gen_sym_links():
   old_path = os.getcwd()
   print "cd", almos_path
   os.chdir(almos_path)

   target = os.path.join(almos_src_dir, 'tools/soclib-bootloader/bootloader-tsar-mipsel.bin')
   link_name = 'bootloader-tsar-mipsel.bin'
   if not os.path.isfile(link_name):
      print "ln -s", target, link_name
      os.symlink(target, link_name)

   target = os.path.join(almos_src_dir, 'kernel/obj.tsar/almix-tsar-mipsel.bin')
   link_name = 'kernel-soclib.bin'
   if not os.path.isfile(link_name):
      print "ln -s", target, link_name
      os.symlink(target, link_name)

   copied_hdd = 'hdd-img.bin'
   print "cp", hdd_img_name, copied_hdd
   shutil.copy(hdd_img_name, copied_hdd)

   os.chdir(old_path)



def compile_app(app_name, nprocs):

   #if app_name in splash2:
   #   app_dir_name = os.path.join(splash2_dir, splash_app_dir[app_name])
   #elif app_name in splash2_ga:
   #   app_dir_name = os.path.join(splash2_ga_dir, splash_ga_app_dir[app_name])
   #else:
   #   app_dir_name = os.path.join(apps_dir, app_name)

     
   app_dir_name = os.path.join(apps_dir, app_name)

   old_path = os.getcwd()
   print "cd", app_dir_name
   os.chdir(app_dir_name)

   # Compilation process is different in splash and other apps
   #if app_name in splash2:
   #   print "make clean"
   #   subprocess.call([ 'make', 'clean' ])

   #   print "rm Makefile"
   #   subprocess.call([ 'rm', 'Makefile' ])

   #   print "ln -s Makefile.soclib Makefile"
   #   subprocess.call([ 'ln', '-s', 'Makefile.soclib', 'Makefile' ])

   #   print "make"
   #   subprocess.call([ 'make' ])

   #else:
   #   print "make clean"
   #   subprocess.call([ 'make', 'clean' ])
 
   #   print "make TARGET=tsar"
   #   subprocess.call([ 'make', 'TARGET=tsar' ])

   print "make clean"
   subprocess.call([ 'make', 'clean' ])
 
   print "make TARGET=tsar"
   retval = subprocess.call([ 'make', 'TARGET=tsar' ])
   if retval != 0:
       sys.exit()
   
   # Creation/Modification du shrc de almos
   if (app_name == "boot_only"):
      shrc = "exec -p 0 /bin/boot_onl\n"
   elif (app_name == "mandel"):
      shrc = "exec -p 0 /bin/mandel -n%(nproc)d\n" % dict(nproc = nprocs)
   elif (app_name == "filter"):
      shrc = "exec -p 0 /bin/filter -l1024 -c1024 -n%(nproc)d /etc/img.raw\n" % dict(nproc = nprocs)
   elif (app_name == "filt_ga"):
      shrc = "exec -p 0 /bin/filt_ga -n%(nproc)d -i /etc/img.raw\n" % dict(nproc = nprocs)
   elif (app_name == "histogram"):
      shrc = "exec -p 0 /bin/histogra -n%(nproc)d /etc/histo.bmp\n" % dict(nproc = nprocs)
   elif (app_name == "kmeans"):
      shrc = "exec -p 0 /bin/kmeans -n %(nproc)d -p %(npoints)d\n" % dict(nproc = nprocs, npoints = 10000) # default npoints = 100000
   elif (app_name == "pca"):
      shrc = "exec -p 0 /bin/pca -n%(nproc)d\n" % dict(nproc = nprocs)
   elif (app_name == "mat_mult"):
      shrc = "exec -p 0 /bin/mat_mult -n%(nproc)d\n" % dict(nproc = nprocs)
   elif (app_name == "showimg"):
      shrc = "exec -p 0 /bin/showimg -i /etc/lena.sgi\n"
   elif (app_name == "barnes"):
      shrc = "exec -p 0 /bin/barnes -n%(nproc)d -b%(nbody)d\n" % dict(nproc = nprocs, nbody = 1024)
   elif (app_name == "fmm"):
      shrc = "exec -p 0 /bin/fmm -n%(nproc)d -p%(nparticles)d\n" % dict(nproc = nprocs, nparticles = 1024)
   elif (app_name == "ocean"):
      shrc = "exec -p 0 /bin/ocean -n%(nproc)d -m%(gridsize)d\n" % dict(nproc = nprocs, gridsize = 66)
   elif (app_name == "radiosity"):
      shrc = "exec -p 0 /bin/radiosit -n %(nproc)d -batch\n" % dict(nproc = nprocs)
   elif (app_name == "raytrace"):
      shrc = "exec -p 0 /bin/raytrace -n%(nproc)d /etc/tea.env\n" % dict(nproc = nprocs)
   elif (app_name == "watern"):
      shrc = "exec -p 0 /bin/watern -n%(nproc)d -m512\n" % dict(nproc = nprocs)
   elif (app_name == "waters"):
      shrc = "exec -p 0 /bin/waters -n%(nproc)d -m512\n" % dict(nproc = nprocs)
   elif (app_name == "cholesky"):
      shrc = "exec -p 0 /bin/cholesky -n%(nproc)d /etc/tk14.O\n" % dict(nproc = nprocs)
   elif (app_name == "fft"):
      shrc = "exec -p 0 /bin/fft -n%(nproc)d -m18\n" % dict(nproc = nprocs)
   elif (app_name == "lu"):
      shrc = "exec -p 0 /bin/lu -n%(nproc)d -m512\n" % dict(nproc = nprocs)
   elif (app_name == "radix"):
      shrc = "exec -p 0 /bin/radix -n%(nproc)d -k1024\n" % dict(nproc = nprocs) # test : 1024 ; simu : 2097152
   elif (app_name == "radix_ga"):
      shrc = "exec -p 0 /bin/radix_ga -n%(nproc)d -k2097152\n" % dict(nproc = nprocs) # p = proc, n = num_keys
   elif (app_name == "fft_ga"):
      shrc = "exec -p 0 /bin/fft_ga -n%(nproc)d -m18\n" % dict(nproc = nprocs)
   else:
      assert(False)
   
   file = open(shrc_file_name, 'w')
   file.write(shrc)
   file.close()

   # Copie du binaire et du shrc dans l'image disque
   print "mcopy -o -i", hdd_img_file_name, shrc_file_name, "::/etc/"
   subprocess.call([ 'mcopy', '-o', '-i', hdd_img_file_name, shrc_file_name, '::/etc/' ])
   print "mcopy -o -i", hdd_img_file_name, app_name, "::/bin/"
   subprocess.call([ 'mcopy', '-o', '-i', hdd_img_file_name, app_name, '::/bin/' ])

   print "cd", old_path
   os.chdir(old_path)
# end of compile_app


print "mkdir -p", os.path.join(scripts_path, data_dir)
subprocess.call([ 'mkdir', '-p', os.path.join(scripts_path, data_dir) ])

gen_sym_links()
gen_soclib_conf()

for i in nb_procs:
   x, y = get_x_y(i)
   nthreads = min(4, x * y)
   gen_hard_config(x, y, hard_config_name)
   gen_arch_info(x, y, arch_info_name, arch_info_bib_name)

   print "cd", top_path
   os.chdir(top_path)
   print "touch", topcell_name
   subprocess.call([ 'touch', topcell_name ])
   print "make"
   retval = subprocess.call([ 'make' ])
   if retval != 0:
       sys.exit()

   for app in apps:
      print "cd", top_path
      os.chdir(top_path)

      compile_app(app, i)

      # Launch simulation
      if use_omp:
         print "./simul.x -THREADS", nthreads, ">", os.path.join(scripts_path, data_dir, app + '_' + log_init_name + str(i))
         output = subprocess.Popen([ './simul.x', '-THREADS', str(nthreads) ], stdout = subprocess.PIPE).communicate()[0]
      else:
         print "./simul.x >", os.path.join(scripts_path, data_dir, app + '_' + log_init_name + str(i))
         output = subprocess.Popen([ './simul.x' ], stdout = subprocess.PIPE).communicate()[0]


      # Write simulation results to data directory
      print "cd", scripts_path
      os.chdir(scripts_path)
      filename = os.path.join(data_dir, app + '_' + log_init_name + str(i))
      file = open(filename, 'w')
      file.write(output)
      file.close()

      filename = os.path.join(scripts_path, data_dir, app + '_' + log_term_name + str(i))
      print "mv", os.path.join(top_path, 'term1'), filename
      subprocess.call([ 'mv', os.path.join(top_path, 'term1'), filename ])

      if rerun_stats:
         start2_found = False
         end_found = False
         for line in open(filename):
            if "[THREAD_COMPUTE_START]" in line:
               start2 = line.split()[-1]
               start2_found = True
            if "[THREAD_COMPUTE_END]" in line:
               end = line.split()[-1]
               end_found = True
         assert(start2_found and end_found)
         
         print "cd", top_path
         os.chdir(top_path)

         # Relauching simulation with reset and dump of counters
         if use_omp:
            print "./simul.x -THREADS", nthreads, "--reset-counters %s --dump-counters %s >" % (start2, end), os.path.join(scripts_path, data_dir, app + '_' + log_init_name + str(i))
            output = subprocess.Popen([ './simul.x', '-THREADS', str(nthreads), '--reset-counters', start2, '--dump-counters', end ], stdout = subprocess.PIPE).communicate()[0]
         else:
            print "./simul.x --reset-counters %s --dump-counters %s >" % (start2, end), os.path.join(scripts_path, data_dir, app + '_' + log_init_name + str(i))
            output = subprocess.Popen([ './simul.x', '--reset-counters', start2, '--dump-counters', end ], stdout = subprocess.PIPE).communicate()[0]
         
         # Write simulation results (counters) to data directory
         print "cd", scripts_path
         os.chdir(scripts_path)
         filename = os.path.join(data_dir, app + '_' + log_init_name + str(i))
         file = open(filename, 'w')
         file.write(output)
         file.close()
 

## End of simulations



