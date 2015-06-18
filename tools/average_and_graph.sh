#!/bin/bash

DENSITY=("slow" "average" "fast")
# x: TRACED_SERVERS y: MATCHED_PORTION
get_tserver_mportion() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 22,22n | awk '{printf("%s %s\n", $22, $12)}'
}

# x: TRACED_CLIENTS (ID) y: MATCHED_PORTION
get_tclient_mportion() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 2,2n | awk '{printf("%s %s\n", $2, $12)}'
}

# x: TRACED_CLIENTS (ID) y: MATCHED_PROBABILITY (pmatch)
get_tclient_pmatch() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 2,2n | awk '{printf("%s %s\n", $2, $14)}'
}

# x: TRACED_SERVERS y: MATCHED_PROBABILITY (pmatch)
get_tserver_pmatch() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 22,22n | awk '{printf("%s %s\n", $22, $14)}'
}

# XXX
# x: ID (tre linee) (XXX o densita') y: LOGGED CLIENTS/TRACED_CLIENTS

generate_plot() {
	script="plot "
	val=0
	color=("dark-green" "blue" "red")
	for d in ${DENSITY[@]}; do 

	script=`echo "$script" "\"${d}_analysis.log\" using 1:3:4 with filledcurves linecolor rgb \"${color[$val]}\" title \"${d} 95% confidence interval\" , \"\" using 1:2 with lp ps 0.5 linecolor rgb \"${color[$val]}\" lw 1.5 title \"${d}\","`
	val=$[$val + 1];
	done
	echo $script
}

generate_data() {
for dens in ${DENSITY[@]}; do
	if [ $1 == 'server' ]; then
		density_selected_data_mportion_raw=`get_tserver_mportion $dens $@ `
	elif [ $1 == 'client' ]; then
		density_selected_data_mportion_raw=`get_tclient_mportion $dens $@ `
	elif [ $1 == 'cpmatch' ]; then
		density_selected_data_mportion_raw=`get_tclient_pmatch $dens $@ `
	elif [ $1 == 'spmatch' ]; then
		density_selected_data_mportion_raw=`get_tserver_pmatch $dens $@ `
	fi

	density_ndata=(`echo "$density_selected_data_mportion_raw" | cut -f 1 -d " "|uniq`)

	echo "" > ${dens}_analysis.log

	for k in ${density_ndata[@]}; do
		process_step=`echo "$density_selected_data_mportion_raw" | egrep "^$k[[:blank:]]"`

		nelements=`echo "$process_step" | wc -l`
		elements=(`echo "$process_step" | cut -f 2 -d " "`)

		sum=`printf "%s\n" "${elements[@]}" | awk '{s+=$1} END {printf("%f", s)}'`

		avg=`echo $sum $nelements | awk '{print $1/$2}'`
		
		if [ $nelements -gt 1 ]; then
			stvariance_sig=`printf "$avg %s $nelements\n" "${elements[@]}" | awk '{s+=(($1 - $2) * ($1 - $2))/($3-1)} END {printf("%f", s)}'`
		else
			stvariance_sig=0
		fi

		stvariance=`echo "$stvariance_sig $nelements" | awk '{printf("%f", $1/$2)}'`

		sigma=`echo "sqrt($stvariance)" | bc`
		cinterval_low=`echo "$avg $sigma" | awk '{print $1 - (1.96 * $2)}'`
		cinterval_high=`echo "$avg $sigma" | awk '{print $1 + (1.96 * $2)}'`

		echo "$k $avg $cinterval_low $cinterval_high" >> ${dens}_analysis.log

		echo "elements:"
		printf "$avg %s $nelements\n" "${elements[@]}"
		echo "avg: $avg"

		echo "sigma: $sigma"
	done
done
}

generate_data $@

GENERAL_GNUPLOT_SCRIPT="set style fill transparent solid 0.4 noborder; set yrange [0 : 1]"
PLOT="$(generate_plot)"

echo "$GENERAL_GNUPLOT_SCRIPT;$PLOT" | gnuplot -p



