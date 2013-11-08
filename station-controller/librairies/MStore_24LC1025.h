/*
 * File : MStore_24LC1025.h
 *
 * Version : 0.5.0
 *
 * Purpose : 24LC1025 EEPROM "store" interface library for Arduino
 *
 * Author : Previmeteo (www.previmeteo.com)
 *
 * Project web site : http://oses.previmeteo.com/
 *
 * License: GNU GPL v2 (see License.txt)
 *
 * Creation date : 2013/10/30
 *
 * History :
 * 
 */


/*

 * This library implements a "store" in which "messages" (usually ASCII strings) can be recorded in 
 * and retrieved from the EEPROM.
 *
 * Each store has an "address" corresponding to its TWI address, depending of the configuration of 
 * the A0 and A1 pins of the 24LC1025 chip.
 *
 * A store contains 1024 pages in which messages from variable length up to 124 bytes can be recorded.
 *
 * Each page is identified by an "index" (from 0 to 1023) and is associated to a "writes counter" 
 * corresponding to the number of previously written (and erased) messages in this page. 
 *
 * The writes counter aims to give a representation of the page wear : when the writes counter of a page
 * exceeds a predefined value (MAX_NUM_WRITES_PER_PAGE, set in the MStore_24LC1025.h file), the page is 
 * considered as unreliable and it won't be possible to write any more message on it.  
 *
 * Please note that this counter only makes sense if you start to work with a new chip : the writes counter 
 * does not of course reflect the REAL state / wear of a previously heavily used chip.
 *
 * A page may contain 0 or 1 message which length can not exceed 124 bytes (so a user can at most 
 * record 1024 messages in the store). 
 *
 * A message can be stored, retrieved, or deleted with the help of several methods, by setting the 
 * corresponding page index as parameter.

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