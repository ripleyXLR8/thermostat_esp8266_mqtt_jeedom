# What is it?
I wanted to have a standalone module with a small display and a few buttons to start and stop the thermostat, and increase/decrease the setpoint temperature. The thermostat is managed by Jeedom an open-source domotic platform. (https://www.jeedom.com).

# What do you need?
* A working Jeedom server (https://www.jeedom.com).
* The Thermostat plugin installed and running on your Jeedom server. You must be able to control it from the Jeedom Web-UI or from the Jeedom smartphone application.(https://www.jeedom.com/market/index.php?v=d&p=market&type=plugin&plugin_id=thermostat&&name=thermostat)
* The MQTT plugin installed and running on your Jeedom server. (https://www.jeedom.com/market/index.php?v=d&p=market&type=plugin&plugin_id=thermostat&&name=MQTT)
* An ESP8266 module. It should work with any ESP8266 module if it has a sufficient amount of GPIO. (I'm using a NodeMCU 0.9 ESP8266 ESP-12 board)
* An 64 x 128 OLED I2C display. It could thericaly work with any display but you will have to adapt the DisplayInit, RefreshDisplay, DisplayOn and DisplayOff to fit your display.
* 3 buttons andn 3 pull-up resistor (ON/OFF, increase temperature, decrease temperature)

# Setup
## Jeedom setup
### Creation of MQQT topics and command in Jeedom
Inside the MQTT plugin, you will have to create **4 topics in one MQTT equipment**.
I recommend you to set your MQTT plugin on "topic auto-discovery" and use a MQTT client (I'm using SimpleMessage on OSX) to create all the topics (just send a message with a dummy value and the MQTT plugin will create the topic). You can change the name of this topics but then, you will have to make the change in the module source code accordingly.

* **all_info** : This topic will contain a JSON object containing all the information of the thermostat. It will be populated by the thermostat object in Jeedom each time a value is change on Jeedom or when the module request an update.
* **consigne_temp** : This topic will contain the setpoint temperature set by the module. When the module will publish a new setpoint temperature, a scenario in Jeedom will change the setpoint temperature of the thermostat accordingly.
* **on_off** : This topic will contain the state of the thermostat set by the ESP8266 module (true or false for ON and OFF). When the module will publish a new state, a scenario in Jeedom will change the state of the thermostat accordingly.
* **update_request** : The ESP8266 module will publish a new value to this topic each time it needs to force the update of the "all_info" topic. In the current version, the module publish the millis() value (number of milliseconds since the module has been started) to this topic to ensure the value is always different and trigger the scenario at every change.... (I guess there is a bug around 29 days when the millis() rollback to 0.... but the module will just skip a forced update, which is not a big issue)

Then you will have to create one MQTT command inside the same MQTT. This command will publish the json object inside the all_info topic. Your JSON object must be on this type (once again, you can change the name of the variable... but you will have to change it accordingly in the source code)

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

1) all_info_update

2) on_off_update
This scenario will be triggered when a new message is published on the "on_off" topic. Then if the message is "true" it reset the current setpoint temperature to its current value. If the message is "false" it set the thermotstat mode to "Off". You will find below the view of my scenario in my Jeedom installation.

![Alt Creation of the ON_OFF_UPDATE scenario](/mqtt_scenario_1_jeedom.jpeg?raw=true "Creation of the ON_OFF_UPDATE scenario")

3) setpoint_update
This scenario will be triggered when a new message is published on the "consigne_temp" topic. The published value is then send to the thermostat as the consigne temp. You will find below the view of my scenario in my Jeedom installation.

![Alt Creation of the SETPOINT_UPDATE scenario](/mqtt_scenario_2_jeedom.jpeg?raw=true "Creation of the SETPOINT_UPDATE scenario")


# Future evolutions?
* I'm thinking about solar powering the module. I will first implement a deepsleep mode and then mesure the current to see what battery and what solar panel to choose.

* Writting a plugin to integrate it more "cleanly" inside jeedom will be good.
