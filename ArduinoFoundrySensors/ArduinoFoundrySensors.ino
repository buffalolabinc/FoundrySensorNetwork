#include <OneWire.h>
#include <SoftwareSerial.h>
#include <XBee.h>
#include "types.h"


//#define DEBUG
#undef DEBUG

#define SAMPLE_TIME 10

//device_status_t readTemp(OneWire & a_TempSensor, int *temperature);
int readTemp(TempSensor& a_TempSensor);
void printStatus(int a_index, device_status_t a_status);

SoftwareSerial xbeeSerial(2,3);
XBee xbee = XBee();
XBeeAddress64 coordinator = XBeeAddress64(0x0013A200, 0x40BF94E0);
ZBTxRequest xbeeRequest = ZBTxRequest();
TempInfo tempInfo = TempInfo();

// DS18S20 Temperature chip i/o
//OneWire tempSensor0(4);			// on pin 4
//OneWire tempSensor1(5);			// on pin 5
TempSensor g_TempSensors[] = { TempSensor(4), TempSensor(5) };

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
	unsigned int numSensors = sizeof(g_TempSensors)/sizeof(OneWire);
	int l_Temperature;

	if ((millis() / 1000) % SAMPLE_TIME == 0)
	{
		for (unsigned int i=0; i<numSensors; i++)
		{
			if (g_TempSensors[i].GetStatus() != DS_NO_DEVICE)
			{
				l_Temperature = readTemp(g_TempSensors[i]);
				printStatus(i, g_TempSensors[i].GetStatus());
				if (g_TempSensors[i].GetStatus() == DS_OK)
				{
					Serial.print("temp[");
					Serial.print(i);
					Serial.print("] = ");
					Serial.println(l_Temperature);

	
					tempInfo.SetSensorNum(i);
					tempInfo.SetTemp(l_Temperature);
					xbeeRequest.setPayload((uint8_t*) &tempInfo);
					xbee.send(xbeeRequest);
				}
			}
		}
	}
	Serial.println(" loop");
	delay(1000);
}

int readTemp(TempSensor& a_TempSensor)
{
	byte i;
	byte __attribute__((unused)) present = 0;
	byte data[12];
	byte addr[8];
	float temp;
	int l_Temperature = 0;

	a_TempSensor.GetSensor().reset_search();
	if (!a_TempSensor.GetSensor().search(addr))
	{
#ifdef DEBUG
		Serial.print("No more addresses.\n");
#endif
		a_TempSensor.GetSensor().reset_search();
		a_TempSensor.SetStatus(DS_NO_DEVICE);
		return l_Temperature;
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
		a_TempSensor.SetStatus(DS_BAD_CRC);
		return l_Temperature;
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
		a_TempSensor.SetStatus(DS_NO_DEVICE);
		return l_Temperature;
		break;
	}

	a_TempSensor.GetSensor().reset();
	a_TempSensor.GetSensor().select(addr);
	a_TempSensor.GetSensor().write(0x44, 1);	// start conversion, with parasite power on at the end

	delay(1000);				// maybe 750ms is enough, maybe not
	// we might do a a_TempSensor.GetSensor().depower() here, but the reset will take care of it.

	present = a_TempSensor.GetSensor().reset();
	a_TempSensor.GetSensor().select(addr);
	a_TempSensor.GetSensor().write(0xBE);		// Read Scratchpad

#ifdef DEBUG
	Serial.print("P=");
	Serial.print(present, HEX);
	Serial.print(" ");
#endif

	for (i = 0; i < 9; i++)
	{							// we need 9 bytes
		data[i] = a_TempSensor.GetSensor().read();
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
	{
		a_TempSensor.SetStatus(DS_BAD_CRC);
		return l_Temperature;
	}

	temp =
		((data[1] & 0x07) << 4) + (data[0] >> 4) + (data[0] & 0x0f) / 16.0;
	if (data[1] & 0x80)
		temp = -temp;
	temp = temp * 1.8 + 32.0;

#ifdef DEBUG
	Serial.print("temp = ");
	Serial.println(int (temp));
#endif
	l_Temperature = temp;
	a_TempSensor.SetStatus(DS_OK);
	return l_Temperature;
}


void printStatus(int a_index, device_status_t a_status)
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
	Serial.print("Device ");
	Serial.print(a_index);
	Serial.print(" ");
	Serial.println(l_status);
}

