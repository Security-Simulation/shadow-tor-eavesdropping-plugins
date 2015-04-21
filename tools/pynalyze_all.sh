#!/bin/bash

for a in $(ls -d tracelogs*) ; do 
	pushd $a 
	for i in $(ls) ; do 
		pushd $i
		for j in $(ls) ; do 
			pushd $j 
			/home/simul/shadow/src/plugins/tools/pynalyzer.py --tracefile="$(pwd)/analyzer_trace.log" --tracedir="$(pwd)" > pynalyzer.log
			popd 
		done 
		popd
	done 
	popd
done
