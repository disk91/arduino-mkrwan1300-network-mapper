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
unsigned long  nextConfirmation;
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
  nextConfirmation=0;

  // Init Leds GPIOs
  pinMode(LBLUE,OUTPUT);
  pinMode(LGREEN,OUTPUT);
  pinMode(LRED,OUTPUT);
  pinMode(BOARDLED,OUTPUT);
  setColor(HIGH,HIGH,LOW);
  Serial.println("Go on!");
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
  }
}

void loop() {
  char msg[10] = {0,1,2,3,4,5,6,7,8,9};

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
      nextConfirmation=millis()+10000;
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
      long lat = (long)( ((double)gps.location.lat() + 90.0) * 93206.75 );
      long lng = (long)( ((double)gps.location.lng() + 180.0) * 46603.375 );
      int altitude = gps.altitude.value()/100;
      if ( altitude < 0 || altitude > 1000 ) altitude = 500; // TinyGPS with L80 altitude is strange.. bad is better than crazy
      int  hdop= gps.hdop.value();
      if ( hdop > 900 ) {
        Serial.print("Position invalid -");
        Serial.print(" sat:");Serial.println(gps.satellites.value());
        msg[0]=1;
      } else {
        Serial.print("lat: ");Serial.print(lat);
        Serial.print(", lng: ");Serial.print(lng);
        Serial.print(", alt_src: ");Serial.print(gps.altitude.value());
        Serial.print(", alt: ");Serial.print(altitude);
        Serial.print(", hdop:");Serial.print(hdop);
        Serial.print(", sat:");Serial.println(gps.satellites.value());
        msg[0]=0;
        msg[1]=((altitude)>>8)&0xFF; msg[2]=((altitude)&0xFF);
        msg[3]=hdop/10;
        msg[4]=(lat>>16) & 0xFF; msg[5]=(lat>>8) & 0xFF;msg[6]=(lat) & 0xFF;
        msg[7]=(lng>>16) & 0xFF; msg[8]=(lng>>8) & 0xFF;msg[9]=(lng) & 0xFF;     
      } 
      setColor(HIGH,HIGH,HIGH);
      modem.beginPacket();
      modem.write(msg,10);
      int err = modem.endPacket((nextConfirmation < millis()));
      if ( err <= 0 ) {
        setColor(LOW,HIGH,HIGH);
        err_count++;
        if ( err_count > 50 ) {
          connected = false;
          setColor(HIGH,HIGH,LOW);
        }
        nextCommunication=millis()+20000L; // wait for 20 seconds
        nextConfirmation=millis()+180000L; // wait for 8 minutes - do not want to spam in SF12
      } else {
        delay(50);
        setColor(HIGH,LOW,HIGH);
        err_count = 0;
        nextCommunication=millis()+10000L; // wait for 10 seconds.
        if ( nextConfirmation < millis() ) { // confirmation every 30 seconds
          nextConfirmation = millis()+30000L;
        }
      }
    }
  }
  processGps();
}
