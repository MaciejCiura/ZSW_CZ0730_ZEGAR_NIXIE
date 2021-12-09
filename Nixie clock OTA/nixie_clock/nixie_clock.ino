#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "timeLib.h"
#include "RTClib.h"

//---- shift_register
#define latchPin 12
#define clockPin 14
#define dataPin  13

//---- RTC
RTC_DS1307 rtc;

//---- WiFi
#define STASSID "8level-Net"
#define STAPSK  "supertajnehaslo"

const char * ssid = STASSID; 
const char * pass = STAPSK;  

unsigned int localPort = 2390;      

IPAddress timeServerIP; 
const char* ntpServerName = "pool.ntp.org";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

WiFiUDP udp;

int h, m, s;

void setup() {
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);  
  pinMode(clockPin, OUTPUT);
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  rtc.begin();
  if(rtc.isrunning())
    nixie_display(int(rtc.now().hour()), int(rtc.now().minute()));
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);


  int i=0;
  while(WiFi.status() != WL_CONNECTED && i++ < 20)
  {
    delay(500);
    Serial.print(".");
  }    
    
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  updateTime();


  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void acp()
{
  for(int i=0; i<100; i+=11)
  {
    nixie_display(i, i);
    for(int j=0; j<15; j++)
    {
      delay(1000);
      ArduinoOTA.handle();
    }
    delay(45000);
  }
}

void loop() {
 
    
    DateTime now = rtc.now();
    nixie_display(int(now.hour()), int(now.minute()));
    if(int(now.hour())%12 == 0 && int(now.minute()) == 0 && int(now.second()) <= 5) {
        updateTime();
      }
    delay(1000);
    if(int(now.minute()) % 60 == 0 && int(now.second()) < 10)
      {
       nixie_display(int(now.day()), int(now.month()));
       delay (10000);
       acp();
      }
    ArduinoOTA.handle();
} 

void registerWrite(uint16_t byteToSend) {
  digitalWrite(latchPin, LOW);
  Serial.println(byteToSend);
  shiftOut(dataPin, clockPin, MSBFIRST, byteToSend);
  byteToSend >>= 8;
  Serial.println(byteToSend);
  shiftOut(dataPin, clockPin, MSBFIRST, byteToSend);
  digitalWrite(latchPin, HIGH);
}

void nixie_display(int first_byte, int second_byte) {
  registerWrite (toBinary ( first_byte, second_byte));
}
/*
07:30

0 0 0 0  0 1 1 1     0 0 1 1  0 0 0 0  
_ _ _ _  _ _ _ _     _ _ _ _  _ _ _ _
\--------------/ --- \--------------/
   rejestr 1             rejestr 2

*/

uint16_t toBinary(int h, int m) {
  uint16_t time_bin = 0;
  byte h_bin = 0;
  byte m_bin = 0;
    
  m_bin += m/10;
  m = m % 10;
  m <<= 4;
  m_bin += m;
  
  h_bin += h/10;
  h = h % 10;
  h <<= 4;
  h_bin += h;
  
  time_bin = h_bin;
  time_bin <<= 8;
  time_bin += m_bin;
  return time_bin;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E; 
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void updateTime(){
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available

  delay(500);
  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
    
  } else {
    Serial.print("packet received, length=");
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
  
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears + 3600;
    
    rtc.adjust(DateTime(year(epoch),month(epoch),day(epoch),hour(epoch) ,minute(epoch),second(epoch))); 
  }
}
