#!/usr/bin/python

# Shadow tor eavesdropping analyzer
# 
# Copyright (C) 2014
# Davide Berardi, Matteo Martelli
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
# USA. 

import pickle
import os

if __name__ == '__main__':
	import sys
	
	importFilePath = validTraceFilePath = ""

	if len(sys.argv) > 2 :
		validTraceFilePath = sys.argv[1]
		importFilePath = sys.argv[2]
	else:
		print "Usage: " + sys.argv[0] + " <trace file> <import dict file>"
		sys.exit(1);
	
	with open(importFilePath, 'r') as f:
		new_data = pickle.load(f)
	
	print "imported from " + os.path.abspath(importFilePath)
	print new_data
