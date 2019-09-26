#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT_U.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"

// WiFi credentials
const char* ssid = "OnePlus";
const char* password = "KTVR1810";

// MQTT broker
const char* mqtt_server = "soldier.cloudmqtt.com"; //soldier.cloudmqtt.com
const int mqtt_port = 14292;

// MQTT Credentials
const char* mqtt_user = "myusername"; // Change for your username
const char* mqtt_password = "mypassword"; // Change for your password

// WiFi and MQTT client instantiation
WiFiClient espClient;
PubSubClient client(espClient);

// Vars
long lastMsg = 0; // Manages the temperature publishing interval
int value = 0;
boolean deployer_auto = false;
boolean descender_auto = false;

// DTH type
#define DHTTYPE DHT11
#define DHTPIN  3
DHT dht(DHTPIN, DHTTYPE);

// Deployer Pin definition
int Pin1 = 14; //D5 
int Pin2 = 12; //D6 
int Pin3 = 13; //D7
int Pin4 = 15; //D8

// Descender Pin definition
int Pin5 = 5; //D1
int Pin6 = 4; //D2
int Pin7 = 0; //D3
int Pin8 = 2; //D4

// Sensor Pin definition
int LightSensor_Pin = A0; //A0

// State variables
boolean isDeployerOut = false;
boolean isDescenderDown = false;

// Movement controls
int left_step = 0;
int right_step = 0; 
int count=0;

// Subscription topics
const char* modes = "/modes";
const char* actions = "/actions";

// Publication topics
const char* temperature_value = "/temp";
const char* light_value = "/light";
const char* humidity_value = "/humidity";

// Connect to WiFi
void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Establishes WiFi Connectivity
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Manages the messages received from the subscriptions
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[");
  Serial.print(topic);
  Serial.print("] => ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Manage topics
  
  if (topic[1] == 'm') {
    if ((char)payload[0] == '1') {
    Serial.println("Deployer Automatic Mode : ON");
    deployer_auto = true;
    }

    if ((char)payload[0] == '0') {
      Serial.println("Deployer Automatic Mode : OFF");
      deployer_auto = false;
    }

    if ((char)payload[1] == '0') {
      Serial.println("Descender Automatic Mode : ON");
      descender_auto = true;
    }

    if ((char)payload[1] == '1') {
      Serial.println("Descender Automatic Mode : OFF");
      descender_auto = false;
    }
  }

  if (topic[1] == 'a') {
     if ((char)payload[0] == '1' && deployer_auto == false) {
      Serial.println("ACTIVATING DEPLOYER ROOF");
      if (isDeployerOut == true) {
        left_roof_control(true);
        Serial.println("going in");
        isDeployerOut = false;
        return;
        
      }

      if (isDeployerOut == false) {
        left_roof_control(false);
        Serial.println("Going out");
        isDeployerOut = true;
        return;
        
      }
    }

    if ((char)payload[1] == '1' && descender_auto == false) {
      Serial.println("ACTIVATING DESCENDER ROOF");
      if (isDescenderDown == true) {
        isDescenderDown = false;
        Serial.println("going in");
        right_roof_control(true);
        return;
        
      }

      if (isDescenderDown == false) {
        isDescenderDown = true;
        Serial.println("Going out");
        right_roof_control(false);
        return;
        
      }
    } 
  }  
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    int randNumber = random(3000);
    char cli[16];
    sprintf(cli, "%05d", randNumber);
    
    if (client.connect(cli ,mqtt_user,mqtt_password)) {
      Serial.println("connected");
      float h = dht.readHumidity();
      float t = dht.readTemperature();
      int tmp = static_cast<int>(t);
      int humidity = static_cast<int>(h);
      int light = (analogRead(LightSensor_Pin)*100)/1023;
      Serial.print("[Temperature] =>");
      Serial.println(tmp);
      Serial.print("[Light] =>");
      Serial.println(light);
      Serial.print("[Humidity] =>");
      Serial.println(humidity);
      client.publish(temperature_value, String(tmp).c_str());
      client.publish(light_value, String(light).c_str());
      client.publish(humidity_value, String(humidity).c_str());

      client.subscribe(modes);
      client.subscribe(actions);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Deployer
  pinMode(Pin1, OUTPUT);  
  pinMode(Pin2, OUTPUT);  
  pinMode(Pin3, OUTPUT);  
  pinMode(Pin4, OUTPUT);  
  
  // Descender
  pinMode(Pin5, OUTPUT);
  pinMode(Pin6, OUTPUT);
  pinMode(Pin7, OUTPUT);
  pinMode(Pin8, OUTPUT);

  // initialize DTH sensor
  dht.begin();

  // Setup WiFi and MQTT client
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// Main loop
void loop() {

  // This serves to reconnect to the server automatically in the event
  // of an internet access interruption 
  if (!client.connected()) {
    reconnect();
  }

  // Listens to the server and validates de new messages if any
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
        // Deployer auto mode controls
    if (deployer_auto == true) {
      // If the light levels are high and the roof is not deployed, it deploys it
      if (analogRead(LightSensor_Pin) > 900 && isDeployerOut == false)
      {
        isDeployerOut = true;
        left_roof_control(false);
      }

      // If the light levels are low and the roof is out, it hides it
      if (analogRead(LightSensor_Pin) < 900 && isDeployerOut == true)
      {
        Serial.println(analogRead(LightSensor_Pin));
        isDeployerOut = false;
        left_roof_control(true);
      }
    }

    // Descender auto mode controls
    if (descender_auto == true) {
      
      // If the light levels are high and the roof is not down, it deploys it
      if (analogRead(LightSensor_Pin) > 900 && isDescenderDown == false)
      {
        isDescenderDown = true;
        right_roof_control(true);
      }

      // If the light levels are low and the roof is down, it brings it up
      if (analogRead(LightSensor_Pin) < 900 && isDescenderDown == true)
      {
        isDescenderDown = false;
        right_roof_control(false);
      }
    }
    Serial.println("Sending data");
    lastMsg = now;
    ++value;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int tmp = static_cast<int>(t);
    int humidity = static_cast<int>(h);
    int light = (analogRead(LightSensor_Pin)*100)/1023;
    Serial.print("[Temperature] =>");
    Serial.println(tmp);
    Serial.print("[Light] =>");
    Serial.println(light);
    Serial.print("[Humidity] =>");
    Serial.println(humidity);
    
    client.publish(temperature_value, String(tmp).c_str());
    client.publish(light_value, String(light).c_str());
    client.publish(humidity_value, String(humidity).c_str());
  }
}

// Full deployer action
void left_roof_control(bool direc) {

  for (int i = 0; i < 4096; i++) {
        step(direc);
    }
  
}

// Full descender action
void right_roof_control(bool direc) {

  for (int i = 0; i < 16000; i++) {
        step2(direc);
    }
  
}

// Deployer movement controls
void step(bool dire){
    switch(left_step){ 
   case 0: 
     digitalWrite(Pin1, LOW);  
     digitalWrite(Pin2, LOW); 
     digitalWrite(Pin3, LOW); 
     digitalWrite(Pin4, HIGH); 
   break;  
   case 1: 
     digitalWrite(Pin1, LOW);  
     digitalWrite(Pin2, LOW); 
     digitalWrite(Pin3, HIGH); 
     digitalWrite(Pin4, HIGH); 
   break;  
   case 2: 
     digitalWrite(Pin1, LOW);  
     digitalWrite(Pin2, LOW); 
     digitalWrite(Pin3, HIGH); 
     digitalWrite(Pin4, LOW); 
   break;  
   case 3: 
     digitalWrite(Pin1, LOW);  
     digitalWrite(Pin2, HIGH); 
     digitalWrite(Pin3, HIGH); 
     digitalWrite(Pin4, LOW); 
   break;  
   case 4: 
     digitalWrite(Pin1, LOW);  
     digitalWrite(Pin2, HIGH); 
     digitalWrite(Pin3, LOW); 
     digitalWrite(Pin4, LOW); 
   break;  
   case 5: 
     digitalWrite(Pin1, HIGH);  
     digitalWrite(Pin2, HIGH); 
     digitalWrite(Pin3, LOW); 
     digitalWrite(Pin4, LOW); 
   break;  
     case 6: 
     digitalWrite(Pin1, HIGH);  
     digitalWrite(Pin2, LOW); 
     digitalWrite(Pin3, LOW); 
     digitalWrite(Pin4, LOW); 
   break;  
   case 7: 
     digitalWrite(Pin1, HIGH);  
     digitalWrite(Pin2, LOW); 
     digitalWrite(Pin3, LOW); 
     digitalWrite(Pin4, HIGH); 
   break;  
   default: 
     digitalWrite(Pin1, LOW);  
     digitalWrite(Pin2, LOW); 
     digitalWrite(Pin3, LOW); 
     digitalWrite(Pin4, LOW); 
   break;  
 } 
 if(dire){ 
   left_step++; 
 }else{ 
   left_step--; 
 } 
 if(left_step>7){ 
   left_step=0; 
 } 
 if(left_step<0){ 
   left_step=7; 
 } 
 delay(1); 
}

// Descender movement control
void step2 (bool dire){
    switch(right_step){ 
   case 0: 
     digitalWrite(Pin5, LOW);  
     digitalWrite(Pin6, LOW); 
     digitalWrite(Pin7, LOW); 
     digitalWrite(Pin8, HIGH); 
   break;  
   case 1: 
     digitalWrite(Pin5, LOW);  
     digitalWrite(Pin6, LOW); 
     digitalWrite(Pin7, HIGH); 
     digitalWrite(Pin8, HIGH); 
   break;  
   case 2: 
     digitalWrite(Pin5, LOW);  
     digitalWrite(Pin6, LOW); 
     digitalWrite(Pin7, HIGH); 
     digitalWrite(Pin8, LOW); 
   break;  
   case 3: 
     digitalWrite(Pin5, LOW);  
     digitalWrite(Pin6, HIGH); 
     digitalWrite(Pin7, HIGH); 
     digitalWrite(Pin8, LOW); 
   break;  
   case 4: 
     digitalWrite(Pin5, LOW);  
     digitalWrite(Pin6, HIGH); 
     digitalWrite(Pin7, LOW); 
     digitalWrite(Pin8, LOW); 
   break;  
   case 5: 
     digitalWrite(Pin5, HIGH);  
     digitalWrite(Pin6, HIGH); 
     digitalWrite(Pin7, LOW); 
     digitalWrite(Pin8, LOW); 
   break;  
     case 6: 
     digitalWrite(Pin5, HIGH);  
     digitalWrite(Pin6, LOW); 
     digitalWrite(Pin7, LOW); 
     digitalWrite(Pin8, LOW); 
   break;  
   case 7: 
     digitalWrite(Pin5, HIGH);  
     digitalWrite(Pin6, LOW); 
     digitalWrite(Pin7, LOW); 
     digitalWrite(Pin8, HIGH); 
   break;  
   default: 
     digitalWrite(Pin5, LOW);  
     digitalWrite(Pin6, LOW); 
     digitalWrite(Pin7, LOW); 
     digitalWrite(Pin8, LOW); 
   break;  
 } 
 if(dire){ 
   right_step++; 
 }else{ 
   right_step--; 
 } 
 if(right_step>7){ 
   right_step=0; 
 } 
 if(right_step<0){ 
   right_step=7; 
 } 
  delay(1); 
 }
