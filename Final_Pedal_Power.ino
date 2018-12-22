#include "HX711.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <SPI.h>
#define hallSensorPin D0  //pin where hall sensor is connectied
#define PIN            D7  //pin to your leds
#define NUMPIXELS      3   //number of pixels in your light chain
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
char ssid[]= "yourwirelessnetwork"; // your wireless network name (SSID)
char pass[] = "yournetworkpassword"; // your Wi-Fi network password
WiFiClient client;
PubSubClient mqttClient(client);
const char* server="mqtt.thingspeak.com";

// track the last connection time
unsigned long lastConnectionTime = 0; 
// post data every 20 seconds
const unsigned long postingInterval = 20L * 1000L;
int Z=0;
HX711 scale(D6,D5);  //These are the pins that your amplifier connects to on wemos
const int channelID = 12345; //your channel id goes here
String writeAPIKey = "put your api key here"; // write API key for your ThingSpeak Channel


const unsigned long sampleTime=5000;
float calibration_factor=-53; //this is your calibration factor for your scale 
float units;
float bongoTime=0;
int uploadFinal=0;
int Q=0;
int rpm=0;
int dataDumpR[100];  //this is the array for your watts
int dataDumpK[100];  //this is the array for your kilo wts
int watts;
float timeMaster=0;
const int numReadings = 5;  //this is the setup for your rolling average
int readingsK[numReadings];  
int readingsR[numReadings];
int readIndex = 0;              // the index of the current reading
int totalR = 0; // the running total
int totalK = 0;
int rpmCount=0;
int averageK = 0;                // the average
int averageR = 0;
float overallTime=0.0;
void setup() {
  Serial.begin(115200);
  // Set a temporary WiFi status
  int status = WL_IDLE_STATUS;
  
  Serial.println("Connected to wifi");
  // Set the MQTT broker details
  mqttClient.setServer(server, 1883);  //set-up for your mqtt service
  //wdt_disable();
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readingsK[thisReading] = 0;  //zeros out the memory for rolling average
    readingsR[thisReading] = 0;
  }
   pixels.begin(); // This initializes the NeoPixel library.
  for(int i=0;i<100;i++)dataDumpK[i]=0;  //this zeros out the memory for your weight and watts
  for(int i=0;i<100;i++)dataDumpR[i]=0;
  //WiFi.begin(ssid, password);
    //WiFi.persistent(false);
  
scale.set_scale(calibration_factor);  //starts the scale weight thing
scale.tare();    //zeros out the scale
pinMode(hallSensorPin,INPUT_PULLUP);
overallTime=millis();

}

void loop() {
 if(rpmCount!=666){    //terminates readings of wt and cadance when time to upload data
 int kilo=kiloTotal();  //gets highest weight from function kiloTotal
  rpm=rpmMaster();    //gets cadance from function rpmMaster
  if(rpm==666)      // checks to see if time to upload ---waits for 6 cycles of hall pin on
  {
    rpmCount++;
    if(rpmCount>6)rpmCount=666;
  }
watts=(kilo*rpm*0.182);  //this determines the power based on crank arm lengh and wt and cadance
yield();
Serial.print("kilo");  //output for error checking
Serial.println(kilo);
Serial.print("rpm");
Serial.println(rpm);
Serial.print("watts");
Serial.println(watts);
pixelOutput(watts);  //outputs power level in pixel color
yield();
if(rpm>1 && kilo>0) //saves data if you are moving
{
  yield();
    totalK = totalK - readingsK[readIndex];  //rolling averages for power and wt
    totalR = totalR - readingsR[readIndex];
  readingsK[readIndex] = kilo;
  readingsR[readIndex] = watts;
  // add the reading to the total:
  totalK = totalK + readingsK[readIndex];
  totalR = totalR + readingsR[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
    //oledOutput( average);
  }

  // calculate the average:
  averageR = totalR / numReadings;
  averageK = totalK / numReadings;
}
  if(millis()-overallTime>1000*60){  //saves averages of wt and watts every minute for 100 readings
    delay(10);
    overallTime=millis();
    dataDumpR[Q] = averageR;
    dataDumpK[Q] = averageK;
   
    Q++;
    Serial.print("Q=  ");
    Serial.println(Q);
  }
    if(Q>99)Q=0;
 }
 if(rpmCount==666&&uploadFinal==1){  //terminates upload if done uploading all data
  if(rpmCount==666){  //starts upload sequence if done riding 
    
  
 // Check if MQTT client has connected else reconnect
  for(int Z=0;Z<=Q;Z++){
    bongoTime=millis();
  if (!mqttClient.connected()) 
  {
    reconnect();
  }
  // Call the loop continuously to establish connection to the server
  mqttClient.loop();
  mqttpublish(watts,dataDumpK[Z],dataDumpR[Z]);  
  while( millis()-bongoTime<20000)   //upload new data every 20 seconds--limit on thingspeak
  {
  yield();
  
  for(int p=0;p<NUMPIXELS;p++)  //flash sequential green colors while uploading data
  {
    for(int p=0;p<NUMPIXELS;p++)pixels.setPixelColor(p,0,0,0);
    pixels.show();
    delay(500);
    for(int a=0;a<255;a++)
    {
      pixels.setPixelColor(p,0,a,0);
      pixels.show();
      delay(10);
    }
  yield();
  }
  }
  }
  uploadFinal=1;  //flag  for done uploading
  WiFi.forceSleepBegin(); // Wifi off
}
 }
}




void pixelOutput(int kilo){  //function for flashing power level on pixels blue low red high
  kilo=constrain(kilo,0,300);
   int kiloRed=map(kilo,0,300,0,255);
   int kiloBlue=255-kiloRed;
   for(int i=0;i<NUMPIXELS;i++){
    pixels.setPixelColor(i,pixels.Color(kiloRed,0,kiloBlue));
    yield();
    pixels.show();
    delay(10);
   }
}
   

int rpmMaster(){  //function to determine cadance during 5 second sample
   //wdt_disable();
  int count=0;
  int failCount=0;
  boolean countFlag=LOW;
  float currentTime=0;
  float startTime=millis();
 do
  {
    if(digitalRead(hallSensorPin)==HIGH)
    {
      delay(10);
      countFlag=HIGH;
    }
    else failCount++;
    if(digitalRead(hallSensorPin)==LOW && countFlag==HIGH)
    { 
      delay(10);
      count++;
      countFlag=LOW;
    }
    yield();   //very important yield--will continually reset without this
    currentTime=millis()-startTime;
    } while(currentTime<=sampleTime);
    Serial.print("count");
    Serial.println(count);
    int countRpm=int(60000/float(sampleTime))*count;
   Serial.print("deadcounter");
   Serial.println(failCount);
   if(failCount>300000)countRpm=666;
    return countRpm;

  }

  int kiloTotal(){    //determines max wt on pedal during 5 second check
     //wdt_disable();
    int kiloMaster=0;
    float units=0.0;
     unsigned long currentTime=0;
     unsigned long startTime=millis();
     while(currentTime<=sampleTime)
     {
      units=scale.get_units(),10;
      delay(20);
      if(units<0)
      {
        units=0.0;
      }
      units=units/1000;
      if(units>kiloMaster)kiloMaster=units;
      currentTime =millis()-startTime;
     }
     
     return kiloMaster;
  }
 
void reconnect()    //function for connecting back to internet
{
  // Loop until we're reconnected
  while (!mqttClient.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Connect to the MQTT broker
    if (mqttClient.connect("ArduinoWiFi101Client")) 
    {
      Serial.println("connected");
    } else 
    {
      Serial.print("failed, rc=");
      // Print to know why the connection failed
      // See http://pubsubclient.knolleary.net/api.html#state for the failure code and its reason
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying to connect again
      delay(5000);
    }
  }
}

void mqttpublish(int dataDumpR,int dataDumpK,int R) {  // publishing mqtt fields
  
  // Create data string to send to ThingSpeak
  String data = String("field1=" + String(dataDumpR) + "&field2=" + String(dataDumpK) + "&field3=" + String(R));
  // Get the data string length
  int length = data.length();
  char msgBuffer[length];
  // Convert data string to character buffer
  data.toCharArray(msgBuffer,length+1);
  Serial.println(msgBuffer);
  // Publish data to ThingSpeak. Replace <YOUR-CHANNEL-ID> with your channel ID and <YOUR-CHANNEL-WRITEAPIKEY> with your write API key
  mqttClient.publish("channels/339837/publish/PVJ5VOAVNJ0880HK",msgBuffer);
  // note the last connection time
  lastConnectionTime = millis();
}

  

