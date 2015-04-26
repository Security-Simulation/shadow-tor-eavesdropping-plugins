DIR="$2";
LAUNCHDIR="$1";
TIMES="$3";
OUTTIMES="$4";
SERVER_TRACER="${5:-1}";
CLIENT_TRACER="${6:-1}";

SPEEDS=('slow' 'average' 'fast')

division() {
	echo "$1" "$2" | awk '{printf("%f", $1/$2)}';
}

date > /home/simul/finish.log;
for i in `seq "$OUTTIMES"`; do

	for j in `seq "$TIMES"`; do
		for DENSITY in ${SPEEDS[@]}; do
			rm $TMPDIR/* > /dev/null;
			pushd $LAUNCHDIR;
			pwd;
			echo "running simulation $i->$j->$DENSITY";
			if [[ $i -eq 1 ]]; then
				if [[ $SERVER_TRACER -ne 1 ]]; then
					SERVER_TRACE=`division $j $TIMES`;
				else
					SERVER_TRACE=1.0;
				fi;

				if [[ $CLIENT_TRACER -ne 1 ]]; then
					CLIENT_TRACE=`division $j $TIMES`;
				else
					CLIENT_TRACE=1.0;
				fi;
				/home/simul/shadow/src/plugins/tools/net-builder.py -a 2 -r 10 -e 2 -c 700 -s 10 -D $DENSITY -C $CLIENT_TRACE -S $SERVER_TRACE > shadow.config.xml;
			else
				cat /home/simul/tracelogs1/$j/$DENSITY/shadow.config.xml > shadow.config.xml;
			fi;

			rm simpletcp/*;

			scallion -w 8 -y;
			mkdir /home/simul/tracelogs$i/$j/$DENSITY -p;
			chmod 770 /home/simul/tracelogs$i -R;
			chown :simul /home/simul/tracelogs$i -R;
		 
			rm /home/simul/tracelogs$i/$j/$DENSITY/* -rf;
			cp shadow.config.xml /home/simul/tracelogs$i/$j/$DENSITY/.;
			cp data/analyzer_trace.log /home/simul/tracelogs$i/$j/$DENSITY/.;
#			cp data/scallion.log /home/simul/tracelogs$i/$j/$DENSITY/.;
			cp simpletcp /home/simul/tracelogs$i/$j/$DENSITY/. -r;
			popd;
		done;
	done;
done;

date >> /home/simul/finish.log;

TRACEDIRNAME=savelogs_$(date "+%F-%H-%M-%S")
mkdir $TRACEDIRNAME
mv tracelogs* $TRACEDIRNAME/.

tar cvvf $TRACEDIRNAME.tar $TRACEDIRNAME

rm -rf $TRACEDIRNAME
xz $TRACEDIRNAME.tar
