/*
 * File : Ultimate_GPS.h
 *
 * Version : 0.8.1
 *
 * Purpose : Ultimate GPS V3 (http://www.adafruit.com) interface library for Arduino
 *
 * Author : Previmeteo (www.previmeteo.com)
 *
 * Project web site : http://oses.previmeteo.com/
 *
 * License: GNU GPL v2 (see License.txt)
 *
 * Creation date : 2014/02/20
 *
 * History :
 *
 * - 0.8.1 : bug fixes in the acquireNewPosition() and getFieldContentFromNMEASentence() methods
 * 
 */



#ifndef ULTIMATE_GPS_h
#define ULTIMATE_GPS_h



#include "Arduino.h"

#include "SoftwareSerial.h"



#define GPS_NMEA_SENTENCE_BUFFER_SIZE 101


# define DEBUG_MODE true



struct Position {
  
  float latitude;                             // decimal degrees
  float longitude;                            // decimal degrees
  float altitudeAboveMSL;                     // meters
  float horizontalDilutionOfPrecision;        
  
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
    
    boolean _debugSerialConnectionEnabled;
    
    void getFieldContentFromNMEASentence(char *nmeaSentence, char *fieldData, byte fieldIndex);
    
    boolean isSentenceChecksumOK(char *sentence);
    
    char hexCharToChar(char n);

};



#endif