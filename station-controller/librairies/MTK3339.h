/*
 * File : MTK3339.h
 *
 * Version : 0.1.0
 *
 * Purpose : MTK3339 GPS module library for Arduino
 *
 * Company : Previmeteo
 *
 * Web site : http://oses.previmeteo.com
 *
 * License: GNU GPL v2 (see License.txt)
 *
 * Creation date : 2013/10/08
 *
 * History :
 * 
 */



#ifndef MTK3339_h
#define MTK3339_h



#include "Arduino.h"

#include "SoftwareSerial.h"



#define GPS_NMEA_SENTENCE_BUFFER_SIZE 100




struct Position {
  
  float latitude;                   // decimal degrees
  float longitude;                  // decimal degrees
  float heightAboveEllipsoid;       // meters
  float relativeAccuracy;           // meters
  
  byte fix_Y_utc;
  byte fix_M_utc;
  byte fix_D_utc;
  byte fix_h_utc;
  byte fix_m_utc;
  byte fix_s_utc;
  
};




class MTK3339GpsModule {

  public:

    MTK3339GpsModule(HardwareSerial *gpsSerialConnection, byte onOffPin);

    MTK3339GpsModule(HardwareSerial *gpsSerialConnection, byte onOffPin, SoftwareSerial *debugSerialConnection);
    
    void init(unsigned long baudRate);
    
    boolean firstPositionAcquired;
    
    Position position;
        
    void powerOn();
   
    void powerOff();
        
    void configure();
    
    boolean acquireNewPosition(float accuracyLimit, int timeoutInS);
    

  private:
  
    HardwareSerial *_gpsSerialConnection;
    
    byte _onOffPin;
    
    SoftwareSerial *_debugSerialConnection;
    
    void getFieldContentFromNMEASentence(char *nmeaSentence, char *fieldData, byte fieldIndex);
    
    boolean isSentenceChecksumOK(char *sentence);
    
    char hexCharToChar(char n);

};



#endif