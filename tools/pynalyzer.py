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
import sys
import getopt

#Constants
CLIENT = 0
SERVER = 1

#Global variables default values 
THRESHOLD_MIN = 1000
THRESHOLD_MAX = 6000000

DEBUG = False

def printDebug(string):
	if DEBUG:
		print string

def printPretty(connections):
	'''
	Human readable print of connections dictionary
	'''
	for client in connections:
		bestserver = bestCandidate(connections, client)
		print (client + " best server: " + bestserver["id"] 
			+ " avg: " + str(bestserver["avg"]))
		
		for conn in connections[client]:
			print "\t" + str(conn['time'])
			for sv in conn['servers']:
				svData = conn['servers'][sv]
				print "\t\t%d    %.3f    %s" % (svData[1], svData[0], sv)
		
		print ""

def getAllServers(connections, client):
	'''
	Return all the server candidates for a certain client
	'''
	servers = {}
	
	for conn in connections[client]:
		for sv in conn['servers']:
			if sv not in servers:
				servers[sv] = {}
				servers[sv]['sum'] = 0
				servers[sv]['avg'] = 0
				servers[sv]['conns'] = []
			server_data = {}
			server_data['ctime'] = conn['time']
			server_data['stime'] = conn['servers'][sv][1]
			server_data['prob'] = conn['servers'][sv][0]
			servers[sv]['conns'].append(server_data)
			servers[sv]['sum'] += server_data['prob']
	
	for s in servers:
		servers[s]['avg'] = servers[s]['sum'] / len(servers[s]['conns'])
		printDebug("client: " + client + " server: " + s 
			+ " avg: " + str(servers[s]['avg']))

	return servers

def bestCandidate(connections, client):
	#TODO this can be directly done in getAllServers adding a 'best' key
	# to the servers dictionary
	'''
	Get the best server candidate for a certain server
	'''
	best = {'avg' : -1}
	first = True	
	servers = getAllServers(connections, client)
	for s in servers:
		tmp_avg = servers[s]['avg']
		if tmp_avg > best['avg']:
			best['id'] = s
			best['avg'] = tmp_avg
	return best

def calcProb(time1, time2):
	'''
	Calculate the probability that a client connection was related or not
	to a certain server connection. The time2 parameter should be the server
	connection timestamp and the time1 the client connection timestamp.
	The probability is calculated in relation to the fixed THRESHOLD.
	'''
	timeDistance = time2 - time1
	
	if time1 > time2 or timeDistance <= THRESHOLD_MIN:
		#Skip this server, it may probably be related to some other client
		return False 
	
	prob = 0.0

	if timeDistance <= THRESHOLD_MAX:		
		prob = 1.0 - (float(timeDistance - THRESHOLD_MIN) / float(THRESHOLD_MAX))
	
	return prob 

def connectionParams(line):
	'''
	Retrieve the params parsing a line
	'''
	params = line.split(';')
	
	if params[0] == 'c':
		side = CLIENT
	else:
		side = SERVER
	
	hostname = params[1]	
	timestamp = int(params[2])

	return {'side': side, 'hostname' : hostname, 'time' : timestamp}

def analyzeForward(startConn, startIdx, data):
	'''
	Starting from a certain client connection, scan the data forward in time.
	At each server connection line, calculate and store the probability of
	how much the server connection is related to the start client connection.
	'''
	servers = {}
	for j in data[startIdx:]:
		servConn = connectionParams(j)
		if servConn['side'] == SERVER:
			prob = calcProb(startConn['time'], servConn['time'])
			if prob:
				if prob <= 0.0:
					break 
		
				if not servConn['hostname'] in servers:
					servers[servConn['hostname']] = [prob, servConn['time']]

	return servers

def analyze(traceFilePath):
	'''
	Parse and analyze the traced data. 
	Each line identifies an autosys connection and has the following format:
	serverside/clientside(c/s);hostname;timestamp(sss..ssmmmuuu)
	For each client side connection, check and store the server 
	candidates for that connection, scanning the connections 
	list moving forward in time (analyzeForward).
	'''
	connections = {} 
	traceFile = open(traceFilePath)
	data = traceFile.readlines()
	traceFile.close()
	for i in data:
		startConn = connectionParams(i)			
		if startConn['side'] == CLIENT:	
			if not startConn['hostname'] in connections:
				connections[startConn['hostname']] = []
			conn = {}
			conn['time'] = startConn['time']
			conn['servers'] = analyzeForward(startConn, 
				data.index(i) + 1, data)
			connections[startConn['hostname']].append(conn)
	
	return connections

def exitUsage(appname, usage):
	print appname + usage
	sys.exit(2)

if __name__ == '__main__':
	#TODO: 
	# - sort lines list (if the user asks for it)
	# - print dictionary as the scallion scripts

	exportFilePath = traceFilePath = ""
	dumpData = False

	usage = (" --help --tracefile=<path> --exportfile=<path> " 
				"--threshold=<MIN,MAX> --dump --debug")
	
	try:
		opts, args = getopt.getopt(sys.argv[1:], "ht:e:l:pd", 
			["help","tracefile=","exportfile=", "threshold=", "dump", "debug"])
	except:
		exitUsage(sys.argv[0], usage)
	
	for o, a in opts:
		if o in ("-h", "--help"):
			exitUsage(sys.argv[0], usage)
		if o in ("-t", "--tracefile"):
			traceFilePath = a
		if o in ("-e", "--exportfile"):
			exportFilePath = a
		if o in ("-l", "--threshold"):
			th = a.split(',')
			THRESHOLD_MIN = th[0]
			THRESHOLD_MAX = th[1]
		if o in ("-p", "--dump"):
			dumpData = True
		if o in ("-d", "--debug"):
			DEBUG = True
	
	if not traceFilePath:
		print "The tracefile is needed"
		exitUsage(sys.argv[0], usage)

	connections = analyze(traceFilePath)
	
	if dumpData:
		print connections
	else:
		printPretty(connections)

	getAllServers(connections, 'client95')

	# If the user wants to export the dictionary structure
	if exportFilePath :
		with open(exportFilePath, 'w') as f:
			pickle.dump(connections, f)
		print ("exported to \"" + os.path.abspath(exportFilePath) 
				+ "\" with pickle format.")
