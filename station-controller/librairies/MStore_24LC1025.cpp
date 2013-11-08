/*
 * File : MStore_24LC1025.cpp
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
 * Creation date : 2013/10/08
 *
 * History :
 * 
 */
 
 


#include "Arduino.h"

#include "Wire.h"

#include "MStore_24LC1025.h"




MStore_24LC1025::MStore_24LC1025(byte storeAddress) {

  _storeAddress = storeAddress;

}



void MStore_24LC1025::init() {

  for(int pageIndex = 0 ; pageIndex < 1024 ; pageIndex++) {
  
    long writesCount = getWritesCount(pageIndex);
    
    if(writesCount == 0xFFFFFF) smashPage(pageIndex);     // here we have a page which has never been written before
                                                          //  (if of course we use a new chip !)
    
  }

}


  
byte MStore_24LC1025::getTwiAddress(int pageIndex) {

  byte twiAddress;
  
  if(pageIndex < 512) twiAddress = _storeAddress;
  else twiAddress = _storeAddress | (1 << 2);
  
  return twiAddress;
  
}
    
    
    
unsigned int MStore_24LC1025::getPageStartAddress(int pageIndex) {

  unsigned int pageStartAddress;
  
  if(pageIndex < 512) pageStartAddress = pageIndex * 128;
  else  pageStartAddress = (pageIndex - 512) * 128;
    
  return pageStartAddress;
  
}
   


unsigned long MStore_24LC1025::getWritesCount(int pageIndex) {

  unsigned long writesCount;

  byte twiAddress = getTwiAddress(pageIndex);
  unsigned int pageStartAddress = getPageStartAddress(pageIndex);

  Wire.beginTransmission(twiAddress);
  Wire.write((int)((pageStartAddress) >> 8));   
  Wire.write((int)((pageStartAddress) & 0xFF)); 
  Wire.endTransmission();
 
  Wire.requestFrom((int) twiAddress, 3);
  
  writesCount = (Wire.read() << 16) +  (Wire.read() << 8) + Wire.read();
    
  return writesCount;
  
}


    
byte MStore_24LC1025::getMessageLength(int pageIndex) {

  byte twiAddress = getTwiAddress(pageIndex);
  unsigned int pageStartAddress = getPageStartAddress(pageIndex);
  
  Wire.beginTransmission(twiAddress);
  Wire.write((int)((pageStartAddress + 3) >> 8));   
  Wire.write((int)((pageStartAddress + 3) & 0xFF)); 
  Wire.endTransmission();
 
  Wire.requestFrom((int) twiAddress, 1);
  
  byte messageLength = Wire.read();
  
  return messageLength;
  
}


  
boolean MStore_24LC1025::writeMessage(int pageIndex, char* message) {

  boolean canWriteInPage = true;
  
  long writesCount = getWritesCount(pageIndex);
  
  if(writesCount > MAX_NUM_WRITES_PER_PAGE) canWriteInPage = false;
  
  if(canWriteInPage) {
  
    byte twiAddress = getTwiAddress(pageIndex);
    unsigned int pageStartAddress = getPageStartAddress(pageIndex);
  
    byte messageLength = 0;
    while(message[messageLength] != '\0') messageLength++;
  
    // writesCount update (value stored in the 3 first bytes of the page)
  
    writesCount++;
      
    Wire.beginTransmission(twiAddress);
      
    Wire.write((int)(pageStartAddress >> 8));  
    Wire.write((int)(pageStartAddress & 0xFF)); 
        
    Wire.write((int)((writesCount >> 16) & 0xFF));
    Wire.write((int)((writesCount >> 8) & 0xFF));
    Wire.write((int)(writesCount & 0xFF));
         
    Wire.endTransmission();
    delay(10);
      
      
    // messageLength writing (value stored in the 4th byte of the page)
     
    Wire.beginTransmission(twiAddress);
    
    Wire.write((int)((pageStartAddress + 3) >> 8));  
    Wire.write((int)((pageStartAddress + 3) & 0xFF)); 
        
    Wire.write((byte) messageLength);
         
    Wire.endTransmission();
    delay(10); 
      
      
    // message writing 
      
    for(byte i = 0 ; i < 124 ; i ++) {
    
      if((i % MAX_CHUNK_SIZE) == 0) {                                           // start of chunk
      
        Wire.beginTransmission(twiAddress);
        Wire.write((int)((pageStartAddress + 4 + i) >> 8)); 
        Wire.write((int)((pageStartAddress + 4 + i) & 0xFF));
    
      }
    
      if(i < messageLength) Wire.write((byte) message[i]);
    
      else Wire.write(0);
       
      if(((i % MAX_CHUNK_SIZE) == (MAX_CHUNK_SIZE - 1)) || (i == 126)) {            // end of chunk 
      
        Wire.endTransmission();
        delay(10); 
      
      }
    
    }
  
  }
  
  return canWriteInPage;
  
}
    
    
    
boolean MStore_24LC1025::storeMessage(char* message) {

  boolean success = false;
  
  boolean pageIndexFound = false;
  
  int pageIndex = 0;

  while((!pageIndexFound) && (pageIndex < 1024)) {
  
    if((getMessageLength(pageIndex) == 0) && (getWritesCount(pageIndex) < MAX_NUM_WRITES_PER_PAGE)) pageIndexFound = true;
    else pageIndex++;
  
  }
  
  if(pageIndexFound) success = writeMessage(pageIndex, message);
  
  return success;
  
}



int MStore_24LC1025::getMessagesCount() {

  int messagesCount = 0;
  
  for(int pageIndex = 0 ; pageIndex < 1024 ; pageIndex++) {
  
    if(getMessageLength(pageIndex)) messagesCount++;
  
  }

  return messagesCount;
  
}


    
void MStore_24LC1025::retrieveMessage(int pageIndex, char* message) {

  byte twiAddress = getTwiAddress(pageIndex);
  unsigned int pageStartAddress = getPageStartAddress(pageIndex);
  
  byte messageLength = getMessageLength(pageIndex);
  
  byte numCharsReaden = 0;
  
  byte numChunks = messageLength / MAX_CHUNK_SIZE + 1;
  
  unsigned int chunkAddress;
  byte chunkLength;
 
  
  for(byte i = 0 ; i < numChunks ; i++) {
  
    chunkAddress = pageStartAddress + 4 + i * MAX_CHUNK_SIZE;
    
    if(i < numChunks - 1) chunkLength = MAX_CHUNK_SIZE;
    else chunkLength = messageLength % MAX_CHUNK_SIZE;
    
    Wire.beginTransmission(twiAddress);
    Wire.write((int)(chunkAddress >> 8));   
    Wire.write((int)(chunkAddress & 0xFF)); 
    Wire.endTransmission();
    
    Wire.requestFrom((int) twiAddress, (int) chunkLength);
    
    for(byte j = 0 ; j < chunkLength ; j++) {
      
      message[numCharsReaden++] = Wire.read();
     
    } 
  
  }
  
  message[numCharsReaden++] = '\0';

}


    
void MStore_24LC1025::clearPage(int pageIndex) {

  byte messageLength = getMessageLength(pageIndex);
  
  if(messageLength) {
  
    byte twiAddress = getTwiAddress(pageIndex);
    unsigned int pageStartAddress = getPageStartAddress(pageIndex);
      
    for(byte i = 0 ; i < 125 ; i ++) {
        
      if((i % MAX_CHUNK_SIZE) == 0) {                                           // start of chunk
          
        Wire.beginTransmission(twiAddress);
        Wire.write((int)((pageStartAddress + 3 + i) >> 8)); 
        Wire.write((int)((pageStartAddress + 3 + i) & 0xFF));
        
      }
        
       Wire.write(0);
           
      if(((i % MAX_CHUNK_SIZE) == (MAX_CHUNK_SIZE - 1)) || (i == 124)) {            // end of chunk 
          
        Wire.endTransmission();
        delay(10); 
          
      }
        
    }
  
  }

}



void MStore_24LC1025::clearAllPages() {
  
  for(int pageIndex = 0 ; pageIndex < 1024 ; pageIndex++) clearPage(pageIndex);
  
}


     
void MStore_24LC1025::smashPage(int pageIndex) {

  byte twiAddress = getTwiAddress(pageIndex);
  unsigned int pageStartAddress = getPageStartAddress(pageIndex);
  
  for(byte i = 0 ; i < 128 ; i ++) {
    
    if((i % MAX_CHUNK_SIZE) == 0) {                                           // start of chunk
      
      Wire.beginTransmission(twiAddress);
      Wire.write((int)((pageStartAddress + i) >> 8)); 
      Wire.write((int)((pageStartAddress + i) & 0xFF));
    
    }
    
     Wire.write(0);
       
    if(((i % MAX_CHUNK_SIZE) == (MAX_CHUNK_SIZE - 1)) || (i == 127)) {            // end of chunk 
      
      Wire.endTransmission();
      delay(10); 
      
    }
    
  }


}



void MStore_24LC1025::smashAllPages() {

  for(int pageIndex = 0 ; pageIndex < 1024 ; pageIndex++) smashPage(pageIndex);

}




void MStore_24LC1025::dumpPageHex(int pageIndex) {

  Serial.print("dumping page ");
  Serial.print(pageIndex);
  Serial.print(" : ");
  
  byte twiAddress = getTwiAddress(pageIndex);
  unsigned int pageStartAddress = getPageStartAddress(pageIndex);
 
  for(byte i = 0 ; i < 128 ; i++) {
    
    Wire.beginTransmission(twiAddress);
    Wire.write((int)((pageStartAddress + i) >> 8));   
    Wire.write((int)((pageStartAddress + i) & 0xFF)); 
    Wire.endTransmission();
    
    Wire.requestFrom((int) twiAddress, 1);
    
    Serial.print(Wire.read(), HEX);
    Serial.print(" ");
  
  }
  
  Serial.println();


}



void MStore_24LC1025::dumpPageHuman(int pageIndex) {

  Serial.print("dumping page ");
  Serial.print(pageIndex);
  Serial.print(" : ");
  
  byte twiAddress = getTwiAddress(pageIndex);
  unsigned int pageStartAddress = getPageStartAddress(pageIndex);
  
  long writesCount = getWritesCount(pageIndex);
  
  Serial.print(writesCount);
  Serial.print(" : ");
  
  byte messageLength = getMessageLength(pageIndex);
  
  Serial.print(messageLength);
  Serial.print(" : ");
 
  for(byte i = 0 ; i < 124 ; i++) {
    
    Wire.beginTransmission(twiAddress);
    Wire.write((int)((pageStartAddress + 4 + i) >> 8));   
    Wire.write((int)((pageStartAddress + 4 + i) & 0xFF)); 
    Wire.endTransmission();
    
    Wire.requestFrom((int) twiAddress, 1);
    
    Serial.print((char) Wire.read());
  
  }
  
  Serial.println();


}

