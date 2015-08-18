
set terminal svg size 1500 500 fixed
set output "%(svg_name)s.svg"

# border 3 -> bords en bas et a gauche (2 derniers bits à 1)
set border 3 front linetype -1 linewidth 1.000
set boxwidth 1 absolute
set style fill solid 1.00 border -1

# front : devant les autres objets (courbes) ; fc : fillcolor
#set style rectangle front fc lt -3 fillstyle solid 1.00 border -1
set style rectangle front fc lt 2 fillstyle solid 1.00 border -1
set grid noxtics nomxtics ytics mytics noztics nomztics nox2tics nomx2tics noy2tics nomy2tics nocbtics nomcbtics

# Lignes derriere le graphique. 2e type de ligne pour les lignes verticales ?
set grid layerdefault linetype 0 linewidth 1.000, linetype 0 linewidth 1.000

# Legende des traits ou batons
set key outside right Left reverse invert center
#set key inside vertical left Left reverse invert enhanced autotitles columnhead nobox
set style histogram rowstacked title offset character 2, 0.25, 0
set datafile missing '-'
set style data histograms

# border : ne croise pas l'axe ; in : a l'interieur ; scale : taille (major,minor) ; offset : offset du texte
set xtics border in scale 0.0,0.0 nomirror rotate by 0 offset character 0,0 font "Times,10"
set ytics border out scale 1,0.5 nomirror norotate offset character 0,0

set xtics %(xtics_str)s

set xlabel " "
set ylabel "%(ylabel_str)s" font "Times,14" 

%(app_labels)s
%(prot_labels)s

plot %(plot_str)s

