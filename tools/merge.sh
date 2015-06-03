#!/bin/bash

merged=""
for i in $(ls) ; do 
	if test -d $i && pushd $i > "/dev/null"; then
		for j in $(ls) ; do 
			if test -d $j  && pushd $j > "/dev/null" ; then
				traced_info=`cat shadow.config.xml | egrep '<application[[:blank:]]+'|egrep 'plugin="autosys"'`
				straced=`echo "$traced_info" | egrep 'arguments="[^"]*server[^"]*"'|wc -l`
				ctraced=`echo "$traced_info" | egrep -v 'arguments="[^"]*server[^"]*"'|wc -l`

				real_info=`cat shadow.config.xml | egrep '<application[[:blank:]]+'|egrep 'plugin="simpletcp"'`
				realservers=`echo "$real_info" | egrep 'arguments="[[:blank:]]*server[^"]*"'|wc -l`
				realclients=`echo "$real_info" | egrep 'arguments="[[:blank:]]*client[^"]*"'|wc -l`

				merged="$merged""idx:$i;density:$j;$(tail -n 2 pynalyzer.log);real_clients:$realclients;traced_clients:$ctraced;real_servers:$realservers;traced_servers:$straced\n"
				popd > "/dev/null" 
			fi
		done
		popd > "/dev/null"
	fi
done 


echo -e "$merged" | sed 's/:/ /g; s/;/ /g' | sort -k 2n
