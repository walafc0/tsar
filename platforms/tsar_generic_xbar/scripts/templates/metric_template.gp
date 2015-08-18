
set terminal svg size 1000 400 fixed
set output "%(svg_name)s.svg"

set xtics nomirror out scale 0,0 rotate by 0 font "Times,10"

set xtics %(xtics_str)s
# Creates something like:
#set xtics ("1" 1, "4" 2, "8" 3, "16" 4, "32" 5, "64" 6, "128" 7, "256" 8, "1" 9.5, "4" 10.5, "8" 11.5, "16" 12.5, "32" 13.5, "64" 14.5, "128" 15.5, "256" 16.5, "1" 18, ...)

set ylabel "%(ylabel_str)s %(norm_factor_str)s" font "Times,12"
set xlabel " "

%(app_labels)s

set xrange [0:%(xmax_val)f]

#set mytics 0.1

set grid noxtics
set grid nomxtics
set grid nomx2tics
set grid ytics
set grid mytics


set key inside left top Left reverse

set boxwidth 1
set style fill solid border -1

plot %(plot_str)s


