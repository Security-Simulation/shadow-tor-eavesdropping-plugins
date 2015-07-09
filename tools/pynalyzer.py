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
import operator
import time
import math
#Constants
CLIENT = 0
SERVER = 1

#Global variables default values 
THRESHOLD_MIN = 100000  #100 ms
THRESHOLD_MAX = 6000000 #6 s

DEBUG = False

def printDebug(string):
	if DEBUG:
		print string

def printPretty(connections, clients_stat, real_stat):
	'''
	Human readable print of connections dictionary
	'''
	print "Threshold: <" + str(THRESHOLD_MIN) + "," + str(THRESHOLD_MAX) + ">"

	print "\nREAL STATS"
	for client in real_stat:
		print(client + "\n\tcandidates")
		for candidate in real_stat[client]['candidates']:
			
			print ("\t\tN: %3d   %s\n" % 
				(candidate['nconns'], candidate['server'])),
		
	print "\nEVALUATED STATS"
	for client in clients_stat:
		print(client + " matched accuracy: %.3f \n\tcandidates" % clients_stat[client]['maccuracy']) 
		for candidate in clients_stat[client]['candidates']:
		    print("\t\tN: %3d AVG: %.3f  SCR: %.2f  GAP: %.3f " % 
                        (candidate['nconns'], candidate['avg'], candidate['score'], candidate['gap'])),
                    if 'dgap' in candidate and 'dscore' in candidate:
	                print("DGAP %.3f  DSCR %.3f %s \n" % 
                            (candidate['dgap'], candidate['dscore'], candidate['server']))
                    else:
                        print("%s \n" % (candidate['server']))

	#for client in connections:
				
		#for conn in connections[client]:
		#	print "\t" + str(conn['time'])
		#	for sv in conn['servers']:
		#		svData = conn['servers'][sv]
		#		print "\t\t%d    %.3f    %s" % (svData[1], svData[0], sv)
		#
		#print ""

def candidateRankings(connections):
	clients_stat = {}
	
	for client in connections:
		servers = getAllServers(connections, client)
		clients_stat[client] = {}
		clients_stat[client]['candidates'] = []
		for candidate in servers:
			if 'dgap' in candidate[1] and 'dscore' in candidate[1]:
                            clients_stat[client]['candidates'].append({
				'server' : candidate[0],
				'nconns' : candidate[1]['nconns'],
				'avg': candidate[1]['avg'],
                                'score': candidate[1]['score'],
                                'gap': candidate[1]['gap'],
                                'dscore': candidate[1]['dscore'],
                                'dgap': candidate[1]['dgap']
			    })
                        else:
                            clients_stat[client]['candidates'].append({
				'server' : candidate[0],
				'nconns' : candidate[1]['nconns'],
				'avg': candidate[1]['avg'],
                                'score': candidate[1]['score'],
                                'gap': candidate[1]['gap']
			    })
                        
	
	return clients_stat

def getAllServers(connections, client):
	'''
	Return all the server candidates for a certain client
	sorterd in decreasing order based on the avg value 
	(the first server will be the best candidate)
	'''
	servers = {}
	servers_infos = {}
	sorted_servers = []
	
        #Scan all the connections of the given clients.
        #Each connection has some related server data, that we are going
        #to summarize in a dictionary that is indexed by servers (servers_infos)
	for cliconn in connections[client]:
                for sv in cliconn['servers']:
			server_data = {}
			server_data['ctime'] = cliconn['time']
			server_data['stime'] = cliconn['servers'][sv][1]
			server_data['prob'] = cliconn['servers'][sv][0]
			
                        #Create the server entry if it is its fisrt visit
                        if sv not in servers_infos:
				servers_infos[sv] = {}
				servers_infos[sv]['sum'] = 0
				servers_infos[sv]['conns'] = []
                                servers_infos[sv]['gap'] = 0
                                servers_infos[sv]['last_prob'] = server_data['prob']
			
                        
                        servers_infos[sv]['conns'].append(server_data)
			servers_infos[sv]['sum'] += server_data['prob']
                        servers_infos[sv]['gap'] += (math.fabs(float(server_data['prob']) - 
                            float(servers_infos[sv]['last_prob'])))
                        servers_infos[sv]['last_prob'] = server_data['prob'] 
	
        # Let's move the servers_infos dictionary in a smaller one filtering 
        # the necessary informations only
	for sv in servers_infos:
		ssum = servers_infos[sv]['sum']
		nconns = len(servers_infos[sv]['conns'])
                gap = servers_infos[sv]['gap']
		servers[sv] = {}
		servers[sv]['nconns'] = nconns
		servers[sv]['avg'] = ssum / nconns
                servers[sv]['gap'] = gap / nconns 
           #     servers[sv]['dgap'] = -1.0
           #     servers[sv]['dscore'] = -1.0
                servers[sv]['score'] = float(ssum) / (gap + 1)
		printDebug("client: " + client + " server: " + sv
			+ " avg: " + str(servers[sv]['avg']) + " gap : " + str(servers[sv]['gap']))
	
	
	sorted_servers = sorted(servers.items(), 
		key = lambda x :x[1]['score'], reverse = True)
	
       
        if len(sorted_servers) > 1 :
            sorted_servers[0][1]['dgap'] = (sorted_servers[1][1]['gap'] - 
                                        sorted_servers[0][1]['gap'])
            sorted_servers[0][1]['dscore'] = (sorted_servers[0][1]['score'] - 
                                        sorted_servers[1][1]['score'])
	return sorted_servers

def calcProb(time1, time2, met_clients):
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
                #TODO can we do something here?
		prob = 1.0 - (float(timeDistance - THRESHOLD_MIN) / float(THRESHOLD_MAX - THRESHOLD_MIN))
               # prob *= 1.0 / float(met_clients) 
               # for c in met_clients :
               #     cTimeDistance = met_clients[c] - time1
               #     cprob = (float(cTimeDistance - THRESHOLD_MIN) / float(THRESHOLD_MAX - THRESHOLD_MIN))
               #     prob = prob - cprob
 
	
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
        met_clients = 1.0
	for j in data[startIdx:]:
		forwConn = connectionParams(j)
                #break the loop if the window is outside the threshold window
                if forwConn['time'] - startConn['time'] > THRESHOLD_MAX:
                    break
                if forwConn['side'] == CLIENT and forwConn['time'] - startConn['time'] > THRESHOLD_MIN:
                    met_clients *= math.exp(1)
                    #met_clients[len(met_clients)] = forwConn['time']
		elif forwConn['side'] == SERVER:
			prob = calcProb(startConn['time'], forwConn['time'], met_clients)
			if prob:
				if prob <= 0.0: #sanity check (just in case shit happens)
					break 
		
				if not forwConn['hostname'] in servers:
					servers[forwConn['hostname']] = [prob, forwConn['time']]

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
        ith = 0
	connections = {} 
	traceFile = open(traceFilePath)
	data = traceFile.readlines()
	traceFile.close()
        
        printDebug("Analisis started")
	for i in data:
                ith += 1
                printDebug('%3.2f' % ((float(ith)/float(len(data)) * 100)))
		startConn = connectionParams(i)			
		if startConn['side'] == CLIENT:	
			if not startConn['hostname'] in connections:
				connections[startConn['hostname']] = []
			conn = {}
			conn['time'] = startConn['time']
			conn['servers'] = analyzeForward(startConn, 
				data.index(i) + 1, data)
			connections[startConn['hostname']].append(conn)
        printDebug("Analisis finished")	
	return connections

def updateRealConnections(r, s, out):
    f = open(r + "/" + s);
    print r + "/" + s
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


def realConnectionsParser(path):
    out = {}
    for r,d,files in os.walk(path):
        for f in files:
            if (f[0:6] == "server"):
                updateRealConnections(r, f, out);
    return out;


def exitUsage(appname, usage):
	print appname + usage
	sys.exit(2)

if __name__ == '__main__':
	traceDirPath = traceFilePath = ""
	connections = realConnection = clients_stat = real_stat = {}
	dumpData = False
	rclients_missing = n_clients = matched_clients = 0
        matched_portion = matched_accuracy = 0.0
	usage = (" --help --tracefile=<path> --tracedir=<path> " 
				"--threshold=<MIN,MAX> --dump --debug")
	
	try:
		opts, args = getopt.getopt(sys.argv[1:], "ht:s:i:l:pd", 
			["help","tracefile=","tracedir=","importfile=", 
				"threshold=", "dump", "debug"])
	except:
		exitUsage(sys.argv[0], usage)
	
	for o, a in opts:
		if o in ("-h", "--help"):
			exitUsage(sys.argv[0], usage)
		if o in ("-t", "--tracefile"):
			traceFilePath = a
		if o in ("-s", "--tracedir"):
			traceDirPath = a
		if o in ("-l", "--threshold"):
			th = a.split(',')
			THRESHOLD_MIN = int(th[0])
			THRESHOLD_MAX = int(th[1])
		if o in ("-p", "--dump"):
			dumpData = True
		if o in ("-d", "--debug"):
			DEBUG = True
	
	if not traceFilePath:
		print "The tracefile is needed"
		exitUsage(sys.argv[0], usage)

	connections = analyze(traceFilePath)
	
	clients_stat = candidateRankings(connections)

	print clients_stat
	# If the user wants to import the real connections data
	if traceDirPath :

		n_clients = len(clients_stat)	
		realConnections = realConnectionsParser(traceDirPath)
		
		for cli in realConnections:
			real_stat[cli] = {}
			real_stat[cli]['candidates'] = []
			for sv in realConnections[cli]:
				real_stat[cli]['candidates'].append({
					'nconns' : len(realConnections[cli][sv]),
					'server' : sv})

		for eclient in clients_stat:
			clients_stat[eclient]['maccuracy'] = -1.0
 			
			# Some problem occured if a logged connection does not appear 
			# in the real connections
			if not eclient in real_stat:
				rclients_missing += 1
			else :
				#TODO: select realsv according to some policy (the one with
				# highest nconns
				realsv = real_stat[eclient]['candidates'][0]
                                
                                if len(clients_stat[eclient]['candidates']) > 0: 
                                    sv = clients_stat[eclient]['candidates'][0] #best candidate

                                    #for sv in clients_stat[eclient]['candidates']:
                                    if sv['server'] == realsv['server']:
                                            p = 0.0
                                            p = (float(min(realsv['nconns'], sv['nconns'])) / 
                                                            float(max(realsv['nconns'], sv['nconns'])))
                                            clients_stat[eclient]['maccuracy'] = p
			
			#matched client
			if clients_stat[eclient]['maccuracy'] != -1:
                                matched_clients += 1
				matched_accuracy += clients_stat[eclient]['maccuracy']
                
                if matched_clients > 0:
                    matched_accuracy /= float(matched_clients)
	    
	if dumpData:
		print connections
	else:
		print printPretty(connections, clients_stat, real_stat)

        if matched_clients > 0:
            matched_portion = matched_clients / float(n_clients)

        print ("missing:%d;matched:%d;n_traced_clients:%d;matched_portion:%f;matching_accuracy:%f\n" % 
            (rclients_missing, matched_clients, n_clients, matched_portion, matched_accuracy))
