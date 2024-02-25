/*
  Web Server
 
 A simple web server that shows the value of the analog input pins.
 using an Arduino Wiznet Ethernet shield. 
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * LCD Hitachi 44780 compatible attached to digital pins 8,9,5,4,3,2
 * Various sensors attached to analog pins
 
 
 created February 18, 2021
 by Dan Hartman
  
 */

#include <SPI.h>
#include <Ethernet.h>
#include <WString.h>

// Section: Included library 
#include "HD44780_LCD_PCF8574.h"

// Section: Defines
#define DISPLAY_DELAY_1 1000
#define DISPLAY_DELAY_2 2000
#define DISPLAY_DELAY 5000

// Section: Globals
HD44780LCD myLCD( 4, 20, 0x27, &Wire); // instantiate an object


// thermistor and voltage divider characteristics:
#define THERMISTORNOMINAL 10000
#define TEMPERATURENOMINAL 25
#define BCOEFFICIENT 3950
#define SERIESRESISTOR 10990
#define EthernetResetMillis 300000 // 300 seconds = 5 minutes

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEB };
IPAddress ip(192,168,1,94);
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
int fireStarting = 0;
byte indicatorToggle = 0;
String draftString = String(14);
String httpGetString = String(100);
bool lcdRefresh = false;

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(8000);

void setup()
{

  
  // begin the serial backpack LCD:
  delay(50);
  myLCD.PCF8574_LCDInit(myLCD.LCDCursorTypeOff);
  myLCD.PCF8574_LCDClearScreen();
  myLCD.PCF8574_LCDBackLightSet(true);

  // set up the LCD's number of columns and rows:
  // Print a message to the LCD.
  myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberOne, 0);
  myLCD.PCF8574_LCDSendString("Two-Tank Wood Burner");
  myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberTwo, 0);
  myLCD.PCF8574_LCDSendString("Tank1:    Tank2:    ");
  myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberThree, 0);
  myLCD.PCF8574_LCDSendString("Fire:               ");
  myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberFour, 0);
  myLCD.PCF8574_LCDSendString("Draft:              ");
  
  // disable the SD card on the ethernet shield:
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  
  // begin the ethernet card:
  Ethernet.begin(mac, ip, dns, gateway, subnet); // this calls softReset() and some other stuff
  server.begin();

  delay(1000);
  digitalWrite(4, LOW);
  

  analogReference(DEFAULT); // use 5v reference

  // set pin 6 as PWM out, and set to 50% output:
  // (this will drive the charge pump for the
  // instrumentation amp's negative supply)
  pinMode(6, OUTPUT);
  analogWrite(6, 128);

  // set pin 7 as digital output, for opening/closing the draft:
  pinMode(7, OUTPUT);
  
  // setup timer interrupt for handling events, writing LCD, etc:
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
  lastMillis = millis();
  lastWebCheck = millis();
  
  Serial.begin(9600); // open the serial port at 9600 bps:
  Serial.println("started. ");
}

// one-second loop here:
SIGNAL(TIMER0_COMPA_vect) {
  unsigned long currentMillis = millis();
  
  // once every second:
  if(currentMillis - lastMillis > 2000) {
    lcdRefresh = true;
    lastMillis = currentMillis;
  }
 
  // when millis() overflows:
  else if(currentMillis < lastMillis) {
    lastMillis = currentMillis;
  }

}

void doLCDRefresh() {
    
    // print the T1 and T2 temp:
    // reading water temps:
    waterTemp1 = analogRead(1);
    waterTemp2 = analogRead(2);
    
    // see if the user called for draft at starting or stoking:
    if(analogRead(5) < 500) {
      fireStarting = 900;
    }
    if(fireStarting > 0) { fireStarting--; fireStarting--; } // because we get called every 2 seconds
    
    // take an average to determine what we should do about draft:
    // float waterTempF = ohmsToF((waterTemp1 + waterTemp2) / 2);
    float waterTempF = ohmsToF(waterTemp2); // or not...
    
    myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberTwo, 6);
    if(waterTemp1 > 1000) {
      myLCD.PCF8574_LCDSendString("+++ ");
    } else if(waterTemp1 < 10) {
      myLCD.PCF8574_LCDSendString("--- ");
    } else {
      myLCD.PCF8574_LCDSendString(String(ohmsToF(waterTemp1), 0).c_str());
      myLCD.PCF8574_LCDSendChar("F");
    }
    myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberTwo, 16);
    if(waterTemp2 > 1000) {
      myLCD.PCF8574_LCDSendString("+++ ");
    } else if(waterTemp2 < 10) {
      myLCD.PCF8574_LCDSendString("--- ");
    } else {
      myLCD.PCF8574_LCDSendString(String(ohmsToF(waterTemp2), 0).c_str());
      myLCD.PCF8574_LCDSendString("F");
    }
    
    // print the Fire temp:
    fireTemp = analogRead(0);
    float fireTempF = (fireTemp / .54) + 70;
    if(fireTemp > 0) {
      myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberThree, 5);
      myLCD.PCF8574_LCDSendString(String(fireTempF, 0).c_str());
      myLCD.PCF8574_LCDSendString("F ");
    } else {
      myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberThree, 5);
      myLCD.PCF8574_LCDSendString("---   ");
    }

    // do draft control logic:
    // is there a fire?
    myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberFour, 6);
    draftString = "---";
    
    // fire is going:
    if(fireTempF > 150) {
      
      
      if(waterTempF < 140) { // fire is going, and we want hotter
        // open the draft:
        digitalWrite(7, HIGH);
        draftString = "Fire increase ";
      } 
      
      else if(waterTempF > 160) { // water is too hot, we want to slow any fires
        digitalWrite(7, LOW);
        draftString = "Fire decrease ";
      } 
      
      else if(fireStarting > 0) { // water seems hot enough, but we want more fire
        digitalWrite(7, HIGH);
        draftString = "Manually open ";
      }
      
    } 
    
    // fire is out:
    else {
      if(fireStarting > 0) {
        digitalWrite(7, HIGH); // we are starting or stoking fire and want draft open
        draftString = "Startup mode  ";
      } else {
        digitalWrite(7, LOW); // fire is out, we want to close draft to hold heat
        draftString = "Holding heat  ";
      }
    }
    
    myLCD.PCF8574_LCDSendString(draftString.c_str());
    

    // toggle the indicator:
    if(indicatorToggle == 0) {
      indicatorToggle = 1;
      myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberOne, 3);
      myLCD.PCF8574_LCDSendString("-");
    } else {
      indicatorToggle = 0;
      myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberOne, 3);
      myLCD.PCF8574_LCDSendString(" ");
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
  delay(1500);
  debugSerialLoop();
}

void loop()
{
  
  // if(digitalRead(5) == LOW) {
  //  debugSerial(); // this never returns, you have to unground pin 5, and reset. 
  //}

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        // get the query string in case we are receiving a command:
        if(httpGetString.length() < 100) {
          httpGetString.concat(c);
        }
        
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          if(httpGetString.indexOf("draft-open") > -1) {
            fireStarting = 900;
          } else if(httpGetString.indexOf("draft-close") > -1) {
            fireStarting = 0;
          }
          
          // get the readings:
          // fireTemp = analogRead(0);
          // waterTemp1 = analogRead(1);
          // waterTemp2 = analogRead(2);

          // output the complete page with various readings:
          client.print(F("HTTP/1.1 200 OK\nContent-Type: text/html\n\n<html><head>\n<meta http-equiv=\"refresh\" content=\"10;url=/\">\n"));
          stylePrint(client, fireTemp);
          client.print(F("</head><body><form><table class=\"main\" align=\"center\" border=\"1\"><tr><td align=\"center\" valign=\"middle\">\n"));
          client.print(millis());
          client.println(F("<br /><br /><p class=\"big\">Two-Tank Wood Burner<br>Status<br /><br />"));
          
          waterPrint(client, waterTemp2, waterTemp1);
          
          firePrint(client, fireTemp);

          draftPrint(client, draftString);
          
          client.print(F("</p><br /><input type=\"button\" onclick=\"javascript:location=location.protocol+'//'+location.hostname+(location.port ? ':'+location.port: '');\" value=\"Refresh\"><br />\n"));
          client.print(F("<input type=\"button\" onclick=\"javascript:location=location.protocol+'//'+location.hostname+(location.port ? ':'+location.port: '')+'/draft-open';\" value=\"Open draft\"><br />\n"));
          client.print(F("<input type=\"button\" onclick=\"javascript:location=location.protocol+'//'+location.hostname+(location.port ? ':'+location.port: '')+'/draft-close';\" value=\"Close draft\"><br />\n"));
          // client.print("<tr><td><pre>"); // debugging the get string
          // client.print(httpGetString);
          // client.print("</pre></td></tr>")
          
          client.print("</td></tr></table></body></html>\n");
          httpGetString = "";
          
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
    delay(100);
    // close the connection:
    client.stop();
    lastWebCheck = millis();
  }

  if(lcdRefresh == true) {
    Serial.print("refresh lcd at ");
    Serial.println(millis());
    doLCDRefresh();
    lcdRefresh = false;
  }
}

void draftPrint(EthernetClient &client, String draftStatus) {
  client.print("Draft status: ");
  client.print(draftStatus);
  if(fireStarting > 0) {
    client.print(" (");
    client.print(fireStarting);
    client.print(" seconds)");
  }
  client.print("<br />");
}

void waterPrint(EthernetClient &client, int waterTemp1, int waterTemp2) {
  client.print("Tank1: ");
  client.print(ohmsToF(waterTemp2), 0);
  client.print("&deg;F<br />Tank2: ");
  client.print(ohmsToF(waterTemp1), 0);
  client.println("&deg;F<br />");
  
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
  client.print("<br />Fire Temp: <b>");
  float floatFireTempF = (fireTemp / .54) + 70;
  if(floatFireTempF > 2000) {
    client.print("thermocouple error");
  }

  else if(floatFireTempF < 120) {
    client.print("(out)");
  }

  else {
    client.print(floatFireTempF, 0);
    client.print("&deg;F");
  }

  client.println("</b><br />");
  
}

void stylePrint(EthernetClient &client, unsigned int fireTemp) {
  float floatFireTempF = (fireTemp / .54) + 70;
  client.println("<style>p.big b{ color:");
  if(floatFireTempF > 300) {
    client.println("red;background:yellow } ");
  }
  
  else if(floatFireTempF > 200) {
    client.println("white;background:red; white-space:nowrap; } ");
  }
  
  else {
    client.println("white;background:#000000; } ");
  }
  
  client.println(F(".main{height:100%;width:100%;} .big{font-size:40px;} input{width:200px;height:100px;margin-bottom:1em;font-size:35px;}</style>"));
}
