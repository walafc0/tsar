
set terminal svg size 500 400 fixed
set output "%(svg_name)s.svg"

set ylabel "Coherence Cost for Application %(app_name)s\nNormalized w.r.t. the Number of Cycles (x 1000)" font "Times,12"
set xlabel "Number of Cores" font "Times,12"

set grid noxtics
set grid nomxtics
set grid nomx2tics
set grid ytics
set grid mytics

#set logscale x 2
#set logscale y 2
set xrange [8:256]
#set yrange [0:30]

set key inside left top Left reverse


plot %(plot_str)s


