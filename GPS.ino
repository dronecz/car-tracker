#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <SD.h>
#include <SPI.h>

SoftwareSerial GPS=SoftwareSerial(D1, D2);

TinyGPSPlus nmea;

void blink(int times){
  for(int i=0;i<times+1;i++){
    digitalWrite(D4, LOW);
    delay(70);
    digitalWrite(D4, HIGH);
    delay(70);
  }
}

char secret[33];

void sendUBX(byte *UBXmsg, byte msgLength) {
  for(int i = 0; i < msgLength; i++) {
    GPS.write(UBXmsg[i]);
    GPS.flush();
  }
  GPS.println();
  GPS.flush();
}

void setupGPS(){
  byte powerSettings[] = {
    0xB5, 0x62, 0x06, 0x3B, 0x2C, 0x00, 0x01, 0x06, 0x00, 0x00, 0x00, 0x90, 0x42, 0x01, 0xE8, 0x03, 0x00, 0x00,
    0x10, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x4F, 0xC1,
    0x03, 0x00, 0x87, 0x02, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x64, 0x40, 0x01, 0x00, 0x8F, 0xDF
  };
  sendUBX(powerSettings, sizeof(powerSettings));
  byte navSettings[] = {
    0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 
    0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x4B, 0x97
  };
  sendUBX(navSettings, sizeof(navSettings));
  byte rateSettings[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xF4, 0x01, 0x01, 0x00, 0x01, 0x00, 0x0B, 0x77};
  sendUBX(rateSettings, sizeof(rateSettings));
  delay(100);
  GPS.flush();
}

void sleepyGPS(){
  byte powerSettings[] = {
    0xB5, 0x62, 0x06, 0x3B, 0x2C, 0x00, 0x01, 0x06, 0x00, 0x00, 0x00, 0x90, 0x40, 0x01, 0x80, 0xEE, 0x36, 0x00,
    0x10, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x4F, 0xC1,
    0x03, 0x00, 0x87, 0x02, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x64, 0x40, 0x01, 0x00, 0x8F, 0xDF
  };
  sendUBX(powerSettings, sizeof(powerSettings));
  delay(100);
  GPS.flush();
}

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println("Debugging started");

  // Input for user button
  pinMode(D3, INPUT_PULLUP);

  // Softserial pins for GPS module
  pinMode(D1, INPUT);
  pinMode(D2, OUTPUT);
  
  // LED on the ESP-12e
  pinMode(D4, OUTPUT);

  // Start Softserial for GPS
  GPS.begin(9600);
  GPS.enableRx(true);
  Serial.println("SoftSerial started");
  Serial.println("Sending gps settings");
  setupGPS();
  Serial.println("Setttings done");

  // Check if button is pressed on boot
  if(!digitalRead(D3)){
    while(true){
      if(Serial.available()){
        GPS.write(Serial.read());
      }
      if(GPS.available()){
        Serial.write(GPS.read());
      }
      delay(0); 
    }
  }

  Serial.println("Starting SD");
  if(SD.begin(SS)){
    Serial.println("SD Init success");
    blink(4);
  }else{
    Serial.println("SD Init failure");
    blink(8);
    ESP.deepSleep(0);
  }

  File secretFile = SD.open("secret.txt");
  secret[32]= '\0';
  secretFile.read(secret, 32);
  secretFile.close();
  Serial.printf("Secret: %s\n", secret);
}

bool connect_wifi(){
  Serial.println("Connecting to wifi network");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("Scanning nearby networks");
  int scanCount = WiFi.scanNetworks();

  Serial.println("Loading possible networks");
  File wifiFile = SD.open("wifi.txt");
  char fileSSID[40];
  char fileKEY[40];
  int charIndex = 0;
  bool inSSID = true;
  bool matchFound = false;
  while(!matchFound){
    int fileChar = wifiFile.read();
    if(fileChar < 0){
      break;
    }
    if(fileChar == '\n'){
      fileKEY[charIndex]=(char)0;
      charIndex = 0;
      inSSID = true;

      for(int i=0;i<scanCount;i++){
        Serial.printf("Compare against %s\n", WiFi.SSID(i).c_str());
        if(strcmp(fileSSID, WiFi.SSID(i).c_str())==0){
          Serial.printf("Found matching network: %s\n", fileSSID);
          matchFound = true;
          WiFi.begin(fileSSID, fileKEY);
          break;
        }
      }
      
    }else if(fileChar == ':'){
      inSSID = false;
      fileSSID[charIndex]=(char)0;
      Serial.printf("Possible SSID: %s\n", fileSSID);
      charIndex = 0;
    }else{
      if(inSSID){
        fileSSID[charIndex++]=(char)fileChar;
      }else{
        fileKEY[charIndex++]=(char)fileChar;
      }
    }
  }
  
  if(matchFound){
    int timeout = 2*30;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      if(--timeout == 0){
        Serial.println("Couldn't connect to the SSID");
        return false;
      }
    }
    Serial.println(WiFi.localIP());
    
  }else{
    Serial.println("No network available");
  }

  return matchFound;
}

void queueFile(const char* name){
  Serial.printf("Queueing %s\n", name);
  File queue = SD.open("queue.txt", FILE_WRITE);
  //queue.seek(queue.size());
  queue.printf("%s\n", name);
  queue.flush();
  queue.close();
}

void uploadTrack(char* name){
  File log = SD.open(name);
  WiFiClient client;
  int tracker = ESP.getChipId();
  String trackerId = String(tracker);
  Serial.printf("Uploading %s\n", name);
  if(client.connect("tracker.brixit.nl", 80)){
    Serial.println("Sending headers");
    String length = String(log.size());
    String headers = String("POST /upload.php HTTP/1.0\r\n")+
      "Host: tracker.brixit.nl\r\n" +
      "Connection: close\r\n" + 
      "Content-Type: text/csv\r\n" + 
      "Content-Length: " + length + "\r\n" +
      "X-Tracker: " + trackerId + "\r\n" +
      "X-Secret: " + secret + "\r\n" + 
      "X-Track: " + name + "\r\n" +
      "\r\n";
    client.print(headers);
    Serial.print(headers);
    Serial.println("Sending data");

    char buffer[512];
    while(true){
      int bytes = log.read(buffer,512);
      if(bytes < 1){
        break;
      }
      client.write_P(buffer, bytes);
      delay(1);
    }
    Serial.println("Upload done");
    client.stop();
  }

  
}

void uploadQueue(){
  File queue = SD.open("queue.txt");
  char filename[17];
  char charIndex = 0;
  Serial.println("Starting upload queue");
  while(true){
    int fileChar = queue.read();
    if(fileChar < 0){
      Serial.println("Reached EOF");
      break;
    }
    if(fileChar == '\n'){
      filename[charIndex] = (char)0;
      charIndex = 0;
      uploadTrack(filename);
    }else{
      filename[charIndex++]=(char)fileChar;
    }
  }
  queue.close();
  SD.remove("queue.txt");
}

void fmtDouble(double val, byte precision, char *buf, unsigned bufLen = 0xffff);
unsigned fmtUnsigned(unsigned long val, char *buf, unsigned bufLen = 0xffff, byte width = 0);

void loop() {
  blink(4);
  Serial.println("Starting to parse nmea messages");
  bool locked = false;
  int lastSats = 0;
  File logfile;
  String name;

  bool driving = true;
  
  while(driving){
    delay(10);
    while(GPS.available() && driving){

      if(!digitalRead(D3)){
        driving=false;
        queueFile(name.c_str());
      }
      int gpsChar = GPS.read();
      //Serial.write(gpsChar);
      if(nmea.encode(gpsChar) && nmea.location.isUpdated()){
          if(!locked && nmea.location.isValid()){
            Serial.println("Got GPS fix");
            name = String(nmea.time.value());
            
            logfile = SD.open(name.c_str(), FILE_WRITE);
            logfile.print("date,time,latitude,longitude,speed\n");
            
            locked = true;
          }
          if(locked && !nmea.location.isValid()){
            Serial.println("Lost GPS fix");
            logfile.close();
            queueFile(name.c_str());
            locked = false;
          }
        
          if(locked){
            char lat[20];
            char lng[20];
            char speed[20];
            fmtDouble(nmea.location.lat(), 10, lat);
            fmtDouble(nmea.location.lng(), 10, lng);
            fmtDouble(nmea.speed.kmph(),2, speed);
            logfile.printf("%d,%d,%s,%s,%s\n", nmea.date.value(), nmea.time.value(), lat, lng, speed);
            logfile.flush();
            if(!driving){
              logfile.close();
            }
          }else{
            if(nmea.satellites.value() > lastSats){
              lastSats = nmea.satellites.value();
              Serial.printf("Got new sattelite. Now %d\n", lastSats);
            }
          }
      }
    }
  }
  GPS.enableRx(false);

  blink(1);
  Serial.println("Track done");
  if(connect_wifi()){
    uploadQueue();
  }
  blink(1);
  sleepyGPS();
  ESP.deepSleep(0);
}


//
// Produce a formatted string in a buffer corresponding to the value provided.
// If the 'width' parameter is non-zero, the value will be padded with leading
// zeroes to achieve the specified width.  The number of characters added to
// the buffer (not including the null termination) is returned.
//
unsigned
fmtUnsigned(unsigned long val, char *buf, unsigned bufLen, byte width)
{
 if (!buf || !bufLen)
   return(0);

 // produce the digit string (backwards in the digit buffer)
 char dbuf[10];
 unsigned idx = 0;
 while (idx < sizeof(dbuf))
 {
   dbuf[idx++] = (val % 10) + '0';
   if ((val /= 10) == 0)
     break;
 }

 // copy the optional leading zeroes and digits to the target buffer
 unsigned len = 0;
 byte padding = (width > idx) ? width - idx : 0;
 char c = '0';
 while ((--bufLen > 0) && (idx || padding))
 {
   if (padding)
     padding--;
   else
     c = dbuf[--idx];
   *buf++ = c;
   len++;
 }

 // add the null termination
 *buf = '\0';
 return(len);
}

//
// Format a floating point value with number of decimal places.
// The 'precision' parameter is a number from 0 to 6 indicating the desired decimal places.
// The 'buf' parameter points to a buffer to receive the formatted string.  This must be
// sufficiently large to contain the resulting string.  The buffer's length may be
// optionally specified.  If it is given, the maximum length of the generated string
// will be one less than the specified value.
//
// example: fmtDouble(3.1415, 2, buf); // produces 3.14 (two decimal places)
//
void
fmtDouble(double val, byte precision, char *buf, unsigned bufLen)
{
 if (!buf || !bufLen)
   return;

 // limit the precision to the maximum allowed value
 const byte maxPrecision = 6;
 if (precision > maxPrecision)
   precision = maxPrecision;

 if (--bufLen > 0)
 {
   // check for a negative value
   if (val < 0.0)
   {
     val = -val;
     *buf = '-';
     bufLen--;
   }

   // compute the rounding factor and fractional multiplier
   double roundingFactor = 0.5;
   unsigned long mult = 1;
   for (byte i = 0; i < precision; i++)
   {
     roundingFactor /= 10.0;
     mult *= 10;
   }

   if (bufLen > 0)
   {
     // apply the rounding factor
     val += roundingFactor;

     // add the integral portion to the buffer
     unsigned len = fmtUnsigned((unsigned long)val, buf, bufLen);
     buf += len;
     bufLen -= len;
   }

   // handle the fractional portion
   if ((precision > 0) && (bufLen > 0))
   {
     *buf++ = '.';
     if (--bufLen > 0)
       buf += fmtUnsigned((unsigned long)((val - (unsigned long)val) * mult), buf, bufLen, precision);
   }
 }

 // null-terminate the string
 *buf = '\0';
}

