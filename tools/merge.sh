#!/bin/bash
		
		merged=""
        for i in $(ls) ; do 
            if pushd $i > "/dev/null"; then
                for j in $(ls) ; do 
                    if pushd $j > "/dev/null" ; then
						merged="$merged"$'\n'"idx:$i,$j;$(tail -n 2 pynalyzer.log)"
                        popd > "/dev/null" 
                    fi
                done
                popd > "/dev/null"
            fi
        done 
		
		echo "$merged"
