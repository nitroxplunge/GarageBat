#include "painlessMesh.h"

#define URECHO_LEFT  23        // PWM output 0-50000us, every 50us represent 1cm (blue)
#define URTRIG_LEFT  22        // Trigger pin (green)
#define URECHO_RIGHT 19         // PWM output 0-50000us, every 50us represent 1cm (blue)
#define URTRIG_RIGHT 18        // Trigger pin (green)

#define DELAY_INTERLACE 25     // Delay time between switching between sensors
#define DELAY_TRIG 10          // Trigger pulse duration

///
#define   MESH_PREFIX     "GarageBat"
#define   MESH_PASSWORD   "garagePassword"
#define   MESH_PORT       5555
painlessMesh network;
Scheduler signScheduler; // tasks control
bool debug = false;
///

///
#include <Adafruit_NeoPixel.h>
#include "images.cpp"
#include "colors.h"
#define PIN_NEO_PIXEL 16  // The ESP32 pin GPIO16 connected to NeoPixel
bool serialControl = true;
Adafruit_NeoPixel NeoPixel(NUM_PIXELS, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800);
int image[5][125];  //row 0: image, row 1: R, row 2: G, row 3: B, row 4: dictionary
int dictionary[125];
long int startTime = 0;   //for controlling led blinks
int blinkCount = 0;
//basic LED RF control
bool rf = false;    //desired LED state from rf - false is off
bool signState = false;   //state of LEDs - false is off
///


///

//uses dictionary bundled with image
void led_display(const int _image[][NUM_PIXELS], int _numPixels) {
  NeoPixel.clear();
  if (debug) Serial.println("starting sign");
  for (int pixel = 0; pixel < _numPixels; pixel++) {
    if (debug) Serial.println("at index " + pixel);
      if (_image[0][pixel] == 1) {
        if (debug) Serial.println("turning on ID: " + _image[4][pixel]);
        if (_image[1][pixel] == 0 && _image[2][pixel] == 0 && _image[3][pixel] == 0) {
          NeoPixel.setPixelColor(_image[4][pixel], NeoPixel.Color(255, 255, 255));
        } else {
          //looks up the pixel ID at the index(pixel) and sets it to the specified RGB value at that same index
          NeoPixel.setPixelColor(_image[4][pixel], NeoPixel.Color(_image[1][pixel], _image[2][pixel], _image[3][pixel]));
        }
      }

  }

  NeoPixel.show();

}

//using a override color for bars and text - none will use the stored value
void led_display(const int _image[][NUM_PIXELS], int _numPixels, led_color _textColor, led_color _barColor) {
  NeoPixel.clear();
  if (debug) Serial.println("starting sign");
  for (int pixel = 0; pixel < _numPixels; pixel++) {
    if (debug) Serial.println("at index " + pixel);
      if (_image[0][pixel] == 1) {
        if (debug) Serial.println("turning on ID: " + _image[4][pixel]);
        if (_image[4][pixel] >= 0 && _image[4][pixel] <= 4 || _image[4][pixel] >= 120 && _image[4][pixel] <= 124) {
            if (_barColor.red == none.red) {
              NeoPixel.setPixelColor(_image[4][pixel], NeoPixel.Color(_image[1][pixel], _image[2][pixel], _image[3][pixel]));
            } else {
              NeoPixel.setPixelColor(_image[4][pixel], NeoPixel.Color(_barColor.red, _barColor.green, _barColor.blue));
            }
        }else {
          //looks up the pixel ID at the index(pixel) and sets it to the specified RGB value at that same index
          if (_textColor.red == none.red) {
            NeoPixel.setPixelColor(_image[4][pixel], NeoPixel.Color(_image[1][pixel], _image[2][pixel], _image[3][pixel]));
          } else {
            NeoPixel.setPixelColor(_image[4][pixel], NeoPixel.Color(_textColor.red, _textColor.green, _textColor.blue));
          }
        } 
      }

  }

  NeoPixel.show();

}
///

void setup() {
  // Serial.begin(115200);

  ///
  network.setDebugMsgTypes( ERROR | STARTUP );  
  network.init( MESH_PREFIX, MESH_PASSWORD, &signScheduler, MESH_PORT );    //start network with name, password, task controller, on port number
  network.onReceive(&receivedCallback);   //runs every time a message is received
  network.onNewConnection(&newConnectionCallback);    //runs when a device joins
  network.onChangedConnections(&changedConnectionCallback);   //runs when connections change
  network.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);  //runs when device updates it's time to match the network
  ///

  ///
  NeoPixel.begin();  // initialize NeoPixel strip object (REQUIRED)

  // if (debug || serialControl) Serial.begin(9600);
  // Serial.setTimeout(10);
  // if (serialControl) Serial.println("serial control on");
  // if (debug) Serial.println("debug on");  

  NeoPixel.clear();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called

  //startup demo
  for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {           // for each pixel
    NeoPixel.setPixelColor(pixel, NeoPixel.Color(200, 0, 0));  // it only takes effect if pixels.show() is called
  }
  NeoPixel.show();  // update to the NeoPixel Led Strip
  delay(200);   

  NeoPixel.clear();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called
  NeoPixel.show();  // update to the NeoPixel Led Strip
  ///
}

void loop() {
  network.update();

  // if (serialControl) {
  //   if (Serial.available() > 0) {
  //     String str = Serial.readString();
  //     if (str == "on") {
  //       rf = true;
  //       Serial.println ("turned on sign");
  //     } else if (str == "off"){
  //       rf = false;
  //       Serial.println ("turned off sign");
  //     }
  //   }
  // }


//basic LED RF control
  if (rf && !signState) {   //rf wants on, and sign is off
    signState = true;
    led_display(stop_image, 125, red, red);
    NeoPixel.show();  // update to the NeoPixel Led Strip
    delay(4000);
  } else if (!rf && signState) {  //rf wants off, and sign is on
    signState = false;
    NeoPixel.clear();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called
    NeoPixel.show();  // update to the NeoPixel Led Strip
  }

}

///

//Callbacks
//##############################################################
//This is where you act on a message that is received
void receivedCallback( uint32_t from, String &msg ) {   //when a message directed to this device is received | from is the sender ID, msg is the message as a String 
    if (msg == "stop") {
        Serial.println("stop received from sensor at " + String(millis()));
        rf = true;
        network.sendBroadcast("stop received");
        //Serial.println(millis());

        //start doing led and sign things
        //could create it as a task and then just enable and disable it
    } else if (msg == "clear") {
        Serial.println("clear message received at " + String(millis()));
        rf = false;
        //stop doing led things
        //stop the do led and sign things task
    }
}
//###############################################################

void newConnectionCallback(uint32_t _nodeID) {
    
}

void changedConnectionCallback() {

}

void nodeTimeAdjustedCallback(int32_t offset) {
    if (debug) Serial.printf("time adjusted: %u. new offset = %d\n", network.getNodeTime(),offset);
}