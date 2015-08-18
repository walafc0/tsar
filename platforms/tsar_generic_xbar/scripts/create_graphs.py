#!/usr/bin/python

import subprocess
import os
import re
import sys


apps = [ 'filter', 'lu', 'fft_ga' ]
#apps = [ 'histogram', 'mandel', 'filter', 'fft', 'fft_ga', 'filt_ga', 'pca', 'lu' ]  # radix radix_ga kmeans
#apps = [ 'filt_ga' ]
nb_procs = [ 1, 4, 8, 16, 32, 64 ]
single_protocols = ['dhccp', 'rwt' ]
joint_protocols = ['dhccp', 'rwt' ]
#joint_protocols = []

top_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "..")
scripts_path = os.path.join(top_path, 'scripts')
counter_defs_name = os.path.join(scripts_path, "counter_defs.py")

exec(file(counter_defs_name))

gen_dir = 'generated'
graph_dir = 'graph'
template_dir = 'templates'
data_dir = 'data'

log_stdo_name = '_stdo_'
log_term_name = '_term_'

coherence_tmpl = os.path.join(scripts_path, template_dir, 'coherence_template.gp') # 1 graph per appli
speedup_tmpl   = os.path.join(scripts_path, template_dir, 'speedup_template.gp')
metric_tmpl    = os.path.join(scripts_path, template_dir, 'metric_template.gp') # 1 graph per metric
stacked_tmpl   = os.path.join(scripts_path, template_dir, 'stacked_template.gp')



def create_file(name, content):
   file = open(name, 'w')
   file.write(content)
   file.close()
   
def is_numeric(s):
   try:
      float(s)
      return True
   except ValueError:
      return False

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



# We first fill the m_metric_id table
for metric in all_metrics:
   for tag in all_tags:
      if m_metric_tag[metric] == tag:
         m_metric_id[tag] = metric
         break


# We start by processing all the log files
# Term files are processed for exec time only
# Init files are processed for all metrics
exec_time = {}
metrics_val = {}
for prot in single_protocols:
   metrics_val[prot] = {}
   exec_time[prot] = {}
   for app in apps:
      exec_time[prot][app] = {}
      metrics_val[prot][app] = {}
      for i in nb_procs:
         metrics_val[prot][app][i] = {}
         log_stdo_file = os.path.join(scripts_path, data_dir, app + '_' + prot + log_stdo_name + str(i))
         log_term_file = os.path.join(scripts_path, data_dir, app + '_' + prot + log_term_name + str(i))
   
         # Term
         lines = open(log_term_file, 'r')
         for line in lines:
            tokens = line[:-1].split()
            if len(tokens) > 0 and tokens[0] == "[PARALLEL_COMPUTE]":
               exec_time[prot][app][i] = int(tokens[len(tokens) - 1])
   
         # Init files
         lines = open(log_stdo_file, 'r')
         for line in lines:
            tokens = line[:-1].split()
            if len(tokens) == 0:
               continue
            tag = tokens[0]
            value = tokens[len(tokens) - 1]
            pattern = re.compile('\[0[0-9][0-9]\]')
            if pattern.match(tag):
               metric = m_metric_id[tag]
               if (not metrics_val[prot][app][i].has_key(metric) or tag == "[000]" or tag == "[001]"):
                  # We don't add cycles of all Memcaches (they must be the same for all)
                  metrics_val[prot][app][i][metric] = int(value)
               else:
                  metrics_val[prot][app][i][metric] += int(value)
    
# Completing unset metrics (i.e. they are not present in the data file) with 0
for prot in single_protocols:
   for app in apps:
      for i in nb_procs:
         for metric in all_metrics:
            if metric not in metrics_val[prot][app][i]:
               metrics_val[prot][app][i][metric] = 0

# We make a 2nd pass to fill the derived fields, e.g. nb_total_updates
for prot in single_protocols:
   for app in apps:
      for i in nb_procs:
         x, y = get_x_y(i)
         metrics_val[prot][app][i]['total_read']      = metrics_val[prot][app][i]['local_read']      + metrics_val[prot][app][i]['remote_read']
         metrics_val[prot][app][i]['total_write']     = metrics_val[prot][app][i]['local_write']     + metrics_val[prot][app][i]['remote_write']
         metrics_val[prot][app][i]['total_ll']        = metrics_val[prot][app][i]['local_ll']        + metrics_val[prot][app][i]['remote_ll']
         metrics_val[prot][app][i]['total_sc']        = metrics_val[prot][app][i]['local_sc']        + metrics_val[prot][app][i]['remote_sc']
         metrics_val[prot][app][i]['total_cas']       = metrics_val[prot][app][i]['local_cas']       + metrics_val[prot][app][i]['remote_cas']
         metrics_val[prot][app][i]['total_update']    = metrics_val[prot][app][i]['local_update']    + metrics_val[prot][app][i]['remote_update']
         metrics_val[prot][app][i]['total_m_inv']     = metrics_val[prot][app][i]['local_m_inv']     + metrics_val[prot][app][i]['remote_m_inv']
         metrics_val[prot][app][i]['total_cleanup']   = metrics_val[prot][app][i]['local_cleanup']   + metrics_val[prot][app][i]['remote_cleanup']
         metrics_val[prot][app][i]['total_cleanup_d'] = metrics_val[prot][app][i]['local_cleanup_d'] + metrics_val[prot][app][i]['remote_cleanup_d']
         metrics_val[prot][app][i]['total_getm']      = metrics_val[prot][app][i]['local_getm']      + metrics_val[prot][app][i]['remote_getm']
         metrics_val[prot][app][i]['total_inval_ro']  = metrics_val[prot][app][i]['local_inval_ro']  + metrics_val[prot][app][i]['remote_inval_ro']
         metrics_val[prot][app][i]['total_direct']    = metrics_val[prot][app][i]['total_read']      + metrics_val[prot][app][i]['total_write']
         metrics_val[prot][app][i]['total_ncc_to_cc'] = metrics_val[prot][app][i]['ncc_to_cc_read']  + metrics_val[prot][app][i]['ncc_to_cc_write']
         metrics_val[prot][app][i]['direct_cost']     = metrics_val[prot][app][i]['read_cost']       + metrics_val[prot][app][i]['write_cost']
         metrics_val[prot][app][i]['broadcast_cost']  = metrics_val[prot][app][i]['broadcast'] * (x * y - 1)
         if metrics_val[prot][app][i]['broadcast'] < metrics_val[prot][app][i]['write_broadcast']:
            # test to patch a bug in mem_cache
            metrics_val[prot][app][i]['nonwrite_broadcast'] = 0
            print "*** Error which should not happen anymore: incorrect number of Broadcasts/Write Broadcasts"
         else:
            metrics_val[prot][app][i]['nonwrite_broadcast'] = metrics_val[prot][app][i]['broadcast'] - metrics_val[prot][app][i]['write_broadcast']
   
         metrics_val[prot][app][i]['total_stacked'] = 0
         for stacked_metric in stacked_metrics:
            metrics_val[prot][app][i]['total_stacked'] += metrics_val[prot][app][i][stacked_metric]

            
print "mkdir -p", os.path.join(scripts_path, gen_dir)
subprocess.call([ 'mkdir', '-p', os.path.join(scripts_path, gen_dir) ])

print "mkdir -p", os.path.join(scripts_path, graph_dir)
subprocess.call([ 'mkdir', '-p', os.path.join(scripts_path, graph_dir) ])

############################################################
### Graph 1 : Coherence traffic Cost per application     ###
############################################################

for prot in single_protocols:
   for app in apps:
      data_coherence_name = os.path.join(scripts_path, gen_dir, prot + '_' + app + '_coherence.dat')
      gp_coherence_name   = os.path.join(scripts_path, gen_dir, prot + '_' + app + '_coherence.gp')
   
      # Creating the data file
      width = 15
      content = ""
      
      for metric in [ '#nb_procs' ] + grouped_metrics:
         content += metric + " "
         nb_spaces = width - len(metric)
         content += nb_spaces * ' '
      content += "\n"
   
      for i in nb_procs:
         content += "%-15d " % i
         for metric in grouped_metrics:
            val = float(metrics_val[prot][app][i][metric]) / exec_time[prot][app][i] * 1000
            content += "%-15f " % val
         content += "\n"
      
      create_file(data_coherence_name, content)
   
      # Creating the gp file
      template_file = open(coherence_tmpl, 'r')
      template = template_file.read()
      
      plot_str = ""
      col = 2
      for metric in grouped_metrics:
         if metric != grouped_metrics[0]:
            plot_str += ", \\\n    "
         plot_str += "\"" + data_coherence_name + "\" using ($1):($" + str(col) + ") lc rgb " + colors[col - 2] + " title \"" + m_metric_name[metric] + "\" with linespoint"
         col += 1
      gp_commands = template % dict(app_name = m_app_name[app], nb_procs = nb_procs[-1] + 1, plot_str = plot_str, svg_name = os.path.join(graph_dir, prot + '_' + app + '_coherence'))
      
      create_file(gp_coherence_name, gp_commands)
      
      # Calling gnuplot
      print "gnuplot", gp_coherence_name
      subprocess.call([ 'gnuplot', gp_coherence_name ])


############################################################
### Graph 2 : Speedup per Application                    ###
############################################################

for prot in single_protocols:
   for app in apps:
   
      data_speedup_name = os.path.join(scripts_path, gen_dir, prot + '_' + app + '_speedup.dat')
      gp_speedup_name   = os.path.join(scripts_path, gen_dir, prot + '_' + app + '_speedup.gp')
      
      # Creating data file
      width = 15
      content = "#nb_procs"
      nb_spaces = width - len(content)
      content += nb_spaces * ' '
      content += "speedup\n"
   
      for i in nb_procs:
         content += "%-15d " % i
         val = exec_time[prot][app][i]
         content += "%-15f\n" % (exec_time[prot][app][1] / float(val))
   
      plot_str = "\"" + data_speedup_name + "\" using ($1):($2) lc rgb \"#654387\" title \"Speedup\" with linespoint"
      
      create_file(data_speedup_name, content)
      
      # Creating the gp file
      template_file = open(speedup_tmpl, 'r')
      template = template_file.read()
      
      gp_commands = template % dict(appli = m_app_name[app], nb_procs = nb_procs[-1] + 1, plot_str = plot_str, svg_name = os.path.join(graph_dir, prot + '_' + app + '_speedup'))
      
      create_file(gp_speedup_name, gp_commands)
      
      # Calling gnuplot
      print "gnuplot", gp_speedup_name
      subprocess.call([ 'gnuplot', gp_speedup_name ])


############################################################
### Graph 3 : All speedups on the same Graph             ###
############################################################

for prot in single_protocols:
   # This graph uses the same template as the graph 2
   data_speedup_name = os.path.join(scripts_path, gen_dir, prot + '_all_speedup.dat')
   gp_speedup_name   = os.path.join(scripts_path, gen_dir, prot + '_all_speedup.gp')
   
   # Creating data file
   width = 15
   content = "#nb_procs"
   nb_spaces = width - len(content)
   content += (nb_spaces + 1) * ' '
   for app in apps:
      content += app + " "
      content += (width - len(app)) * " "
   content += "\n"
   
   for i in nb_procs:
      content += "%-15d " % i
      for app in apps:
         val = exec_time[prot][app][i]
         content += "%-15f " % (exec_time[prot][app][1] / float(val))
      content += "\n"
   
   create_file(data_speedup_name, content)
   
   # Creating gp file
   template_file = open(speedup_tmpl, 'r')
   template = template_file.read()
   
   plot_str = ""
   col = 2
   for app in apps:
      if app != apps[0]:
         plot_str += ", \\\n     "
      plot_str += "\"" + data_speedup_name + "\" using ($1):($" + str(col) + ") lc rgb %s title \"" % (colors[col - 2])  + m_app_name[app] + "\" with linespoint"
      col += 1
   
   gp_commands = template % dict(appli = "All Applications", nb_procs = nb_procs[-1] + 1, plot_str = plot_str, svg_name = os.path.join(graph_dir, prot + '_all_speedup'))
      
   create_file(gp_speedup_name, gp_commands)
      
   # Calling gnuplot
   print "gnuplot", gp_speedup_name
   subprocess.call([ 'gnuplot', gp_speedup_name ])


############################################################
### Graph 4 : Graph per metric                           ###
############################################################

# The following section creates the graphs grouped by measure (e.g. #broadcasts)
# The template file cannot be easily created otherwise it would not be generic
# in many ways. This is why it is mainly created here.
# Graphs are created for metric in the "individual_metrics" list

for prot in single_protocols:
   for metric in individual_metrics:
      data_metric_name = os.path.join(scripts_path, gen_dir, prot + '_' + metric + '.dat')
      gp_metric_name   = os.path.join(scripts_path, gen_dir, prot + '_' + metric + '.gp')
   
      # Creating the gp file
      # Setting xtics, i.e. number of procs for each application
      xtics_str = "("
      first = True
      xpos = 1
      app_labels = ""
      for num_appli in range(0, len(apps)):
         for i in nb_procs:
            if not first:
               xtics_str += ", "
            first = False
            if i == nb_procs[0]:
               xpos_first = xpos
            xtics_str += "\"%d\" %.1f" % (i, xpos)
            xpos_last = xpos
            xpos += 1.5
         xpos += 0.5
         app_name_xpos = float((xpos_first + xpos_last)) / 2
         app_labels += "set label \"%s\" at first %f,character 1 center font \"Times,12\"\n" % (m_app_name[apps[num_appli]], app_name_xpos)
      xtics_str += ")"
   
      xmax_val = float(xpos - 1)
   
      # Writing the lines of "plot"
      plot_str = ""
      xpos = 0
      first = True
      column = 2
      for i in range(0, len(nb_procs)):
         if not first:
            plot_str += ", \\\n    "
         first = False
         plot_str += "\"%s\" using ($1+%.1f):($%d) lc rgb %s notitle with boxes" % (data_metric_name, xpos, column, colors[i])
         column += 1
         xpos += 1.5
   
      template_file = open(metric_tmpl, 'r')
      template = template_file.read()
   
      gp_commands = template % dict(xtics_str = xtics_str, app_labels = app_labels, ylabel_str = m_metric_name[metric], norm_factor_str = m_norm_factor_name[m_metric_norm[metric]], xmax_val = xmax_val, plot_str = plot_str, svg_name = os.path.join(graph_dir, prot + '_' + metric))
   
      create_file(gp_metric_name, gp_commands)
      
      # Creating the data file
      width = 15
      content = "#x_pos"
      nb_spaces = width - len(content)
      content += nb_spaces * ' '
      for i in nb_procs:
         content += "%-15d" % i
      content += "\n"
   
      x_pos = 1
      for app in apps:
         # Computation of x_pos
         content += "%-15f" % x_pos
         x_pos += len(nb_procs) * 1.5 + 0.5
         for i in nb_procs:
            if m_metric_norm[metric] == "N":
               content += "%-15d" % (metrics_val[prot][app][i][metric])
            elif m_metric_norm[metric] == "P":
               content += "%-15f" % (float(metrics_val[prot][app][i][metric]) / i)
            elif m_metric_norm[metric] == "C":
               content += "%-15f" % (float(metrics_val[prot][app][i][metric]) / exec_time[prot][app][i] * 1000)
            elif m_metric_norm[metric] == "W":
               content += "%-15f" % (float(metrics_val[prot][app][i][metric]) / float(metrics_val[prot][app][i]['total_write'])) # Number of writes
            elif m_metric_norm[metric] == "R":
               content += "%-15f" % (float(metrics_val[prot][app][i][metric]) / float(metrics_val[prot][app][i]['total_read'])) # Number of reads
            elif m_metric_norm[metric] == "D":
               content += "%-15f" % (float(metrics_val[prot][app][i][metric]) / float(metrics_val[prot][app][i]['total_direct'])) # Number of req.
            elif is_numeric(m_metric_norm[metric]):
               content += "%-15f" % (float(metrics_val[prot][app][i][metric]) / float(metrics_val[prot][app][int(m_metric_norm[metric])][metric]))
            else:
               assert(False)
   
         app_name = m_app_name[app]
         content += "#" + app_name + "\n"
      
      create_file(data_metric_name, content)
   
      # Calling gnuplot
      print "gnuplot", gp_metric_name
      subprocess.call([ 'gnuplot', gp_metric_name ])


############################################################
### Graph 5 : Stacked histogram with counters            ###
############################################################

# The following section creates a stacked histogram containing
# the metrics in the "stacked_metric" list
# It is normalized per application w.r.t the values on 256 procs

for prot in single_protocols:
   data_stacked_name = os.path.join(scripts_path, gen_dir, prot + '_stacked.dat')
   gp_stacked_name   = os.path.join(scripts_path, gen_dir, prot + '_stacked.gp')
   
   norm_factor_value = nb_procs[-1]
   
   # Creating the gp file
   template_file = open(stacked_tmpl, 'r')
   template = template_file.read()
   
   xtics_str = "("
   first = True
   xpos = 1
   app_labels = ""
   for num_appli in range(0, len(apps)):
      for i in nb_procs:
         if not first:
            xtics_str += ", "
         first = False
         if i == nb_procs[0]:
            xpos_first = xpos
         xtics_str += "\"%d\" %d -1" % (i, xpos)
         xpos_last = xpos
         xpos += 1
      xpos += 1
      app_name_xpos = float((xpos_first + xpos_last)) / 2
      app_labels += "set label \"%s\" at first %f,character 1 center font \"Times,12\"\n" % (m_app_name[apps[num_appli]], app_name_xpos)
   xtics_str += ")"
   
   plot_str = "newhistogram \"\""
   n = 1
   for stacked_metric in stacked_metrics:
      plot_str += ", \\\n    " + "'" + data_stacked_name + "'" + " using " + str(n) + " lc rgb " + colors[n] + " title \"" + m_metric_name[stacked_metric] + "\""
      n += 1
   
   ylabel_str = "Breakdown of Coherence Traffic Normalized w.r.t. \\nthe Values on %d Processors" % norm_factor_value
   content = template % dict(svg_name = os.path.join(graph_dir, prot + '_stacked'), xtics_str = xtics_str, plot_str = plot_str, ylabel_str = ylabel_str, app_labels = app_labels, prot_labels = "")
   
   create_file(gp_stacked_name, content)
   
   # Creating the data file
   # Values are normalized by application, w.r.t. the number of requests for a given number of procs
   content = "#"
   for stacked_metric in stacked_metrics:
      content += stacked_metric
      content += ' ' + ' ' * (15 - len(stacked_metric))
   content += "\n"
   for app in apps:
      if app != apps[0]:
         for i in range(0, len(stacked_metrics)):
            content += "%-15f" % 0.0
         content += "\n"
      for i in nb_procs:
         for stacked_metric in stacked_metrics:
            content += "%-15f" % (float(metrics_val[prot][app][i][stacked_metric]) / metrics_val[prot][app][norm_factor_value]['total_stacked'])
         content += "\n"
   
   create_file(data_stacked_name, content)
   # Calling gnuplot
   print "gnuplot", gp_stacked_name
   subprocess.call([ 'gnuplot', gp_stacked_name ])



#################################################################################
### Graph 6 : Stacked histogram with coherence cost compared to r/w cost      ###
#################################################################################

# The following section creates pairs of stacked histograms, normalized w.r.t. the first one.
# The first one contains the cost of reads and writes, the second contains the cost
# of m_inv, m_up and broadcasts (extrapolated)

for prot in single_protocols:
   data_cost_filename = os.path.join(scripts_path, gen_dir, prot + '_relative_cost.dat')
   gp_cost_filename   = os.path.join(scripts_path, gen_dir, prot + '_relative_cost.gp')
   
   direct_cost_metrics = [ 'read_cost', 'write_cost' ]
   coherence_cost_metrics = ['update_cost', 'm_inv_cost', 'broadcast_cost' ]
   
   # Creating the gp file
   template_file = open(stacked_tmpl, 'r')
   template = template_file.read()
   
   xtics_str = "("
   first = True
   xpos = 1
   app_labels = ""
   for num_appli in range(0, len(apps)):
      first_proc = True
      for i in nb_procs:
         if i > 4:
            if not first:
               xtics_str += ", "
            first = False
            if first_proc:
               first_proc = False
               xpos_first = xpos
            xtics_str += "\"%d\" %f -1" % (i, float(xpos + 0.5))
            xpos_last = xpos
            xpos += 3
      app_name_xpos = float((xpos_first + xpos_last)) / 2
      app_labels += "set label \"%s\" at first %f,character 1 center font \"Times,12\"\n" % (m_app_name[apps[num_appli]], app_name_xpos)
      #xpos += 1
   xtics_str += ")"
   
   plot_str = "newhistogram \"\""
   n = 1
   for cost_metric in direct_cost_metrics + coherence_cost_metrics:
      plot_str += ", \\\n    " + "'" + data_cost_filename + "'" + " using " + str(n) + " lc rgb " + colors[n] + " title \"" + m_metric_name[cost_metric] + "\""
      n += 1
   
   ylabel_str = "Coherence Cost Compared to Direct Requests Cost,\\nNormalized per Application for each Number of Processors"
   content = template % dict(svg_name = os.path.join(graph_dir, prot + '_rel_cost'), xtics_str = xtics_str, plot_str = plot_str, ylabel_str = ylabel_str, app_labels = app_labels, prot_labels = "")
   
   create_file(gp_cost_filename, content)
   
   # Creating the data file
   # Values are normalized by application, w.r.t. the number of requests for a given number of procs
   content = "#"
   for cost_metric in direct_cost_metrics:
      content += cost_metric
      content += ' ' + ' ' * (15 - len(cost_metric))
   for cost_metric in coherence_cost_metrics:
      content += cost_metric
      content += ' ' + ' ' * (15 - len(cost_metric))
   content += "\n"
   for app in apps:
      if app != apps[0]:
         for i in range(0, len(direct_cost_metrics) + len(coherence_cost_metrics)):
            content += "%-15f" % 0.0
         content += "\n"
      for i in nb_procs:
         if i > 4:
            for cost_metric in direct_cost_metrics:
               content += "%-15f" % (float(metrics_val[prot][app][i][cost_metric]) / metrics_val[prot][app][i]['direct_cost'])
            for cost_metric in coherence_cost_metrics:
               content += "%-15f" % 0.0
            content += "\n"
            for cost_metric in direct_cost_metrics:
               content += "%-15f" % 0.0
            for cost_metric in coherence_cost_metrics:
               content += "%-15f" % (float(metrics_val[prot][app][i][cost_metric]) / metrics_val[prot][app][i]['direct_cost'])
            content += "\n"
            if i != nb_procs[-1]:
               for j in range(0, len(direct_cost_metrics) + len(coherence_cost_metrics)):
                  content += "%-15f" % 0.0
               content += "\n"
   
   create_file(data_cost_filename, content)
   # Calling gnuplot
   print "gnuplot", gp_cost_filename
   subprocess.call([ 'gnuplot', gp_cost_filename ])


#################################################################################
### Joint Graphs to several architectures                                     ###
#################################################################################

if len(joint_protocols) == 0:
   sys.exit()

#################################################################################
### Graph 7: Comparison of Speedups (normalized w.r.t. 1 proc on first arch)  ###
#################################################################################


for app in apps:

   data_speedup_name = os.path.join(scripts_path, gen_dir, 'joint_' + app + '_speedup.dat')
   gp_speedup_name   = os.path.join(scripts_path, gen_dir, 'joint_' + app + '_speedup.gp')
   
   # Creating data file
   width = 15
   content = "#nb_procs"
   nb_spaces = width - len(content)
   content += nb_spaces * ' '
   content += "speedup\n"

   for i in nb_procs:
      content += "%-15d " % i
      for prot in joint_protocols:
         val = exec_time[prot][app][i]
         content += "%-15f " % (exec_time[joint_protocols[0]][app][1] / float(val))
      content += "\n"

   create_file(data_speedup_name, content)
   
   # Creating the gp file
   template_file = open(speedup_tmpl, 'r')
   template = template_file.read()
  
   plot_str = ""
   col = 2
   for prot in joint_protocols:
      if prot != joint_protocols[0]:
         plot_str += ", \\\n     "
      plot_str += "\"" + data_speedup_name + "\" using ($1):($" + str(col) + ") lc rgb %s title \"" % (colors[col - 2])  + m_prot_name[prot] + "\" with linespoint"
      col += 1
  
   gp_commands = template % dict(appli = m_app_name[app] + " Normalized w.r.t. " + m_prot_name[joint_protocols[0]] + " on 1 Processor", nb_procs = nb_procs[-1] + 1, plot_str = plot_str, svg_name = os.path.join(graph_dir, 'joint_' + app + '_speedup'))
   
   create_file(gp_speedup_name, gp_commands)
   
   # Calling gnuplot
   print "gnuplot", gp_speedup_name
   subprocess.call([ 'gnuplot', gp_speedup_name ])


#################################################################################
### Graph 8 : Joint Stacked histogram with coherence cost and r/w cost        ###
#################################################################################

# The following section creates pairs of stacked histograms for each arch for each number of proc for each app, normalized by (app x number of procs) (with first arch, R/W cost, first of the 2*num_arch histo). It is close to Graph 6

data_cost_filename = os.path.join(scripts_path, gen_dir, 'joint_relative_cost.dat')
gp_cost_filename   = os.path.join(scripts_path, gen_dir, 'joint_relative_cost.gp')
   
direct_cost_metrics = [ 'read_cost', 'write_cost', 'getm_cost' ]
coherence_cost_metrics = ['update_cost', 'm_inv_cost', 'broadcast_cost', 'inval_ro_cost', 'cleanup_cost', 'cleanup_d_cost' ]
 
# Creating the gp file
template_file = open(stacked_tmpl, 'r')
template = template_file.read()
   
xtics_str = "("
first = True
xpos = 1 # successive x position of the center of the first bar in a application
app_labels = ""
prot_labels = ""
for num_appli in range(0, len(apps)):
   first_proc = True
   for i in nb_procs:
      if i > 4:
         x = 0 # local var for computing position of protocol names
         for prot in joint_protocols:
            prot_labels += "set label \"%s\" at first %f, character 2 center font \"Times,10\"\n" % (m_prot_name[prot], float((xpos - 0.5)) + x) # -0.5 instead of +0.5, don't know why... (bug gnuplot?)
            x += 2

         if not first:
            xtics_str += ", "
         first = False
         if first_proc:
            first_proc = False
            xpos_first = xpos
         xtics_str += "\"%d\" %f -1" % (i, float(xpos - 0.5 + len(joint_protocols)))
         xpos_last = xpos
         xpos += 1 + len(joint_protocols) * 2
   app_name_xpos = float((xpos_first + xpos_last)) / 2
   app_labels += "set label \"%s\" at first %f,character 1 center font \"Times,12\"\n" % (m_app_name[apps[num_appli]], app_name_xpos)
   xpos += 1
xtics_str += ")"

plot_str = "newhistogram \"\""
n = 1
for cost_metric in direct_cost_metrics + coherence_cost_metrics:
   plot_str += ", \\\n    " + "'" + data_cost_filename + "'" + " using " + str(n) + " lc rgb " + colors[n] + " title \"" + m_metric_name[cost_metric] + "\""
   n += 1

ylabel_str = "Coherence Cost and Direct Requests Cost,\\nNormalized per Application for each Number of Processors"
content = template % dict(svg_name = os.path.join(graph_dir, 'joint_rel_cost'), xtics_str = xtics_str, plot_str = plot_str, ylabel_str = ylabel_str, app_labels = app_labels, prot_labels = prot_labels)

create_file(gp_cost_filename, content)

# Creating the data file
# Values are normalized by application, w.r.t. the number of requests for a given number of procs
content = "#"
for cost_metric in direct_cost_metrics:
   content += cost_metric
   content += ' ' + ' ' * (15 - len(cost_metric))
for cost_metric in coherence_cost_metrics:
   content += cost_metric
   content += ' ' + ' ' * (15 - len(cost_metric))
content += "\n"
for app in apps:
   if app != apps[0]:
      for j in range(0, len(direct_cost_metrics) + len(coherence_cost_metrics)):
         content += "%-15f" % 0.0
      content += "\n"
   for i in nb_procs:
      if i > 4:
         for prot in joint_protocols:
            for cost_metric in direct_cost_metrics:
               content += "%-15f" % (float(metrics_val[prot][app][i][cost_metric]) / metrics_val[joint_protocols[0]][app][i]['direct_cost'])
            for cost_metric in coherence_cost_metrics:
               content += "%-15f" % 0.0
            content += "\n"
            for cost_metric in direct_cost_metrics:
               content += "%-15f" % 0.0
            for cost_metric in coherence_cost_metrics:
               content += "%-15f" % (float(metrics_val[prot][app][i][cost_metric]) / metrics_val[joint_protocols[0]][app][i]['direct_cost'])
            content += "\n"
         if i != nb_procs[-1]:
            for j in range(0, len(direct_cost_metrics) + len(coherence_cost_metrics)):
               content += "%-15f" % 0.0
            content += "\n"

create_file(data_cost_filename, content)
# Calling gnuplot
print "gnuplot", gp_cost_filename
subprocess.call([ 'gnuplot', gp_cost_filename ])



#################################################################################
### Graph 9 :         ###
#################################################################################


data_metric_filename = os.path.join(scripts_path, gen_dir, 'single_metric.dat')
gp_metric_filename   = os.path.join(scripts_path, gen_dir, 'single_metric.gp')
   
metric = 'total_write'
 
# Creating the gp file
template_file = open(stacked_tmpl, 'r')
template = template_file.read()
   
xtics_str = "("
first = True
xpos = 0 # successive x position of the center of the first bar in a application
app_labels = ""
prot_labels = ""
for num_appli in range(0, len(apps)):
   first_proc = True
   for i in nb_procs:
      x = 0 # local var for computing position of protocol names
      #for prot in joint_protocols:
         #prot_labels += "set label \"%s\" at first %f, character 2 center font \"Times,10\"\n" % (m_prot_name[prot], float((xpos - 0.5)) + x) # -0.5 instead of +0.5, don't know why... (bug gnuplot?)
         #x += 1

      if not first:
         xtics_str += ", "
      first = False
      if first_proc:
         first_proc = False
         xpos_first = xpos
      xtics_str += "\"%d\" %f -1" % (i, float(xpos - 0.5 + len(joint_protocols)))
      xpos_last = xpos
      xpos += 1 + len(joint_protocols)
   app_name_xpos = float((xpos_first + xpos_last)) / 2
   app_labels += "set label \"%s\" at first %f,character 1 center font \"Times,12\"\n" % (m_app_name[apps[num_appli]], app_name_xpos)
   xpos += 1
xtics_str += ")"

n = 1
plot_str = "newhistogram \"\""
for prot in joint_protocols:
   plot_str += ", \\\n    " + "'" + data_metric_filename + "'" + " using " + str(n) + " lc rgb " + colors[n] + " title \"" + m_metric_name[metric] + " for " + m_prot_name[prot] + "\""
   n += 1

ylabel_str = "%(m)s" % dict(m = m_metric_name[metric])
content = template % dict(svg_name = os.path.join(graph_dir, 'single_metric'), xtics_str = xtics_str, plot_str = plot_str, ylabel_str = ylabel_str, app_labels = app_labels, prot_labels = prot_labels)

create_file(gp_metric_filename, content)

# Creating the data file
content = "#" + metric
content += "\n"
for app in apps:
   if app != apps[0]:
      for prot in joint_protocols:
         for p in joint_protocols:
            content += "%-15f " % 0.0
         content += "\n"
   for i in nb_procs:
      for prot in joint_protocols:
         for p in joint_protocols:
            if p != prot:
               content += "%-15f " % 0
            else:
               content += "%-15f " % (float(metrics_val[prot][app][i][metric]))
         content += "\n"
      if i != nb_procs[-1]:
         for p in joint_protocols:
            content += "%-15f " % 0.0
         content += "\n"

create_file(data_metric_filename, content)
# Calling gnuplot
print "gnuplot", gp_metric_filename
subprocess.call([ 'gnuplot', gp_metric_filename ])




