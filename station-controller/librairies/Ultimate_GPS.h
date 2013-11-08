/*
 * File : Ultimate_GPS.h
 *
 * Version : 0.5.0
 *
 * Purpose : Ultimate GPS V3 interface library for Arduino
 *
 * Author : Previmeteo (www.previmeteo.com)
 *
 * Project web site : http://oses.previmeteo.com/
 *
 * License: GNU GPL v2 (see License.txt)
 *
 * Creation date : 2013/10/08
 *
 * History :
 * 
 */



#ifndef ULTIMATE_GPS_h
#define ULTIMATE_GPS_h



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




class UltimateGPS {

  public:

    UltimateGPS(HardwareSerial *gpsSerialConnection, byte onOffPin);

    UltimateGPS(HardwareSerial *gpsSerialConnection, byte onOffPin, SoftwareSerial *debugSerialConnection);
    
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