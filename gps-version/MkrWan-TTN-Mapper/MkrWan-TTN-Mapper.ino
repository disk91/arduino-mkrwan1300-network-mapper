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

#define COLOR_BLACK  0x000000
#define COLOR_BLUE   0x0000FF
#define COLOR_RED    0xFF0000
#define COLOR_GREEN  0x00FF00


uint32_t previousLedValue;
void setColor(uint32_t color, boolean save) {
  int r = (color >> 16) & 0xff;
  int g = (color >> 8) & 0xff;
  int b = (color ) & 0xff;

  if ( r == 0xff ) digitalWrite(LRED,LOW);
  else if ( r == 0x00 ) digitalWrite(LRED,HIGH);
  else analogWrite(LRED,256-r);

  if ( g == 0xff ) digitalWrite(LGREEN,LOW);
  else if ( g == 0x00 ) digitalWrite(LGREEN,HIGH);
  else analogWrite(LGREEN,256-g);

  if ( b == 0xff ) digitalWrite(LBLUE,LOW);
  else if ( b == 0x00 ) digitalWrite(LBLUE,HIGH);
  else analogWrite(LBLUE,256-b);

  if ( save ) previousLedValue=color;
}

void restoreColor() {
  setColor(previousLedValue,false);
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
  setColor(COLOR_BLUE,true);
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
      setColor(COLOR_GREEN,true);
    } else {
      setColor(COLOR_RED,true);
    }
  }
  if ( ! gps.location.isValid() || !gps.altitude.isValid() ) {
    // Wait for GPS to be positionned
    setColor(COLOR_BLACK,false);
    delayWithGps(1000);
    for ( int i = 0 ; i < gps.satellites.value()+1 ; i++ ) {
      setColor(COLOR_BLACK,false);
      delayWithGps(150);
      restoreColor();
      delayWithGps(100);  
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
        Serial.print(", alt: ");Serial.print(altitude);
        Serial.print(", hdop:");Serial.print(hdop);
        Serial.print(", sat:");Serial.println(gps.satellites.value());
        msg[0]=0;
        msg[1]=((altitude)>>8)&0xFF; msg[2]=((altitude)&0xFF);
        msg[3]=hdop/10;
        msg[4]=(lat>>16) & 0xFF; msg[5]=(lat>>8) & 0xFF;msg[6]=(lat) & 0xFF;
        msg[7]=(lng>>16) & 0xFF; msg[8]=(lng>>8) & 0xFF;msg[9]=(lng) & 0xFF;     
      } 
      setColor(COLOR_BLACK,false);
      modem.beginPacket();
      modem.write(msg,10);
      boolean toBeConfirmed = (nextConfirmation < millis());
      int err = modem.endPacket(toBeConfirmed);
      if ( err <= 0 ) {
        // Should only be here when in confirmation mode
        // with an error
        setColor(COLOR_RED,true);
        err_count++;
        if ( err_count > 50 ) {
          connected = false;
          setColor(COLOR_BLUE,true);
        }
        nextCommunication=millis()+20000L; // wait for 20 seconds
        nextConfirmation=millis()+300000L; // wait for 5 minutes - do not want to spam in SF12
      } else {
        delayWithGps(200); 
        if (toBeConfirmed) {
          err_count = 0;
          setColor(COLOR_GREEN,true);
          nextConfirmation = millis()+30000L; // Next confirmation in 30 seconds
        } else {
          restoreColor();
        }
        nextCommunication=millis()+10000L; // wait for 10 seconds.
      }
    }
  }
  processGps();
}
