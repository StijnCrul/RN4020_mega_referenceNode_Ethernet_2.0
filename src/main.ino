#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#define DEBUG

#define DEVICE_ID    1
#define NUM_DEV      6

#define RX           0
#define TX           1
#define WAKESW       2
#define MLDP         3
#define CONN         4            //used by ethernet shield
#define MLDPEV       5
#define ACTIVITY     6
#define WAKEHW       7
#define BUTTON       8
#define LED          13            //used by ethernet shield


//states for statemachine
enum states{
  NOTHING,
  HTTP_POLL,
  NODEJS_CONNECT,
  BT_CONFIGURE_SCAN,
  BT_SCANNING,
  BT_CONFIGURE_ADVERTISE,
  BT_ADVERTISING,
  COMMISSIONED
};
states state = NODEJS_CONNECT;
states previousState = NOTHING;

//Arrays for device information
String MAC;
String priv;
String RSSIHex;
String brcst;

String httpReply = "";

//RSSI values
signed char RSSIValue;
signed char TXPower;

IPAddress server(10,128,69,169);                      //10,128,69,169 of 192,168,0,102

EthernetClient client;
byte mac[NUM_DEV][6] = {{0x90, 0xA2, 0xDA, 0x0E, 0xF4, 0xBA},
                        {0x90, 0xA2, 0xDA, 0x0E, 0xF4, 0xBB},
                        {0x90, 0xA2, 0xDA, 0x0E, 0xF4, 0xBC},
                        {0x90, 0xA2, 0xDA, 0x0E, 0xF4, 0xBD},
                        {0x90, 0xA2, 0xDA, 0x0E, 0xF4, 0xBE},
                        {0x90, 0xA2, 0xDA, 0x0E, 0xF4, 0xBF}};



void setupTimer(){        //datasheet atmega2560
  cli(); 				          //disable global interupts
  TCCR1A = 0;             // set entire TCCR1A register to 0
  TCCR1B = 0;             // same for TCCR1B
  TCNT1  = 0;             //initialize counter value to 0
  OCR1A = 62499; 			    //set compare match register to 62499 (+ 1 for reset) => counts till 62,5k => (8 / 16 MHz * 62500 k = 1 second and one tick takes 16 µs
  TCCR1B |= 1 << CS12; 		  //prescaler op 256 (datasheet page 157, CSn2, = 1);
  TCCR1B |= 1 << WGM12; 	  //set compare timer mode (datasheet page 133 table);
  TIMSK1 |= 1 << OCIE1A; 	  //enable time compare interrupt
  sei(); 				          //enable global interrupts
}

void disableTimerInterrupt(){
  #ifdef DEBUG
    Serial.println("disable timer");
  #endif
  TCCR1B &= ~(1 << CS12); 		  //prescaler op 256 (datasheet page 157, CSn2, = 1);
  TCCR1B &= ~(1 << WGM12);
  TIMSK1 &= ~(1 << OCIE1A);
}

void enableTimerInterrupt(){
  #ifdef DEBUG
    Serial.println("enable timer");
  #endif
  TCCR1B |= 1 << CS12; 		  //prescaler op 256 (datasheet page 157, CSn2, = 1);
  TCCR1B |= 1 << WGM12; 	  //set compare timer mode (datasheet page 133 table);
  TIMSK1 |= 1 << OCIE1A; 	  //enable time compare interrupt
}

//Read from BT-module and echo to pc.
void BTRead(){
  char temp;
  delay(100);
  while(Serial1.available() && temp != '\n'){
    temp = Serial1.read();
    #ifdef DEBUG
      Serial.write(temp);
    #endif
  }
}

//Read from BT-module and echo to pc (idem but without delay).
void BTReadNoDelay(){
  char temp;
  while(Serial1.available() && temp != '\n'){
    temp = Serial1.read();
    #ifdef DEBUG
      Serial.write(temp);
    #endif
  }
}

void NodeJSConnect(){
  String httpResponse;
  // setup ethernet
  if (Ethernet.begin(mac[DEVICE_ID]) == 0) {
    #ifdef DEBUG
      Serial.println(F("Failed to configure Ethernet using DHCP"));
    #endif
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  // print your local IP address:
  Serial.println(Ethernet.localIP());
  // setup connection to MATLAB
  if (client.connect(server, 3000)) {
    #ifdef DEBUG
      Serial.println("connected");
    #endif
    delay(100);
    //client.write('a');
    // Make a HTTP request:
    String url = "GET /devices/";
    url.concat(DEVICE_ID);
    url.concat("/");
    for(int i = 0; i < 6; i++){
      url.concat(String(mac[DEVICE_ID][i]));
    }
    #ifdef DEBUG
      Serial.println(url);
    #endif
    client.println(url);
    client.println("Host: 10.128.69.169");
    client.println("Connection: close");
    client.println();
    httpResponse = "";
    //get response (delay 5 ms to give time for reply)
    delay(50);
    while (client.available()) {
      char c = client.read();
      httpResponse.concat(c);
    }
    getBody(&httpResponse);
    #ifdef DEBUG
      Serial.println("Http request sent and closed");
      Serial.println("Http response: ");
      Serial.println(httpResponse);
      Serial.println("End of http response.");
      /*getBody(&httpResponse);
      Serial.println("Body of response: ");
      Serial.println(httpResponse);
      Serial.println("End of body");*/
    #endif
  } else {
    #ifdef DEBUG
      Serial.println("connection failed");
    #endif
  }
  client.stop();
}

String HTTPPoll(){                          //do a proper poll
  String httpResponse;
  if (client.connect(server, 3000)) {
    #ifdef DEBUG
      Serial.println("connected");
    #endif
    delay(100);
    //client.write('a');
    // Make a HTTP request:
    String url = "GET /poll/";
    url.concat(DEVICE_ID);
    #ifdef DEBUG
      Serial.println(url);
    #endif
    client.println(url);
    client.println("Host: 10.128.69.169");
    client.println("Connection: close");
    client.println();
    httpResponse = "";
    //get response (delay 5 ms to give time for reply)
    delay(50);
    while (client.available()) {
      char c = client.read();
      httpResponse.concat(c);
    }
    getBody(&httpResponse);
    #ifdef DEBUG
      Serial.println("Http request sent and closed");
      Serial.println("Http response: ");
      Serial.println(httpResponse);
      Serial.println("End of http response.");
      /*getBody(&httpResponse);
      Serial.println("Body of response: ");
      Serial.println(httpResponse);
      Serial.println("End of body");*/
    #endif
  } else {
    #ifdef DEBUG
      Serial.println("connection failed");
    #endif
  }
  client.stop();
  return httpResponse;
}

void getBody(String * responsePointer) {
  // Get body out of response, the empty line in the HTTP packet is where
  // the body begins
  #ifdef DEBUG
    Serial.println(getStatus(responsePointer));
  #endif
  if (getStatus(responsePointer) == 200) {
    int newLinePosition = responsePointer->indexOf(F("\r\n\r\n"));
    *responsePointer = responsePointer->substring(newLinePosition + 4);
    responsePointer->trim(); // Trim whitespace at the end and the beginning from body
  } else {
    *responsePointer = String("");
  }
}

int getStatus(String * responsePointer) {
  // Get status of HTTP message
  byte spacePosition = responsePointer->indexOf(' ');
  String statusString = responsePointer->substring(spacePosition + 1, spacePosition + 4);
  return statusString.toInt();
}

states parseHttpReply(String http){
  disableTimerInterrupt();
  states nextState = NOTHING;
  if(previousState == NOTHING){
    if(http.startsWith("POLL_ACK_GO_SCAN")){
      nextState = BT_CONFIGURE_SCAN;
    }else if(http.startsWith("POLL_ACK_GO_BROADCAST")){
      nextState = BT_CONFIGURE_ADVERTISE;
    }
  }else{
    if(http.startsWith("POLL_ACK_SCANNING")){
      nextState = BT_SCANNING;
    }else if(http.startsWith("POLL_ACK_BROADCASTING")){
      nextState = BT_ADVERTISING;
    }
  }
  enableTimerInterrupt();
  return nextState;
}

void BTSetupAdvertise(){
  // Setup RN4020
  digitalWrite(WAKESW, HIGH);
  digitalWrite(WAKEHW, LOW);
  digitalWrite(MLDP, LOW);
  delay(500);
  BTRead();                              //read the "CMD" + empty buffer
  BTReadNoDelay();
  BTReadNoDelay();
  //Serial.println("+");                 //echo
  BTRead();
  Serial1.println("SF,1");               //factory reset
  BTRead();
  Serial1.println("SS,C0000000");        //set supported services
  BTRead();
  Serial1.println("SR,10000000");        //set features (peripheral, mldp enabled)
  BTRead();
  BTRead();
  Serial1.println("R,1");                //reboot so actions take effect
  delay(1500);                           //delay after reboot important!
  BTRead();                              //read "reboot" and "CMD"
  digitalWrite(WAKESW, LOW);
  digitalWrite(WAKEHW, LOW);
  digitalWrite(MLDP, LOW);
  BTRead();
}

void BTAdvertise(){
  digitalWrite(WAKESW, HIGH);
  digitalWrite(WAKEHW, LOW);
  digitalWrite(MLDP, LOW);
  delay(500);
  Serial1.println("SP,4");
  BTRead();
  Serial1.println("N,00000004");
  BTRead();
  Serial1.println("A");
  BTRead();
  digitalWrite(WAKESW, LOW);
  digitalWrite(WAKEHW, LOW);
  digitalWrite(MLDP, LOW);
  BTRead();
}

//maybe not necessary todo! (probably delete and change with multiple settings functions for the state machine)
void BTSetupScan(){
  digitalWrite(WAKESW, HIGH);
  digitalWrite(WAKEHW, LOW);
  digitalWrite(MLDP, LOW);
  delay(1000);
  BTReadNoDelay();
  Serial1.println(F("derp"));           //first command always gives error (don't know why, get the ERR message back)
  BTRead();
  BTReadNoDelay();
  //Serial.println("+");                //echo
  BTRead();
  Serial1.println(F("V"));               //check device information
  BTRead();
  Serial1.println(F("SF,1"));            //factory reset
  BTRead();
  Serial1.println(F("SS,C0000000"));     //set supported services
  BTRead();

  Serial1.println(F("SR,90000000"));     //set features (central, MLDP enabled)
  BTRead();
  BTRead();
  Serial1.println(F("R,1"));             //reboot so actions take effect
  delay(1500);                          //delay after reboot important!
  BTRead();                             //read "reboot" and "CMD"
  digitalWrite(WAKESW, LOW);
  digitalWrite(WAKEHW, HIGH);
  digitalWrite(MLDP, LOW);
  BTRead();
}

//scan for BLE devices in the environment
void setupScanDevices(){
  #ifdef DEBUG
    Serial.println("Setup BT scan");
  #endif
  digitalWrite(WAKESW, HIGH);
  digitalWrite(WAKEHW, LOW);
  digitalWrite(MLDP, LOW);
  delay(100);
  Serial1.println(F("J,1"));           //observer mode
  BTRead();
  Serial1.println(F("F"));             //scan environment for devices
  delay(100);
  BTRead();
  digitalWrite(WAKESW, LOW);
  digitalWrite(WAKEHW, HIGH);
  digitalWrite(MLDP, LOW);
}

//Parse all scanned BT devices and save all their variables.
void parseDevices(){
  MAC = "";
  priv = "";
  RSSIHex = "";
  brcst = "";
  bool keepReading = true;
  char incomingChar;
  int stringCnt = 0;        //The incoming string exists out of 4 parts seperated by commas: MAC, private, RSSI, broadcast (this is specifically for broadcaster role, regular advertisements exist of out 5 parts: MAC, private, device name, UUID, RSSI)
  while (keepReading){
    keepReading = false;
    if(Serial1.available() > 0){
      keepReading = true;
      incomingChar = Serial1.read();
      Serial.write(incomingChar);
      if(incomingChar == ','){
        stringCnt++;
      }else if(incomingChar == '\n'){
        stringCnt = 0;
      }else{
        switch(stringCnt){
          case 0:
            MAC += incomingChar;
            break;
          case 1:
            priv += incomingChar;
            break;
          case 2:
            RSSIHex += incomingChar;
            break;
          case 3:
            brcst += incomingChar;
            break;
          default: ;
            #ifdef DEBUG
              Serial.println(F("Voor gilles <3"));
            #endif
        }
      }
    }
  }
}

//Parse an ASCII-HEX value and return a DEC int.
void hexToInt(){
  signed char value = 0;
  bool negative = false;
  if(RSSIHex[0] == '-'){
    negative = true;
    for(int j = 1 ; j < RSSIHex.length(); j++){
      if(RSSIHex[j] >= 0x30 && RSSIHex[j] <= 0x39){       //between 0x30 (0) && 0x39 (9) substract 0x30 for conversion to int
        value = value * 0x10 + RSSIHex[j] - 0x30;
      }else if(RSSIHex[j] >= 0x41 && RSSIHex[j] <= 0x45){ //between 0x41 (A) && 0x45 (E) substract 0x37 for conversion to int
        value = value * 0x10 + RSSIHex[j] - 0x37;
      }
    }
    value *= -1;
    if(value > -20) value = 0;
  } else {
    for(int j = 0 ; j < RSSIHex.length(); j++){
      if(RSSIHex[j] >= 0x30 && RSSIHex[j] <= 0x39){       //between 0x30 (0) && 0x39 (9) substract 0x30 for conversion to int
        value = value * 0x10 + RSSIHex[j] - 0x30;
      }else if(RSSIHex[j] >= 0x41 && RSSIHex[j] <= 0x46){ //between 0x41 (A) && 0x46 (F) substract 0x37 for conversion to int
        value = value * 0x10 + RSSIHex[j] - 0x37;
      }
    }
  }

  if(value != 0 && negative){
    RSSIValue = value;
    TXPower = brcst[brcst.length()-2] - 0x30;
    #ifdef DEBUG
      Serial.println(F("RSSIHex, RSSIDec, TXPower: "));
      Serial.println(RSSIHex);
      Serial.println(RSSIValue, DEC);
      Serial.println(TXPower, DEC);
    #endif
  }
  value = 0;                                            //reset variables
  negative = false;
}

void setup(){
  // setting up pins
  pinMode(WAKESW, OUTPUT);
  pinMode(MLDP, OUTPUT);
  pinMode(WAKEHW, OUTPUT);

  // setup timer for polling
  setupTimer();

  // setup serial for debugging
  Serial.begin(115200);
  Serial1.begin(115200);
  #ifdef DEBUG
    Serial.println("Setup is done, let's get started");
  #endif

}

void loop(){
  switch(state){
    case NOTHING:                         //todo
      state = NOTHING;
      break;
    case HTTP_POLL:                       //todo
      Serial.println("HTTP_POLL!");
      httpReply = HTTPPoll();             //parse reply and change state appropriately
      state = parseHttpReply(httpReply);
      break;
    case NODEJS_CONNECT:                  //todo
      NodeJSConnect();
      state = NOTHING;
      break;
    case BT_CONFIGURE_ADVERTISE:
      Serial.println("Configure the advertisements");
      disableTimerInterrupt();    //TEMPORARY! HAS TO POLL TO KNOW WHEN TO STOP
      BTSetupAdvertise();
      BTAdvertise();
      state = BT_ADVERTISING;
      break;
    case BT_CONFIGURE_SCAN:
      Serial.println("Configure the scan");
      disableTimerInterrupt();
      setupScanDevices();
      state = BT_SCANNING;
      break;
    case BT_ADVERTISING:          //todo
      state = BT_ADVERTISING;
      break;
    case BT_SCANNING:              //todo
      delay(100);
      parseDevices();
      hexToInt();
      state = BT_SCANNING;
      break;
    case COMMISSIONED:         //todo
      state = NOTHING;
      break;
    default:
      #ifdef DEBUG
        Serial.println(F("statemachine default, something went wrong"));
      #endif
      state = NOTHING;
      BTReadNoDelay();
  }
  //Serial.print("State: ");
  //Serial.println(state);
}

ISR(TIMER1_COMPA_vect){
  previousState = state;
  state = HTTP_POLL;
}
