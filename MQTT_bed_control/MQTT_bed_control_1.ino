#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ***** Update these with values suitable for your network. ****
// Wireless SSID
const char* ssid = "YOUR_SSID";
// Wireless password
const char* password = "YOUR_WIRELESS_PASSWORD";
// MQTT Broker network address
const char* mqtt_server = "192.168.0.254";
// MQTT account username for the device
const char* mqtt_user = "BROKER_CLIENT_ACCOUNT";
// MQTT account password
const char* mqtt_pass = "BROKER_CLIENT_ACCOUNT_PASSWORD";
// MQTT topic that this devices subscribes to
char* mqtt_pub_topic = "master_bed_control";
char* mqtt_pub_availability = "/availability";
char* mqtt_pub_cmd = "master_bed_control/cmd";
// ^^^^ Update these with values suitable for your network. ^^^^


char myIP[16];                                    // Working variable
long lastReconnectAttempt = 0;                    // Reconnect counter

int currentMode = 0;                              // currentMode sets what the module should be doing right now
                                                  // Mode 0 - Nothing. Both relays off
                                                  // Mode 1 - Relay 1 on and relay 2 off
                                                  // Mode 2 - Relay 1 off and relay 2 on
                                                  // Always start up with both relays set to off.
 
int lastMode = 0;                                 //Previous operational mode
int relayPin1 = D1;                               //Relay 1 digital signal pin
int relayPin2 = D2;                               //Relay 2 digital signal pin

WiFiClient espClient;                             // Create wifi library object
PubSubClient client(espClient);                   // Create mqtt library object

unsigned long intervalUpdate = 60000;             // Check in interval in milliseconds 
unsigned long intervalStop = 20000;               // Interval to turn off relays if no command to stop is received by

bool StateChangeTriggered = false;                // Flag used to know when to process state changes
unsigned long lastIntervalUpdate = 0;             // Working timer variable
unsigned long lastIntervalStop = 0;               // Working timer variable

void setup() {

  currentMode = 0;                                // Start up in mode 0 - all relays off
  pinMode(relayPin1, OUTPUT);                     // Define relay 1 digital pin 
  digitalWrite(relayPin1, HIGH);                  // Define relay 1 digital pin state
  pinMode(relayPin2, OUTPUT);                     // Define relay 2 digital pin
  digitalWrite(relayPin2, HIGH);                  // Define relay 2 digital pin state

  Serial.begin(115200);
  setup_wifi();                                   // Call function to start wifi
  client.setServer(mqtt_server, 1883);            // Setup MQTT connection variables
  client.setCallback(callback);                   // Setup MQTT process responses
  lastReconnectAttempt = 0;
  
  StateChangeTriggered = true;                    // Go ahead and start processing the first time through the loop
  
  delay(1000);

}

void loop() {

  // Check to see if it has been a while since communicating with broker
  // If so, go ahead and set the flag to check in
  if ((millis() - lastIntervalUpdate) > intervalUpdate) {
    Serial.println("timmer triggered");
    StateChangeTriggered = true;
    lastIntervalUpdate = millis();
  }

  // If one of the relays is on, check to see if it has been on a while
  // if it has been on too long, shut it off
  if (currentMode > 0){
    if ((millis() - lastIntervalStop) > intervalStop) {
      Serial.println("Time to shut off the relays");
      currentMode = 0;
      StateChangeTriggered = true;
      lastIntervalStop = millis();
    }
  }

  // If the mode of operation has changed then set the outputs accordingly
  if (lastMode != currentMode){
    Serial.print("Processing mode change: ");
    Serial.println (currentMode);

    if (currentMode == 0){                        // Mode 0 - turn off the both relays
      digitalWrite(relayPin1, HIGH);
      digitalWrite(relayPin2, HIGH);
    }

    if (currentMode == 1){                        // Mode 1 - Relay 1 on and relay 2 off
      digitalWrite(relayPin1, LOW);
      digitalWrite(relayPin2, HIGH);
    }

    if (currentMode == 2){                        // Mode 2 - Relay 1 off and relay 2 on
      digitalWrite(relayPin1, HIGH);
      digitalWrite(relayPin2, LOW);
    }
    
    lastMode = currentMode;                       // Update the previous mode with the current one
    lastIntervalStop = millis();                  // Update the clock variable
    StateChangeTriggered = true;                  // Set flag to process mode
  }
  
  if (StateChangeTriggered == true){              // If the communication with the broker needs to happen, do it now
    if (!client.connected()) {                    // Check to see if the device is currently connected and if not, connect
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {    // if it takes longer than 5 seconds to connect then timeout and try again
        lastReconnectAttempt = now;
        // Attempt to reconnect
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {                                      // Client connected - Go ahead and check in
      char pubAvailability[25];
      sprintf(pubAvailability,"%s%s", mqtt_pub_topic, mqtt_pub_availability);
      client.publish(pubAvailability, "online");
      
      StateChangeTriggered = false;               // Reset flag
    }
  }
  
  client.loop();
}

// Function to connect to the wifi network using the variable above
void setup_wifi() {

  delay(10);
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
  
  sprintf(myIP, WiFi.localIP().toString().c_str());
  Serial.println(myIP);
}

// Function to process received data from broker
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");

  // From Home Assistant, we can expect one of there commands on the subscribed state topic - "UP", "DOWN", and "STOP"
  if (String(topic) == String(mqtt_pub_cmd)){
    String cmd = "";
    for (int i = 0; i < length; i++) {
        cmd.concat((char)payload[i]);
    }
    Serial.println(cmd);
    if (cmd == "UP"){
      currentMode = 1;
    }else if(cmd == "DOWN"){
      currentMode = 2;
    }else{
      currentMode = 0;
    }
    lastIntervalStop = millis();
  }

}

// Function to connect/reconnect to the MQTT broker
boolean reconnect() {

    if (client.connect(mqtt_pub_topic, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe(mqtt_pub_cmd);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
    return client.connected();
}
