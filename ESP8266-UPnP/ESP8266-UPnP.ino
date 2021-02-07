#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <WiFiUdp.h>

#ifndef STASSID
#define STASSID  "your_ssid" 
#define STAPSK   "your_psk"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

int statuspin = D9;
unsigned int portlocal;

// buffer to hold incoming packet
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1]; 

// every single devices response
char *recv;

//buffer to hold devices responses for http response
char response[4096] = {0};

WiFiUDP udp_msearch;
WiFiUDP udp_receive;

// IPAddress and port for broadcast;
IPAddress broadcast=IPAddress(239, 255, 255, 250);
unsigned int port = 1900; 

//M-search request string
String sMsearch = \
    "M-SEARCH * HTTP/1.1\r\n" \
    "HOST:239.255.255.250:1900\r\n" \
    "ST: upnp:rootdevice\r\n" \
    "MX: 2\r\n" \
    "MAN: \"ssdp:discover\"\r\n" \
    "\r\n";

ESP8266WebServer HTTP(80);

void msearch_send(String str){
  char msg[1024];

  // convert string to char array
  str.toCharArray(msg,1024);
  
  udp_msearch.beginPacketMulticast(broadcast, port, WiFi.localIP());
  udp_msearch.write(msg);
  Serial.println("M-Search sent!");

  // I don't know why, endPacket needs to be run first to get localPort. 
  udp_msearch.endPacket();  

  /* You should save the localPort before stopAll
  * We need the local port because devices will respond to 
  * this port we use when sending the M-search request. */
  portlocal = udp_msearch.localPort(); 
  
  // To create a new listener with localPort, you must stop the current one. 
  udp_msearch.stopAll();
}



char *msearch_receive(){
  int packetSize = udp_receive.parsePacket();
  // Is there a packet we get? 
  if (packetSize) {
    Serial.println("===================================================================="); 
    Serial.printf("Received packet of size %d from %s:%d\n\t\t\t   to %s:%d\n",
                  packetSize,
                  udp_receive.remoteIP().toString().c_str(), udp_receive.remotePort(),
                  udp_receive.destinationIP().toString().c_str(), udp_receive.localPort());

    // read the packet into packetBufffer
    int n = udp_receive.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    packetBuffer[n] = 0;
    Serial.println(packetBuffer);
    return packetBuffer;
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    digitalWrite(statuspin, HIGH);
    Serial.println("Connected to Wi-Fi!");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.printf("Starting HTTP Server...\n");
    HTTP.on("/index.html", HTTP_GET, []() {
      HTTP.send(200, "text/plain", "Hello World!");
    });

    HTTP.on("/msearch.html", HTTP_GET, []() {
      Serial.printf("Discovery Session is starting...\n");
      
      // to create and send M-search request
      msearch_send((String)sMsearch);

      // to create listener to get M-search responses. 
      udp_receive.begin(portlocal);
      
      delay(10);
      /* Since the response is created in the loop function, 
       * the first HTTP response will be empty. This means that 
       * every "/msearch.html" response is from the previous request. */
      HTTP.send(200, "text/plain", response); 

      // to clear old responses
      memset(response, 0, sizeof response);
    });

    HTTP.on("/description.xml", HTTP_GET, []() {
      SSDP.schema(HTTP.client());
    });
    HTTP.begin();

    Serial.printf("Starting SSDP...\n");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("NodeMCU ESP8266");
    SSDP.setSerialNumber(ESP.getChipId());
    SSDP.setURL("index.html");
    SSDP.setModelName("ESP-12E");
    SSDP.setModelNumber("1.0");
    SSDP.setModelURL("https://www.nodemcu.com");
    SSDP.setManufacturer("Espressif Systems");
    SSDP.setManufacturerURL("https://www.espressif.com/");
    SSDP.begin();
    Serial.printf("SSDP NOTIFY sent!\n");

  } else {
    Serial.printf("WiFi Failed\n");
    digitalWrite(statuspin, LOW);
    while (1) {
      delay(10);
    }
  }
}

void loop() {
  recv = msearch_receive();
  // If a packet is received, save it in the buffer.
  if(recv){
      int len = strlen(response);
      sprintf(response+len, recv);
  }
  delay(1);
  HTTP.handleClient();
}
