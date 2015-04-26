#!/bin/bash

for a in $(ls -d tracelogs*) ; do 
	if pushd $a ; then
        for i in $(ls) ; do 
            if pushd $i ; then
                for j in $(ls) ; do 
                    if pushd $j ; then
                        /home/simul/shadow/src/plugins/tools/pynalyzer.py --tracefile="$(pwd)/analyzer_trace.log" --tracedir="$(pwd)" > pynalyzer.log
                        popd 
                    fi
                done 
                popd
            fi
        done 
        popd
    fi
done
