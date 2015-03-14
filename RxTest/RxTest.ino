/**
 * Copyright (c) 2009 Andrew Rapp. All rights reserved.
 *
 * This file is part of XBee-Arduino.
 *
 * XBee-Arduino is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * XBee-Arduino is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XBee-Arduino.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <XBee.h>
#include <SoftwareSerial.h>

/*
This example is for Series 2 XBee
Receives a ZB RX packet and prints the packet to softserial
*/

SoftwareSerial xbeeSerial(2, 3);
XBee xbee = XBee();
XBeeResponse response = XBeeResponse();
// create reusable response objects for responses we expect to handle 
ZBRxResponse rx = ZBRxResponse();
ModemStatusResponse msr = ModemStatusResponse();

unsigned char payload[32];

int baseRelayPin = 8;
int numRelayPins = 6;

void setup()
{  
	// start serial
	Serial.begin(115200);
	xbeeSerial.begin(9600);
	xbee.setSerial(xbeeSerial);
	for(int i=0; i<numRelayPins; i++)
	{
		int pin = i + baseRelayPin;
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
	}

	Serial.println("Starting up!");
}

// continuously reads packets, looking for ZB Receive or Modem Status
void loop()
{
	int pin;
	int state;

	xbee.readPacket();

	if (xbee.getResponse().isAvailable())
	{
		// got something

		Serial.println("Got something!");

		if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE)
		{
			// got a zb rx packet

			// now fill our zb rx class
			xbee.getResponse().getZBRxResponse(rx);

			Serial.println("Got an rx packet!");

			if (rx.getOption() == ZB_PACKET_ACKNOWLEDGED)
			{
				// the sender got an ACK
				Serial.println("packet acknowledged");
			}
			else
			{
				Serial.println("packet not acknowledged");
			}

			Serial.print("checksum is ");
			Serial.println(rx.getChecksum(), HEX);

			Serial.print("packet length is ");
			Serial.println(rx.getPacketLength(), DEC);
	
			for (int i = 0; i < rx.getDataLength(); i++)
			{
				Serial.print("payload [");
				Serial.print(i, DEC);
				Serial.print("] is ");
				Serial.println(rx.getData()[i], HEX);
				payload[i] = rx.getData()[i];
			}

			pin = payload[0] + baseRelayPin;
			state = payload[1]?HIGH:LOW;
			Serial.println(pin);
			Serial.println(state);
		
			digitalWrite(pin, state);

			for (int i = 0; i < xbee.getResponse().getFrameDataLength(); i++)
			{
				Serial.print("frame data [");
				Serial.print(i, DEC);
				Serial.print("] is ");
				Serial.println(xbee.getResponse().getFrameData()[i], HEX);
			}
		}
	}
	else if (xbee.getResponse().isError())
	{
		Serial.print("oh no!!! error code:");
		Serial.println(xbee.getResponse().getErrorCode());
	}
}
