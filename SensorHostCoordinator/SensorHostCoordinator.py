#!/usr/bin/python
# -*- coding: ascii -*-

import sys
import serial
import json
import time
import datetime
import threading
import TaskThread
import twitter
import phant

from twilio.rest import TwilioRestClient
from xbee import ZigBee
from struct import *
from ctypes import c_longlong

consoleOutput = True
consoleFlush = True

# interval for sending data to services
SparkFunDataInterval = 5*60	# 5 minutes
TwitterInterval = 15*60		# 15 minutes
TwilioInterval = 15			# 15 seconds
XBInterval = 10				# 10 seconds

Relay0 = False

PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200
COMMAND = 'NI'
FRAME = 'A'

# json files with the credentials
# There is files so that they aren't in the source on github
TwitterCredentialFile = 'Twitter-Oauth-Token_TheFoundryTemp.json'
SparkFunCredentialFile = 'TheFoundryBuildingSensors_SparkFun.json'
TwilioCredentialFile = 'TheFoundryBuildingSensors_Twilio.json'


nodesByName = {}
nodesByAddr = {}
nodeData = {}

# enum the fields in the SparkFunData thingy
nodeNames = ['z0temp0', 'z0temp1']
# map all nodes and sensors to sparkfundata fields
nodeMap = { ('Zone 0' , 0) : 'z0temp0', ('Zone 0', 1) : 'z0temp1' }

broadcast64 = '\x00\x00\x00\x00\x00\x00\xFF\xFF'
broadcast16 = '\xFF\xFE'
serialPort = serial.Serial(PORT, BAUD_RATE)

def str_iequal(a, b):
	try:
		return a.lower() == b.lower()
	except AttributeError:
		return a == b

def printAddr(addr):
	addrList = unpack('BB', addr)
	print ' '.join('%02x'%n for n in addrList)

def printAddrLong(addrLong):
	addrList = unpack('BBBBBBBB', addrLong)
	print ' '.join('%02x'%n for n in addrList)

def processXBeeData(data):
	"""
	This method is called whenever data is received
	from the associated XBee device. Its first and
	only argument is the data contained within the
	frame.
	"""
	if data['id'] == 'rx':
		addr = data['source_addr_long']
		#printAddrLong(addr)
		if addr in nodesByAddr.keys():
			nodeName =  nodesByAddr[addr]
		else:
			nodeName = 'Unknown'
			if consoleOutput:
				print 'Unknown Node Found - querying all Nodes'
			queryNodes();
			return

		# get data from packet 
		nodeInfo = data['rf_data']
		packet = unpack('BBh', nodeInfo);
		sensorNum = packet[1]
		sensorData = packet[2]

		key = (nodeName, sensorNum)
		if key in nodeMap:
			SFDataField = nodeMap[key]
			nodeData[SFDataField] = sensorData;
			print nodeData

		if consoleOutput:
			st = datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')
			print st, nodeName, 'sensor', sensorNum, sensorData, 'degrees'
			if consoleFlush:
				sys.stdout.flush()


	elif data['id'] == 'remote_at_response':
		addr64 = data['source_addr_long']
		addr16 = data['source_addr']
		nodeName = data['parameter']
		printAddrLong(addr64)
		print nodeName
		nodesByName[nodeName] = {'source_addr_long':addr64, 'source_addr':addr16}
		nodesByAddr[addr64] = nodeName

def queryNodes():
	nodesByName = {}
	nodesByAddr = {}
	xbee.send('remote_at', frame_id='\x01', dest_addr_long = broadcast64, dest_addr=broadcast16, command=COMMAND, parameter=None)

def checkNodesForData():
	"""
		Make sure all nodes have received Data from a remote sensor
	"""
	for name in nodeNames:
		if not name in nodeData:
			return False
	return True

class TwitterEventThread(TaskThread.TaskThread):
	def __init__(self):
		TaskThread.TaskThread.__init__(self)
		self.setInterval(TwitterInterval)

	def task(self):
		global nodeData
		print 'Running Twitter Thread'
		if checkNodesForData():
			self.setInterval(TwitterInterval)
			timeStamp = datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M')
			twitterPost = ' '.join([timeStamp] + ['%s:%d'%(name, nodeData[name]) for name in nodeNames])
			print twitterPost

			try:
				TwitterApi.PostUpdate(twitterPost)
			except:
				"""
				Do something better here
				"""
				print 'Exception in Twitter Thread'
				print 'Error:', sys.exc_info()[0]
			else:
				print 'No Exception in Twitter Thread'
		else:
			## if data isn't ready yet - set the tak interval to 60 seconds
			self.setInterval(60);


class SparkFunDataEventThread(TaskThread.TaskThread):
	def __init__(self):
		TaskThread.TaskThread.__init__(self)
		self.setInterval(SparkFunDataInterval)

	def task(self):
		global nodeData
		print 'Running SparkFunData Thread'
		if checkNodesForData():
			self.setInterval(SparkFunDataInterval)
			try:
				SparkFunLog.log(*(nodeData[name] for name in nodeNames))
			except:
				"""
				Do something better here
				"""
				print 'Exception in SparkFunData Thread'
				print 'Error:', sys.exc_info()[0]
			else:
				print 'No Exception in SparkFunData Thread'
		else:
			## if data isn't ready yet - set the tak interval to 60 seconds
			self.setInterval(60);

class TwilioEventThread(TaskThread.TaskThread):
	def __init__(self):
		TaskThread.TaskThread.__init__(self)
		self.setInterval(TwilioInterval)

	def task(self):
		print 'Running Twilio Thread'
		target = 'Zone 1'
		for inMessage in twilioClient.messages.list():
			print inMessage.date_sent,inMessage.sid, inMessage.from_, inMessage.direction, inMessage.body
#			outMessage = twilioClient.messages.create( body="Received - " + inMessage.body, to=inMessage.from_, from_="+17167144090",)
			relayData = None
			if str_iequal(inMessage.body, 'on'):
				relayData = '\x00\x01'
			elif str_iequal(inMessage.body, 'off'):
				relayData = '\x00\x00'
			inMessage.delete()
#			outMessage.delete()
			if relayData:
				if target in nodesByName:
					print 'Sending Packet to '
					printAddrLong(nodesByName[target]['source_addr_long'])
					printAddr(nodesByName[target]['source_addr'])
					xbee.send('tx', frame_id='\x01', dest_addr_long = nodesByName[target]['source_addr_long'], dest_addr = nodesByName[target]['source_addr'], data=relayData) 

class XBEventThread(TaskThread.TaskThread):
	def __init__(self):
		TaskThread.TaskThread.__init__(self)
		self.setInterval(XBInterval)

	def task(self):
		global Relay0
		print 'Running XB Thread'
		target = 'Zone 1'

		if target in nodesByName:
			if Relay0:
				Relay0 = False
				relayData = '\x00\x00'
			else:
				Relay0 = True
				relayData = '\x00\x01'

			print 'Sending Packet to '
			printAddrLong(nodesByName[target]['source_addr_long'])
			printAddr(nodesByName[target]['source_addr'])
			xbee.send('tx', frame_id='\x01', dest_addr_long = nodesByName[target]['source_addr_long'], dest_addr = nodesByName[target]['source_addr'], data=relayData) 



if __name__ == '__main__':

	# get the twitter Credentials
	json_data = open(TwitterCredentialFile)
	data = json.load(json_data)
	json_data.close()

	TwitterConsumerKey = data['ConsumerKey']
	TwitterConsumerSecret = data['ConsumerSecret']
	TwitterAccessToken = data['AccessToken']
	TwitterAccessTokenSecret = data['AccessTokenSecret']

	# get the sparkfun Credentials
	json_data = open(SparkFunCredentialFile)
	data = json.load(json_data)
	json_data.close()

	SparkFunPublicKey = data['publicKey']
	SparkFunPrivateKey = data['privateKey']

	# get the twilio Credentials
	json_data = open(TwilioCredentialFile)
	data = json.load(json_data)
	json_data.close()

	TwilioAccountSid = data['AccountSid']
	TwilioAuthToken = data['AuthToken']

	# Hook up to zigbee network over the serial port
	xbee = ZigBee(serialPort, escaped=True, callback=processXBeeData)

	# Hook up to the SparkFun data portal
	try:
		SparkFunLog = phant.Phant(	SparkFunPublicKey,
									'z0temp0',
									'z0temp1',
									private_key=SparkFunPrivateKey)
	except:
		print "Couldn't open data.sparkfun.com"
	else:
		SparkFunDataThread = SparkFunDataEventThread()
		SparkFunDataThread.start()

	# Hook up to twitter
	try:
		TwitterApi = twitter.Api(	consumer_key = TwitterConsumerKey,
									consumer_secret = TwitterConsumerSecret,
									access_token_key = TwitterAccessToken,
									access_token_secret = TwitterAccessTokenSecret)
	except:
		print "Couldn't login to Twitter"
	else:
		TwitterThread = TwitterEventThread()
		TwitterThread.start()
		time.sleep(0.20)

	# Hook up to twilio
	try:
		twilioClient = TwilioRestClient(TwilioAccountSid, TwilioAuthToken)
	except:
		print "Couldn't login to Twilio"
	else:
		TwilioThread = TwilioEventThread()
		TwilioThread.start()

	# start timed threads
	#XBThread = XBEventThread()
	#XBThread.start()

	while True:
		try:
			time.sleep(0.1)
		except KeyboardInterrupt:
			break

	if 'XBThread' in globals():
		print 'shutting down XBThread'
		XBThread.shutdown()

	if 'TwilioThread' in globals():
		print 'shutting down Twilio'
		TwilioThread.shutdown()

	if 'SparkFunDataThread' in globals():
		print 'shutting down Sparkfun'
		SparkFunDataThread.shutdown()

	if 'TwitterThread' in globals():
		print 'shutting down Twiter'
		TwitterThread.shutdown()

	xbee.halt()
	serialPort.close()



