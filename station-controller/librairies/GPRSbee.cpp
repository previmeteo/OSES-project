/*
 * File : GPRSbee.cpp
 *
 * Version : 0.8.0
 *
 * Purpose : GPRSBEE modem (http://www.gprsbee.com) interface library for Arduino
 *
 * Author : Previmeteo (www.previmeteo.com)
 *
 * Project web site : http://oses.previmeteo.com/
 *
 * License: GNU GPL v2 (see License.txt)
 *
 * Creation date : 2014/01/29
 * 
 */
 
 
 
#include "Arduino.h"

#include "SoftwareSerial.h"

#include "GPRSbee.h"



GPRSbee::GPRSbee(byte onOffPin, byte statusPin, byte rxPin, byte txPin):serialConnection(rxPin, txPin) { 
    
  _onOffPin = onOffPin;
  _statusPin = statusPin;
  _debugSerialConnectionEnabled = false;
  
}



GPRSbee::GPRSbee(byte onOffPin, byte statusPin, byte rxPin, byte txPin, SoftwareSerial *debugSerialConnection):serialConnection(rxPin, txPin) { 
    
  _onOffPin = onOffPin;
  _statusPin = statusPin;
  _debugSerialConnectionEnabled = true;
  
  _debugSerialConnection = debugSerialConnection;
  
}



void GPRSbee::init(long baudRate) {
  
  pinMode(_onOffPin, OUTPUT);    
  digitalWrite(_onOffPin, HIGH); 
  
  pinMode(_statusPin, INPUT);  
  
  serialConnection.begin(baudRate);
  serialConnection.listen();
  
  delay(1000);

}



void GPRSbee::togglePowerState() {
  
  boolean success = false;
  
  boolean powerStateInit = isOn();
  
  for(byte i=0 ; (i<3) && !success ; i++) {

    digitalWrite(_onOffPin, LOW);
     
    delay(1000);
    
    digitalWrite(_onOffPin, HIGH);
    
    delay(1500);
    
    digitalWrite(_onOffPin, LOW); 
    
    delay(3000); 
  
    if(isOn() == !powerStateInit) success = true;
    
  }
    
}



void GPRSbee::powerOn() {
  
  if(!isOn()) togglePowerState();
  
}



void GPRSbee::powerOff() {
  
  if(isOn()) togglePowerState();
  
}



void GPRSbee::powerOff_On() {
  
  if(isOn()) {
  
    powerOff();
    
    delay(1000);
    
    powerOn();
    
  }
  
}



boolean GPRSbee::isOn() {

  boolean on = false;
  
  byte status = digitalRead(_statusPin);
  
  if(status) on = true;
  
  return on;
  
}



void GPRSbee::activateCommunication() {
  
  requestAT(F("AT"), 2, AT_INIT_RESP_TIMOUT_IN_MS);              // required for autobaud rate detection by the modem
  
  delay(500);
  
  requestAT(F("ATE0"), 2, AT_DEFAULT_RESP_TIMOUT_IN_MS);         // echo desactivation
   
  delay(300);
  
}
    


boolean GPRSbee::isCommunicationActivated() {
  
  boolean communicationActivated = false;
  
  requestAT(F("AT"), 2, AT_DEFAULT_RESP_TIMOUT_IN_MS);

  if(strstr(_atRxBuffer, "OK") != NULL) communicationActivated = true;  
 
  return communicationActivated;
  
}


    
void GPRSbee::configure() {
    
   //requestAT(F("AT+CMEE=2"), 2, AT_DEFAULT_RESP_TIMOUT_IN_MS);    // error reporting in verbose format
   
  requestAT(F("AT+CIPSPRT=2"), 2, AT_DEFAULT_RESP_TIMOUT_IN_MS);    // tcp send / prompt configuration : do not echo '>' 
                                                                    // and do not show "SEND OK" when the data is successfully sent 
}



void GPRSbee::retrieveIMEI(char IMEIBuffer[16]) {

  requestAT(F("AT+CGSN"), 2, AT_DEFAULT_RESP_TIMOUT_IN_MS);  
  
  for(byte i = 0 ; i < 15 ; i++) {
  
    IMEIBuffer[i] = _atRxBuffer[i+2];
  
  }
  
  IMEIBuffer[15] = '\0';
  
}



void GPRSbee::enterPINCode(char *pinCode) {

  char reqBuffer[14];
  
  strcpy(reqBuffer, "AT+CPIN=");
  strcat(reqBuffer, pinCode);
  requestAT(reqBuffer, 2, AT_DEFAULT_RESP_TIMOUT_IN_MS);

}



boolean GPRSbee::isRegistered() {

  boolean registered = false;
  
  requestAT(F("AT+CREG?"), 2, AT_DEFAULT_RESP_TIMOUT_IN_MS);
  
  if(strstr(_atRxBuffer, "0,1") != NULL) registered = true;                 // expected response : "+CREG: 0,1"
 
  return registered;
  
}



void GPRSbee::attachGPRS() {

  requestAT(F("AT+CGATT=1"), 2, AT_CGATT_RESP_TIMOUT_IN_MS);  

}
 
 

void GPRSbee::dettachGPRS() {

  requestAT(F("AT+CGATT=0"), 2, AT_CGATT_RESP_TIMOUT_IN_MS);
  
}



boolean GPRSbee::isGPRSAttached() {
  
  boolean GPRSAttached = false;
  
  requestAT(F("AT+CGATT?"), 2, AT_DEFAULT_RESP_TIMOUT_IN_MS);
    
  if(strstr(_atRxBuffer, ": 1") != NULL) GPRSAttached = true;        // expected response : "+CGATT: 1"  
  
  return GPRSAttached;

}



void GPRSbee::connectToNet(char *networkAPN, char *username, char *password) {
  
  char reqBuffer[70];
  
  strcpy(reqBuffer, "AT+CSTT=\"");
  strcat(reqBuffer, networkAPN);
  strcat(reqBuffer, "\",\"");
  strcat(reqBuffer, username);
  strcat(reqBuffer, "\",\"");
  strcat(reqBuffer, password);
  strcat(reqBuffer, "\"");
  
  requestAT(reqBuffer, 2, AT_CSTT_RESP_TIMOUT_IN_MS);
  
  requestAT(F("AT+CIICR"), 2, AT_CIICR_RESP_TIMOUT_IN_MS);
  

}



void GPRSbee::disconnectFromNet() {

  requestAT(F("AT+CIPSHUT"), 2, AT_CIPSHUT_RESP_TIMOUT_IN_MS); 
  
}   



boolean GPRSbee::isConnectedToNet() {

  // we could use here the AT command "AT+CIPSTATUS" but there are too much possible responses to parse : 
  // IP GPRSACT, TCP CONNECTING, CONNECT OK, IP CLOSE, TCP CLOSED...
  // so we prefer the AT command "AT+CIFSR" which gives the current IP address or the response "ERROR"
  
  boolean connectedToNetwork = true;
  
  requestAT(F("AT+CIFSR"), 2, AT_CIFSR_RESP_TIMOUT_IN_MS); 
  
  if(strstr(_atRxBuffer, "ERROR") != NULL) connectedToNetwork = false;                
  
  return connectedToNetwork;

}



boolean GPRSbee::tcpConnect(char *serverName, char *serverPort, byte maxNumConnectAttempts) {
  
  boolean connected = false;
  
  char connectRequestBuffer[50];
  
  strcpy(connectRequestBuffer, "AT+CIPSTART=\"TCP\",\"");
  strcat(connectRequestBuffer, serverName);
  strcat(connectRequestBuffer, "\",\"");
  strcat(connectRequestBuffer, serverPort);
  strcat(connectRequestBuffer, "\"");
  
  for(byte attemptsConnect = 0 ; (attemptsConnect < maxNumConnectAttempts) && !connected ; attemptsConnect++) {
      
    requestAT(connectRequestBuffer, 4, AT_CIPSTART_RESP_TIMOUT_IN_MS);
      
    if(strstr(_atRxBuffer, "CONN") != NULL) connected = true;              // expected response : "CONNECT OK"
    
  }
  
  return connected;
  
}



void GPRSbee::tcpSendChars(char *chars) {  

  delay(300);

  requestAT(F("AT+CIPSEND"), 2, AT_CIPSEND_RESP_TIMOUT_IN_MS);
  
  serialConnection.print(chars);
    
  serialConnection.print((char) 26);
  
}



void GPRSbee::tcpClose() {

  requestAT(F("AT+CIPCLOSE"), 2, AT_CIPCLOSE_RESP_TIMOUT_IN_MS);

}



void GPRSbee::echoHttpRequestInitHeaders(char *serverName, char *serverURL, char *method) {
  
  serialConnection.print(method);
  serialConnection.print(F(" "));
  serialConnection.print(serverURL);
  serialConnection.print(F(" HTTP/1.1\r\n"));
        
  serialConnection.print(F("HOST: "));
  serialConnection.print(serverName);
  serialConnection.print(F("\r\n"));

}



void GPRSbee::echoHttpPostURLEncodedRequestAdditionalHeaders(long encodedDataLength) {

  serialConnection.print(F("Content-Type: application/x-www-form-urlencoded\r\n"));
  serialConnection.print(F("Content-Length: "));
  serialConnection.print(encodedDataLength);
  serialConnection.print(F("\r\n\r\n")); 

}



void GPRSbee::echoHttpPostFileRequestAdditionalHeadersPart1(long fileContentLength, char *formFieldName) {

  long formFieldNameLength = 0;
  while(formFieldName[formFieldNameLength] != '\0') formFieldNameLength++;
  
  char boundary[] = HTTP_POST_FILE_BOUNDARY; 
  long boundaryLength = 0;
  while(boundary[boundaryLength] != '\0') boundaryLength++;
    
  serialConnection.print(F("Content-Type: multipart/form-data; boundary="));
  serialConnection.print(boundary);
  serialConnection.print(F("\r\nContent-Length: "));
  serialConnection.print((95 + formFieldNameLength + 2 * boundaryLength + fileContentLength));
  serialConnection.print(F("\r\n\r\n--")); 
  serialConnection.print(HTTP_POST_FILE_BOUNDARY);   
  serialConnection.print(F("\r\nContent-Disposition: form-data; name=\""));
  serialConnection.print(formFieldName);
  serialConnection.print(F("\"; filename=\"none\"\r\n"));
  serialConnection.print(F("Content-Type: text/plain\r\n\r\n"));

}



void GPRSbee::echoHttpPostFileRequestAdditionalHeadersPart2() {
  
  serialConnection.print(F("\r\n--"));
  serialConnection.print(HTTP_POST_FILE_BOUNDARY);  
  serialConnection.print(F("--"));  

}



boolean GPRSbee::httpGet(char *serverName, char *serverPort, char *serverURL, byte maxNumConnectAttempts) {

  // returns true if the status line of the response contains the http code : 200 

  boolean requestSuccess = false; 

  boolean tcpConnectSuccess = tcpConnect(serverName, serverPort, maxNumConnectAttempts);
  
  if(tcpConnectSuccess) {
  
    char httpResponseStatusLineBuffer[60];
    
    requestAT(F("AT+CIPSEND"), 2, AT_CIPSEND_RESP_TIMOUT_IN_MS);

    echoHttpRequestInitHeaders(serverName, serverURL, "GET");
  
    serialConnection.print('\n');
  
    serialConnection.print((char) 26);
    
    retrieveHttpResponseStatusLine(httpResponseStatusLineBuffer, sizeof(httpResponseStatusLineBuffer), HTTP_RESP_TIMOUT_IN_MS);
  
    if(strstr(httpResponseStatusLineBuffer, "200") != NULL) requestSuccess = true; 
  
    tcpClose();
  
  }
  
  return requestSuccess;

}



boolean GPRSbee::httpPostEncodedData(char *serverName, char *serverPort, char *serverURL, char *encodedData, byte maxNumConnectAttempts) {
  
  // encodedData example : "A=1&B=2&C=3" (URL encoded data)
  // returns true if the status line of the response contains the http code : 200 
  
  boolean requestSuccess = false; 
  
  long encodedDataLength = 0;
  while(encodedData[encodedDataLength] != '\0') encodedDataLength++;

  boolean tcpConnectSuccess = tcpConnect(serverName, serverPort, maxNumConnectAttempts);
  
  if(tcpConnectSuccess) {
  
    char httpResponseStatusLineBuffer[60];

    requestAT(F("AT+CIPSEND"), 2, AT_CIPSEND_RESP_TIMOUT_IN_MS);

    echoHttpRequestInitHeaders(serverName, serverURL, "POST");
    
    echoHttpPostURLEncodedRequestAdditionalHeaders(encodedDataLength);
    
    serialConnection.print(encodedData);
    
    serialConnection.print((char) 26);
    
    retrieveHttpResponseStatusLine(httpResponseStatusLineBuffer, sizeof(httpResponseStatusLineBuffer), HTTP_RESP_TIMOUT_IN_MS);
  
    if(strstr(httpResponseStatusLineBuffer, "200") != NULL) requestSuccess = true; 
  
    tcpClose();
    

  }
  
  return requestSuccess;
  
}



boolean GPRSbee::httpPostTextFile(char *serverName, char *serverPort, char *serverURL, char *fileContent, byte maxNumConnectAttempts) {

  // returns true if the status line of the response contains the http code : 200 

  boolean requestSuccess = false; 
  
  long fileContentLength = 0;
  while(fileContent[fileContentLength] != '\0') fileContentLength++;
  

  boolean tcpConnectSuccess = tcpConnect(serverName, serverPort, maxNumConnectAttempts);
  
  if(tcpConnectSuccess) {
  
  
    char httpResponseStatusLineBuffer[60];
    
    char formFieldName[] = HTTP_POST_FILE_DEFAULT_FORM_FIELD_NAME;
    
     
    requestAT(F("AT+CIPSEND"), 2, AT_CIPSEND_RESP_TIMOUT_IN_MS);
    
    echoHttpRequestInitHeaders(serverName, serverURL, "POST");
    
    echoHttpPostFileRequestAdditionalHeadersPart1(fileContentLength, formFieldName);
    
    serialConnection.print((char) 26);
    
    
    delay(300);
    
      
    requestAT(F("AT+CIPSEND"), 2, AT_CIPSEND_RESP_TIMOUT_IN_MS);
    
    serialConnection.print(fileContent);
    
    serialConnection.print((char) 26);
    
    
    delay(300);
    
    
    requestAT(F("AT+CIPSEND"), 2, AT_CIPSEND_RESP_TIMOUT_IN_MS);
    
    echoHttpPostFileRequestAdditionalHeadersPart2();  
   
    serialConnection.print((char) 26);
    

    retrieveHttpResponseStatusLine(httpResponseStatusLineBuffer, sizeof(httpResponseStatusLineBuffer), HTTP_RESP_TIMOUT_IN_MS);
  
    if(strstr(httpResponseStatusLineBuffer, "200") != NULL) requestSuccess = true; 
  
    tcpClose();

  }
  
  return requestSuccess;

}



void GPRSbee::retrieveIncomingCharsFromLineToLine(char *incomingCharsBuffer, byte incomingCharsBufferLength, byte fromLine, byte toLine, long timeOutInMS) {

  // note : the fromLine and toLine indexed lines are included !!!
  
  int numOfCharsReceived = 0;
  byte numOfLines = 0;
  
  long clockTimeOut = millis() + timeOutInMS;
  
  while((millis() < clockTimeOut) && (serialConnection.available() <= 0)) delay(100);
    
  while((millis() < clockTimeOut) && (numOfLines <= toLine) && (numOfCharsReceived < (incomingCharsBufferLength - 1))) {         
      
    if(serialConnection.available() > 0) {
         
      char c = serialConnection.read(); 
         
      if(numOfLines  >= fromLine) {
         
        incomingCharsBuffer[numOfCharsReceived] = c;
        numOfCharsReceived++;
        
      }
         
      if(c=='\n') numOfLines += 1;
         
    }
    
    else delay(100);
    
  }
  
  incomingCharsBuffer[numOfCharsReceived] = '\0';
  
  delay(500);
  
  serialConnection.flush();
  
}



void GPRSbee::retrieveHttpResponseStatusLine(char *httpResponseStatusLineBuffer, byte httpResponseStatusLineBufferLength, long timeOutInMS) {

  retrieveIncomingCharsFromLineToLine(httpResponseStatusLineBuffer, httpResponseStatusLineBufferLength, 0, 0, timeOutInMS);
  
}



void GPRSbee::retrieveHttpResponseBodyFromLineToLine(char *httpResponseBodyBuffer, byte httpResponseBodyBufferLength, 
                                                     byte fromLine, byte toLine, long timeOutInMS) {

  // note : the fromLine and toLine indexed lines are included !!!
  
  boolean responseBodyAvailable = false;
  
  int numOfResponseHeadersCharsReceived = 0;
  int lastPosOfNewLineChar = 0;
  
  
  long responseHeadersRetrieveStartMillis = millis();
  
  long clockTimeOut = responseHeadersRetrieveStartMillis + timeOutInMS;
  
  while((millis() < clockTimeOut) && (serialConnection.available() <= 0)) delay(100);
  
  while((millis() < clockTimeOut) && !responseBodyAvailable) {  
  
    if(serialConnection.available() > 0) {
         
      char c = serialConnection.read(); 
         
      if(c=='\n') {
      
        if((numOfResponseHeadersCharsReceived - lastPosOfNewLineChar) == 2) responseBodyAvailable = true;
        else lastPosOfNewLineChar = numOfResponseHeadersCharsReceived;
      
      }
      
      numOfResponseHeadersCharsReceived++;
         
    }
    
    else delay(100);
    
  }
  
  long responseBodyRetrieveStartMillis = millis();


  // now we can retrieve the response's body !!!

  retrieveIncomingCharsFromLineToLine(httpResponseBodyBuffer, httpResponseBodyBufferLength, fromLine, toLine, 
                                      timeOutInMS - (responseBodyRetrieveStartMillis - responseHeadersRetrieveStartMillis));

}



void GPRSbee::requestAT(char *command, byte respMaxNumOflines, long timeOutInMS) {
  
  
  if(DEBUG_MODE and _debugSerialConnectionEnabled) {
  
    _debugSerialConnection->print("-> ");
    _debugSerialConnection->println(command);
  
  }
  
  delay(150);
  
  serialConnection.flush();

  byte numOfCharsReceived = 0;
  byte numOfLines = 0;
    
  long clockTimeOut = millis() + timeOutInMS;
  
  serialConnection.print(command); 
  serialConnection.print("\r\n");  
  
  while ((millis() < clockTimeOut) && (numOfLines < respMaxNumOflines)) {  
    
    if(serialConnection.available() > 0) {
       
      char c = serialConnection.read();

      _atRxBuffer[numOfCharsReceived] = c;
      numOfCharsReceived++;
       
      if(c=='\n')  numOfLines += 1;
       
    }
    
  }
  
  _atRxBuffer[numOfCharsReceived] = '\0';
  
  delay(150);

  serialConnection.flush();
  
  
  if(DEBUG_MODE and _debugSerialConnectionEnabled) {
  
    _debugSerialConnection->println(_atRxBuffer);
  
  }
  
}



void GPRSbee::requestAT(const __FlashStringHelper *commandF, byte respMaxNumOflines, long timeOutInMS) {

  byte commandLength = 0;
  
  prog_char *commandFPtr = (prog_char*) commandF;

  while(pgm_read_byte_near( commandFPtr + commandLength ) != '\0' ) commandLength++;
  
  char command[commandLength+1];
  
  memcpy_P(command, commandF, (commandLength + 1));
  
  command[commandLength] = '\0';
  
  requestAT(command, respMaxNumOflines, timeOutInMS);
  
}




boolean GPRSbee::isAtRXBufferEmpty() {

  boolean isEmpty = true;

  byte numChars = 0;
  while(_atRxBuffer[numChars] != '\0') numChars++;
  
  if(numChars) isEmpty = false;
  
  return isEmpty;

}







