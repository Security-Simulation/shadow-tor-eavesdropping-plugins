#!/usr/bin/python

class Connection():
	'''
	A data object used to store every connection revealed by an autosys host. 
	The side tells us if the autosys was attached in a client or in a server.
	'''
	def __init__(self, side, hostname, timestamp):
		self.side = side
		self.hostname = hostname
		self.timestamp = timestamp

class Analyzer():
	'''
	The core class. 
	The analyzer does the whole dirty job.	
	'''
	def __init__(self, traceFilePath):
		self.traceFile = open(traceFilePath)
		self.connections = []		
	
	def parseData(self):
		'''
		Parse the traced data. Each line has the following format:
		serverside/clientside(c/s);hostname;timestamp(sss..ssmmmuuu)
		Create a connection object per each parsed line
		'''
		for line in self.traceFile.readlines():
			params = line.split(';')
			
			if params[0] == 'c':
				side = 0
			else:
				side = 1
			
			hostname = params[1]	
			timestamp = int(params[2])
			
			c = Connection(side, hostname, timestamp)
			self.connections.append(c)

		self.traceFile.close()

if __name__ == '__main__':
	traceFilePath = "/tmp/trace.log"
	analyzer = Analyzer(traceFilePath)
	analyzer.parseData()

	for c in analyzer.connections:
		print c.side,c.hostname,c.timestamp
