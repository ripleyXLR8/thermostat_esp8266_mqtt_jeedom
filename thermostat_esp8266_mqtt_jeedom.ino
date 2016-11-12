#include <ArduinoJson.h> // Library for Json Parsing
#include <Ticker.h> // Library for Timer management
#include <ESP8266WiFi.h> // Library for Wifi management
#include <PubSubClient.h> // Library for the MQTT client
#include <Wire.h> // Library for I2C
#include <Adafruit_GFX.h> // Library for OLED Display
#include <Adafruit_SSD1306.h>

#include <gfxfont.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

Adafruit_SSD1306 display; // Déclaration de l'écran
Ticker autoUpdateTimer; // Timer for publishing sending the auto update p
Ticker autoSleepTimer; // Timer for going into deepspleep mode
WiFiClient WiFiClient; // Wifi client declaration
PubSubClient MQTTclient(WiFiClient); // MQTT client declaration

//MQTT Configuration data
const char* mqtt_server = "IP OF YOUR MQTT SERVER - JEEDOM IP";
const int mqtt_port = 1883;
const char* mqtt_all_info_topic = "thermostat_salon_mqtt/all_info";
const char* mqtt_ON_OFF_topic = "thermostat_salon_mqtt/on_off";
const char* mqtt_update_request_topic = "thermostat_salon_mqtt/update_request";
const char* mqtt_consigne_temp_topic = "thermostat_salon_mqtt/consigne_temp";
const char* mqqt_identifier = "ESP8266_therm_salon";
const String room_name = "Therm. salon"; // String to be displayed at the top of the display

// Configuration variable
float step_temp_change = 0.5; // Step between two consigne temperature change
float max_temp_consigne = 25; // Température consigne maximale
float min_temp_consigne = 5; // Température consigne minimale
int refresh_rate = 60; // Time between two forced refresh in second.
int sleep_time_if_no_activity = 10; // The system will go in deepsleep mode if no activity during after this time in second.
int time_before_send_command = 1000; // Time in microsecond before sending the update consigne command to reduce the number of message published when adjusting the temp.

// Wifi Configuration Data
const char* Network_SSID = "YOUR NETWORK SSID"; // SSID for the WiFi Network
const char* Network_Password = "YOUR WIFI PASSWORD"; // Password for the WiFi Network

// Pin configuration Data
int ON_OFF_Button = D5; // ON OFF Button
int increaseTempButton = D1; //Increase Temperature button
int decreaseTempButton = D2; // Decrease Temperature button
int sda_i2c_pin = D3; // i2c sda pin
int scl_i2c_pin = D4; // i2c scl pin
uint8_t display_i2c_address = 0x3C; // i2c display address

long debouncing_time = 200; // Debouncing Time in Milliseconds
volatile unsigned long last_micros; // Variable to store the last time a key pressed (for debouncing purpose)
volatile unsigned long next_min_micros_to_send_consigne; // Variable to store the next time where we will authorize the module to publish a message.

// Main variable declaration and initialization
float temp = 0; // The room temperature
float temp_consigne = 0; // The consigne temperature
bool etat_thermostat = false; // Etat du thermostat (ON / OFF)
bool etat_chauffe = false; // Etat chauffe (ON / OFF)

// Flag variable for the key interrupt or screen state
bool consigne_to_be_increased = false;
bool consigne_to_be_decreased = false;
bool ON_OFF_to_be_switched = false;
bool display_state = false;
bool new_consigne_to_be_sent = false;

static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B10000000,
  B00000001, B10000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B00000001, B11110000,
  B00000100, B11110000,
  B00001111, B11111000,
  B00001111, B11111000,
  B00011111, B01111100,
  B00111111, B00111100,
  B00111110, B00111100,
  B00111100, B00011100,
  B00011100, B00011000,
  B00001100, B00011000,
  B00000110, B00110000 };

 
void setup() {
  Serial.begin(115200); // Setting up the communication speed
  Serial.println("INFO - Serial port is ready."); 
  Serial.println("INFO - Beggining of the Setup.");

  DisplayInit();   // Init display
  WifiClientConnection();  // Wifi client initialization
  MQTTclientSetup();  // MQTT client setup (port, adress, callback)
  
  autoUpdateTimer.attach(refresh_rate,sendMQTTUpdateRequest); // Start the auto-publishing timer
  autoSleepTimer.attach(sleep_time_if_no_activity, activate_sleeping_mode); // Start the auto-sleep timer
     
  attachInterrupt(ON_OFF_Button, debounceInterrupt_ON_OFF, FALLING); // attache l'interruption au bouton d'augmentation de la température
  attachInterrupt(increaseTempButton, debounceInterrupt_INCREASE, FALLING); // attache l'interruption au bouton d'augmentation de la température
  attachInterrupt(decreaseTempButton, debounceInterrupt_DECREASE, FALLING); // attache l'interruption au bouton d'augmentation de la température

  Serial.println("INFO - End of the Setup... Starting the loop.");
}

void sendMQTTUpdateRequest(){

  Serial.println("INFO - Publishing refresh message over MQTT");

  char* temp_millis_builder_char = "";
  ltoa(millis(), temp_millis_builder_char, 10);
  MQTTclient.publish(mqtt_update_request_topic, temp_millis_builder_char);
}

void displayOff() {
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  display.ssd1306_command(SSD1306_CHARGEPUMP);
  display.ssd1306_command(SSD1306_SETHIGHCOLUMN);
}

void displayOn() {
  display.ssd1306_command(SSD1306_CHARGEPUMP);
  display.ssd1306_command(0x14);
  display.ssd1306_command(SSD1306_DISPLAYON);
}

void debounceInterrupt_ON_OFF() {
  
  if((long)(micros() - last_micros) >= debouncing_time * 1000) {
    Serial.println("INFO - ON OFF button pressed.");

    last_micros = micros();
    
    // A key has been pressed, we are postponing the sleep.
    if(!key_pressed_routine()){
      Serial.println("INFO - Going out Sleeping mode.");
      return;
    }
    ON_OFF_to_be_switched = true;
  }
}

void debounceInterrupt_INCREASE() {
  
  if((long)(micros() - last_micros) >= debouncing_time * 1000) {
    Serial.println("INFO - INCREASE button pressed.");

    last_micros = micros();

    // A key has been pressed, we are postponing the sleep.
    if(!key_pressed_routine()){
      Serial.println("INFO - Going out Sleeping mode.");
      return;
    }
    
    if(temp_consigne < max_temp_consigne){
      consigne_to_be_increased = true;
      // We are calculating the next time we are authorizing the program to publish the new consigne temperature.
      // Each time we presse the + or - button, this time is increased.
      next_min_micros_to_send_consigne = millis() + time_before_send_command;
    } else {
      Serial.println("INFO - Consigne allready at the maximum value. No change made.");
    }
  }
}

void debounceInterrupt_DECREASE() {
  
  if((long)(micros() - last_micros) >= debouncing_time * 1000) {
    Serial.println("INFO - DECREASE button pressed.");

    last_micros = micros();

    // A key has been pressed, we are postponing the sleep.
    if(!key_pressed_routine()){
      Serial.println("INFO - Going out Sleeping mode.");
      return;
    }

    if(temp_consigne > min_temp_consigne){
      consigne_to_be_decreased = true;
      // We are calculating the next time we are authorizing the program to publish the new consigne temperature.
      // Each time we presse the + or - button, this time is increased.
       next_min_micros_to_send_consigne = millis() + time_before_send_command; 
    } else {
      Serial.println("INFO - Consigne allready at the minimum value. No change made.");
    }
  }
}

void activate_sleeping_mode(){
  if(display_state){
      displayOff();
      display_state = false;
      autoSleepTimer.detach();
      Serial.println("INFO - Activating sleep mode.");
  }
}

bool key_pressed_routine(){
 
  bool result_temp = false;

  if(!display_state){
    displayOn();
    display_state = true;
    Serial.println("INFO - Event occured - Switching display ON");
    result_temp = false;
  } else {
    result_temp = true;
  }
 
  // We are detaching are re-attaching the timer to reset it.
  autoSleepTimer.detach();
  autoSleepTimer.attach(sleep_time_if_no_activity, activate_sleeping_mode);
  Serial.println("INFO - Event occured - Postponing sleep time");

  return result_temp;
}

void loop() {

  // Checking and reconnecting Wifi Client, we are looping here untill the wifi connection is established.
  if(WiFi.status() != WL_CONNECTED){ 
    Serial.println("ERROR - No Wifi connection. Trying to connect (or reconnect) in 5 seconds.");
    delay(5000);
    WifiClientConnection();
  }

  // Checking and reconnecting MQTT Client, we are looping here untill the connection to the MQTT server is established.
  if (!MQTTclient.connected()) {
    MQTTclientReconnect();
  }
  MQTTclient.loop();

  //server.handleClient();

  if(ON_OFF_to_be_switched){
    ON_OFF_to_be_switched = false;
    ON_OFF_THERMOSTAT();
  }

  // On vérifie si l'un des flags de modification de la consigne a été activé
  if(consigne_to_be_increased){
    temp_consigne = temp_consigne + step_temp_change;
    RefreshDisplay(room_name, temp, temp_consigne, etat_thermostat, etat_chauffe);
    Serial.print("INFO - Nouvelle consigne : ");
    Serial.println(temp_consigne);
    consigne_to_be_increased = false;
    new_consigne_to_be_sent = true;
  } else if (consigne_to_be_decreased) {
    temp_consigne = temp_consigne - step_temp_change;
    RefreshDisplay(room_name, temp, temp_consigne, etat_thermostat, etat_chauffe);
    Serial.print("INFO - Nouvelle consigne : ");
    Serial.println(temp_consigne);
    consigne_to_be_decreased = false;
    new_consigne_to_be_sent = true;
  }

  // If the millis time is over the timer to send the change consigne temp, then we send the request, else... we wait.
  // This is to reduce the number of publish message when we set the temperature. We don't want to publish a message each time we presse a button.
  
  if( (millis() > next_min_micros_to_send_consigne) && new_consigne_to_be_sent) {
    new_consigne_to_be_sent = false;
    publish_consigne_temp(temp_consigne);
  }
  
}

void publish_consigne_temp(float temp){

  // Converting the float temperature to char[] in order to send it with mqtt.
  char temp_data_to_be_sent[10];
  dtostrf(temp,2, 1, temp_data_to_be_sent);
 
  Serial.print("INFO : Publishing the new consigne temp : ");
  Serial.println(temp_data_to_be_sent);
    
  MQTTclient.publish(mqtt_consigne_temp_topic, temp_data_to_be_sent);

}

void ON_OFF_THERMOSTAT(){
  
  Serial.println("INFO : Inversing thermostat state");
  
  if(etat_thermostat) {
    MQTTclient.publish(mqtt_ON_OFF_topic, "false");
    Serial.println("INFO - Publishing OFF for the thermostat state");
  } else {
    MQTTclient.publish(mqtt_ON_OFF_topic, "true");
    Serial.println("INFO - Publishing ON for the thermostat state");
  }

  etat_thermostat = !etat_thermostat;

  RefreshDisplay(room_name, temp, temp_consigne, etat_thermostat, etat_chauffe);
}

void parseIncommingJSON(char payload[]){
  
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(payload);

  if (!root.success()) {
    Serial.println("ERROR - Incomming JSON Parsing Fail");
  } else {
    Serial.println("INFO - Incomming JSON Parsing Succeed");
    temp_consigne = root["consigne"];
    temp = root["temperature"];

    String temp1 = root["statut"];
    String temp2 = root["mode"];

    etat_chauffe = temp1.equals("Chauffage");
    etat_thermostat = !temp2.equals("Off");
    
    RefreshDisplay(room_name, temp, temp_consigne, etat_thermostat, etat_chauffe);
    
  }
}

void RefreshDisplay(String room_name, float temperature, float temperature_consigne, bool etat_thermostat, bool etat_chauffe){
  Serial.println("INFO - Refreshing display");

  display.setFont(&FreeSans9pt7b);

  // Effacement de l'écran actuel.
  display.clearDisplay();

  // Affichage du nom de la pièce dans la partie haute de l'écran
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,13);
  display.print(room_name);

  // Affichage de la température et de la consigne.
  display.setFont(&FreeSans12pt7b);
  display.setTextSize(1);
  display.setCursor(0,37);
  display.print(temperature,1);
  display.print(" > ");
  display.print(temperature_consigne,1);

  // Affichage de l'état du thermostat
  //display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.setFont(&FreeSans9pt7b);
  display.setCursor(0,62);
  display.setTextSize(1);
  if(etat_thermostat){
    display.print("MARCHE");
  } else {
    display.print("ARRET");
  }

  // Affichage de la flamme en cas de chauffe
  if(etat_chauffe){
    display.drawBitmap(112, 48,  logo16_glcd_bmp, 16, 16, 1); // Affiche le logo de chauffe sur l'écran
  }

  // Affichage du buffer.
  display.display();
}

void DisplayInit() { // Initialization of the display

  Serial.println("INFO - Display initialization.");
  
  Wire.begin(sda_i2c_pin, scl_i2c_pin); // On déclare les pin du port I2C

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, display_i2c_address);  // initialize with the I2C addr, use a scanner to find it.
  
  display.clearDisplay(); // Clearing the buffer.
  display.display();
  display_state = true;
  
}

void MQTTclientSetup() {
  MQTTclient.setServer(mqtt_server, mqtt_port);
  MQTTclient.setCallback(MQTTcallback);
}

void MQTTclientReconnect() {
  // We are looping until we're reconnected
  while (!MQTTclient.connected()) {
    Serial.println("INFO - Attempting connection to MQTT server.");
    // Attempt to connect
    if (MQTTclient.connect(mqqt_identifier)) {
      Serial.println("INFO - MQTT connected.");

      // We subscribe to the all_info topic.
      MQTTclient.subscribe(mqtt_all_info_topic);
      
      sendMQTTUpdateRequest();
      // This is a workaround in order to request a new data to jeedom when we connect.
      // The best solution will be to use a retain message but this is not implemented in the MQTT Jeedom plugin.
      // So we send publish a message, then a scenario is triggered inside jeedom and force the publication of new data.
      
    } else {
      Serial.print("ERROR - MQTT connection failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" retrying in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  
  // We are displaying the raw message.
  Serial.print("INFO - MQTT message arrived on topic [");
  Serial.print(topic);
  Serial.print("] : ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  
  Serial.println();

  // We are passing the message to the parsing function to extract all data.
  parseIncommingJSON((char *) payload);
  
}

void WifiClientConnection()  // Connexion au réseau WiFi
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  
  Serial.println("INFO - Starting WiFi connection");
  display.println("WiFi Connection");
  display.display();
  Serial.print("INFO - Connecting to ");
  display.print("Connecting to ");
  display.display();
  Serial.print(Network_SSID);
  display.println(Network_SSID);
  display.display();
  
  WiFi.begin(Network_SSID, Network_Password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
  }

  Serial.println("");
  display.println(".");
  display.display();
  
  Serial.println("INFO - WiFi connected");
  display.println("WiFi connected");
  display.display();
  Serial.print("INFO - IP address: ");
  display.println("IP address: ");
  display.display();
  Serial.println(WiFi.localIP());
  display.println(WiFi.localIP());
  display.display();
  display.println("Waiting for data...");
  display.display();
  
}
