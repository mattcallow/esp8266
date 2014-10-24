/*
 * Test sketch for esp8266 module
 * works with teensy3
 * Matt Callow October 2014
 * 
 * This sketch will connect api.thingspeak.com and update field1 in a 
 * channel with current millis() value
 * Configuration is done via the serial monitor. To configure, make sure you 
 * have the arduino serial monitor set to send CR at the end of each line
 * You will then be asked to enter your WIFI SSID, password and thingspeak key
 * These are stored in EEPROM
 *
 */

#include <EEPROM.h>

static const unsigned long TIMEOUT  =    5000; // mS
// define the reset pin here
const int RESET_PIN=2;

class ESP8266
{
private:
	int resetPin;
	int timeout;
	HardwareSerial &ser;
	Stream &dbg;
public:
	enum status_t { OK=0, BUSY, DNSFAIL, TIMEOUT, COMMS_ERROR };

	ESP8266(HardwareSerial& ser, Stream& dbg, int resetPin, int timeout) : 
		resetPin(resetPin), 
		timeout(timeout),
		ser(ser),
		dbg(dbg)
	{
	}

	void begin()
	{
		ser.begin(115200);
		ser.setTimeout(timeout);
		pinMode(resetPin, OUTPUT);
		digitalWrite(resetPin, HIGH);
		reset();
	}

	status_t reset()
	{
		dbg.println("Hardware reset");
		digitalWrite(resetPin, LOW);
		delay(100);
		digitalWrite(resetPin, HIGH);
		status_t status = waitFor("ready");
		if (status != OK) return status;
		return sendAndWait("AT+CWQAP", "OK"); // quit current AP
	}

	// Read characters from WiFi module and echo to serial until keyword occurs or timeout.
	status_t waitFor(String keyword)
	{
// TODO - improve this search. I think it will fail for some cases
		byte current_char   = 0;
		byte keyword_length = keyword.length();
		// Fail if the target string has not been sent by deadline.
		unsigned long deadline = millis() + timeout;
		while(millis() < deadline)
		{
			if (ser.available())
			{
				char ch = ser.read();
				dbg.write(ch);
				if (ch == keyword[current_char])
				{
					if (++current_char == keyword_length)
					{
						  dbg.println();
						  return OK;
					}
				}
			}
		}
		return TIMEOUT;
	}

	// Echo module output until 3 newlines encountered.
	// (Used when we're indifferent to "OK" vs. "no change" responses.)
	void skip()
	{
		waitFor("\n");        // Search for nl at end of command echo
		waitFor("\n");        // Search for 2nd nl at end of response.
		waitFor("\n");        // Search for 3rd nl at end of blank line.
	}

	// Send a command to the module and wait for acknowledgement string
	// (or flush module output if no ack specified).
	// Echoes all data received to the serial monitor.
	status_t sendAndWait(String cmd, String ack)
	{
		//ser.println(cmd);
/* attempt to slow down write to module - doesn't seem to help with 'busy'  */
		for (unsigned int i=0;i<cmd.length();i++)
		{
			ser.write(cmd[i]);
			ser.flush();
			delay(1);
		}
		ser.write("\r");
		ser.write("\n");
		// If no ack response specified, skip all available module output.
		if (ack == "")
		{
			skip();
		}
		else
		{
			// Otherwise wait for ack.
			return waitFor(ack);
		}
		return OK;
	}
	
	// Read and echo all available module output.
	// (Used when we're indifferent to "OK" vs. "no change" responses or to get around firmware bugs.)
	void flush()
	{
		while(ser.available()) dbg.write(ser.read());
	}

	status_t connectToAP(String ssid, String password)
	{
		String cmd = "AT+CWJAP=\"" + ssid + "\",\"" + password + "\"";
		return sendAndWait(cmd, "OK");
	}

	status_t connectToServer(byte cnum, String type, String host, int port)
	{
		// Allow multiple connections (we'll only use the first).
		// we set this each time, as it seem that the module can loose this setting?
		status_t status = sendAndWait("AT+CIPMUX=1", "OK");    
		if (status != OK) return status;
		String cmd = "AT+CIPSTART=" + String(cnum) + ",\"" + String(type) + "\",\"" + String(host) + "\"," + String(port);
		return sendAndWait(cmd, "Linked");
		/* 
		need to wait for 'OK' then  'Linked'
		other possible responses are 'DNS Fail'
		'Link typ ERROR' - can occur when MUX=0

		*/
	}
	status_t sendNetworkData(byte cnum, String data)
	{
		String cmd = "AT+CIPSEND=" + String(cnum) + "," + String(data.length());
		status_t status = sendAndWait(cmd, ">");
		if (status != OK) return status;
		dbg.print("Sending:[");
		dbg.print(data);
		dbg.println("]");
		return sendAndWait(data, "SEND OK");
		// possible other responses include 'link is not'
	}

	status_t closeConnection(byte cnum)
	{
		String cmd = "AT+CIPCLOSE=" + String(cnum);
		return sendAndWait(cmd, "OK");
		// possible other responses include 'link is not'
	}
};

class Config
{
#define SSID_LEN 32
#define WIFI_PASSWORD_LEN 32
#define API_KEY_LEN 16
#define START_MAGIC 0x44
#define END_MAGIC 0xBB
public:
	char ssid[SSID_LEN+1];
	char wifi_password[WIFI_PASSWORD_LEN+1];
	char api_key[API_KEY_LEN+1];

/* format of config in EEPROM is:
 0: Start Magic
 1: start of SSID
 2: start of password
 3: start of write key
 4: End Magic
Therefore valid config will have magic at 0 and 4, non-zero at 1,2,3
*/
	bool read()
	{
		if (EEPROM.read(0) != START_MAGIC) return false;
		if (EEPROM.read(1) == 0) return false;
		if (EEPROM.read(2) == 0) return false;
		if (EEPROM.read(3) == 0) return false;
		if (EEPROM.read(4) != END_MAGIC) return false;
		read_str(EEPROM.read(1), ssid, SSID_LEN);
		read_str(EEPROM.read(2), wifi_password, WIFI_PASSWORD_LEN);
		read_str(EEPROM.read(3), api_key, API_KEY_LEN);
		return true;
	}

	void print(Stream &out)
	{
		if (!read())
		{
			out.println("No valid config");
			return;
		}
		out.println("Current Config:");
		out.print("SSID    :"); out.println(ssid);
		out.print("Password:"); out.println(wifi_password);
		out.print("API KEY :"); out.println(api_key);
	}

	void set(Stream &in, Stream &out)
	{
		int l,i,start;
		while (in.available()) in.read();
		out.println("Enter SSID:");
		l = read_line(in, ssid, SSID_LEN);
		start=5;
		EEPROM.write(1, start);
		i=0;
		while(i<l)
		{
			EEPROM.write(start++, ssid[i++]);
		}
		EEPROM.write(start++, 0);
		out.println("Enter password:");
		l = read_line(in, wifi_password, WIFI_PASSWORD_LEN);
		EEPROM.write(2, start);
		i=0;
		while(i<l)
		{
			EEPROM.write(start++, wifi_password[i++]);
		}
		EEPROM.write(start++, 0);
		out.println("Enter write key:");
		l = read_line(in, api_key, API_KEY_LEN);
		out.println(l);
		out.println(api_key);
		EEPROM.write(3, start);
		i=0;
		while(i<l)
		{
			EEPROM.write(start++, api_key[i++]);
		}
		EEPROM.write(start++, 0);
		EEPROM.write(0, START_MAGIC);
		EEPROM.write(4, END_MAGIC);
	}
private:
	void read_str(int start, char *buf, int l)
	{
		int p=start;
		int i=0;
		while(i<l)
		{
			buf[i] = EEPROM.read(p++);
			if (buf[i] == 0) return;
			i++;
		}
	}
	/* read a line from 'in' store up to 'l' chars in 'buf' 
	 * buffer should be 1 byte bigger then l
   */
	int read_line(Stream &in, char *buf, int l)
	{
		int i=0;
		char *p=buf;
		char c;
		do {
			while(!in.available());
			c=(char)in.read();
			*p++=c;
			i++;
			if (i>l) break;
		} while (c!='\r' && c!='\n');
		if (i>0) i--;
		buf[i]='\0';
		return i;
	}
};

#define DBG Serial

ESP8266 wifi(Serial1, DBG, RESET_PIN, TIMEOUT);
Config config;

void setup()
{
	DBG.begin(115200);
	int i=0;
	for(;;) {
		config.print(DBG);
		DBG.println("Press 'c' to configure, any other key to start");
		if (DBG.available()) break;
		if (i++>10 && config.read()) break;
		delay(1000);
	} 
	if (DBG.available() && DBG.read() == 'c') config.set(DBG, DBG);
	config.print(DBG);
	DBG.println("Starting...");
	wifi.begin();
	DBG.println("Module is ready.");
  
	wifi.sendAndWait("AT+GMR", "OK");   // Retrieves the firmware ID (version number) of the module. 
	wifi.sendAndWait("AT+CWMODE?","OK");// Get module access mode. 
	wifi.sendAndWait("AT+CWMODE=1", "");    // Station mode
	wifi.connectToAP(config.ssid, config.wifi_password);
}

void loop()
{
	ESP8266::status_t status;
	DBG.println(millis());
	if ((status=wifi.connectToServer(0, "TCP", "api.thingspeak.com", 80)) != ESP8266::OK)
	{
		DBG.print(status);
		DBG.println("Connect to server failed");
		return;
	}

	String data = "GET /update?key=" + String(config.api_key) + "&field1=" + String(millis());
	data += " HTTP/1.1\r\nHost: api.thingspeak.com\r\n\r\n";
	wifi.sendNetworkData(0, data);
	wifi.skip();
	wifi.skip();
	wifi.skip();
	wifi.closeConnection(0);
  delay(60*1000);
}

// vim: ts=2 sw=2 
