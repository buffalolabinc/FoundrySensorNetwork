
typedef enum
{
	DS_OK,
	DS_NO_DEVICE,
	DS_BAD_CRC,
} device_status_t;


#define TEMP_PACKET 0

class TempSensor
{
public:
	TempSensor(int pin) : m_Sensor(pin), m_Status(DS_OK)
	{
	};

	OneWire& GetSensor(void)					{ return m_Sensor;		}

	device_status_t GetStatus(void)				{ return m_Status;		}
	void SetStatus(device_status_t a_Status)	{ m_Status = a_Status; 	}

protected:
	OneWire m_Sensor;
	device_status_t m_Status;
};

		
class NodeInfo
{
public:
	NodeInfo(uint8_t a_NodeType)			{ m_NodeType = a_NodeType;		}

	uint8_t GetPacketType(void)				{ return m_NodeType;			}
	void SetPacketType(uint8_t a_NodeType)	{ m_NodeType = a_NodeType;		}

protected:
	uint8_t m_NodeType;
};

class TempInfo : public NodeInfo
{
public:
	TempInfo() : NodeInfo(TEMP_PACKET)	{}
	void SetTemp(int a_temp)			{ m_temp = a_temp;				}
	void SetSensorNum(uint8_t a_num)	{ m_sensorNum = a_num;			}

protected:
	uint8_t m_sensorNum;
	int m_temp;
};


