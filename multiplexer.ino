//
// 8-port RS232 to ethernet multiplexer
// For Teensy 4.1 

#include <EEPROM.h>
#include "html.h"
#include <QNEthernet.h>

using namespace qindesign::network;

const int nCircuits = 8; // number of physical serial ports
const char XON  = 17; // ctrl-Q
const char XOFF = 19; // strl-S
unsigned long lastBlink = 0;

struct Circuit {
  Circuit(HardwareSerial* serialPort)
      : serialPort(serialPort),          
        suspended(false){}

  EthernetServer* server;
  HardwareSerial* serialPort;
  EthernetClient client;
  bool suspended;  
};

// Map to physical port layout
Circuit circuits[8] = 
{
  // top row
  {&Serial7}, {&Serial6}, {&Serial2}, {&Serial1},
  // bottom row
  {&Serial8}, {&Serial3}, {&Serial4}, {&Serial5}
};


EthernetServer webServer(80);
EthernetClient webClient;
IPAddress IpAddress;
IPAddress NetMask;
IPAddress Gateway;
String readString; 

void(* reboot) (void) = 0;

// The Teensy has restrictions on which pins can be used
// for CTS/RTS. These have been tested to work and are 
// grouped close to the corresponding TX/RX pins
//
// Function DebugCheckPins() below can be used to check assignment if
// these are modified.
//
struct Pins
{
  uint8_t rx[8] =  {0, 7,15,16,21,25,28,34};
  uint8_t tx[8] =  {1, 8,14,17,20,24,29,35};
  uint8_t cts[8] = {2, 3, 4, 5,30,31,32,33};
  uint8_t rts[8] = {6, 9,41,18,19,12,27,36};

  uint8_t config = 5;
  uint8_t led = 13;
};
Pins pins;

byte currentVersion = 2; // increment if layout of Settings struct changes

struct Settings
{
  byte version = currentVersion;
  bool configMode = true;
  int baud[8] = {9600,9600,9600,9600,9600,9600,9600,9600};
  char flow[8] = {'n','n','n','n','n','n','n','n'};
  int ipPort[8] = {20,21,22,23,24,25,26,27};
  char ip[16] = "192.168.1.80";
  char nm[16] = "255.255.255.0";
  char gw[16] = "192.168.1.254";
};
Settings settings;
Settings defaultSettings;

void setup() 
{
  pinMode(pins.config, INPUT_PULLUP);
  pinMode(pins.led, OUTPUT);

  readString.reserve(512);
  Serial.begin(9600);

  stdPrint = &Serial;
  Serial.println("Multiplexer starting");
  
  LoadSettings();
    
  if(settings.version != currentVersion)
  {
     Serial.println("Eeprom version differs, using default settings");
     settings = defaultSettings;
  }  
    
  IpAddress.fromString(settings.ip);
  NetMask.fromString(settings.nm);
  Gateway.fromString(settings.gw);

  Ethernet.onLinkState(LinkStateChanged);
  if(!Ethernet.begin({IpAddress},{NetMask},{Gateway}))
  {
    Serial.println("no ethernet!"); 
  }

  while (Ethernet.linkState() != LinkON) 
  {
    Ethernet.loop();
    //Serial.println("Ethernet cable is not connected.");
    delay(250); // fast blink
    digitalWrite(pins.led, !digitalRead(pins.led));
  }
  digitalWrite(pins.led, LOW);
  
  if(settings.configMode)
  {
     webServer.begin();
     Serial.print("webserver at "); Serial.println(Ethernet.localIP());   
  }
  else
  {  
    for(int i = 0; i < nCircuits; i++)
    {
       circuits[i].server = new EthernetServer(settings.ipPort[i]);
       circuits[i].server->begin();
       circuits[i].serialPort->begin(settings.baud[i]);
    }

    Serial.print("servers at "); Serial.println(Ethernet.localIP());
  }
}
  
void loop() 
{
  if(!settings.configMode && digitalRead(pins.config) == LOW)
  {
    settings.configMode = true;
    SaveSettings();
    Serial.println("rebooting to config mode..");
    reboot();
  }

  while (Ethernet.linkState() != LinkON) 
  {
    //Serial.println("Ethernet cable is not connected.");
    Ethernet.loop();
    delay(250); // fast blink
    digitalWrite(pins.led, !digitalRead(pins.led));
  }
  
  if(settings.configMode)
  {
    HandleConfig();
    return;
  }
  digitalWrite(pins.led, HIGH);
  
  for(int i = 0; i < nCircuits; i++)
  {
    Circuit& circuit = circuits[i];
    
    EthernetClient newClient = circuit.server->accept();
    if(newClient && !circuit.client)
    {
       Serial.println();Serial.print("new client: ");Serial.println(i);
       circuit.client = newClient;
    }
    
    if (circuit.client) 
    {      
      while (!circuit.suspended && circuit.client.available() && circuit.serialPort->availableForWrite()) 
      {      
        digitalWrite(pins.led, HIGH);
        char c = circuit.client.read();
        circuit.serialPort->write(c);
        //Serial.print(c, HEX);            
      }         
      digitalWrite(pins.led, LOW);     
      
      while(circuit.serialPort->available()) 
      {
         digitalWrite(pins.led, HIGH);
         char c = circuit.serialPort->read(); 
         if(settings.flow[i] == 'x') // XON/XOFF
         {
           if(c == XOFF)
           {
              circuit.suspended = true;
              //Serial.print("XOFF");
              break;
           }
           else if(c == XON)
           {
              circuit.suspended = false;
              //Serial.print("XON");
              break;
           }
         }
         else
         {
            circuit.client.writeFully(c);
            //Serial.print(c, HEX);  
         }
      }
      digitalWrite(pins.led, LOW);
      
      if (!circuit.client.connected())
      {
         circuit.client.stop();
      }
    }
  }
}

void LinkStateChanged(bool state)
{
  Serial.print("Link state changed ");Serial.println(state);
}

void HandleConfig()
{
  if(millis() - lastBlink > 1000)
  {
    lastBlink = millis(); // slow blink
    digitalWrite(pins.led, !digitalRead(pins.led));
  }
  
  webClient = webServer.available();
  if (webClient) 
  {
    while (webClient.connected()) 
    {
      if (webClient.available()) 
      {
        char c = webClient.read();

        if (readString.length() < 512) 
        {
          readString += c; 
        } 

        //if HTTP request has ended
        if (c == '\n') 
        {
          Serial.println(readString); 
          
          bool rebootRequired = UpdateSettings(readString);
          SendWebPage();
                             
          delay(1);
          webClient.stop();
          readString="";

          if(rebootRequired)
          {
            settings.configMode = false;
            SaveSettings();
            Serial.println("rebooting to normal mode..");
            reboot();
          }
        }
      }
    }
  }
}

bool UpdateSettings(String readString)
{ 
  char* command = strtok(readString.c_str(), "?");
  Serial.println(command);
  bool reboot = !strncmp(command, "GET /boot", 9);
  
  // Read each command pair 
  command = strtok(0, "&");
  while (command != 0)
  {
      // Split the command in two values
      char* value = strchr(command, '=');
      if (value != 0)
      {
          // Actually split the string in 2: replace '=' with 0
          *value = 0;
          ++value;

          UpdateSetting(command, value);
      }
      // Find the next command in input string
      command = strtok(0, "&");
  }

  SaveSettings();
  //DebugCheckPins();
  
  return reboot;
}

// check for valid pin assignments
void DebugCheckPins()
{
  for(int i = 0; i < nCircuits; i++)
  { 
    for(int j = 0; j < nCircuits; j++)
       if(i != j && pins.rts[i] == pins.rts[j])
          Serial.println("duplicate rts pins!");
    for(int j = 0; j < nCircuits; j++)
       if(i != j && pins.cts[i] == pins.rts[j])
          Serial.println("duplicate cts pins!");         
  }
  
  for(int i = 0; i < nCircuits; i++)
  { 
    circuits[i].serialPort->begin(9600); // need to call begin before attachRts or attachCts    
        
    if(!circuits[i].serialPort->attachRts(pins.rts[i]))
    {
      Serial.print("circuit: ");Serial.print(i);Serial.print(" invalid rts pin: ");Serial.println(pins.rts[i]);
    }
    if(!circuits[i].serialPort->attachCts(pins.cts[i]))      
    {
      Serial.print("circuit: ");Serial.print(i);Serial.print(" invalid cts pin: ");Serial.println(pins.cts[i]);
    }      
  }
}

void UpdateSetting(char* name, char* value)
{
    if(!strncmp(name,"bd_",3))
    {
      int index = atoi(name+3);
      int val = atoi(value);
      settings.baud[index] = val;
    }
    else if(!strncmp(name,"fl_",3))
    {
      int index = atoi(name+3);
      char val = *(value); 
      settings.flow[index] = val;
    }
    else if(!strncmp(name,"pt_",3))
    {
      int index = atoi(name+3);
      int val = atoi(value);
      settings.ipPort[index] = val;
    }
    else if(!strncmp(name,"ip" ,2))
    {     
      strncpy(settings.ip, value, 16);
    }
    else if(!strncmp(name,"nm" ,2))
    {     
      strncpy(settings.nm, value, 16);
    }
    else if(!strncmp(name,"gw" ,2))
    {     
      strncpy(settings.gw, value, 16);
    }
}

bool SendWebPage()
{
  for(const char * hline : headerTemplate)
  {
     String line(hline);
     line.replace("$ip",settings.ip);
     line.replace("$nm",settings.nm);
     line.replace("$gw",settings.gw);

     WebWriteLn(line.c_str());
  }    
    
  for(int i = 0; i < 8; i++)
  {
    for( const char * pline : portTemplate)
    {
       String line(pline);
       line.replace("port_$","Port " + String(i));
       line.replace("pt_$","pt_" + String(i));
       line.replace("$pt",String(settings.ipPort[i]));
       line.replace("bd_$","bd_" + String(i));
       line.replace("fl_$","fl_" + String(i));
    
       line.replace("$sel_" + String(settings.baud[i]), "selected");
       line.replace("$sel_115200", "");
       line.replace("$sel_57600", "");
       line.replace("$sel_38400", "");
       line.replace("$sel_19200", "");
       line.replace("$sel_9600", "");
       line.replace("$sel_4800", "");
       line.replace("$sel_2400", "");
       line.replace("$sel_1200", "");
    
       line.replace("$sel_" + String(settings.flow[i]), "selected");
       line.replace("$sel_n", "");
       line.replace("$sel_r", "");
       line.replace("$sel_x", "");

       WebWriteLn(line.c_str());
    }
  }

  for(const char * line : footerTemplate)
  {
    WebWriteLn(line);
  }
}

void WebWriteLn(char * line)
{
   webClient.writeFully(line);
   webClient.writeFully("\n");
}
void SaveSettings()
{
  settings.version = currentVersion;
  EEPROM.put(0, settings);
  delay(100);
}

void LoadSettings()
{
  EEPROM.get(0, settings);
  delay(100);
}
