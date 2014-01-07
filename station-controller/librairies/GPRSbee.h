/*
 * File : GPRSbee.h
 *
 * Version : 0.5.1
 *
 * Purpose : GPRSBEE modem interface library for Arduino
 *
 * Author : Previmeteo (www.previmeteo.com)
 *
 * Project web site : http://oses.previmeteo.com/
 *
 * License: GNU GPL v2 (see License.txt)
 *
 * Creation date : 2013/11/28
 *
 * History :
 *
 * - 0.5.1 : addition of http requests response retrieval functions
 * 
 */
 
 

#ifndef GPRSBEE_h
#define GPRSBEE_h



#include "Arduino.h"
#include "SoftwareSerial.h"




#define AT_RX_BUFFER_SIZE 60


#define AT_INIT_RESP_TIMOUT_IN_MS 3000

#define AT_DEFAULT_RESP_TIMOUT_IN_MS 2000

#define AT_CSTT_RESP_TIMOUT_IN_MS 5000

#define AT_CIICR_RESP_TIMOUT_IN_MS 15000

#define AT_CIFSR_RESP_TIMOUT_IN_MS 5000

#define AT_CGATT_RESP_TIMOUT_IN_MS 10000

#define AT_CIPSTART_RESP_TIMOUT_IN_MS 15000

#define AT_CIPSEND_RESP_TIMOUT_IN_MS 15000

#define AT_CIPCLOSE_RESP_TIMOUT_IN_MS 5000

#define AT_CIPSHUT_RESP_TIMOUT_IN_MS 5000

#define HTTP_RESP_TIMOUT_IN_MS 60000


#define HTTP_POST_FILE_DEFAULT_FORM_FIELD_NAME "f"

#define HTTP_POST_FILE_BOUNDARY "BOUNDARY"






class GPRSbee {


  public:

    SoftwareSerial serialConnection;
    
    GPRSbee(byte onOffPin, byte statusPin, byte rxPin, byte txPin);
    
    GPRSbee(byte onOffPin, byte statusPin, byte rxPin, byte txPin, SoftwareSerial *debugSerialConnection);
    
    void init(long baudRate);
    
    void powerOn();
   
    void powerOff();
    
    void powerOff_On();
    
    boolean isOn();
    
    void requestAT(char *command, byte respMaxNumOflines, long timeOutInMS);
    
    void requestAT(const __FlashStringHelper *commandF, byte respMaxNumOflines, long timeOutInMS);
    
    boolean isAtRXBufferEmpty();
    
    void activateCommunication();
    
    boolean isCommunicationActivated();
    
    void configure();
    
    void enterPINCode(char *pinCode);
    
    void retrieveIMEI(char IMEIBuffer[16]);
    
    boolean isRegistered();
    
    void attachGPRS();
   
    void dettachGPRS();
    
    boolean isGPRSAttached();
    
    void connectToNet(char *networkAPN, char *username, char *password);
    
    void disconnectFromNet();   
    
    boolean isConnectedToNet();
    
    boolean tcpConnect(char *serverName, char *serverPort, byte maxNumConnectAttempts);
    
    void tcpSendChars(char *chars);
    
    void tcpClose();
    
    void echoHttpRequestInitHeaders(char *serverName, char *serverURL, char *method);
    
    void echoHttpPostURLEncodedRequestAdditionalHeaders(long encodedDataLength);
    
    void echoHttpPostFileRequestAdditionalHeadersPart1(long fileContentLength, char * formFieldName);
    
    void echoHttpPostFileRequestAdditionalHeadersPart2();
    
    boolean httpGet(char *serverName, char *serverPort, char *serverURL, byte maxNumConnectAttempts);
    
    boolean httpPostEncodedData(char *serverName, char *serverPort, char *serverURL, char *encodedData, byte maxNumConnectAttempts);
    
    boolean httpPostTextFile(char *serverName, char *serverPort, char *serverURL, char *fileContent, byte maxNumConnectAttempts);
    
    void retrieveIncomingCharsFromLineToLine(char *incomingCharsBuffer, byte incomingCharsBufferLength, byte fromLine, byte toLine, long timeOutInMS);
    
    void retrieveHttpResponseStatusLine(char *httpResponseStatusLineBuffer, byte httpResponseStatusLineBufferLength, long timeOutInMS);
    
    void retrieveHttpResponseBodyFromLineToLine(char *httpResponseBodyBuffer, byte httpResponseBodyBufferLength, byte fromLine, byte toLine, long timeOutInMS);

  private:
    
    char _atRxBuffer[AT_RX_BUFFER_SIZE];
    
    byte _onOffPin;
    
    byte _statusPin;
    
    SoftwareSerial *_debugSerialConnection;
    
    void togglePowerState();

};


#endif
