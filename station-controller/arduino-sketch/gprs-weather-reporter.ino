/*
 *
 * Description : 
 *
 * GPRS Weather station, configured to report temperature, relative humidity and pressure every 15 mn (POST requests to a web server).
 *
 * The Arduino Pro Mini is put in "sleep" mode between two reports, and the GPS and GPRS modules are also switched off during this period 
 * in order to reduce the power consumption of the station. 
 *
 * The global position of the station is updated every 6 hours : the reports and position acquisition tasks can be easily rescheduled 
 * in the scheduleNextTaskAndSleep() function.
 *
 * Company : Previmeteo
 *
 * Web site : http://oses.previmeteo.com
 *
 * License: GNU GPL v2 (see License.txt)
 *
 * Creation date : 2013/10/08
 * 
 */
 

#include "SoftwareSerial.h"

#include "LowPower.h"

#include "Wire.h"

#include "Rtc_Pcf8563.h"

#include "Adafruit_BMP085.h"

#include "dht.h"

#include "GL865Quad.h"

#include "MTK3339.h"




// pins definition

#define GPS_POWER_PIN 2

#define MODEM_POWER_PIN 3
#define MODEM_TX_PIN 4
#define MODEM_RX_PIN 5

#define DEBUG_TX_PIN 9
#define DEBUG_RX_PIN 255

#define TEMPERATURE_HUMIDITY_SENSOR_DATA_PIN 11
#define TEMPERATURE_HUMIDITY_SENSOR_POWER_PIN 10

#define DEVICE_BATTERY_VOLTAGE_PIN A3


// "undefined values" definition

#define TEMPERATURE_UNDEFINED_VALUE -100.0
#define HUMIDITY_UNDEFINED_VALUE -10.0
#define PRESSURE_UNDEFINED_VALUE 0.0
#define BATTERY_VOLTAGE_UNDEFINED_VALUE -1.0
#define DEVICE_TEMPERATURE_UNDEFINED_VALUE -100.0


// GPS acquisition parameters

#define GPS_FIRST_ACQUISITION_TIMEOUT_IN_SECONDS 600
#define GPS_FIRST_ACQUISITION_ACCURACY_LIMIT_IN_METERS 10.0
#define GPS_ACQUISITION_TIMEOUT_IN_SECONDS 300
#define GPS_ACQUISITION_ACCURACY_LIMIT_IN_METERS 10.0


// GPRS connection parameters

#define GPRS_NETWORK_APN "your_gprs_apn"
#define GPRS_USERNAME "your_gprs_username"
#define GPRS_PASSWORD "your_gprs_password"


// Server connection parameters

#define SERVER_NAME "your_server_host_name"         // Example : www.server.com
#define SERVER_PORT "80"   
#define SERVER_POST_URL "your_server_post_url"      // Example : /weather/post.php


// station identifier

#define STATION_ID "st01"                                    


// encoded data payload buffer length

#define ENCODED_DATA_PAYLOAD_BUFFER_LENGTH 120 


// tasks identifiers

#define TASK_NONE 0
#define TASK_GPS_ACQUIRE_POSITION_POWER_ON_POWER_OFF 1
#define TASK_READ_SENSORS_AND_SEND_MESSAGE_MODEM_POWER_ON_POWER_OFF 2 
#define TASK_RESEND_PREVIOUS_MESSAGE_MODEM_POWER_ON_POWER_OFF 3


// calibration data

#define ADC_REF_VOLTAGE 3.4



SoftwareSerial softSerialDebug(DEBUG_RX_PIN, DEBUG_TX_PIN); 


GL865QuadModem modem(MODEM_POWER_PIN, MODEM_RX_PIN, MODEM_TX_PIN);


MTK3339GpsModule gps(&Serial, GPS_POWER_PIN);


Rtc_Pcf8563 rtc;


dht temperatureHumiditySensor;


Adafruit_BMP085 pressureSensor;


char encodedMessageToSend[ENCODED_DATA_PAYLOAD_BUFFER_LENGTH];


unsigned long timestampStationUp = 0;


byte nextTaskID = TASK_NONE;
unsigned long nextTaskTimestamp = 0;
boolean mustTryToResendPreviousMessage = false;





/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// setup() and loop() functions definition
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void setup()  {
  
  pinMode(TEMPERATURE_HUMIDITY_SENSOR_POWER_PIN, OUTPUT);    
  digitalWrite(TEMPERATURE_HUMIDITY_SENSOR_POWER_PIN, LOW); 
  
  softSerialDebug.begin(9600);
  
  gps.init(9600);
  
  modem.init(9600);

  Wire.begin();
  
  pressureSensor.begin(); 

  delay(500);
  
  boolean acquisitionSuccess = acquireCurrentPosition_gpsOnOff(GPS_FIRST_ACQUISITION_ACCURACY_LIMIT_IN_METERS, GPS_FIRST_ACQUISITION_TIMEOUT_IN_SECONDS, true);

  if(acquisitionSuccess) {
    
    timestampStationUp = getTimeStamp(gps.position.fix_Y_utc, gps.position.fix_M_utc, gps.position.fix_D_utc, gps.position.fix_h_utc, gps.position.fix_m_utc, gps.position.fix_s_utc);
  
  }
  
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
  
  unsigned long counterStop = (unsigned long) (seconds / 1.05) + 1;
  
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
  
  
  // when will the next READ_SENSORS_AND_SEND_MESSAGE task occur ?         

  unsigned long nextTask_READ_SENSORS_AND_SEND_MESSAGE_timestamp;
  
  byte minutes_READ_SENSORS_AND_SEND_MESSAGE[4] = {0, 15, 30, 45};
  byte second_READ_SENSORS_AND_SEND_MESSAGE = 0;
  
  nextTask_READ_SENSORS_AND_SEND_MESSAGE_timestamp = getTimeStamp(Y_now, M_now, D_now, h_now, minutes_READ_SENSORS_AND_SEND_MESSAGE[0], second_READ_SENSORS_AND_SEND_MESSAGE) + 3600 ;
  
  for(char i = sizeof(minutes_READ_SENSORS_AND_SEND_MESSAGE) - 1 ; i>=0 ; i--) {
    
    if((m_now < minutes_READ_SENSORS_AND_SEND_MESSAGE[i]) or ((m_now == minutes_READ_SENSORS_AND_SEND_MESSAGE[i]) and (s_now < second_READ_SENSORS_AND_SEND_MESSAGE))) {
      nextTask_READ_SENSORS_AND_SEND_MESSAGE_timestamp = getTimeStamp(Y_now, M_now, D_now, h_now, minutes_READ_SENSORS_AND_SEND_MESSAGE[i], second_READ_SENSORS_AND_SEND_MESSAGE);
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
  
 
  // which is the priority task between READ_SENSORS_AND_SEND_MESSAGE and GPS_ACQUIRE_POSITION ? 
  
  nextTaskTimestamp = nextTask_READ_SENSORS_AND_SEND_MESSAGE_timestamp;
  nextTaskID = TASK_READ_SENSORS_AND_SEND_MESSAGE_MODEM_POWER_ON_POWER_OFF;
  
  if(nextTask_GPS_ACQUIRE_POSITION_timestamp < nextTaskTimestamp) {
    
    nextTaskTimestamp = nextTask_GPS_ACQUIRE_POSITION_timestamp;
    nextTaskID = TASK_GPS_ACQUIRE_POSITION_POWER_ON_POWER_OFF;
    
  }
  

  //  has the RESEND_MESSAGE task been required ?
  
  if(mustTryToResendPreviousMessage) {
    
    byte resendDelayInSeconds = 60;
    
    if((timestampNow + resendDelayInSeconds + GPS_ACQUISITION_TIMEOUT_IN_SECONDS < nextTaskTimestamp)) {
          
      // the RESEND_PREVIOUS_MESSAGE becomes the new priority task
      
      nextTaskTimestamp = timestampNow + resendDelayInSeconds;
      nextTaskID = TASK_RESEND_PREVIOUS_MESSAGE_MODEM_POWER_ON_POWER_OFF;
     
    }
       
  }
  

  mustTryToResendPreviousMessage = false;     // there will be only one attempt to resend the message (if required)   

  sleepSeconds(nextTaskTimestamp - timestampNow);
  
}



void executeScheduledTask() {
  
  boolean success = false;
  
  unsigned long timestampNow = getTimeStampNow();
 
  while((timestampNow > 0) and (timestampNow < nextTaskTimestamp)) {
   
    sleepSeconds(1);  
    
    timestampNow = getTimeStampNow();
    
  }
  
  if(nextTaskID == TASK_READ_SENSORS_AND_SEND_MESSAGE_MODEM_POWER_ON_POWER_OFF) {
    
    success = readSensorsAndSendMessageToServer_modemOnOff();
    
    mustTryToResendPreviousMessage = !success;
    
  }
  
  else if(nextTaskID == TASK_GPS_ACQUIRE_POSITION_POWER_ON_POWER_OFF) {
    
    success = acquireCurrentPosition_gpsOnOff(GPS_ACQUISITION_ACCURACY_LIMIT_IN_METERS, GPS_ACQUISITION_TIMEOUT_IN_SECONDS, true);
    
  }
  
  else if (nextTaskID == TASK_RESEND_PREVIOUS_MESSAGE_MODEM_POWER_ON_POWER_OFF) {      
  
    success = resendPreviousMessageToServer_modemOnOff();

  }
  
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Global position acquisition function (with GPS module power on / power off)
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


boolean acquireCurrentPosition_gpsOnOff(float accuracyLimit, int timeoutInS, boolean updateDateTimeIfsuccess) {

  boolean newPositionAcquired = false;
  
  gps.powerOn();
  
  gps.configure();
  
  newPositionAcquired = gps.acquireNewPosition(accuracyLimit, timeoutInS);
  
  if(newPositionAcquired && updateDateTimeIfsuccess) {
    
    rtc.setTime(gps.position.fix_h_utc, gps.position.fix_m_utc, gps.position.fix_s_utc);         //hr, min, sec
    rtc.setDate(gps.position.fix_D_utc, 0, gps.position.fix_M_utc, 0, gps.position.fix_Y_utc);   //day, weekday, month, century(1=1900, 0=2000), year(0-99)
  

}
  
  gps.powerOff();
  
  return newPositionAcquired;
  
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Sensors reading and message sending / resending functions (with GPRS modem power on / power off)
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


boolean readSensorsAndSendMessageToServer_modemOnOff() {
  
  boolean success = false;
  
  char numToCharsBuffer[12];
  
  rtc.getDate();
  rtc.getTime();
  
  unsigned long sensorDataAcquisitionTimestamp = getTimeStamp(rtc.getYear(), rtc.getMonth(), rtc.getDay(), rtc.getHour(), rtc.getMinute(), rtc.getSecond());
  

  // temperature and humidity reading

  float temperature = TEMPERATURE_UNDEFINED_VALUE;
  float humidity = HUMIDITY_UNDEFINED_VALUE;
  
  
  digitalWrite(TEMPERATURE_HUMIDITY_SENSOR_POWER_PIN, HIGH);                    
 
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
  
  digitalWrite(TEMPERATURE_HUMIDITY_SENSOR_POWER_PIN, LOW);                    
  
  
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
  
  
  // encoded message construction
  
  char sep[2] = {'|', '\0'};

  strcpy(encodedMessageToSend, "P=");
  
  strcat(encodedMessageToSend, STATION_ID);
  
  strcat(encodedMessageToSend, sep);
  
  strcat(encodedMessageToSend, ultoa(sensorDataAcquisitionTimestamp, numToCharsBuffer, 10));
           
  strcat(encodedMessageToSend, sep);

  if(temperature != TEMPERATURE_UNDEFINED_VALUE) {
   
    strcat(encodedMessageToSend, dtostrf(temperature, 1, 1, numToCharsBuffer));
    
  }

  strcat(encodedMessageToSend, sep);
  
  if(humidity != HUMIDITY_UNDEFINED_VALUE) {
    
    strcat(encodedMessageToSend, dtostrf(humidity, 1, 1, numToCharsBuffer));
    
  }
  
  strcat(encodedMessageToSend, sep);
  
  if(pressure != PRESSURE_UNDEFINED_VALUE) {
    
    strcat(encodedMessageToSend, dtostrf(pressure, 1, 1, numToCharsBuffer));
    
  }
  
  if(gps.firstPositionAcquired) {

    strcat(encodedMessageToSend, sep);
    
    unsigned long fixPositionTimestamp = getTimeStamp(gps.position.fix_Y_utc, gps.position.fix_M_utc, gps.position.fix_D_utc, gps.position.fix_h_utc, gps.position.fix_m_utc, gps.position.fix_s_utc);

    strcat(encodedMessageToSend, ultoa(fixPositionTimestamp, numToCharsBuffer, 10));
      
    strcat(encodedMessageToSend, sep);

    strcat(encodedMessageToSend, dtostrf(gps.position.latitude, 1, 4, numToCharsBuffer));
      
    strcat(encodedMessageToSend, sep);
      
    strcat(encodedMessageToSend, dtostrf(gps.position.longitude, 1, 4, numToCharsBuffer));
      
    strcat(encodedMessageToSend, sep);
      
    strcat(encodedMessageToSend, itoa(gps.position.heightAboveEllipsoid, numToCharsBuffer, 10)); 
    
  }
  
  else {
   
   for(byte i = 0 ; i < 4 ; i++) strcat(encodedMessageToSend, sep);
    
  }

  
  strcat(encodedMessageToSend, sep);
  
  if(deviceTemperature != DEVICE_TEMPERATURE_UNDEFINED_VALUE) {
    
    strcat(encodedMessageToSend, dtostrf(deviceTemperature, 1, 1, numToCharsBuffer));
    
  }
  
  strcat(encodedMessageToSend, sep);
  
  if(deviceBatteryVoltage != BATTERY_VOLTAGE_UNDEFINED_VALUE) {
    
    strcat(encodedMessageToSend, dtostrf(deviceBatteryVoltage, 1, 1, numToCharsBuffer));
    
  }
  
 
  // message sending
  
  // softSerialDebug.println(encodedMessageToSend)
       
  success = sendDataToServer_modemOnOff(encodedMessageToSend);
  
  return success;

 }



boolean resendPreviousMessageToServer_modemOnOff() {

  boolean success = false;

  success = sendDataToServer_modemOnOff(encodedMessageToSend);
  
  return success;

}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Communication functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


boolean sendDataToServer_modemOnOff(char *encodedMessage) {
  
  boolean success = false;
 
  modem.powerOn();
  
  modem.activateCommunication();
  
  boolean communicationActivated = modem.isCommunicationActivated();
  
  if(communicationActivated) {
  
    modem.configure();
    
    success = sendDataToServer(encodedMessage);
  
  }
  
  modem.powerOff();
  
  return success;
  
}



boolean sendDataToServer(char *encodedMessage) {
  
  boolean success = false; 
      
      
  // is the modem registered ?
  
  boolean registered = false;
  
  for(byte attempt=0 ; attempt < 10 ; attempt++) {
    
    registered = modem.isRegistered();
    
    if(registered) break;
    
    delay(5000);
    
  }
  
  if(registered) delay(500);

  
  // GPRS attachment
  
  boolean GPRSAttached = false;
  
  if(registered) {
 
    for(byte attempt=0 ; attempt < 10 ; attempt++) {
      
      GPRSAttached = modem.isGPRSAttached();
      
      if(GPRSAttached) break;
      
      else {
        
        delay(3000);
        modem.attachGPRS();
        
      }
      
    }
  
  }
  
  if(GPRSAttached) delay(500);

  
  // PDP context activation
  
  boolean PDPactivated = false;
  
  if(GPRSAttached) {
 
    for(byte attempt=0 ; attempt < 10 ; attempt++) {
      
      PDPactivated = modem.isPDPActivated();
      
      if(PDPactivated) break;
      
      else {
        
        delay(3000);
        
        modem.activatePDP(GPRS_NETWORK_APN, GPRS_USERNAME, GPRS_PASSWORD);
        
      }
      
    }
  
  }
  
  if(PDPactivated) delay(500);
  
  
  // Server post request
  
  boolean httpPostSuccess = false;
  
  if(PDPactivated) {
    
    for(byte attempt=0 ; attempt < 3 ; attempt++) {
    
      httpPostSuccess = modem.httpPost(SERVER_NAME, SERVER_PORT, SERVER_POST_URL, encodedMessage, 10);
      
      if(httpPostSuccess) {
        
        success = true;
        break;
        
      }
      
      else delay(5000);
      
    }
    
  }
  
  return success;
  
}


