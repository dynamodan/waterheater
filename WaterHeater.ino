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
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  // Print a message to the LCD.
  lcd.print("Two-Tank Wood Burner");
  lcd.setCursor(0, 1);
  lcd.print("T1:       T2:       ");
  lcd.setCursor(0, 2);
  lcd.print("Fire:         Dft:  ");

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
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
 
}

// one-second loop here:
SIGNAL(TIMER0_COMPA_vect) {
  unsigned long currentMillis = millis();
  // once every second:
  if(currentMillis - lastMillis > 1000) {
    // print the T1 and T2 temp:
    // reading water temps:
    waterTemp1 = analogRead(1);
    waterTemp2 = analogRead(2);

    // see if the user called for draft at starting or stoking:
    if(analogRead(5) < 500) {
      fireStarting = 900;
    }
    if(fireStarting > 0) { fireStarting--; }
    
    // take an average to determine what we should do about draft:
    // float waterTempF = ohmsToF((waterTemp1 + waterTemp2) / 2);
    float waterTempF = ohmsToF(waterTemp2); // or not...
    
    lcd.setCursor(4, 1);
    if(waterTemp1 > 1000) {
      lcd.print("+++  ");
    } else if(waterTemp1 < 10) {
      lcd.print("---  ");
    } else {
      lcd.print(ohmsToF(waterTemp1));
      lcd.print("F");
    }
    lcd.setCursor(14, 1);
    if(waterTemp2 > 1000) {
      lcd.print("+++  ");
    } else if(waterTemp2 < 10) {
      lcd.print("---  ");
    } else {
      lcd.print(ohmsToF(waterTemp2));
      lcd.print("F");
    }
    
    // print the Fire temp:
    fireTemp = analogRead(0);
    if(fireTemp > 0) {
      float fireTempF = (fireTemp / .54) + 70;
      lcd.setCursor(5, 2);
      lcd.print(fireTempF);
      lcd.print("F ");
    } else {
      lcd.setCursor(5, 2);
      lcd.print("---    ");
    }

    // do draft control logic:
    // is there a fire?
    lcd.setCursor(18, 2);
    if(fireTemp > 50 && waterTempF < 140) { // fire is going, and we want hotter
      // open the draft:
      digitalWrite(7, HIGH);
      lcd.print("F+");
    } else if(waterTempF > 160) { // water is too hot, we want to slow any fires
      digitalWrite(7, LOW);
      lcd.print("F-");
    } else if(fireStarting > 0) { // we are starting or stoking fire and want draft open
      digitalWrite(7, HIGH);
      lcd.print("FS");
    } else {
      digitalWrite(7, LOW); // fire is out, we want to close draft to hold heat
      lcd.print("--");
    }
    
    
    
    // print the number of seconds since reset:
    lcd.setCursor(0, 3);
    lcd.print("millis: ");
    lcd.print(millis());
    lcd.print("  ");
    lastMillis = currentMillis;
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
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          
          // get the readings:
          fireTemp = analogRead(0);
          waterTemp1 = analogRead(1);
          waterTemp2 = analogRead(2);

          // output the complete page with various readings:
          client.println("<html><head>");
          client.println("<meta http-equiv=\"refresh\" location=\"/\" content=\"10\">");
          client.println("<html><head>");
          stylePrint(client, fireTemp);
          client.println("</head><body>");
          client.println("<form><table class=\"main\" align=\"center\" border=\"1\"><tr><td align=\"center\" valign=\"middle\">");
          client.print(millis());
          client.println("<br /><br /><p class=\"big\">-- Water Heater Stats --<br /><br />");
          
          waterPrint(client, waterTemp1, waterTemp2);
          
          firePrint(client, fireTemp);
          
          client.println("</p><br /><input type=\"button\" onclick=\"javascript:location.reload();\" value=\"Check again\"><br />");
          client.println("</td></tr></table></body></html>");
          
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
  }
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
  
  else if(fireTemp > 18) {
    client.print("MED FIRE");
  }
  
  else if(fireTemp > 12) {
    client.print("HOT COALS");
  }
  else if(fireTemp > 8) {
    client.print("MED COALS");
  }

  else if(fireTemp > 4) {
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
