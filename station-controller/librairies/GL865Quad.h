/*
 * File : GL865Quad.h
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
 
 

#ifndef GL865QUAD_h
#define GL865QUAD_h



#include "Arduino.h"
#include "SoftwareSerial.h"



#define MODEM_RX_BUFFER_SIZE 50



class GL865QuadModem {

  public:

    GL865QuadModem(byte onOffPin, byte rxPin, byte txPin);
    
    GL865QuadModem(byte onOffPin, byte rxPin, byte txPin, SoftwareSerial *debugSerialConnector);
    
    void init(unsigned long baudRate);
    
    void powerOn();
   
    void powerOff();
    
    void activateCommunication();
    
    boolean isCommunicationActivated();
    
    void configure();
    
    void enterPINCode(char *pinCode);
    
    boolean isRegistered();
    
    void attachGPRS();
   
    void dettachGPRS();
    
    boolean isGPRSAttached();
    
    void activatePDP(char *networkAPN, char *username, char *password);
    
    void desactivatePDP();   
    
    boolean isPDPActivated();
    
    boolean httpPost(char *serverName, char *serverPort, char *serverPostURL, char *encodedData, byte maxNumConnectAttempts);
    
    

  private:
    
    char atRxBuffer[MODEM_RX_BUFFER_SIZE];
    
    byte _onOffPin;
    
    SoftwareSerial _modemSerialConnection;
    
    SoftwareSerial *_debugSerialConnection;
    
    void request(const char *command, byte respMaxNumOflines, unsigned int timeOutInMS);
    
    void request(const __FlashStringHelper *commandF, byte respMaxNumOflines, unsigned int timeOutInMS);

};


#endif
