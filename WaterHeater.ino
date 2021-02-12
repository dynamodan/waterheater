/*
  Web Server
 
 A simple web server that shows the value of the analog input pins.
 using an Arduino Wiznet Ethernet shield. 
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * Analog inputs attached to pins A0 through A5 (optional)
 
 created 18 Dec 2009
 by David A. Mellis
 modified 9 Nov 2013
 by Dan Hartman
 
 */

#include <SPI.h>
#include <Ethernet.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,98);

// these are for the analogReads:
unsigned int fireTemp = 0;
int waterTemp1 = 0;
int waterTemp2 = 0;

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(8000);

void setup()
{
  analogReference(INTERNAL);
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
}

void loop()
{
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
  resistor = 27000.0; // a 27k resistor and using the 3.3v voltage for division
  // resistance = (resistor / (1 - (rawTemp / 3300))) - resistor;
  // resistance = rawTemp / 3300.0; // use this for 3.3v reference
  resistance = rawTemp / 5000.0; // use this for 5.0v reference
  resistance = (resistor / (1 - resistance)) - resistor;

  Temp = log(resistance);
  Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
  Temp = Temp - 273.15;            // Convert Kelvin to Celcius
  Temp = (Temp * 9.0) / 5.0 + 32.0;
  return Temp;
}


void firePrint(EthernetClient &client, unsigned int fireTemp) {
  client.print("<br />fire status is <br /><b>");
  if(fireTemp > 16) {
    client.print("ROARING FIRE");
  }
  
  else if(fireTemp > 12) {
    client.print("HOT FIRE");
  }
  
  else if(fireTemp > 9) {
    client.print("MED FIRE");
  }
  
  else if(fireTemp > 6) {
    client.print("HOT COALS");
  }
  else if(fireTemp > 2) {
    client.print("MED COALS");
  }

  else if(fireTemp > 0) {
    client.print("LOW COALS");
  }
  
  else {
    client.print("-- OUT --");
  }

  client.println("</b>");
  
}

void stylePrint(EthernetClient &client, unsigned int fireTemp) {
  client.println("<style>p.big b{ color:");
  if(fireTemp > 16) {
    client.println("red;background:yellow } ");
  }
  
  else if(fireTemp > 6) {
    client.println("white;background:red; white-space:nowrap; } ");
  }
  
  else {
    client.println("white;background:#000000; } ");
  }
  
  client.println(".main{height:100%;width:100%;} .big{font-size:40px;} input{width:100px;height:75px;}</style>");
}

