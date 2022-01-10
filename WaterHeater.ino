/*
  Web Server
 
 A simple web server that shows the value of the analog input pins.
 using an Arduino Wiznet Ethernet shield. 
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * Various sensors attached to analog pins
 
 created 18 Dec 2009
 by David A. Mellis
 modified 9 Nov 2013
 by Dan Hartman
 
 updated February 24, 2021
 by Dan Hartman
  
 */

#include <SPI.h>
#include <Ethernet.h>
#include <utility/w5100.h>

// thermistor and voltage divider characteristics:
#define THERMISTORNOMINAL 10000
#define TEMPERATURENOMINAL 25
#define BCOEFFICIENT 3950
#define SERIESRESISTOR 10990
#define EthernetResetMillis 300000 // 300 seconds = 5 minutes

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF };
IPAddress ip(192,168,1,98);
byte gateway[] = { 192, 168, 1, 6 };
byte dns[] = { 8, 8, 4, 4 };
byte subnet[] = { 255, 255, 255, 0 };

// these are for the analogReads:
unsigned int fireTemp = 0;
int waterTemp1 = 0;
int waterTemp2 = 0;
unsigned long lastMillis = 0;
unsigned long lastWebCheck = 0;
unsigned long lastEthernetReset = 0; // debugging

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(8000);

void setup()
{
  analogReference(EXTERNAL); // use a forward-biased silicon diode for 0.6-0.7v
  pinMode(5, INPUT_PULLUP); // weak pullup (this is checked for LOW later for debugging)

  // setup timer interrupt for handling events, writing LCD, etc:
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
  lastMillis = millis();
  lastWebCheck = millis();
  
  // start the Ethernet connection and the server:
  resetEthernet();
  // Ethernet.begin(mac, ip, dns, gateway, subnet);
  server.begin();

  
  
}

void resetEthernet() {
  delay(50);             //wait for voltage to stabilize
  pinMode(7, OUTPUT);   //pin connected to w5100 shield's reset
  digitalWrite(7, LOW);  //pull line low for 100ms to reset ethernet shield
  delay(100);
  digitalWrite(7, HIGH);  //set line high and now ignore pin the rest of the time
  delay(100);
  
  W5100.initialized = false; // this requires a little hackery of w5100.cpp and w5100.h
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  lastMillis = millis();
  lastWebCheck = millis();
  lastEthernetReset = millis();
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

// one-second loop here, do the analog reads here instead of inside web server loop:
SIGNAL(TIMER0_COMPA_vect) {
  unsigned long currentMillis = millis();
  
  // once every second:
  if(currentMillis - lastMillis > 1000) {
    // reading water temps:
    waterTemp1 = analogRead(1);
    waterTemp2 = analogRead(2);
    fireTemp = analogRead(0);

    // check how long it's been since we reset the ethernet:
    if(millis() - lastWebCheck > EthernetResetMillis) { // it's been 5 minutes.  Do a reset.
      // resetFunc();
      resetEthernet();
    }
    
    lastMillis = currentMillis;
  }
  
  else if(currentMillis < lastMillis) {
    lastMillis = currentMillis;
  }

  // check how long it's been since we reset the ethernet:
  if(millis() - lastWebCheck > EthernetResetMillis) { // it's been 5 minutes.  Do a reset.
    // resetFunc();
    resetEthernet();
  }
}


void debugSerial() {
  Serial.begin(9600); // open the serial port at 9600 bps:
  debugSerialLoop();
}

void debugSerialLoop() {
  // debugging:

  // reading water temps:
  waterTemp1 = analogRead(1);
  waterTemp2 = analogRead(2);

  // read the thermocouple:
  fireTemp = analogRead(0);
  
  Serial.print("time: ");
  Serial.print(millis());
  Serial.print("\n");
  Serial.print("top: ");
  Serial.print(ohmsToF(waterTemp2), 1);
  Serial.print("F\nbottom: ");
  Serial.print(ohmsToF(waterTemp1), 1);
  Serial.print("F\n");
  Serial.print("analogRead: ");
  Serial.print(waterTemp1);
  Serial.print("\n");
  Serial.print("fire: ");
  Serial.print(fireTemp);
  Serial.print("\n");

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.");
  }
  else if (Ethernet.hardwareStatus() == EthernetW5100) {
    Serial.println("W5100 Ethernet controller detected.");
  }
  else if (Ethernet.hardwareStatus() == EthernetW5200) {
    Serial.println("W5200 Ethernet controller detected.");
  }
  else if (Ethernet.hardwareStatus() == EthernetW5500) {
    Serial.println("W5500 Ethernet controller detected.");
  }

  
  delay(1500);
  debugSerialLoop();
}

void loop()
{
  
  if(digitalRead(5) == LOW) {
     //debugSerial(); // this never returns, you have to unground pin 5, and reset. 
  }
  
  
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          
          // get the readings:
          // fireTemp = analogRead(0);
          // waterTemp1 = analogRead(1);
          // waterTemp2 = analogRead(2);

          // output the complete page with various readings:
          client.println(F("<html><head><meta http-equiv=\"refresh\" location=\"/\" content=\"10\">"));
          stylePrint(client, fireTemp);
          client.println(F("<title>40-gallon</title></head><body><form><table class=\"main\" align=\"center\" border=\"1\"><tr><td align=\"center\" valign=\"middle\">"));
          client.print(millis());
          client.println("<br /><br /><p class=\"big\">-- Water Heater Stats --<br /><br />");
          
          waterPrint(client, waterTemp1, waterTemp2);
          
          firePrint(client, fireTemp);

          client.println(F("</p><br /><input type=\"button\" onclick=\"javascript:location.reload();\" value=\"Check again\"><br />"));

          resetPrint(client);
          
          client.println(F("</td></tr></table></body></html>"));

          
          // done, outta here:
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    lastWebCheck = millis();
  }
}

void resetPrint(EthernetClient &client) {
  client.print("Last ethernet reset was ");
  client.print(millis() - lastEthernetReset);
  client.print(" milliseconds ago<br />");

}

void waterPrint(EthernetClient &client, int waterTemp1, int waterTemp2) {
  client.print("top: ");
  client.print(ohmsToF(waterTemp2), 1);
  client.print("F<br />bottom: ");
  client.print(ohmsToF(waterTemp1), 1);
  client.println("F<br />");
  
}

float celciusToFahrenheit(double temp) {
  return (temp * 9) / 5 + 32;
}

double ohmsToF(int rawTemp) {
  double Temp;
  float resistor;
  float resistance;
  float steinhart;
  resistor = SERIESRESISTOR; // a 27k resistor and using the 3.3v voltage for division
  resistance = rawTemp / 1024.0; // use this for 5.0v reference
  resistance = (resistor / (1 - resistance)) - resistor;

  steinhart = resistance / THERMISTORNOMINAL;
  steinhart = log(steinhart);
  steinhart /= BCOEFFICIENT;
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;

  //Temp = log(resistance);
  //Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
  //Temp = Temp - 273.15;            // Convert Kelvin to Celcius
  
  // Convert Celcius to F
  Temp = (steinhart * 9.0) / 5.0 + 32.0;
  
  return Temp;
}


void firePrint(EthernetClient &client, unsigned int fireTemp) {
  client.print("<br />fire status is <br /><b>");
  if(fireTemp > 256) {
    client.print("thermocouple error");
  }
  
  else if(fireTemp > 32) {
    client.print("ROARING FIRE");
  }
  
  else if(fireTemp > 24) {
    client.print("HOT FIRE");
  }
  
  else if(fireTemp > 20) {
    client.print("MED FIRE");
  }
  
  else if(fireTemp > 16) {
    client.print("HOT COALS");
  }
  else if(fireTemp > 10) {
    client.print("MED COALS");
  }

  else if(fireTemp > 6) {
    client.print("LOW COALS");
  }
  
  else {
    client.print("-- OUT --");
  }

  client.println("</b>");
  
}

void stylePrint(EthernetClient &client, unsigned int fireTemp) {
  client.println("<style>p.big b{ color:");
  if(fireTemp > 32) {
    client.println("red;background:yellow } ");
  }
  
  else if(fireTemp > 12) {
    client.println("white;background:red; white-space:nowrap; } ");
  }
  
  else {
    client.println("white;background:#000000; } ");
  }
  
  client.println(".main{height:100%;width:100%;} .big{font-size:40px;} input{width:100px;height:75px;}</style>");
}
