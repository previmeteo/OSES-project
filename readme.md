The Open Source Environmental Sensors (OSES) project 
==============

This project, based on the Arduino and Launchpad / Energia platforms, aims to develop an **autonomous and modular station allowing the reporting, in near realtime via GPRS or other long range wireless technologies, of data collected by a wide range of environmental sensors**.

The goals of this project are :

- create a modular station on which may be plugged various environmental sensors such as weather and air quality meters
- design an easy to assemble station using low-cost and easy to obtain components and modules
- build an energetically autonomous station, powered by the sun or other renewable energies
- build a connected station which can upload, in realtime or near realtime, data to the "cloud" which can be displayed on smartphones or web-sites for example
- embed a GPS module and a RTC clock in order to know exactly where the station is, and what time is it, when it uploads its data  


Hardware and schematics
==============

The schematics and bill of materials required to assemble the current prototype can be found in the "hardware" directory. 

This prototype can be powered by a battery with 3.5V <= VBAT <= 9V. The battery is charged by a small sized solar panel.


Code ans libraries
==============

The code of the station controller (Arduino Pro Mini 328) can be found in the arduino-sketch directory. Take also a look at the librairies/librairies-installation.txt for a quick guide on how to download and install the required librairies.





