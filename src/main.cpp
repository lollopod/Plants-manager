/*
 Basic ESP8266 MQTT example
 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <sensitive.h>


const char* valve_status_topic = "home/plants/valve";
const char* valve_switch_topic = "home/plants/valve/set";
const char* auto_status_topic = "home/plants/auto";
const char* auto_switch_topic = "home/plants/auto/set";

const char* measurement0_topic = "home/plants/measurement";
const char* measurement1_topic = "home/plants/measurement1";
const char* measurement2_topic = "home/plants/measurement2";
const char* measurement3_topic = "home/plants/measurement3";
const int analog_ip = A0;


WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
int valve_status = 0;
int auto_mode = 0;
int measurement0 = 0;
int measurement1 = 0;
int measurement2 = 0;
int measurement3 = 0;
int avg_meas = 0;
int measurement_air = 1024; //it shall be 0%
int measurement_water = 567; //it shall be 100%
int delta = measurement_air - measurement_water;


int moisture_min = -50;
int moisture_max = 2000;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_ota () {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("ESPlant");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if (!strcmp(topic, "home/plants/valve/set")) {
  // Switch on the LED if an 1 was received as first character
  //Switch on also the valve here (to be implemented)
    if ((char)payload[0] == '1') {
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
      digitalWrite(D0, HIGH);   
      valve_status = 1;

    } else {
      digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
      digitalWrite(D0, LOW);  
      valve_status = 0;
    }

    snprintf (msg, MSG_BUFFER_SIZE, "%d", valve_status);
    //Serial.print("Valve Status: ");
    //Serial.println(msg);
    client.publish(valve_status_topic, msg);

  }
  else {
    if ((char)payload[0] == '1') {
      auto_mode = 1;

    } else {  
      auto_mode = 0;
    }
    snprintf (msg, MSG_BUFFER_SIZE, "%d", auto_mode);
    //Serial.print("Valve Status: ");
    //Serial.println(msg);
    client.publish(auto_status_topic, msg);
  }


}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect( clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish(, "hello world");
      // ... and resubscribe
      client.subscribe(valve_switch_topic);
      client.subscribe(auto_switch_topic);
    } else {
      Serial.print("failed, disabling water valve. rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      client.publish(valve_switch_topic, "0");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);    // Initialize the BUILTIN_LED pin as an output
  pinMode(D0, OUTPUT);    // Initialize the valve pin as an output

  pinMode(D1, OUTPUT);    // Initialize the mux pin as an output
  pinMode(D2, OUTPUT);    
  pinMode(D3, OUTPUT);    

  Serial.begin(9600);
  setup_wifi();
  setup_ota();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  digitalWrite(LED_BUILTIN, HIGH); //turning off led at startup
}

void loop() {

  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();


  unsigned long now = millis();
  if (now - lastMsg > 50000) {

    
    lastMsg = now;

    //check measurements on first sensor
    digitalWrite(D1, LOW); 
    digitalWrite(D2, LOW); 
    digitalWrite(D3, LOW); 
    measurement0 = analogRead (analog_ip);
    //measurement0 = (measurement_air - measurement0) / delta * 100;
    snprintf (msg, MSG_BUFFER_SIZE, "%d", measurement0);
    //Serial.print("Analog measurement on A0_0: ");
    Serial.println(msg);
    client.publish(measurement0_topic, msg);
    //check measurements on second sensor
    digitalWrite(D1, HIGH); 
    digitalWrite(D2, LOW); 
    digitalWrite(D3, LOW); 
    measurement1 = analogRead (analog_ip);
    //measurement1 = (measurement_air - measurement1) / delta * 100;
    snprintf (msg, MSG_BUFFER_SIZE, "%d", measurement1);
    //Serial.print("Analog measurement on A0_1: ");
    Serial.println(msg);
    client.publish(measurement1_topic, msg);
    //check measurements on third sensor
    //digitalWrite(D1, LOW); 
    //digitalWrite(D2, HIGH); 
    //digitalWrite(D3, LOW); 
    //measurement2 = analogRead (analog_ip);
    //snprintf (msg, MSG_BUFFER_SIZE, "%d", measurement2);
    //Serial.print("Analog measurement on A0_2: ");
    //Serial.println(msg);
    //client.publish(measurement2_topic, msg);
    //check measurements on fourth sensor
    //digitalWrite(D1, HIGH); 
    //digitalWrite(D2, HIGH); 
    //digitalWrite(D3, LOW); 
    //measurement3 = analogRead (analog_ip);
    //snprintf (msg, MSG_BUFFER_SIZE, "%d", measurement3);
    //Serial.print("Analog measurement on A0_3: ");
    //Serial.println(msg);
    //client.publish(measurement3_topic, msg);

    //checking if auto is enabled
    avg_meas = (measurement0 + measurement1) / 2;
    //avg_meas = (measurement0 + measurement1 + measurement2 + measurement3) / 4;
    if (auto_mode) {
      if (avg_meas < moisture_min) {
        client.publish(valve_switch_topic, "1");
      }
      else if (avg_meas > moisture_max) {
        client.publish(valve_switch_topic, "0");
      }
    }


  }
}

