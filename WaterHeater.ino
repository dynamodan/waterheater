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
#include <LiquidCrystal.h>
#include <WString.h>


// thermistor and voltage divider characteristics:
#define THERMISTORNOMINAL 10000
#define TEMPERATURENOMINAL 25
#define BCOEFFICIENT 3950
#define SERIESRESISTOR 10990

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,96);

// these are for the analogReads:
unsigned int fireTemp = 0;
int waterTemp1 = 0;
int waterTemp2 = 0;
unsigned long lastMillis = 0;
int fireStarting = 0;
byte indicatorToggle = 0;
String draftString = String(14);
String httpGetString = String(100);

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(8000);

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 9, en = 8, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


void setup()
{

  // disable the SD card on the ethernet shield:
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  delay(1000);
  digitalWrite(4, LOW);
  
  
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  // Print a message to the LCD.
  lcd.print("Two-Tank Wood Burner");
  lcd.setCursor(0, 1);
  lcd.print("Tank1:    Tank2:    ");
  lcd.setCursor(0, 2);
  lcd.print("Fire:               ");
  lcd.setCursor(0, 3);
  lcd.print("Draft:              ");

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
  
  
 
}

// one-second loop here:
SIGNAL(TIMER0_COMPA_vect) {
  unsigned long currentMillis = millis();
  
  // once every second:
  if(currentMillis - lastMillis > 2000) {
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
    
    lcd.setCursor(6, 1);
    if(waterTemp1 > 1000) {
      lcd.print("+++  ");
    } else if(waterTemp1 < 10) {
      lcd.print("---  ");
    } else {
      lcd.print(String(ohmsToF(waterTemp1), 0));
      lcd.print("F");
    }
    lcd.setCursor(16, 1);
    if(waterTemp2 > 1000) {
      lcd.print("+++  ");
    } else if(waterTemp2 < 10) {
      lcd.print("---  ");
    } else {
      lcd.print(String(ohmsToF(waterTemp2), 0));
      lcd.print("F");
    }
    
    // print the Fire temp:
    fireTemp = analogRead(0);
    if(fireTemp > 0) {
      float fireTempF = (fireTemp / .54) + 70;
      lcd.setCursor(5, 2);
      lcd.print(String(fireTempF, 0));
      lcd.print("F ");
    } else {
      lcd.setCursor(5, 2);
      lcd.print("---    ");
    }

    // do draft control logic:
    // is there a fire?
    lcd.setCursor(6, 3);
    draftString = "";
    if(fireTemp > 50 && waterTempF < 140) { // fire is going, and we want hotter
      // open the draft:
      digitalWrite(7, HIGH);
      draftString = "Fire increase ";
    } else if(waterTempF > 160) { // water is too hot, we want to slow any fires
      digitalWrite(7, LOW);
      draftString = "Fire decrease ";
    } else if(fireStarting > 0) { // we are starting or stoking fire and want draft open
      digitalWrite(7, HIGH);
      draftString = "Fire starting ";
    } else {
      digitalWrite(7, LOW); // fire is out, we want to close draft to hold heat
      draftString = ("Clamping stack");
    }
    lcd.print(draftString);
    
    lastMillis = currentMillis;

    // toggle the indicator:
    if(indicatorToggle == 0) {
      indicatorToggle = 1;
      lcd.setCursor(3, 0);
      lcd.print("-");
    } else {
      indicatorToggle = 0;
      lcd.setCursor(3, 0);
      lcd.print(" ");
    }
  }
  
  // when millis() overflows:
  else if(currentMillis < lastMillis) {
    lastMillis = currentMillis;
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
          
          client.print(F("</p><br /><input type=\"button\" onclick=\"javascript:location=location.protocol+'//'+location.hostname+(location.port ? ':'+location.port: '');\" value=\"Check again\"><br /></td></tr>\n"));
          // client.print("<tr><td><pre>"); // debugging the get string
          // client.print(httpGetString);
          // client.print("</pre></td></tr>")
          client.print("</table></body></html>\n");
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
  
  client.println(".main{height:100%;width:100%;} .big{font-size:40px;} input{width:100px;height:75px;}</style>");
}
