#!/usr/bin/python

#Constants
CLIENT = 0
SERVER = 1

class Connection():
	'''
	A connection revealed by an autosys host. The side tells us if the 
	autosys plugin was attached in a client or in a server.
	'''
	def __init__(self, side, hostname, timestamp):
		self.side = side
		self.hostname = hostname
		self.timestamp = timestamp
		if side == CLIENT:
			self.servers = []

class Analyzer():
	'''
	The core class. 
	The analyzer does the whole dirty job.	
	'''
	def __init__(self, traceFilePath):
		self.traceFile = open(traceFilePath)
		self.connections = []		
	
	def __analyzeForward(self, startConn):
		cs = self.connections
		for c in cs[cs.index(startConn)+1:]:
			if c.side == SERVER:
				#TODO: time distance
				print (str(cs.index(c))+" analyzing:"
					" "+startConn.hostname+" "+c.hostname)

	def parseData(self):
		'''
		Parse the traced data. Each line has the following format:
		serverside/clientside(c/s);hostname;timestamp(sss..ssmmmuuu)
		Create a connection object per each parsed line
		'''
		for line in self.traceFile.readlines():
			params = line.split(';')
			
			if params[0] == 'c':
				side = CLIENT
			else:
				side = SERVER
			
			hostname = params[1]	
			timestamp = int(params[2])
			
			c = Connection(side, hostname, timestamp)
			self.connections.append(c)

		self.traceFile.close()    

	def analyzeConnections(self):
		'''
		For each client side connection, check and store the server 
		candidates for that connection. Simply scan the connections 
		list moving forward in time and save all the servers that 
		received a connection in the next time slice of THRESHOLD size.
		'''
		for conn in self.connections:
			if conn.side == CLIENT:
				self.__analyzeForward(conn)

if __name__ == '__main__':
	traceFilePath = "/tmp/trace.log"
	#microseconds threshold distance
	threshold = 1000000000 
	analyzer = Analyzer(traceFilePath)
	analyzer.parseData()

	for c in analyzer.connections:
		print c.side,c.hostname,c.timestamp

	print "######"

	analyzer.analyzeConnections()
