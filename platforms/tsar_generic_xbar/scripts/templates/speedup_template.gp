
set terminal svg size 1000 800 fixed
set output "%(svg_name)s.svg"

set ylabel "Speedup for %(appli)s" font "Times,12"
set xlabel "Number of Cores" font "Times,12"

set xrange [1:%(nb_procs)d]
set yrange [1:130]

set grid noxtics
set grid nomxtics
set grid nomx2tics
set grid ytics
set grid mytics

set logscale x 2
set logscale y 2

set key inside left top Left reverse

plot %(plot_str)s


