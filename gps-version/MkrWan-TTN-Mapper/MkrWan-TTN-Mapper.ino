#include <MKRWAN.h>
#include <TinyGPS++.h>

LoRaModem modem;
TinyGPSPlus gps;

#include "arduino_secrets.h" 
// Please enter your sensitive data in the Secret tab or arduino_secrets.h
String appEui = SECRET_APP_EUI;
String appKey = SECRET_APP_KEY;

#define LBLUE 5
#define LGREEN 4
#define LRED 3
#define BOARDLED 6

bool           connected;
unsigned long  lastConnectionTry;
unsigned long  nextCommunication;
int err_count;

void setColor(int r,int g,int b) {
  digitalWrite(LBLUE,b);
  digitalWrite(LGREEN,g);
  digitalWrite(LRED,r);
}

void setup() {
  // Debug Serial port
  Serial.begin(115200);  
  
  // GPS serial port
  Serial1.begin(9600);

  // Init LoRaWan modem
  modem.begin(EU868);
  delay(1000);      // apparently the murata dislike if this tempo is removed...
  connected=false;
  err_count=0;
  lastConnectionTry=0;

  // Init Leds GPIOs
  pinMode(LBLUE,OUTPUT);
  pinMode(LGREEN,OUTPUT);
  pinMode(LRED,OUTPUT);
  pinMode(BOARDLED,OUTPUT);
  setColor(HIGH,HIGH,LOW);
}

void processGps() {
  // Process the Gps data
  while ( Serial1.available() ) {
    char c =Serial1.read();
    //Serial.print(c); 
    gps.encode(c);
  }  
}

void delayWithGps(unsigned long durationMs){
  unsigned long start=millis();
  while ( millis() < (start+durationMs) ) {
    processGps();
    delay(1);
  }
}

void loop() {
  char msg[12] = {0,1,2,3,4,5,6,7,8,9,10,11};

  if ( !connected && ( lastConnectionTry == 0 || lastConnectionTry < millis() + 60000L ) ) {
    lastConnectionTry = millis();
    int ret=modem.joinOTAA(appEui, appKey);
    if ( ret ) {
      connected=true;
      modem.minPollInterval(60);
      modem.dataRate(5);
      delayWithGps(100);
      err_count=0;
      nextCommunication=millis()+10000; // + 10s
    } else {
      setColor(LOW,HIGH,HIGH);
    }
  }
  if ( ! gps.location.isValid() || !gps.altitude.isValid() ) {
    // Wait for GPS to be positionned
    setColor(HIGH,HIGH,HIGH);
    delayWithGps(1000);
    for ( int i = 0 ; i < gps.satellites.value()+1 ; i++ ) {
      setColor(HIGH,HIGH,HIGH);
      delayWithGps(150);
      setColor(HIGH,(connected)?LOW:HIGH,(connected)?HIGH:LOW);
      delayWithGps(150);  
    }
  } else {
    // GPS Position OK
    if ( connected && nextCommunication <= millis() ) {
      long lat = gps.location.rawLat().deg * 1000000;
           lat += gps.location.rawLat().billionths / 1000;
           lat += 100000000;
      long lng = gps.location.rawLng().deg * 1000000;
           lng += gps.location.rawLng().billionths / 1000;
           lng += 100000000;
      int  altitude = -gps.altitude.value()/10;
      int  hdop= gps.hdop.value();
      if ( hdop > 900 ) {
        Serial.print("Position invalid -");
        Serial.print(" sat:");Serial.println(gps.satellites.value());
        msg[0]=1;
      } else {
        Serial.print("lat: ");Serial.print(lat);
        Serial.print(", lng: ");Serial.print(lng);
        Serial.print(", alt: ");Serial.print(altitude);
        Serial.print(", hdop:");Serial.print(hdop);
        Serial.print(", sat:");Serial.println(gps.satellites.value());
        msg[0]=0;
        msg[1]=((altitude)>>8)&0xFF; msg[2]=((altitude)&0xFF);
        msg[3]=hdop/10;
        msg[4]=(lat>>24) & 0xFF; msg[5]=(lat>>16) & 0xFF; msg[6]=(lat>>8) & 0xFF;msg[7]=(lat) & 0xFF;
        msg[8]=(lng>>24) & 0xFF; msg[9]=(lng>>16) & 0xFF; msg[10]=(lng>>8) & 0xFF;msg[11]=(lng) & 0xFF;     
      } 
      modem.beginPacket();
      modem.write(msg,12);
      int err = modem.endPacket(true);
      if ( err <= 0 ) {
        setColor(LOW,HIGH,HIGH);
        err_count++;
        if ( err_count > 50 ) {
          connected = false;
          setColor(HIGH,HIGH,LOW);
        }
        nextCommunication=millis()+120000L; // wait for 2 minutes.
      } else {
        setColor(HIGH,LOW,HIGH);
        err_count = 0;
        nextCommunication=millis()+10000L; // wait for 10 seconds.
      }
    }
  }
  processGps();
}
