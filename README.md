# What is it?

I'm using an open-source software called jeedom (https://www.jeedom.com) to make some automatisation in my house. I have temperature probes in different room and electrical relay on heaters (mainly z-waves relay and probes), so i can use the Jeedom Web-UI or the Jeedom smartphone app to switch heaters ON and OFF or change the setpoint temperature. This is great, but sometimes, you don't want to use your computer or your smartphone is off and you still want to change the temperature...

That's why I wanted to create a standalone module with a small display and a few buttons to start and stop the thermostat, and increase/decrease the setpoint temperature.

![Alt Module](/15053415_10154125135750765_761397922_o.jpg?raw=true "Creation of the SETPOINT_UPDATE scenario")

# Thanks to ...
* Lunarok from the Jeedom forum for is MQTT plugin : https://www.jeedom.com/forum/viewtopic.php?f=28&t=5764
* Loic and sarakha63 from the Jeedom for the Thermostat plugin : https://www.jeedom.com/market/index.php?v=d&p=market&type=plugin&plugin_id=thermostat
* Benoit Blanchon for his JSON library : https://github.com/bblanchon/ArduinoJson
* Ivan Grokhtkov <ivan@esp8266.com> for his ESP8266 Wifi library : https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi
* Nick O'Larry for his MQTT library : https://github.com/knolleary/pubsubclient
* To the jeedom forum for helping me : https://www.jeedom.com/forum/index.php

# What do you need?
* A working Jeedom server (https://www.jeedom.com).
* The Thermostat plugin installed and running on your Jeedom server. You must be able to control it from the Jeedom Web-UI or from the Jeedom smartphone application.(https://www.jeedom.com/market/index.php?v=d&p=market&type=plugin&plugin_id=thermostat&&name=thermostat)
* The MQTT plugin installed and running on your Jeedom server. (https://www.jeedom.com/market/index.php?v=d&p=market&type=plugin&plugin_id=thermostat&&name=MQTT)
* An ESP8266 module. It should work with any ESP8266 module if it has a sufficient amount of GPIO. (I'm using a NodeMCU 0.9 ESP8266 ESP-12 board)
* An 64 x 128 OLED I2C display. It could thericaly work with any display but you will have to adapt the DisplayInit, RefreshDisplay, DisplayOn and DisplayOff to fit your display.
* 3 buttons andn 3 pull-up resistor (10kΩ) (ON/OFF, increase temperature, decrease temperature)

# Setup
## Jeedom setup
### Creation of MQQT topics
Inside the MQTT plugin, you will have to create **4 topics in one MQTT equipment**.
I recommend you to set your MQTT plugin on "topic auto-discovery" and use a MQTT client (I'm using SimpleMessage on OSX) to create all the topics (just send a message with a dummy value and the MQTT plugin will create the topic). You can change the name of this topics but then, you will have to make the change in the module source code accordingly.

* **all_info** : This topic will contain a JSON object containing all the information of the thermostat. It will be populated by the thermostat object in Jeedom each time a value is change on Jeedom or when the module request an update.
* **consigne_temp** : This topic will contain the setpoint temperature set by the module. When the module will publish a new setpoint temperature, a scenario in Jeedom will change the setpoint temperature of the thermostat accordingly.
* **on_off** : This topic will contain the state of the thermostat set by the ESP8266 module (true or false for ON and OFF). When the module will publish a new state, a scenario in Jeedom will change the state of the thermostat accordingly.
* **update_request** : The ESP8266 module will publish a new value to this topic each time it needs to force the update of the "all_info" topic. In the current version, the module publish the millis() value (number of milliseconds since the module has been started) to this topic to ensure the value is always different and trigger the scenario at every change.... (I guess there is a bug around 29 days when the millis() rollback to 0.... but the module will just skip a forced update, which is not a big issue)

###  Creatin of MQTT command "update_data"
Then you will have to create one MQTT command inside the same MQTT. This command will publish the json object inside the all_info topic. Your JSON object must be on this type (once again, you can change the name of the variable inside the JSON object... but again, you will have to change it accordingly in the source code)

`{
  "temperature":X,
  "consigne":Y,
  "mode":Z,
  "statut":XX,
  "actif":YY
  }`
  
* temperature will hold the value of the temperature in the room
* consigne will hold the value of the temperature setpoint in the room
* mode will hold the value of the current thermostat mode (comfort, economy, off, ....)
* statut will hold the value of the current heating status of the thermostat (heating or not)
* actif will hold the actif value of the thermostat (i'm not using this value for the moment but....)
  
For your information, here is my personnal value :
  
`{"temperature":#[Salon][Thermostat Salon][Température]#,"consigne":#[Salon][Thermostat Salon][Consigne]#,"mode":#[Salon][Thermostat Salon][Mode]#,"statut":#[Salon][Thermostat Salon][Statut]#,"actif":#[Salon][Thermostat Salon][Actif]#}`

At the end you must obtain something like that :

![Alt Configuration of the MQTT plugin](/mqtt_config_jeedom.jpeg?raw=true "Configuration of the MQTT plugin")

### Creation of Scenarios in Jeedom

You will have to create 3 differents "triggered" scenarii.

#### 1) all_info_update scenario
This scenario will be triggered by any change on the thermostat value. It will then execute the "update_data" MQTT command.
**Important note** : This scenario must also be triggered when a new message is triggered in the update_request topic.
You will find below the view of my scenario in my Jeedom installation.

![Alt Creation of the ALL_INFO_UPDATE scenario](/mqtt_scenario_3_jeedom.jpeg?raw=true "Creation of the ALL_INFO_UPDATE scenario")

#### 2) on_off_update scenario
This scenario will be triggered when a new message is published on the "on_off" topic. Then if the message is "true" it reset the current setpoint temperature to its current value. If the message is "false" it set the thermotstat mode to "Off". You will find below the view of my scenario in my Jeedom installation.

![Alt Creation of the ON_OFF_UPDATE scenario](/mqtt_scenario_1_jeedom.jpeg?raw=true "Creation of the ON_OFF_UPDATE scenario")

#### 3) setpoint_update scenario
This scenario will be triggered when a new message is published on the "consigne_temp" topic. The published value is then send to the thermostat as the consigne temp. You will find below the view of my scenario in my Jeedom installation.

![Alt Creation of the SETPOINT_UPDATE scenario](/mqtt_scenario_2_jeedom.jpeg?raw=true "Creation of the SETPOINT_UPDATE scenario")

## ESP8266 Setup
### Hardware

![Alt Schema Hardware Thermostat](/schema_thermostat.png?raw=true "Creation of the SETPOINT_UPDATE scenario")

### Software
Just download the "thermostat_esp8266_mqtt_jeedom.ino" file and open it with your Arduino IDE. I assume you know how to configure your IDE to work with your board.

You can then modify the code to fit your needs. Below are the mandatory variable you have to edit.

* **const char* mqtt_server** : Here is the IP of your MQTT server which is very likely to be the IP of your Jeedom server.
* **const int mqtt_port** : The port of your MQTT server... very likely to be 1883.
* **const char* mqtt_all_info_topic** : This must be the name of your all_info topic in the Jeedom MQTT plugin.
* **const char* mqtt_ON_OFF_topic** : This must be the name of your ON_OFF topic in the Jeedom MQTT plugin.
* **const char* mqtt_update_request_topic** : This must be the name of your update_request topic in the Jeedom MQTT plugin.
* **const char* mqtt_consigne_temp_topic** : This must be the name of your setpoint temperature topic in the Jeedom MQTT plugin.
* **const char* Network_SSID** : Your WiFi network SSID
* **const char* Network_Password** : Your WiFi network password
* **int ON_OFF_Button** : The pin where the ON OFF button is connected
* **int increaseTempButton** : The pin where the increase button is connected
* **int decreaseTempButton** : The pin where the decrease button is connected
* **int sda_i2c_pin** : The sda pin of your display
* **int scl_i2c_pin** : The scl pin of your display
* **uint8_t display_i2c_address** : The I2C adress of your display

Just compile it and upload it to your ESP8266 and it should work...

# Future evolutions?
* Making an enclosure to integrate it in my living room. I still don't wall if it will be wall mounted inside a wall power socket with a power supply or place in a case with a rechargable battery.

* I'm thinking about solar powering the module. I will first implement a deepsleep mode and then maesure the current to see what battery and what solar cell choose.

* Writting a plugin to integrate it more "cleanly" inside jeedom will be good. (Not having to set scenario....)

* ~~ Replace the adafruit logo by a fire on the display when the heating is on.~~ DONE

# License
This project is published under the GNU GENERAL PUBLIC LICENSE. Feel free to use it, modify it, distribute it, etc... See Licence.md for more details.
If this project helps you, don't hesitate to send me a message ;-) and remember, software is like sex, it's better when it's free.
