#include "config.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Streaming.h>

#define C1 14           // Count 1 D5
#define C2 12           // Count 2 D6
#define C3 13           // Count 3 D7

const int AnalogIn  = A0;
int readingIn = 0;
int Sensor_Count = 0;
int C1_Count = 0;
int C2_Count = 0;
int C3_Count = 0;
int Last_Value;
int AC_LOAD = 5;    // Output to Opto Triac pin
int dimming = 127;  // Dimming level (0-128)  0 = ON, 128 = OFF
int setpms = 20;    // Milliseconds between each step
uint8_t GPIO_Pin = 4;

/*
static const uint8_t D0   = 16;
static const uint8_t D1   = 5;
static const uint8_t D2   = 4;
static const uint8_t D3   = 0;
static const uint8_t D4   = 2;
static const uint8_t D5   = 14;
static const uint8_t D6   = 12;
static const uint8_t D7   = 13;
static const uint8_t D8   = 15;
static const uint8_t D9   = 3;
static const uint8_t D10  = 1;
*/

#define SLEEP_DELAY_IN_SECONDS  30


WiFiClient espClient;
PubSubClient client(espClient);


void setup() {
  // setup serial port
  Serial.begin(115200);
  // setup WiFi
  setup_wifi();

  pinMode(C1, INPUT);    
  pinMode(C2, INPUT);    
  pinMode(C3, INPUT);    

  //client.setServer(mqtt_server, 17229);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);  
  pinMode(AC_LOAD, OUTPUT);// Set AC Load pin as output
  attachInterrupt(digitalPinToInterrupt(GPIO_Pin), zero_crosss_int, RISING);  // Choose the zero cross interrupt # from the table above
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

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

void callback(char* topic, byte* payload, unsigned int length) {
  char message_buff[100];
  int i = 0;
  int x = 0;
  int bufdimming = dimming;
  int target;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    //Serial.print((char)payload[i]);
    message_buff[i] = payload[i];
    x++;
  }
  message_buff[x] = '\0';
  String msgString = String(message_buff);
  String msgTopic = String(topic);
  Serial.println("Payload: " + msgString);
  if(msgTopic.indexOf("light") >=0){
    Serial.println("light: " + msgString);
    target = msgString.toInt();
    if(target > dimming){
      for (int i=bufdimming; i <= target; i++){
        dimming=i;
        delay(setpms);
      } 
    }
    else{
      for (int i=bufdimming; i >= target; i--){
        dimming=i;
        delay(setpms);
      } 
    }
    Serial.println("light: " + dimming);
  }
  if(msgTopic.indexOf("step") >=0){
    Serial.println("step: " + msgString);
    setpms = msgString.toInt();
  }
  send_status();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Gasmads19", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("/Gasmads/Outdoor/09/input/#");
      //client.publish(mqtt_topic, "Så er vi igang");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  char result[8]; // Buffer big enough for 7-character float 
  if(!Last_Value && (digitalRead(C1) || digitalRead(C2) || digitalRead(C3)) ){
    Sensor_Count++; 
    Last_Value=1;
    Serial.println("Motion detectet");      
    if(digitalRead(C1)){
      Serial.println("C1 active");
      C1_Count ++;
    }
    if(digitalRead(C2)){
      Serial.println("C2 active");
      C2_Count++;
    }
    if(digitalRead(C3)){
      Serial.println("C3 active");
      C3_Count++;
    }
    send_status();
  }
  if(!digitalRead(C1) && !digitalRead(C2) && !digitalRead(C3)){
    //Serial.println("Alt er stille");      
    Last_Value=0;
  }
  //  client.disconnect();
  //  WiFi.disconnect();
}


void send_status(){
//  Convert data to JSON string 
  String json =
  "{"
  "\"count\": \"" + String(Sensor_Count) + "\","
  "\"C1\": \"" + String(C1_Count) + "\","
  "\"C2\": \"" + String(C2_Count) + "\","
  "\"C3\": \"" + String(C3_Count) + "\","
  "\"analog\": \"" + String(readingIn) + "\","
  "\"light\": \"" + String(dimming) + "\"}";
  char jsonChar[200];
  json.toCharArray(jsonChar, json.length()+1);
  // Publish JSON character array to MQTT topic
  //mqtt_topic = "/Gasmads/Outdoor/09/json/";
  if( client.publish(mqtt_topic,jsonChar)){
    Serial.println("Sendt");      
  }
  else{
    Serial.println("Ikke Sendt");      
  }
  Serial.println(" ");
  Serial.println("Data");
  Serial.println(json); 
}

//the interrupt function must take no parameters and return nothing
void zero_crosss_int()  //function to be fired at the zero crossing to dim the light
{
  // Firing angle calculation : 1 full 50Hz wave =1/50=20ms 
  // Every zerocrossing thus: (50Hz)-> 10ms (1/2 Cycle) 
  // For 60Hz => 8.33ms (10.000/120)
  // 10ms=10000us
  // (10000us - 10us) / 128 = 75 (Approx) For 60Hz =>65

  int dimtime = (75*dimming);    // For 60Hz =>65    
  delayMicroseconds(dimtime);    // Wait till firing the TRIAC    
  digitalWrite(AC_LOAD, HIGH);   // Fire the TRIAC
  delayMicroseconds(10);         // triac On propogation delay 
         // (for 60Hz use 8.33) Some Triacs need a longer period
  digitalWrite(AC_LOAD, LOW);    // No longer trigger the TRIAC (the next zero crossing will swith it off) TRIAC
}



