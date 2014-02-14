/*
 * File : MStore_24LC1025.h
 *
 * Version : 0.8.0
 *
 * Purpose : 24LC1025 EEPROM "store" interface library for Arduino
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




#ifndef MSTORE_24LC1025_h
#define MSTORE_24LC1025_h



#include "Arduino.h"

#include "Wire.h"




#define MAX_CHUNK_SIZE 16

#define MAX_NUM_WRITES_PER_PAGE 100000




class MStore_24LC1025 {


  public:
  
    MStore_24LC1025(byte storeAddress);
    
    void init();
    
    boolean writeMessage(int pageIndex, char* message);

    boolean storeMessage(char* message);
    
    int getMessagesCount();
    
    unsigned long getWritesCount(int pageIndex);
    
    byte getMessageLength(int pageIndex);
    
    void retrieveMessage(int pageIndex, char* message);

    void clearPage(int pageIndex);
    
    void clearAllPages();
    
    void smashPage(int pageIndex);
    
    void smashAllPages();
    
    void dumpPageHex(int pageIndex);
    
    void dumpPageHuman(int pageIndex);
    
    
  private:
  
    byte _storeAddress;
  
    byte getTwiAddress(int pageIndex);
    
    unsigned int getPageStartAddress(int pageIndex);
    

};



#endif