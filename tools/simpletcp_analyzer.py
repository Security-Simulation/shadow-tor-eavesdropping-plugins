#!/usr/bin/env python2

###############################################################################
# Simple tcp parser for the py_analyzer                                       #
#                                                                             #
# Copyright (C) 2014                                                          #
# Davide Berardi, Matteo Martelli                                             #
#                                                                             #
# This program is free software; you can redistribute it and/or               #
# modify it under the terms of the GNU General Public License                 #
# as published by the Free Software Foundation; either version 2              #
# of the License, or any later version.                                       #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               #
# GNU General Public License for more details.                                #
#                                                                             #
# You should have received a copy of the GNU General Public License           #
# along with this program; if not, write to the Free Software                 #
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,  #
# USA.                                                                        #
###############################################################################

import sys
import os
import pickle
import getopt

def update(r, s, out):
    f = open(r + "/" + s);
    x = f.readline();

    while (x != ""):
        (key,val) = x[:-1].split(';');

        try:
            out[key][s].append(val);
        except:
            out[key] = {};
            out[key][s] = [];
            out[key][s].append(val);

        x = f.readline();

    f.close();


def main_parser(path):
    out = {}
    for r,d,files in os.walk(path):
        for f in files:
            if (f[0:6] == "server"):
                update(r, f, out);
    return out;

def exitUsage(a, u):
	print a + " " + u;

if __name__ == '__main__':

	exportFilePath = traceDirPath = ""
	dumpData = False

	usage = (" --help --exportfile=<path> --tracedir=<path> --dump")
	
	try:
		opts, args = getopt.getopt(sys.argv[1:], "ht:e:l:p", 
			["help","tracedir=","exportfile=", "threshold=", "dump"])
	except:
		exitUsage(sys.argv[0], usage)
	
	for o, a in opts:
		if o in ("-h", "--help"):
			exitUsage(sys.argv[0], usage)
		if o in ("-t", "--tracedir"):
			traceDirPath = a
		if o in ("-e", "--exportfile"):
			exportFilePath = a
		if o in ("-p", "--dump"):
			dumpData = True
	
	if not traceDirPath:
		print "The trace directory is needed"
		exitUsage(sys.argv[0], usage)

	dumpout = main_parser(traceDirPath)
	
	if dumpData:
		print dumpout

	# If the user wants to export the dictionary structure
	if exportFilePath :
		with open(exportFilePath, 'w') as f:
			pickle.dump(dumpout, f)
		print ("exported to \"" + os.path.abspath(exportFilePath) 
				+ "\" with pickle format.")
