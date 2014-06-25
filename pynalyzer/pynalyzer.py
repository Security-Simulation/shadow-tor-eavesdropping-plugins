#!/usr/bin/python

#Constants
CLIENT = 0
SERVER = 1

#TODO: convert this in a dictionary
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
			self.servers = {}

class Analyzer():
	'''
	The core class. 
	The analyzer does the whole dirty job.	
	'''
	def __init__(self, traceFilePath, threshold):
		self.traceFile = open(traceFilePath)
		self.connections = []
		self.threshold = threshold

	def __calcProb(self, time1, time2):
		if time1 > time2:
			raise Exception("time2 must be bigger than time1")

		timeDistance = time2 - time1
		prob = 0.0

		if timeDistance <= self.threshold:		
			prob = 1.0 - (float(timeDistance) / float(self.threshold))
		
		return prob 

	def __analyzeForward(self, startConn):
		cs = self.connections
		for s in cs[cs.index(startConn)+1:]:
			if s.side == SERVER: 
				prob = self.__calcProb(startConn.timestamp, s.timestamp)
				if prob == 0.0:
					break #TODO: clean this with a while
				
				startConn.servers[s.hostname] = [prob, s.timestamp]

		print (str(startConn.timestamp)+" "+startConn.hostname+" : "
				+str(startConn.servers))

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
			#TODO else: remove it

if __name__ == '__main__':
	traceFilePath = "/tmp/trace.log"
	#microseconds threshold distance
	threshold = 6000000000 
	analyzer = Analyzer(traceFilePath, threshold)
	analyzer.parseData()

	for c in analyzer.connections:
		print c.side,c.hostname,c.timestamp

	print "######"

	analyzer.analyzeConnections()
