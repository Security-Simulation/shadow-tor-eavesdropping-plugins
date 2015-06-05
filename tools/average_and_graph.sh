#!/bin/bash

DENSITY=("slow" "average" "fast")
# x: TRACED_SERVERS y: MATCHED_PORTION
get_tserver_mportion() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 22,22n | awk '{printf("%s %s\n", $22, $12)}'
}
generate_plot() {
	script="plot "
	val=0
	color=("dark-green" "blue" "red")
	for d in ${DENSITY[@]}; do 

	#script=`echo "$script" "\"${d}_analysis.log\" using 1:3:4 with filledcurves lt ${color[$val]} title \"${d} confidence interval\" , \"\" using 1:2 with lp ps 0.5 lt ${color[$val]} lw 1.5 title \"${d}\","`
	script=`echo "$script" "\"${d}_analysis.log\" using 1:3:4 with filledcurves linecolor rgb \"${color[$val]}\" title \"${d} confidence interval\" , \"\" using 1:2 with lp ps 0.5 linecolor rgb \"${color[$val]}\" lw 1.5 title \"${d}\","`
	val=$[$val + 1];
	done
	echo $script
}


# XXX standard deviation confidence interval
for dens in ${DENSITY[@]}; do
	density_tserver_mportion_raw=`get_tserver_mportion $dens $@ `
	density_nservers=(`echo "$density_tserver_mportion_raw" | cut -f 1 -d " "|uniq`)

	echo "" > ${dens}_analysis.log

	for k in ${density_nservers[@]}; do
		process_step=`echo "$density_tserver_mportion_raw" | egrep "^$k[[:blank:]]"`
		cinterval_low=`echo "$process_step" | sort -k 2,2n | head -1 | cut -f 2 -d " "`
		cinterval_high=`echo "$process_step" | sort -k 2,2n | tail -1 |cut -f 2 -d " "`

		nelements=`echo "$process_step" | wc -l`
		elements=(`echo "$process_step" | cut -f 2 -d " "`)

		sum=`printf "%s\n" "${elements[@]}" | awk '{s+=$1} END {print s}'`

		avg=`echo $sum $nelements | awk '{print $1/$2}'`
		echo "$k $avg $cinterval_low $cinterval_high" >> ${dens}_analysis.log
	done

done

GENERAL_GNUPLOT_SCRIPT="set style fill transparent solid 0.4 noborder"
MATCHED_PORTION_SERVER_PLOT="$(generate_plot)"

echo "$GENERAL_GNUPLOT_SCRIPT;$MATCHED_PORTION_SERVER_PLOT" | gnuplot -p

# x: TRACED_CLIENTS y: MATCHED_PORTION

# x: TRACED_CLIENTS (XXX o ID?) y: MATCHED_PROBABILITY (pmatch)
# x: ID (tre linee) (XXX o densita') y: LOGGED CLIENTS/TRACED_CLIENTS

