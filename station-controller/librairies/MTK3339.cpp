/*
 * File : MTK3339.cpp
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



#include "Arduino.h"

#include "SoftwareSerial.h"

#include "MTK3339.h"



MTK3339GpsModule::MTK3339GpsModule(HardwareSerial *gpsSerialConnection, byte onOffPin) { 
  
  _gpsSerialConnection = gpsSerialConnection;
  
  _onOffPin = onOffPin;
  
  firstPositionAcquired = false;
  
}



MTK3339GpsModule::MTK3339GpsModule(HardwareSerial *gpsSerialConnection, byte onOffPin, SoftwareSerial *debugSerialConnection) { 
  
  _gpsSerialConnection = gpsSerialConnection;
  
  _onOffPin = onOffPin;
  
  _debugSerialConnection = debugSerialConnection;
  
  firstPositionAcquired = false;
  
}



void MTK3339GpsModule::init(unsigned long baudRate) {
  
  pinMode(_onOffPin, OUTPUT);    
  digitalWrite(_onOffPin, LOW); 
  
  _gpsSerialConnection->begin(baudRate);
  
  delay(1000);

}



void MTK3339GpsModule::powerOn() {

  digitalWrite(_onOffPin, HIGH);
  
  delay(2000);
  
}
 
 
 
void MTK3339GpsModule::powerOff() {
  
  digitalWrite(_onOffPin, LOW);
  
  delay(500);
  
}

    
    
void MTK3339GpsModule::configure() {
  
  Serial.print("$PMTK220,1000*1F\r\n");                                       // update frequency is set to 1 Hz
  
  Serial.flush();
  
  delay(1000);
  
}



boolean MTK3339GpsModule::acquireNewPosition(float accuracyLimit, int timeoutInS) {
  
  boolean newPositionAcquired = false;
  
  unsigned long newPositionAcquisitionStartMS;
  
  unsigned long timeoutInMS = (unsigned long)timeoutInS * 1000;
  
  unsigned long lineReadStartMS;
  
  char gpsRxBuffer[GPS_NMEA_SENTENCE_BUFFER_SIZE];
  
  byte numOfCharReceived; 
  
  boolean newLineReceived;
  
  boolean newNMEASentenceReceived;
  
  char stringBuffer[12];
  
  float relativeAccuracy;
  
  byte currentDate_Y = 0;
  byte currentDate_M = 0;
  byte currentDate_D = 0;
  
  
  newPositionAcquisitionStartMS = millis();
  
  while (((millis() - newPositionAcquisitionStartMS) < timeoutInMS) and !newPositionAcquired) {
    
 
    // new line reading
  
    newLineReceived = false;
    
    numOfCharReceived = 0;  

    lineReadStartMS = millis();  
    
    while((numOfCharReceived < GPS_NMEA_SENTENCE_BUFFER_SIZE) and !newLineReceived) {
      
      while((millis() - lineReadStartMS < 1500) and !Serial.available());   
      
      char c = Serial.read();
      
      gpsRxBuffer[numOfCharReceived] = c;
      
      if(c == '\n') newLineReceived = true;
      
      numOfCharReceived++;
      
    } 
    
    
    // is the NMEA sentence valid ? 
    
    newNMEASentenceReceived = false;
    
    if(newLineReceived) {
      
      gpsRxBuffer[numOfCharReceived - 2] = '\0';
      gpsRxBuffer[numOfCharReceived - 1] = '\0';
      
      newNMEASentenceReceived = isSentenceChecksumOK(gpsRxBuffer);
      
    }
    
    
    // specific treatments according to the NMEA sentence type
    
    
    if(newNMEASentenceReceived) {      
      
      if(strstr(gpsRxBuffer, "GPRMC") != NULL) {      
        
        // sentence type is GPRMC
        
        getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 9);
        
        byte Y = (stringBuffer[4] - '0') * 10 + (stringBuffer[5] - '0');
          
        if(Y < 80) {
          
          currentDate_Y = Y;
          currentDate_M = (stringBuffer[2] - '0') * 10 + (stringBuffer[3] - '0');
          currentDate_D = (stringBuffer[0] - '0') * 10 + (stringBuffer[1] - '0');

        }
        
      }
     
      
      else if(strstr(gpsRxBuffer, "GPGGA") != NULL) {      
        
        // sentence type is GPGGA
    
        getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 6);                  // stringBuffer -> fix quality string
        
        if((stringBuffer[0] == '1') or (stringBuffer[0] == '2')) {
        
          getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 8);                // stringBuffer -> relative accuracy string
       
          if(strlen(stringBuffer) > 0) {
           
            relativeAccuracy = (float) atof(stringBuffer);
            
            if((relativeAccuracy > 0) and (relativeAccuracy < accuracyLimit)) newPositionAcquired = true;
            
          }
          
        }
    
        if(newPositionAcquired) {
          
          // fix time
      
          getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 1);               //  stringBuffer -> current time string
          
          position.fix_Y_utc = currentDate_Y;
          position.fix_M_utc = currentDate_M;
          position.fix_D_utc = currentDate_D;
          
          position.fix_h_utc = (stringBuffer[0] - '0') * 10 + (stringBuffer[1] - '0');
          position.fix_m_utc = (stringBuffer[2] - '0') * 10 + (stringBuffer[3] - '0');
          position.fix_s_utc = (stringBuffer[4] - '0') * 10 + (stringBuffer[5] - '0');
          
          
          // latitude
          
          getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 2);              
          
          float encodedLatitude = (float) atof(stringBuffer);
          
          position.latitude =  int(encodedLatitude / 100.0) + ((encodedLatitude - int(encodedLatitude / 100.0) * 100) / 60.0);  
        
          getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 3);              
       
          if(stringBuffer[0] == 'S') position.latitude = -position.latitude;
        
  
          // longitude
          
          getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 4);              
          
          float encodedLongitude = (float) atof(stringBuffer); 
          
          position.longitude = int(encodedLongitude / 100.0) + ((encodedLongitude - int(encodedLongitude / 100.0) * 100) / 60.0);  
        
          getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 5);             
     
          if(stringBuffer[0] == 'W') position.longitude = -position.longitude;
         
          
          // heightAboveEllipsoid
          
          getFieldContentFromNMEASentence(gpsRxBuffer, stringBuffer, 11);              
          
          position.heightAboveEllipsoid = (float) atof(stringBuffer);        
          
          position.relativeAccuracy = relativeAccuracy;
   
  
          if(!firstPositionAcquired) firstPositionAcquired = true;
      
    
        }  
  
        
      } 
      
      
    }
    
    
  }
  
  return newPositionAcquired;
  
}


    
void MTK3339GpsModule::getFieldContentFromNMEASentence(char *nmeaSentence, char *fieldData, byte fieldIndex) {
 
  char fieldDelimiter = ',';

  boolean fieldFound = false;

  byte charIndex = 0;
  byte fieldCounter = 0;

  byte fieldStartIndex = 0;
  byte fieldDataLength = sizeof(nmeaSentence) - 1;
  
  while((nmeaSentence[charIndex] != '\0') and (charIndex < GPS_NMEA_SENTENCE_BUFFER_SIZE) and (!fieldFound)) {
    
    if(nmeaSentence[charIndex] == fieldDelimiter) {
      
      if(fieldCounter == 0) fieldDataLength = charIndex;
      
      else {
        
        fieldStartIndex = fieldStartIndex + fieldDataLength + 1;
        fieldDataLength = charIndex - fieldStartIndex;
        
      }

      if (fieldCounter == fieldIndex) fieldFound = true;

      fieldCounter++; 

    }
    
    charIndex++;

  }  
  
  if(fieldFound) {
    
    char* fieldStartPointer = nmeaSentence + fieldStartIndex;
    
    strncpy(fieldData, fieldStartPointer, fieldDataLength); 
      
    fieldData[fieldDataLength] = '\0';
  
  }
  
  else {
    
    fieldData[0] = '\0';
    
  }
  
}
    
    
 
boolean MTK3339GpsModule::isSentenceChecksumOK(char *sentence) {
  
  boolean checkResult = false;
  
  byte sentenceLength = 0;
  
  while((sentence[sentenceLength] != '\0') and (sentenceLength < GPS_NMEA_SENTENCE_BUFFER_SIZE)) sentenceLength++;

  char c;
  
  char computedChecksum = 0;

  for (byte i = 1; i < (sentenceLength - 3); i++) {              
    
    c = sentence[i];
    computedChecksum = char(computedChecksum ^ c);
    
  } 

  char payloadChecksum = hexCharToChar(sentence[sentenceLength - 2]) * 16 + hexCharToChar(sentence[sentenceLength - 1]);
  
  if(computedChecksum == payloadChecksum) checkResult = true;
  
  return checkResult;
  
}
    
    
    
   
    
char MTK3339GpsModule::hexCharToChar(char n) {
  
    if (n >= '0' && n <= '9') return (n - '0');
    
    else if (n >= 'A' && n <= 'F') return (n - 'A' + 10);
    
    else return -1;   
}
    
    




