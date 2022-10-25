#define ESP_DRD_USE_SPIFFS true
//#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <PubSubClient.h>



// config parameter
String device_id = "TEST-0001";
char host[75] = "mqtt.netpie.io";
int port = 1883;
char clientId[50] = "";
char user[50] = "";
char password[50] = "";
char topic[50] = "@shadow/data/update";
int interval  = 300; //x second


WiFiManager wm;           // Define WiFiManager Object
WiFiClient espClient;
PubSubClient client(espClient);

bool shouldSaveConfig = false;  // Flag for saving data


uint64_t last_millis = 0; // latest interval publish sensor
uint64_t mqtt_reconnect = 0; // latest mqtt connected



//-----Wifi--------------
String ssid = device_id;   //ssid name is device_id
//#define ssid_password  "password"

//JSON SPIFFS storage
#define JSON_CONFIG_FILE  "/config.json"  // JSON configuration file
#define TRIGGER_PIN 15    // reset wifi pin !!!



 
// Save Config in JSON format
void saveConfigFile(){
  Serial.println(F("Saving configuration..."));
  
  // Create a JSON document
  StaticJsonDocument<1024> json;
  json["host"] = host;
  json["port"] = port;
  json["clientId"] = clientId;
  json["user"] = user;
  json["password"] = password;
  json["topic"] = topic;
  json["interval"] = interval;
  
  // Open config file
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile){
    Serial.println(F("failed to open config file for writing")); // Error, file did not open
  }
 
  // Serialize JSON data to write to file
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  // Close file
  configFile.close();
}

bool loadConfigFile()// Load existing configuration file
{
  // Uncomment if we need to format filesystem
//   SPIFFS.format();
 
  // Read configuration from FS json
  Serial.println("Mounting File System...");
 
  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true)){
    Serial.println(F("mounted file system"));
    if (SPIFFS.exists(JSON_CONFIG_FILE)){
      // The file exists, reading and loading
      Serial.println(F("reading config file"));
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile){
        Serial.println(F("Opened configuration file"));
        
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);

        if (!error){
          Serial.println("Parsing JSON");
          
          strcpy(host, json["host"]);
          port = json["port"].as<int>();
          strcpy(clientId, json["clientId"]);
          strcpy(user, json["user"]);
          strcpy(password, json["password"]);
          strcpy(topic, json["topic"]);
          interval = json["interval"].as<int>();
          
          
          return true;
        }
        else{
          // Error loading JSON data
          Serial.println(F("Failed to load json config"));
        }
      }
    }
  }
  else{
    // Error mounting file system
    Serial.println(F("Failed to mount FS"));
  }
 
  return false;
}


// Callback notifying us of the need to save configuration
void saveConfigCallback(){
  Serial.println(F("Should save config"));
  shouldSaveConfig = true;
}

// Called when config mode launched
void configModeCallback(WiFiManager *myWiFiManager){
  Serial.println(F("Entered Configuration Mode"));
 
  Serial.print(F("Config SSID: "));
  Serial.println(myWiFiManager->getConfigPortalSSID());
 
  Serial.print(F("Config IP Address: "));
  Serial.println(WiFi.softAPIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Attempt to connect
    if (client.connect(clientId, user, password, NULL,0,0,NULL,1)) {
      Serial.println(F("connected"));
    
      // ... and resubscribe
//      client.subscribe("inTopic");
    } else {
      
      Serial.print(F("failed, rc="));
      Serial.print(client.state());
      Serial.println(F(" try again in 5 seconds"));
      
//      delay(1000);
    }
  }
}



void checkButton(){
  // check for button press
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if( digitalRead(TRIGGER_PIN) == LOW ){
      Serial.println(F("Button Pressed"));
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(2000); // reset delay hold button
      if( digitalRead(TRIGGER_PIN) == LOW ){ 
        
        Serial.println(F("Button Held"));
        Serial.println(F("Erasing Config, restarting"));
        wm.resetSettings();

        
        while(digitalRead(TRIGGER_PIN) == LOW ){ // wait for release button
          delay(10);
        }
        
        delay(100);
        ESP.restart();
      }

    }
  }
}


String request_payload(String msg)
{
  //netpie format {"data":{xxx}}
  String payload = "";
    payload += "{";
    payload += "\"data\":"; 
    payload += "{";
    payload += "\"sensor\":"; // 
    payload += msg;
    payload += "}}";


  return payload;
}



void setup() {
  Serial.begin(115200);
  Serial.println(F("[[ STARTING - ESP32 ]]"));

  
  bool forceConfig = false; // Change to true when testing to force configuration every time we run
  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup){
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }


 
  WiFi.mode(WIFI_STA);  // Explicitly set WiFi mode
  delay(10);
  


  
  // 
  
  Serial.print(F("MAC: "));
  Serial.println(WiFi.macAddress());
  Serial.println();
  
//  if(wm_nonblocking) wm.setConfigPortalBlocking(false);
  
//  wm.resetSettings(); // Reset settings (only for development)
  wm.setConfigPortalTimeout(180); // auto close configportal after n seconds
  wm.setSaveConfigCallback(saveConfigCallback); // Set config save notify callback
  wm.setAPCallback(configModeCallback); // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode

// menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" 
//  std::vector<const char *> menu = {"wifi","param","sep","exit"};
  std::vector<const char *> menu = {"wifi","exit"};
  
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");
  // Custom elements
 
  // Text box (String) - 100 characters maximum
  WiFiManagerParameter custom_host("host", "HOST", host, 75);
  WiFiManagerParameter custom_clientId("clientId", "Client ID", clientId, 50);
  WiFiManagerParameter custom_user("user", "Username", user, 50);
  WiFiManagerParameter custom_password("password", "Password", password, 50);
  WiFiManagerParameter custom_topic("topic", "Topic", topic, 50);
  char portValue[5];
  sprintf(portValue, "%d", port); 
  WiFiManagerParameter custom_port("port", "Port", portValue, 5);
  char intervalValue[5];
  sprintf(intervalValue, "%d", interval); 
  WiFiManagerParameter custom_interval("interval", "Interval (sec)", intervalValue, 5);  
  
  // Add all defined parameters
  wm.addParameter(&custom_host);
  wm.addParameter(&custom_port);
  wm.addParameter(&custom_clientId);
  wm.addParameter(&custom_user);
  wm.addParameter(&custom_password);
  wm.addParameter(&custom_topic);
  wm.addParameter(&custom_interval);
  
  if (forceConfig){ // Run if we need a configuration
    if (!wm.startConfigPortal(ssid.c_str())){
      Serial.println(F("failed to connect and hit timeout"));
      delay(3000);
      ESP.restart();  //reset and try again, or maybe put it to deep sleep
      delay(5000);
    }
  }
  else{
    if (!wm.autoConnect(ssid.c_str(), password)){
      Serial.println(F("failed to connect and hit timeout"));
      delay(3000);
      ESP.restart();  // if we still have not connected restart and try all over again
      delay(5000);
    }
  }

  // Copy the text box value to Variable
  strncpy(host, custom_host.getValue(), sizeof(host));
  strncpy(clientId, custom_clientId.getValue(), sizeof(clientId));
  strncpy(user, custom_user.getValue(), sizeof(user));
  strncpy(password, custom_password.getValue(), sizeof(password));
  strncpy(topic, custom_topic.getValue(), sizeof(topic));
  port = atoi(custom_port.getValue()); 
   
  int tt = atoi(custom_interval.getValue());  
  if(tt <= 0){
    tt = 1; // minimum interval 1 sec , max 99999 sec
  }
  interval = tt;

  // If we get here, we are connected to the WiFi
  Serial.println(F(""));
  Serial.println(F("WiFi connected"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  // Lets deal with the user config values
 

 
  // Save the custom parameters to FS
  if (shouldSaveConfig){
    saveConfigFile();
  }  

  client.setServer(host, port);
//  client.setCallback(callback)
 
}





void loop() {
  // press < 3 sec reset wifi /press > 3 sec reset wifi config
  checkButton();// reset wifi config
   
  // Check WiFi connection status
  if (WiFi.status() == WL_CONNECTED){

    if (!client.connected()) {
      // Wait 5 seconds before retrying
      if((millis() - mqtt_reconnect >= 5000) || mqtt_reconnect ==0){
        reconnect();
        mqtt_reconnect = millis();
      } 
    }
    else{ // mqtt connected
      if ((millis() - last_millis >= (interval*1000)) || last_millis == 0){

        int sensor_data = random(100); 
        String payload = request_payload(String(sensor_data)); // sort payload
        
        
        Serial.print(F("Publish message: "));
        Serial.println(payload);
        client.publish(topic, payload.c_str(), false);
        last_millis = millis();      
      }

      client.loop();
    }
    

    
  }
  else {
    Serial.println(F("WiFi Disconnected"));
  }
  
}
