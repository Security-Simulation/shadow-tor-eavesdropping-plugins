#!/usr/bin/python

#Constants
CLIENT = 0
SERVER = 1

#Global variables default values 
THRESHOLD = 6000000000
TRACE_FILE = "trace.log"

def calcProb(time1, time2):
	'''
	Calculate the probability that a client connection was related or not
	to a certain server connection. The time2 parameter should be the server
	connection timestamp and the time1 the client connection timestamp.
	The probability is calculated in relation to the fixed THRESHOLD.
	'''
	if time1 > time2:
		raise Exception("time2 must be bigger than time1")

	timeDistance = time2 - time1
	prob = 0.0

	if timeDistance <= THRESHOLD:		
		prob = 1.0 - (float(timeDistance) / float(THRESHOLD))
	
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
			if prob == 0.0:
				break #TODO: clean this with a while
		
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
			connections[startConn['hostname']] = []
			conn = {}
			conn['time'] = startConn['time']
			conn['servers'] = analyzeForward(startConn, 
				data.index(i) + 1, data)
			connections[startConn['hostname']].append(conn)
	
	return connections

if __name__ == '__main__':
	traceFilePath = TRACE_FILE
	connections = analyze(traceFilePath)
	print connections
