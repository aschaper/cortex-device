#include "Adafruit_TSL2561_U.h"
#include "Adafruit_DHT.h"

#define buttonPin D2
#define dhtPin    D4
#define maxPin    A0
#define piezoPin  D3
#define tslPinSDA D0
#define tslPinSCA D1
#define pirPin    D5

DHT dht(dhtPin, DHT22);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

bool uniqueIdentifierSet = false;
String uniqueIdentifier;
bool currentlyTracking;
int pirState = LOW;
int previousMillis = 0;
bool movementDetected = false;

void setup() {
  Serial.begin(9600);
  dht.begin();
  tsl.begin();

  currentlyTracking = false;

  pinMode(buttonPin, INPUT);
  pinMode(dhtPin, INPUT);
  pinMode(maxPin, INPUT);
  pinMode(piezoPin, OUTPUT);
  pinMode(pirPin, INPUT);

  subscribeToWebhooks();
  playTurnedOnSound();
}

void subscribeToWebhooks() {
  Spark.subscribe("hook-response/start_tracking", getTrackingUUID, MY_DEVICES);
  Spark.subscribe("hook-response/save_results", NULL);
}

void getTrackingUUID(const char *name, const char *data) {
  String uuid = String(data);

  uniqueIdentifier = uuid;
  uniqueIdentifierSet = true;
}

void loop() {
   if (changeTrackingStatus()) {
     if (currentlyTracking) {
       Serial.println("stop");
       stopTracking();
     } else {
       Serial.println("start");
       startTracking();
     }
   }

   if (currentlyTracking) {
     if (uniqueIdentifierSet) {
       int currentMillis = millis();

       if (!movementDetected) {
         detectMovement();
       }

       if (currentMillis - previousMillis > 60000) {
         previousMillis = currentMillis;
         record();

         movementDetected = false;
       }
     }
   }
}

void record() {
  float temp = temperatureValue();
  float noise = noiseValue();
  float lux = luxValue();

  String json = "{\"temperature\":\"";
  json.concat(String(temp));
  json.concat("\", \"noise\":\"");
  json.concat(String(noise));
  json.concat("\", \"light\":\"");
  json.concat(String(lux));
  json.concat("\", \"moved\":\"");
  json.concat(movementDetected);
  json.concat("\", \"uuid\":\"");
  json.concat(uniqueIdentifier);
  json.concat("\"}");

  Spark.publish("save_results", json);
}

bool changeTrackingStatus() {
  bool trackingStatusShouldChange = false;
  int buttonValue = digitalRead(buttonPin);

  if (buttonValue == 1) {
    trackingStatusShouldChange = true;
  }

  return trackingStatusShouldChange;
}

 void startTracking() {
   currentlyTracking = true;

   Spark.publish("start_tracking");

   playStartedTrackingSound();
 }

 void stopTracking() {
   currentlyTracking = false;

   playStoppedTrackingSound();
 }

float temperatureValue() {
  return dht.getTempFarenheit();
}

 float noiseValue() {
   int sampleWindow = 50;  //Sample window width. 50ms = 20Hz
   int signalMax = 0;
   int signalMin = 4095;
   unsigned long startMillis = millis();
   unsigned int sample;
   unsigned int peakToPeak;

   while (millis() - startMillis < sampleWindow) {
     sample = analogRead(maxPin);
     if (sample < signalMin) {
       if (sample > signalMax) {
         signalMax = sample;
       } else if (sample < signalMin) {
         signalMin = sample;
       }
     }
   }

   peakToPeak = signalMax - signalMin;

   return 20 * log(peakToPeak);
 }

float luxValue() {
  sensors_event_t event;
  tsl.getEvent(&event);

  return event.light;
}

void detectMovement() {
  int pirValue = digitalRead(pirPin);

  if (pirValue == HIGH) {
    if (pirState == LOW) {
      movementDetected = true;
      pirState = HIGH;
    }
  } else {
    if (pirState == HIGH) {
      pirState = LOW;
    }
  }
}

void playTurnedOnSound() {
  int pitch = map(400, 1000, 0, 0, 3000);

  tone(piezoPin, pitch, 20);
  delay(100);
  tone(piezoPin, pitch, 20);
}

void playStartedTrackingSound() {
  int pitches[3] = {800, 600, 400};

  for(int i=0; i < 3; i++) {
    int pitch = map(pitches[i], 1000, 0, 0, 3000);
    tone(piezoPin, pitch, 20);

    delay(100);
  }
}

void playStoppedTrackingSound() {
  int pitches[3] = {400, 600, 800};

  for(int i=0; i < 3; i++) {
    int pitch = map(pitches[i], 1000, 0, 0, 3000);
    tone(piezoPin, pitch, 20);

    delay(100);
  }
}
