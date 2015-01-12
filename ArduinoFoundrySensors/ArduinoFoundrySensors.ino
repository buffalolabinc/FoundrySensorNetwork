#include <OneWire.h>
#include <SoftwareSerial.h>
#include <XBee.h>
#include "types.h"


//#define DEBUG
#undef DEBUG

#define SAMPLE_TIME 10

device_status_t readTemp(OneWire & tempSensor, int *temperature);
void printStatus(device_status_t a_status);

SoftwareSerial xbeeSerial(2,3);
XBee xbee = XBee();
XBeeAddress64 coordinator = XBeeAddress64(0x0013A200, 0x40BF94E0);
ZBTxRequest xbeeRequest = ZBTxRequest();
TempInfo tempInfo = TempInfo();

// DS18S20 Temperature chip i/o
//OneWire tempSensor0(4);			// on pin 4
//OneWire tempSensor1(5);			// on pin 5
OneWire tempSensors[] = { OneWire(4), OneWire(5) };

void setup(void)
{
	// initialize inputs/outputs
	// start serial port
	Serial.begin(115200);
	xbeeSerial.begin(9600);
	xbee.setSerial(xbeeSerial);

	xbeeRequest.setAddress64(coordinator);
	xbeeRequest.setPayloadLength(sizeof(TempInfo));

}


void loop(void)
{
	unsigned int numSensors = sizeof(tempSensors)/sizeof(OneWire);
	int temp[numSensors];
	device_status_t l_status;

	if ((millis() / 1000) % SAMPLE_TIME == 0)
	{
		for (unsigned int i=0; i<numSensors; i++)
		{
			l_status = readTemp(tempSensors[i], &temp[i]);
			printStatus(l_status);
			if (l_status == DS_OK)
			{
				Serial.print("temp[");
				Serial.print(i);
				Serial.print("] = ");
				Serial.println(temp[i]);
			}

	
			tempInfo.SetSensorNum(i);
			tempInfo.SetTemp(temp[i]);
			xbeeRequest.setPayload((uint8_t*) &tempInfo);
			xbee.send(xbeeRequest);
		}
	}
}

device_status_t readTemp(OneWire& tempSensor, int* temperature)
{
	byte i;
	byte __attribute__((unused)) present = 0;
	byte data[12];
	byte addr[8];
	float temp;

	tempSensor.reset_search();
	if (!tempSensor.search(addr))
	{
#ifdef DEBUG
		Serial.print("No more addresses.\n");
#endif
		tempSensor.reset_search();
		return DS_NO_DEVICE;
	}

#ifdef DEBUG
	Serial.print("R=");
	for (i = 0; i < 8; i++)
	{
		Serial.print(addr[i], HEX);
		Serial.print(" ");
	}
#endif

	if (OneWire::crc8(addr, 7) != addr[7])
	{
#ifdef DEBUG
		Serial.print("CRC is not valid!\n");
#endif
		return DS_BAD_CRC;
	}

	switch (addr[0])
	{
	case 0x10:
	case 0x28:
#ifdef DEBUG
		Serial.println("Device is a DS18S20 family device.");
#endif
		break;

	default:
#ifdef DEBUG
		Serial.print("Device family is not recognized: 0x");
		Serial.println(addr[0], HEX);
#endif
		return DS_NO_DEVICE;
		break;
	}

	tempSensor.reset();
	tempSensor.select(addr);
	tempSensor.write(0x44, 1);	// start conversion, with parasite power on at the end

	delay(1000);				// maybe 750ms is enough, maybe not
	// we might do a tempSensor.depower() here, but the reset will take care of it.

	present = tempSensor.reset();
	tempSensor.select(addr);
	tempSensor.write(0xBE);		// Read Scratchpad

#ifdef DEBUG
	Serial.print("P=");
	Serial.print(present, HEX);
	Serial.print(" ");
#endif

	for (i = 0; i < 9; i++)
	{							// we need 9 bytes
		data[i] = tempSensor.read();
#ifdef DEBUG
		Serial.print(data[i], HEX);
		Serial.print(" ");
#endif
	}
#ifdef DEBUG
	Serial.print(" CRC=");
	Serial.print(OneWire::crc8(data, 8), HEX);
	Serial.println();
#endif
	if (OneWire::crc8(data, 8) != data[8])
		return DS_BAD_CRC;

	temp =
		((data[1] & 0x07) << 4) + (data[0] >> 4) + (data[0] & 0x0f) / 16.0;
	if (data[1] & 0x80)
		temp = -temp;
	temp = temp * 1.8 + 32.0;

#ifdef DEBUG
	Serial.print("temp = ");
	Serial.println(int (temp));
#endif
	*temperature = temp;
	return DS_OK;
}


void printStatus(device_status_t a_status)
{
	const char* l_status;
	switch (a_status)
	{
	case DS_OK:
		l_status = "OK";
		break;
	case DS_NO_DEVICE:
		l_status = "No Device Found";
		break;
	case DS_BAD_CRC:
		l_status = "Bad CRC";
		break;
	default:
		l_status = "Unknown";
		break;
	}
	Serial.println(l_status);
}

