#!/bin/bash

merged=""
for i in $(ls) ; do 
	if pushd $i > "/dev/null"; then
		for j in $(ls) ; do 
			if pushd $j > "/dev/null" ; then
				traced_info=`cat shadow.config.xml | egrep '<application[[:blank:]]+'|egrep 'plugin="autosys"'`
				straced=`echo $traced_info | egrep 'arguments="[^"]*server[^"]*"'`
				ctraced=`echo $traced_info | egrep -v 'arguments="[^"]*server[^"]*"'`

				real_info=`cat shadow.config.xml | egrep '<application[[:blank:]]+'|egrep 'plugin="simpletcp"'`
				realservers=`echo $real_info | egrep 'arguments="[[:blank:]]*server[^"]*"'
				realclients=`echo $real_info | egrep 'arguments="[[:blank:]]*client[^"]*"'

				merged="$merged"$'\n'"idx:$i,$j;$(tail -n 2 pynalyzer.log);real_clients:$realclients;real_traced_clients:$ctraced;real_servers:$realservers;real_traced_servers:$straced"
				popd > "/dev/null" 
			fi
		done
		popd > "/dev/null"
	fi
done 

echo "$merged"
