/*
 * File : GL865Quad.cpp
 *
 * Version : 0.1.0
 *
 * Purpose : Telit GL865Quad modem library for Arduino
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

#include "GL865Quad.h"



GL865QuadModem::GL865QuadModem(byte onOffPin, byte rxPin, byte txPin):_modemSerialConnection(rxPin, txPin) { 
    
  _onOffPin = onOffPin;
  
}



GL865QuadModem::GL865QuadModem(byte onOffPin, byte rxPin, byte txPin, SoftwareSerial *debugSerialConnector):_modemSerialConnection(rxPin, txPin) { 
    
  _onOffPin = onOffPin;
  
  _debugSerialConnection = debugSerialConnector;
  
}



void GL865QuadModem::init(unsigned long baudRate) {
  
  pinMode(_onOffPin, OUTPUT);    
  digitalWrite(_onOffPin, HIGH); 
  
  _modemSerialConnection.begin(baudRate);
  _modemSerialConnection.listen();
  
  delay(1000);

}



void GL865QuadModem::powerOn() {
  
  digitalWrite(_onOffPin, LOW);
  
  delay(3000);
  
}



void GL865QuadModem::powerOff() {
  
  digitalWrite(_onOffPin, HIGH);
  
  delay(500);
  
}



void GL865QuadModem::activateCommunication() {
  
  request(F("AT"), 2, 3000);
  
  request(F("ATE0"), 2, 2000);         // echo desactivation
  
}
    


boolean GL865QuadModem::isCommunicationActivated() {
  
  boolean communicationActivated = false;
  
  request(F("AT"), 2, 1000);

  if(strstr(atRxBuffer, "OK") != NULL) communicationActivated = true;  
 
  return communicationActivated;
  
}


    
void GL865QuadModem::configure() {
    
  // request(F("AT+CMEE=2"), 2, 2000);    // error reporting in verbose format
  
  request(F("AT&K0"), 2, 2000);           // no flow control on serial port
  
}



void GL865QuadModem::enterPINCode(char *pinCode) {

  char reqBuffer[14];
  
  strcpy(reqBuffer, "AT+CPIN=");
  strcat(reqBuffer, pinCode);
  request(reqBuffer, 2, 2000);

}



boolean GL865QuadModem::isRegistered() {

  boolean registered = false;
  
  request(F("AT+CREG?"), 2, 500);
  
  if(strstr(atRxBuffer, "0,1") != NULL) registered = true;                 // expected response : "CREG: 0,1"
 
  return registered;
  
}



void GL865QuadModem::attachGPRS() {

  request(F("AT+CGATT=1"), 2, 5000);  

}
 
 

void GL865QuadModem::dettachGPRS() {

  request(F("AT+CGATT=0"), 2, 5000);
  
}



boolean GL865QuadModem::isGPRSAttached() {
  
  boolean GPRSAttached = false;
  
  request(F("AT+CGATT?"), 2, 3000);
    
  if(strstr(atRxBuffer, ": 1") != NULL) GPRSAttached = true;        // expected response : "CGATT: 1"
  
  return GPRSAttached;

}



void GL865QuadModem::activatePDP(char *networkAPN, char *username, char *password) {
  
  char reqBuffer[50];

  strcpy(reqBuffer, "AT+CGDCONT=1,\"IP\",\"");
  strcat(reqBuffer, networkAPN);
  strcat(reqBuffer, "\",\"0.0.0.0\",0,0");
  
  request(reqBuffer, 2, 2000);
  
  strcpy(reqBuffer, "AT#SGACT=1,1,\"");
  strcat(reqBuffer, username);
  strcat(reqBuffer, "\",\"");
  strcat(reqBuffer, password);
  strcat(reqBuffer, "\"");
  
  request(reqBuffer, 2, 10000);

}



void GL865QuadModem::desactivatePDP() {

  request(F("AT#SGACT=1,0"), 2, 5000); 
  
}   



boolean GL865QuadModem::isPDPActivated() {

  boolean PDPActivated = false;
  
  request(F("AT#SGACT?"), 2, 1000); 
  
  if(strstr(atRxBuffer, "1,1") != NULL) PDPActivated = true;                 // expected response : "SGACT: 1,1"
  
  return PDPActivated;

}



boolean GL865QuadModem::httpPost(char *serverName, char *serverPort, char *serverPostURL, char *encodedData, byte maxNumConnectAttempts) {
  
  boolean requestSuccess = false; 
  
  byte encodedDataLength = 0;
  
  while(encodedData[encodedDataLength] != '\0') {
    encodedDataLength++;
  }
  
  boolean connected = false;
  
  char connectRequestBuffer[50];
  
  strcpy(connectRequestBuffer, "AT#SD=1,0,");
  strcat(connectRequestBuffer, serverPort);
  strcat(connectRequestBuffer, ",\"");
  strcat(connectRequestBuffer, serverName);
  strcat(connectRequestBuffer, "\",0,0");
  
  for(byte attemptsConnect=0 ; attemptsConnect < maxNumConnectAttempts ; attemptsConnect++) {
      
    request(connectRequestBuffer, 2, 15000);
      
    if(strstr(atRxBuffer, "CONN") != NULL) connected = true;              // expected response : "CONNECT"
      
    if(connected) break;
    
  }

  if(connected) {
    
    byte numOfCharReceived = 0;
    
    byte numOfLines = 1;
      
    char serverResponseBuffer[50];  
    
    delay(200);
    
    char postRequestBuffer[80];
    
    strcpy(postRequestBuffer, "POST ");
    strcat(postRequestBuffer, serverPostURL);
    strcat(postRequestBuffer, " HTTP/1.1\r\n");
        
    _modemSerialConnection.print(postRequestBuffer);  
    
    strcpy(postRequestBuffer, "HOST: ");
    strcat(postRequestBuffer, serverName);
    strcat(postRequestBuffer, "\r\n");
    
    _modemSerialConnection.print(postRequestBuffer); 
    
    _modemSerialConnection.print(F("Content-Type: application/x-www-form-urlencoded\r\n"));
    _modemSerialConnection.print(F("Content-Length: "));
    _modemSerialConnection.print(encodedDataLength);
    _modemSerialConnection.print(F("\r\n")); 
    _modemSerialConnection.print(F("\r\n")); 
    _modemSerialConnection.print(encodedData); 
  
    _modemSerialConnection.flush(); 
    
    unsigned long clockTimeOut = millis() + 30000;
    
    while ((millis() < clockTimeOut) && (numOfLines < 2)) {  
      
      if(_modemSerialConnection.available() > 0) {
         
         char c = _modemSerialConnection.read(); 
         
         serverResponseBuffer[numOfCharReceived] = c;
         numOfCharReceived++;
         
         if(c=='\n') numOfLines += 1;
         
      }
      
      serverResponseBuffer[numOfCharReceived] = '\0';
      
    }
    
    if(strstr(serverResponseBuffer, "200") != NULL) requestSuccess = true;  
    
    _modemSerialConnection.print(F("+++"));
    
    delay(3000);
    
    request(F("AT#SH=1"), 2, 5000);

  }
  
  return requestSuccess;
  
}



void GL865QuadModem::request(const char *command, byte respMaxNumOflines, unsigned int timeOutInMS) {

  byte numOfCharReceived = 0;
  
  byte numOfLines = 0;

  _modemSerialConnection.flush(); 
  
  _modemSerialConnection.println(command);                
  
  unsigned long clockTimeOut = millis() + timeOutInMS;
  
  while ((millis() < clockTimeOut) && (numOfLines < respMaxNumOflines)) {  
    
    if(_modemSerialConnection.available() > 0) {
       
      char c = _modemSerialConnection.read();

      atRxBuffer[numOfCharReceived] = c;
      numOfCharReceived++;
       
      if(c=='\n')  numOfLines += 1;
       
    }
    
  }
  
  atRxBuffer[numOfCharReceived] = '\0';
  
}



void GL865QuadModem::request(const __FlashStringHelper *commandF, byte respMaxNumOflines, unsigned int timeOutInMS) {

  byte commandLength = 0;
  
  prog_char *commandFPtr = (prog_char*) commandF;

  while(pgm_read_byte_near( commandFPtr + commandLength ) != '\0' ) commandLength++;
  
  char command[commandLength+1];
  
  memcpy_P(command, commandF, commandLength);
  
  command[commandLength] = '\0';
  
  request(command, respMaxNumOflines, timeOutInMS);
  
}




