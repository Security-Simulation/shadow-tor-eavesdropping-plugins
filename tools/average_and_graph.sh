#!/bin/bash


DENSITY=("slow" "average" "fast")

if [ $1 == "average" ]; then
	DENSITY=("none" "average")
	shift
elif [ $1 == "slow" ]; then
	DENSITY=("slow")
	shift
elif [ $1 == "fast" ]; then
	DENSITY=("none" "none" "fast")
	shift
fi

if [ $1 == "both" -o $1 == "bothpmatch" ]; then
	DENSITY=("all")
fi

# x: TRACED_SERVERS y: MATCHED_PORTION
get_tserver_mportion() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 22,22n | awk '{printf("%s %s\n", $22, $12)}'
}

# x: TRACED_SERVERS y: MATCHED_PROBABILITY (pmatch)
get_tserver_pmatch() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 22,22n | awk '{printf("%s %s\n", $22, $14)}'
}

# x: TRACED_CLIENTS (ID) y: MATCHED_PORTION
get_tclient_mportion() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 2,2n | awk '{printf("%f %s\n", ($2)/10, $12)}'
}

# x: TRACED_PORTION (ID) y: MATCHED_PORTION
get_tportion_mportion_all() {
	cat $@ | grep -v '^$' | sort -k 2,2n | awk '{printf("%f %s\n", ($2)/10.0, $12)}'
}

# x: TRACED_PORTION (ID) y: pmatch
get_tportion_pmatch_all() {
	cat $@ | grep -v '^$' | sort -k 2,2n | awk '{printf("%f %s\n", ($2)/10.0, $14)}'
}

# x: TRACED_CLIENTS (ID) y: MATCHED_PROBABILITY (pmatch)
get_tclient_pmatch() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 2,2n | awk '{printf("%s %s\n", $2/10, $14)}'
}

# x: ID y: LOGGED CLIENTS/TRACED_CLIENTS
get_clients_rate() {
	dens=$1;
	shift
	cat $@ | grep $dens | sort -k 2,2n | awk '{if ($18 > 0){printf("%s %f\n", $2/10.0, $10/$18)}}'
}

generate_plot() {
	script="plot "
	val=0
	if [ $1 == "both" -o $1 == "bothpmatch" ]; then
		color=("brown")
	else
		color=("dark-green" "blue" "red")
	fi;
	colornum=(25600 255 16711680)
	if [ $1 != "clients_rate" ]; then
		for d in ${DENSITY[@]}; do 
			script=`echo "$script" "\"${d}_analysis.log\" using 1:3:4 with filledcurves linecolor rgb \"${color[$val]}\" title \"\", \"\" using 1:2 with lp ps 0.5 linecolor rgb \"${color[$val]}\" lw 1.5 title \"${d}\","`
			val=$[$val + 1];
		done
	else
		for d in ${DENSITY[@]}; do 
		echo "" > histogram_analysis.log
		done
		for d in ${DENSITY[@]}; do 
			sum=0;
			valarray="`cat ${d}_analysis.log|cut -d " " -f 2`"
			nelements=`echo ${valarray[@]}|grep -v "^$" | wc -w`
			sum=`printf "%s\n" "${valarray[@]}" | awk '{s+=$1} END {printf("%f", s)}'`
			avg=`echo $sum $nelements | awk '{print $1/$2}'`

			echo "$d $avg ${colornum[$val]}" >> histogram_analysis.log

			val=$[$val + 1];
		done
		script=`echo "$script" "\"histogram_analysis.log\" using 0:2:3:xticlabels(1) with boxes lc rgb variable title ''"`
			
	fi;
	echo $script
}

generate_data() {
	graph_type=$1
	shift
	for dens in ${DENSITY[@]}; do
		if [ $graph_type == 'server' ]; then
			density_selected_data_mportion_raw=`get_tserver_mportion $dens $@ `
		elif [ $graph_type == 'both' ]; then
			density_selected_data_mportion_raw=`get_tportion_mportion_all $@ `
		elif [ $graph_type == 'bothpmatch' ]; then
			density_selected_data_mportion_raw=`get_tportion_pmatch_all $@ `
		elif [ $graph_type == 'client' ]; then
			density_selected_data_mportion_raw=`get_tclient_mportion $dens $@ `
		elif [ $graph_type == 'cpmatch' ]; then
			density_selected_data_mportion_raw=`get_tclient_pmatch $dens $@ `
		elif [ $graph_type == 'spmatch' ]; then
			density_selected_data_mportion_raw=`get_tserver_pmatch $dens $@ `
		elif [ $graph_type == 'clients_rate' ]; then
			density_selected_data_mportion_raw=`get_clients_rate $dens $@ `
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

if [ $# -lt 1 ]; then
	echo "usage: $0 <server | client | cpmatch | spmatch | clients_rate> [files]"
	exit 1
fi

generate_data $@

GENERAL_GNUPLOT_SCRIPT="set key vertical bottom right;set style fill transparent solid 0.4 noborder; set yrange [0 : 1]"
if [ $1 == 'server' ]; then
	LABEL_SCRIPT="set xlabel \"Traced Servers\"; set ylabel \"Matched Portion\""
elif [ $1 == 'both' ]; then
	LABEL_SCRIPT="set xlabel \"Traced Portion\"; set ylabel \"Matched Portion\""
elif [ $1 == 'client' ]; then
	LABEL_SCRIPT="set xlabel \"Traced Client Portion\"; set ylabel \"Matched Portion\""
elif [ $1 == 'cpmatch' ]; then
	LABEL_SCRIPT="set xlabel \"Traced Client Portion\"; set ylabel \"Matching accuracy\""
elif [ $1 == 'spmatch' ]; then
	LABEL_SCRIPT="set xlabel \"Traced Servers\"; set ylabel \"Matching accuracy\""
elif [ $1 == 'bothpmatch' ]; then
	LABEL_SCRIPT="set xlabel \"Traced Portion\"; set ylabel \"Matching accuracy\""
elif [ $1 == 'clients_rate' ]; then
	GENERAL_GNUPLOT_SCRIPT="set yrange [0 : 1]; set boxwidth 0.5 relative; set style fill solid 0.5"
	LABEL_SCRIPT="set xlabel \"Density\"; set ylabel \"Traced Clients/Logged Clients\""
fi
PLOT="$(generate_plot $1)"

echo "$GENERAL_GNUPLOT_SCRIPT;$LABEL_SCRIPT;$PLOT" | gnuplot -p
