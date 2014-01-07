/*
 *
 * Description :
 *
 * GPRS Weather station, configured to measure temperature, relative humidity and pressure every 5 mn, store the corresponding values 
 * and other monitoring informations in the EEPROM and post them (as a file) to a web server every 15 minutes.
 *
 * The Atmega is put in "sleep" mode between these tasks, and the GPS and GPRS modules are also switched off during this interlude 
 * in order to reduce the power consumption of the station. 
 *
 * The global position of the station is updated every 6 hours : the acquisition and upload tasks can be easily rescheduled 
 * in the scheduleNextTaskAndSleep() function.
 *
 * Author : Previmeteo (www.previmeteo.com)
 *
 * Project web site : http://oses.previmeteo.com/
 *
 * License: GNU GPL v2 (see License.txt)
 *
 * Creation date : 2013/11/07
 * 
 */
 

#include "SoftwareSerial.h"

#include "LowPower.h"

#include "Wire.h"

#include "Rtc_Pcf8563.h"

#include "Adafruit_BMP085.h"

#include "dht.h"

#include "GPRSbee.h"

#include "Ultimate_GPS.h"

#include "MStore_24LC1025.h"



// pins definition

#define DEBUG_TX_PIN 3
#define DEBUG_RX_PIN 255

#define GPS_POWER_PIN 4

#define MODEM_POWER_PIN 5
#define MODEM_TX_PIN 6
#define MODEM_RX_PIN 7
#define MODEM_STATUS_PIN 8

#define SENSORS_POWER_PIN 9

#define TEMPERATURE_HUMIDITY_SENSOR_DATA_PIN 10

#define DEVICE_BATTERY_VOLTAGE_PIN A3


// TWI devices adresses

#define EEPROM_STORE_ADDRESS 0x53       // A0 and A1 pins of the 24LC1025 chip tied to VCC


// "undefined values" definition

#define TEMPERATURE_UNDEFINED_VALUE -100.0
#define HUMIDITY_UNDEFINED_VALUE -10.0
#define PRESSURE_UNDEFINED_VALUE 0.0
#define BATTERY_VOLTAGE_UNDEFINED_VALUE -1.0
#define DEVICE_TEMPERATURE_UNDEFINED_VALUE -100.0


// GPS acquisition parameters

#define GPS_FIRST_ACQUISITION_TIMEOUT_IN_SECONDS 300
#define GPS_FIRST_ACQUISITION_HDOP_LIMIT 10
#define GPS_ACQUISITION_TIMEOUT_IN_SECONDS 160
#define GPS_ACQUISITION_HDOP_LIMIT 10


// GPRS connection parameters

#define GPRS_NETWORK_APN "your_apn"
#define GPRS_USERNAME "your_username"
#define GPRS_PASSWORD "your_password"


// Server connection parameters

#define SERVER_NAME "your_server_name"
#define SERVER_PORT "80"   
#define SERVER_POST_URL "your_post_url"


// station identifier

#define STATION_ID "st01"                                    


// report buffer length

#define REPORT_BUFFER_LENGTH 124 


// tasks identifiers

#define TASK_NONE 0
#define TASK_READ_SENSORS_AND_STORE_REPORT 1
#define TASK_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF 2
#define TASK_GPS_ACQUIRE_POSITION_POWER_ON_POWER_OFF 3


// calibration data

#define ADC_REF_VOLTAGE 3.3




SoftwareSerial softSerialDebug(DEBUG_RX_PIN, DEBUG_TX_PIN); 


GPRSbee modem(MODEM_POWER_PIN, MODEM_STATUS_PIN, MODEM_RX_PIN, MODEM_TX_PIN, &softSerialDebug);


UltimateGPS gps(&Serial, GPS_POWER_PIN, &softSerialDebug);


MStore_24LC1025 mStore(EEPROM_STORE_ADDRESS);


Rtc_Pcf8563 rtc;


dht temperatureHumiditySensor;


Adafruit_BMP085 pressureSensor;


byte nextTaskID = TASK_NONE;

unsigned long nextTaskTimestamp = 0;





/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// setup() and loop() functions definition
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void setup()  {
  
  pinMode(SENSORS_POWER_PIN, OUTPUT);    
  digitalWrite(SENSORS_POWER_PIN, HIGH); 
  
  softSerialDebug.begin(9600);
  
  gps.init(9600);
  
  modem.init(9600);

  Wire.begin();
  
  pressureSensor.begin(); 

  mStore.init();

  delay(5000);     // required to obtain a reliable response from the following modem.isOn() call 
 
  if(modem.isOn()) modem.powerOff();
  
  delay(1000);
  
  
  boolean taskSuccess;
    
  taskSuccess = modemPowerOn_httpPostStoredReports_modemPowerOff(1024);
  
  
  
  boolean firstPositionAcquired = false;
  
  while(1) {
    
    firstPositionAcquired = acquireCurrentPosition_gpsOnOff(GPS_FIRST_ACQUISITION_HDOP_LIMIT, GPS_FIRST_ACQUISITION_TIMEOUT_IN_SECONDS);
    
    if(firstPositionAcquired) {
      break;
    }
    
    else sleepSeconds(120);
    
  }
 
  // Notes :
  //
  // - the RTC date and time are automatically updated, with the help of the GPS module, at each new position acquisition
  // - we suppose here that the RTC is in an unknown state at the beginning of the sketch, so we need to set the correct date / time a first time by acquiring the position of the station
  // - this is why we go here in a loop until we get a first position : this should not be a problem when the station is used outdoor
  

  taskSuccess = readSensorsAndStoreReport();
  
  taskSuccess = modemPowerOn_httpPostStoredReports_modemPowerOff(1024);
  
}



void loop() {
  
  scheduleNextTaskAndSleep();
  
  executeScheduledTask();
  
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Sleeping / low power management
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void sleepSeconds(unsigned long seconds) {
    
  unsigned long counterStop = (unsigned long) (seconds / 1.08) + 1;
  
  for(unsigned long counter = 0 ; counter < counterStop; counter++) {
    
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
    
 }

}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Time & tasks management 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



unsigned long getTimeStamp(byte year2K, byte month, byte day, byte hour, byte minute, byte second) {
  
  // simplified timestamp computation formula, valid from 2001 to 2099 (year2K = 1->99)
  
  unsigned long timestamp = 978307200;                                                  // 01 Jan 2001 00:00:00 GMT   
  
  timestamp += ((((unsigned long)(year2K-1)) * 365) + (((year2K-1)) / 4))  * 86400;     // timestamp at the beginning of the year
 
  int accuDays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
   
   if(!(year2K % 4)) {
    for(byte i=2 ; i<12 ; i++) accuDays[i] += 1;
  }
 
  timestamp += ((unsigned long)accuDays[month - 1]) * 86400;     // timestamp at the beginning of the month
  
  timestamp += ((unsigned long)day - 1) * 86400;                 // timestamp at the beginning of the day 
  
  timestamp += ((unsigned long)hour) * 3600 + ((int)minute) * 60 + second;

  return timestamp;
  
}



unsigned long getTimeStampNow() {
  
  unsigned long timestampNow = 0;
  
  rtc.getDate();
  rtc.getTime();
  
  byte Y_now = rtc.getYear();
  byte M_now = rtc.getMonth();
  byte D_now = rtc.getDay();
  byte h_now = rtc.getHour();
  byte m_now = rtc.getMinute();
  byte s_now = rtc.getSecond();
  
  timestampNow = getTimeStamp(Y_now, M_now, D_now, h_now, m_now, s_now);
   
  return timestampNow;

}



void scheduleNextTaskAndSleep() {
  
  unsigned long timestampNow = 0;
  
  rtc.getDate();
  rtc.getTime();
  
  byte Y_now = rtc.getYear();
  byte M_now = rtc.getMonth();
  byte D_now = rtc.getDay();
  byte h_now = rtc.getHour();
  byte m_now = rtc.getMinute();
  byte s_now = rtc.getSecond();
  
  timestampNow = getTimeStamp(Y_now, M_now, D_now, h_now, m_now, s_now);
  
  
  // when will the next READ_SENSORS_AND_STORE_REPORT task occur ?         

  unsigned long nextTASK_READ_SENSORS_AND_STORE_REPORT_timestamp;
  
  byte minutes_READ_SENSORS_AND_STORE_REPORT[12] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55};

  byte second_READ_SENSORS_AND_STORE_REPORT = 0;
  
  nextTASK_READ_SENSORS_AND_STORE_REPORT_timestamp = getTimeStamp(Y_now, M_now, D_now, h_now, minutes_READ_SENSORS_AND_STORE_REPORT[0], second_READ_SENSORS_AND_STORE_REPORT) + 3600 ;
  
  for(char i = sizeof(minutes_READ_SENSORS_AND_STORE_REPORT) - 1 ; i>=0 ; i--) {
    
    if((m_now < minutes_READ_SENSORS_AND_STORE_REPORT[i]) or ((m_now == minutes_READ_SENSORS_AND_STORE_REPORT[i]) and (s_now < second_READ_SENSORS_AND_STORE_REPORT))) {
      nextTASK_READ_SENSORS_AND_STORE_REPORT_timestamp = getTimeStamp(Y_now, M_now, D_now, h_now, minutes_READ_SENSORS_AND_STORE_REPORT[i], second_READ_SENSORS_AND_STORE_REPORT);
    }
  
  }
  
  
  // when will the next SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF task occur ?     
  
  unsigned long nextTASK_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF_timestamp;
  
  byte minutes_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF[4] = {0, 15, 30, 45};
  
  byte second_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF = 10;
  
  nextTASK_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF_timestamp = getTimeStamp(Y_now, M_now, D_now, h_now, minutes_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF[0], second_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF) + 3600 ;
  
  for(char i = sizeof(minutes_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF) - 1 ; i>=0 ; i--) {
    
    if((m_now < minutes_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF[i]) or ((m_now == minutes_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF[i]) and (s_now < second_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF))) {
      nextTASK_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF_timestamp = getTimeStamp(Y_now, M_now, D_now, h_now, minutes_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF[i], second_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF);
    }
  
  }
  
  
  // when will the next GPS_ACQUIRE_POSITION task occur ?      

  unsigned long nextTask_GPS_ACQUIRE_POSITION_timestamp;
  
  byte hours_GPS_ACQUIRE_POSITION[4] = {1, 7, 13, 19};
  byte minute_GPS_ACQUIRE_POSITION = 52;

  nextTask_GPS_ACQUIRE_POSITION_timestamp = getTimeStamp(Y_now, M_now, D_now, hours_GPS_ACQUIRE_POSITION[0], minute_GPS_ACQUIRE_POSITION, 0) + 86400 ;

  for(char i = sizeof(hours_GPS_ACQUIRE_POSITION) - 1 ; i>=0 ; i--) {
    
    if((h_now < hours_GPS_ACQUIRE_POSITION[i]) or ((h_now == hours_GPS_ACQUIRE_POSITION[i]) and (m_now < minute_GPS_ACQUIRE_POSITION))) {
      nextTask_GPS_ACQUIRE_POSITION_timestamp = getTimeStamp(Y_now, M_now, D_now, hours_GPS_ACQUIRE_POSITION[i], minute_GPS_ACQUIRE_POSITION, 0);
    }
  
  }
  
 
  // which is the priority task between READ_SENSORS_AND_STORE_REPORT, SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF and GPS_ACQUIRE_POSITION ? 
  
  nextTaskTimestamp = nextTASK_READ_SENSORS_AND_STORE_REPORT_timestamp;
  nextTaskID = TASK_READ_SENSORS_AND_STORE_REPORT;
  
  if(nextTASK_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF_timestamp < nextTaskTimestamp) {
    
    nextTaskTimestamp = nextTASK_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF_timestamp;
    nextTaskID = TASK_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF;
    
  }
  
  if(nextTask_GPS_ACQUIRE_POSITION_timestamp < nextTaskTimestamp) {
    
    nextTaskTimestamp = nextTask_GPS_ACQUIRE_POSITION_timestamp;
    nextTaskID = TASK_GPS_ACQUIRE_POSITION_POWER_ON_POWER_OFF;
    
  }
  
  
  sleepSeconds(nextTaskTimestamp - timestampNow);
  
}



void executeScheduledTask() {
  
  boolean success = false;
  
  unsigned long timestampNow = getTimeStampNow();
 
  while((timestampNow > 0) and (timestampNow < nextTaskTimestamp)) {
   
    sleepSeconds(1);  
    
    timestampNow = getTimeStampNow();
    
  }
  
  if(nextTaskID == TASK_READ_SENSORS_AND_STORE_REPORT) {
    
    success = readSensorsAndStoreReport();
    
  }
  
  else if(nextTaskID == TASK_SEND_STORED_REPORTS_MODEM_POWER_ON_POWER_OFF) {
    
    success = modemPowerOn_httpPostStoredReports_modemPowerOff(1024);
    
  }
  
  else if(nextTaskID == TASK_GPS_ACQUIRE_POSITION_POWER_ON_POWER_OFF) {
    
    success = acquireCurrentPosition_gpsOnOff(GPS_ACQUISITION_HDOP_LIMIT, GPS_ACQUISITION_TIMEOUT_IN_SECONDS);
    
  }
  
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Global position acquisition function (with GPS module power on / power off)
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



boolean acquireCurrentPosition_gpsOnOff(float accuracyLimit, int timeoutInS) {
  
  boolean positionAcquired = false;
  
  gps.powerOn();
  
  gps.configure();
  
  positionAcquired = gps.acquireNewPosition(accuracyLimit, timeoutInS);
  
  if(positionAcquired) {
    
    rtc.setTime(gps.position.fix_h_utc, gps.position.fix_m_utc, gps.position.fix_s_utc);         // hr, min, sec
    rtc.setDate(gps.position.fix_D_utc, 0, gps.position.fix_M_utc, 0, gps.position.fix_Y_utc);   // day, weekday, month, century(1=1900, 0=2000), year(0-99)

  }
  
  gps.powerOff();
  
  return positionAcquired;
  
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Sensors reading and report storing functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



boolean readSensorsAndStoreReport() {
  
  boolean success = false;
  
  char report[REPORT_BUFFER_LENGTH];
  
  char numToCharsBuffer[12];
  
  unsigned long sensorDataAcquisitionTimestamp = getTimeStampNow();
  

  // temperature & humidity sensor power on
  
  digitalWrite(SENSORS_POWER_PIN, LOW);          


  // temperature and humidity reading

  float temperature = TEMPERATURE_UNDEFINED_VALUE;
  float humidity = HUMIDITY_UNDEFINED_VALUE;          
 
  delay(2000);                                          
  
  for(byte i=0 ; i < 3 ; i++) {              
    
    int chk = temperatureHumiditySensor.read22(TEMPERATURE_HUMIDITY_SENSOR_DATA_PIN);
  
    if(chk == DHTLIB_OK) {
    
      temperature = temperatureHumiditySensor.temperature;
      humidity = temperatureHumiditySensor.humidity;
      
      break;
    
    } 
    
    delay(1000);
    
  }
  
  // temperature & humidity sensor power off
  
  digitalWrite(SENSORS_POWER_PIN, HIGH);                    
  
  
  // pressure reading
  
  float pressure = PRESSURE_UNDEFINED_VALUE;
  
  float measuredPressure = pressureSensor.readPressure() / 100.0;
   
  if((measuredPressure > 900.0) and (measuredPressure < 1100.0)) {
     
   pressure = measuredPressure;
     
  }


  // device temperature reading
  
  float deviceTemperature = DEVICE_TEMPERATURE_UNDEFINED_VALUE;
  
  float measuredDeviceTemperature = pressureSensor.readTemperature();
  
  deviceTemperature = measuredDeviceTemperature;
  
  
  // device battery voltage reading
  
  float deviceBatteryVoltage = BATTERY_VOLTAGE_UNDEFINED_VALUE;
  
  unsigned long accuAdcReadings = 0;
    
  for(byte i=0 ; i < 10; i++) {
    accuAdcReadings += analogRead(DEVICE_BATTERY_VOLTAGE_PIN);
  }
  
  if(accuAdcReadings > 0) deviceBatteryVoltage = accuAdcReadings * ADC_REF_VOLTAGE * 156.0 / (10 * 1024 * 56.0);
  
  
  // report construction
  
  char sep[2] = {'|', '\0'};
  
  strcpy(report, STATION_ID);
  
  strcat(report, sep);
  
  if(sensorDataAcquisitionTimestamp != 0) strcat(report, ultoa(sensorDataAcquisitionTimestamp, numToCharsBuffer, 10));
           
  strcat(report, sep);

  if(temperature != TEMPERATURE_UNDEFINED_VALUE) {
   
    strcat(report, dtostrf(temperature, 1, 1, numToCharsBuffer));
    
  }

  strcat(report, sep);
  
  if(humidity != HUMIDITY_UNDEFINED_VALUE) {
    
    strcat(report, dtostrf(humidity, 1, 1, numToCharsBuffer));
    
  }
  
  strcat(report, sep);
  
  if(pressure != PRESSURE_UNDEFINED_VALUE) {
    
    strcat(report, dtostrf(pressure, 1, 1, numToCharsBuffer));
    
  }
  
  if(gps.firstPositionAcquired) {

    strcat(report, sep);
    
    unsigned long fixPositionTimestamp = getTimeStamp(gps.position.fix_Y_utc, gps.position.fix_M_utc, gps.position.fix_D_utc, gps.position.fix_h_utc, gps.position.fix_m_utc, gps.position.fix_s_utc);

    strcat(report, ultoa(fixPositionTimestamp, numToCharsBuffer, 10));
      
    strcat(report, sep);

    strcat(report, dtostrf(gps.position.latitude, 1, 4, numToCharsBuffer));
      
    strcat(report, sep);
      
    strcat(report, dtostrf(gps.position.longitude, 1, 4, numToCharsBuffer));
      
    strcat(report, sep);
      
    strcat(report, itoa(gps.position.altitudeAboveMSL, numToCharsBuffer, 10)); 
    
  }
  
  else {
   
   for(byte i = 0 ; i < 4 ; i++) strcat(report, sep);
    
  }

  
  strcat(report, sep);
  
  if(deviceTemperature != DEVICE_TEMPERATURE_UNDEFINED_VALUE) {
    
    strcat(report, dtostrf(deviceTemperature, 1, 1, numToCharsBuffer));
    
  }
  
  strcat(report, sep);
  
  if(deviceBatteryVoltage != BATTERY_VOLTAGE_UNDEFINED_VALUE) {
    
    strcat(report, dtostrf(deviceBatteryVoltage, 1, 2, numToCharsBuffer));
    
  }
  
 
  // report storing
       
  success = mStore.storeMessage(report);
  
  return success;

 }



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GPRSbee communication functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    
boolean modemPowerOn_httpPostStoredReports_modemPowerOff(int maxNumOfReportsToBeSent) {
  
  boolean success = false;
  
  // are there any stored reports to be sent ?
  
  int numOfReportsStored = mStore.getMessagesCount();
  
  if(numOfReportsStored > 0) {
     
    boolean connectedToNet = modemPowerOn_ConnectToNet();
  
    if(connectedToNet) success = httpPostStoredReports(maxNumOfReportsToBeSent);

    modem.powerOff();
  
  }
  
  else success = true;
  
  return success;
  
}
    
    

boolean modemPowerOn_ConnectToNet() {
    
  if(modem.isOn()) modem.powerOff_On();
  else modem.powerOn();
  
  boolean modemIsOn = modem.isOn();
  
  boolean registered = false;
  boolean GPRSAttached = false;
  boolean connectedToNet = false;
    
  modem.activateCommunication();
  

  // is the modem registered ?
  
  if(modem.isCommunicationActivated()) {
    
    modem.configure();
    
    for(byte attempt=0 ; attempt < 60 ; attempt++) {
      
      registered = modem.isRegistered();
      
      if(registered) break;
      
      delay(1000);

    }
    
  }
  
      
  // GPRS attachment
    
  if(registered) {
    
    delay(500);
 
    for(byte attempt=0 ; attempt < 30 ; attempt++) {
      
      GPRSAttached = modem.isGPRSAttached();
      
      if(GPRSAttached) break;
      
      else {
        
        delay(1000);
        modem.attachGPRS();
        
      }
      
    }

  }
    
    
  // PDP context activation
  
  if(GPRSAttached) {
    
    delay(500);
 
    for(byte attempt=0 ; attempt < 30 ; attempt++) {
      
      connectedToNet = modem.isConnectedToNet();
      
      if(connectedToNet) break;
      
      else {
        
        delay(1000);
        
        modem.connectToNet(GPRS_NETWORK_APN, GPRS_USERNAME, GPRS_PASSWORD);
        
      }
      
    }
  
  }
      
      
  return connectedToNet;
  
}



boolean httpPostStoredReports(int maxNumOfReportsToBeSent) {
  
  // returns true if the first header of the response contains the http code : 200 
  
  boolean success = false;
  
  // are there any stored reports to be sent ?
  
  int numOfReportsStored = mStore.getMessagesCount();
    
  if(numOfReportsStored == 0) success = true;
  
  else {
    
    boolean connectedToNet = modem.isConnectedToNet();
  
    
    if(connectedToNet) {
    
      delay(500);
      
      
      // which is the real number of reports to be sent, and which is the corresponding total file content length ?
      
      int numOfReportsToBeSent = min(numOfReportsStored, maxNumOfReportsToBeSent);
      
      int reportsCounter = 0;
 
      byte reportLength;
      long totalContentLength = 0;    
   

      for(int pageIndex = 0 ; (pageIndex < 1024) && (reportsCounter < numOfReportsToBeSent) ; pageIndex++) {
        
        reportLength = mStore.getMessageLength(pageIndex);
        
        if(reportLength > 0) {
          
          totalContentLength += (reportLength + 2);     // "\r\n" will be sent after each report 
          reportsCounter++;
          
        }
        
      }
      

      byte maxNumOfReportsForOneBlockTransmission = 6;          
      byte maxNumOfReportsPerTransmissionBlock = 10;

      char report[124];

      char incomingCharsBuffer[80];
      
      
      // TCP connection and request transmission
      
      boolean tcpConnectSuccess = modem.tcpConnect(SERVER_NAME, SERVER_PORT, 10);
      
      if(tcpConnectSuccess) {
        
        
        char formFieldName[] = "uploadedfile";                     // this value must march the corresponding form field name on the server side 
        
        
        if(numOfReportsToBeSent <= maxNumOfReportsForOneBlockTransmission) {                            
          
          modem.requestAT(F("AT+CIPSPRT=2"), 2, 2000);             // we don't want the "SEND OK" message to be returned after each transmission
          
          modem.requestAT(F("AT+CIPSEND"), 2, 15000);
          
          modem.echoHttpRequestInitHeaders(SERVER_NAME, SERVER_POST_URL, "POST");
          modem.echoHttpPostFileRequestAdditionalHeadersPart1(totalContentLength, formFieldName);
          
          reportsCounter = 0;
          
          for(int pageIndex = 0 ; (pageIndex < 1024) && (reportsCounter < numOfReportsToBeSent); pageIndex++) {
            
            reportLength = mStore.getMessageLength(pageIndex);
            
            if(reportLength > 0) {
              
              mStore.retrieveMessage(pageIndex, report);
              
              modem.serialConnection.print(report);
              modem.serialConnection.print("\r\n");
              
              reportsCounter++;
              
            }
            
          }
          
          modem.echoHttpPostFileRequestAdditionalHeadersPart2();
          
          modem.serialConnection.print((char) 26);
          
        }
        
          
        else {      
            
          boolean sendError = false;
          
          modem.requestAT(F("AT+CIPSPRT=1"), 2, 2000);             // we want the "SEND OK" message to be returned after each transmission, in order to check that everything was OK 
  
          modem.requestAT(F("AT+CIPSEND"), 2, 15000);
          
          modem.echoHttpRequestInitHeaders(SERVER_NAME, SERVER_POST_URL, "POST");
          modem.echoHttpPostFileRequestAdditionalHeadersPart1(totalContentLength, formFieldName);
          
          modem.serialConnection.print((char) 26);
          
          modem.retrieveIncomingCharsFromLineToLine(incomingCharsBuffer, sizeof(incomingCharsBuffer), 0, 1, 30000);
          if(strstr(incomingCharsBuffer, "OK") == NULL) sendError = true;
                   
      
          delay(300);
          
          
          reportsCounter = 0;
  
          for(int pageIndex = 0 ; (pageIndex < 1024) && (reportsCounter < numOfReportsToBeSent) && !sendError; pageIndex++) {
            
            reportLength = mStore.getMessageLength(pageIndex);
            
            if(reportLength > 0) {
              
              if((reportsCounter % maxNumOfReportsPerTransmissionBlock) == 0) {
                
                delay(300);
                
                modem.requestAT(F("AT+CIPSEND"), 2, 15000);
                
              }
                
              mStore.retrieveMessage(pageIndex, report);
              
              modem.serialConnection.print(report);
              modem.serialConnection.print("\r\n");
              
              
              if(((reportsCounter % maxNumOfReportsPerTransmissionBlock) == (maxNumOfReportsPerTransmissionBlock - 1)) || (reportsCounter == (numOfReportsToBeSent - 1))) {
                
                modem.serialConnection.print((char) 26);
                
                modem.retrieveIncomingCharsFromLineToLine(incomingCharsBuffer, sizeof(incomingCharsBuffer), 0, 1, 30000);                 
                if(strstr(incomingCharsBuffer, "OK") == NULL) sendError = true;
                
              }
              
              reportsCounter++;
              
            }
            
          }
          
          
          if(!sendError) {
        
            delay(300);
          
            modem.requestAT(F("AT+CIPSPRT=2"), 2, 2000);             // we don't want anymore the "SEND OK" message to be returned after each transmission, in order to get only the server's response after the last one
            
            modem.requestAT(F("AT+CIPSEND"), 2, 15000);
            modem.echoHttpPostFileRequestAdditionalHeadersPart2();
            modem.serialConnection.print((char) 26);
            
          }
             
        }
          
        
       // server's response interpretation        
        
        modem.retrieveHttpResponseStatusLine(incomingCharsBuffer, sizeof(incomingCharsBuffer), 90000);    // the timeout must be long enough for the server to respond after the "ingestion" of tens of reports
        
        if(strstr(incomingCharsBuffer, "200") != NULL) success = true;
                
        modem.tcpClose();
        
        
        // if success, we delete the corresponding reports in the store
         
        if(success) {
          
          reportsCounter = 0;
         
          for(int pageIndex = 0 ; (pageIndex < 1024) && (reportsCounter < numOfReportsToBeSent) ; pageIndex++) {
            
            reportLength = mStore.getMessageLength(pageIndex);
            
            if(reportLength > 0) {
              
              mStore.clearPage(pageIndex);
              
              reportsCounter++;
              
            }
            
          }
          
        }  
        
        
      }
    
    }
    
  }

  return success;

}  
    

