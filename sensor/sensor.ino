#include "painlessMesh.h"
#include <Preferences.h>

#define URECHO_LEFT  23        // PWM output 0-50000us, every 50us represent 1cm (blue)
#define URTRIG_LEFT  22        // Trigger pin (green)
#define URECHO_RIGHT 19         // PWM output 0-50000us, every 50us represent 1cm (blue)
#define URTRIG_RIGHT 18        // Trigger pin (green)

#define DELAY_INTERLACE 25     // Delay time between switching between sensors
#define DELAY_TRIG      10     // Trigger pulse duration

#define NETWORK_RATE 1

///
#define   MESH_PREFIX     "GarageBat"
#define   MESH_PASSWORD   "garagePassword"
#define   MESH_PORT       5555
painlessMesh network;
Scheduler sensorScheduler; // tasks control
bool debug = true;
///

unsigned long t = millis();
String state;

void leftSensorRead();
void rightSensorRead();
Task tl(100, TASK_FOREVER, &leftSensorRead);
Task tr(100, TASK_FOREVER, &rightSensorRead);
Scheduler runner;

bool left_stop = false;
bool right_stop = false;

Preferences prefs;
int dist_cutoff;

void setup() {
  Serial.begin(115200);

  pinMode(URTRIG_LEFT, OUTPUT);           // A low pull on pin COMP/TRIG
  pinMode(URTRIG_RIGHT, OUTPUT);
  digitalWrite(URTRIG_LEFT, HIGH);        // Set to HIGH
  digitalWrite(URTRIG_RIGHT, HIGH);
  pinMode(URECHO_LEFT, INPUT);            // Send enable PWM mode command
  pinMode(URECHO_RIGHT, INPUT);

  ///
  network.setDebugMsgTypes( ERROR | STARTUP );
  network.init( MESH_PREFIX, MESH_PASSWORD, &sensorScheduler, MESH_PORT );    //start network with name, password, task controller, on port number
  network.onReceive(&receivedCallback);   //runs every time a message is received
  network.onNewConnection(&newConnectionCallback);    //runs when a device joins
  network.onChangedConnections(&changedConnectionCallback);   //runs when connections change
  network.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);  //runs when device updates it's time to match the network
  ///

  prefs.begin("GarageBat");
  dist_cutoff = prefs.getInt("dist_cutoff", 100); // default to 100

  // prefs.putInt("counter", counter);

  delay(500); // Wait for initialization
  Serial.println("Sensors initialized");

  runner.init();
  runner.addTask(tl);
  runner.addTask(tr);
  tl.enable();
  tr.enable();
}

void loop() {
  runner.execute();
  network.update();
}

void leftSensorRead() {
  // Take a reading by sending out a trigger pulse and reading the width of the response pulse
  digitalWrite(URTRIG_LEFT, LOW);
  delay(DELAY_TRIG);
  digitalWrite(URTRIG_LEFT, HIGH);
  unsigned long pulse_time_left = pulseIn(URECHO_LEFT, LOW);
  unsigned int dist_left = pulse_time_left > 50000 ? 0 : pulse_time_left / 50.0;
  if (debug) Serial.printf("Left: %3dcm \n", dist_left);
  left_stop = checkInRange(dist_left, dist_cutoff);
  if (right_stop) {
    checkAndSendNetworkBroadcast("stop");
  } else {
    checkAndSendNetworkBroadcast((left_stop ? "stop" : "clear"));
  }
}

void rightSensorRead() {
  // Take a reading by sending out a trigger pulse and reading the width of the response pulse
  digitalWrite(URTRIG_RIGHT, LOW);
  delay(DELAY_TRIG);
  digitalWrite(URTRIG_RIGHT, HIGH);
  unsigned long pulse_time_right = pulseIn(URECHO_RIGHT, LOW);
  unsigned int dist_right = pulse_time_right > 50000 ? 0 : pulse_time_right / 50.0;
  if (debug) Serial.printf("Right: %3dcm \n", dist_right);
  right_stop = checkInRange(dist_right, dist_cutoff);
  if (left_stop) {
    checkAndSendNetworkBroadcast("stop");
  } else {
    checkAndSendNetworkBroadcast((right_stop ? "stop" : "clear"));
  }
}

bool checkInRange(int dist, int cutoff) {
  if (dist == 0) return false;
  return (dist < cutoff) ? true : false;
}

void checkAndSendNetworkBroadcast(String new_state) {
  if (new_state != state) {
    network.sendBroadcast(new_state);
    timer_rst();
    if (debug) Serial.printf("Update send: %s\n", new_state);
  } else if (millis() - timer() > (1000.0 / NETWORK_RATE)) {
    network.sendBroadcast(new_state);
    timer_rst();
    if (debug) Serial.printf("Timed send: %s\n", new_state);
  }
  state = new_state;
}

int timer() {
  if (millis() < t) t = millis() + (sizeof(unsigned long) - t);
  return t;
}

void timer_rst() {
  t = millis();
}

//Callbacks
void receivedCallback( uint32_t from, String &msg ) {       //when a message directed to this device is received | from is the sender ID, msg is the message as a String 
    if (debug) {
        Serial.println("callback received from sign: " + msg);
    }
}

void newConnectionCallback(uint32_t _nodeID) { 
    if (debug) Serial.println("");
}

void changedConnectionCallback() {

}

void nodeTimeAdjustedCallback(int32_t offset) {
    if (debug) Serial.printf("time adjusted: %u. new offset = %d\n", network.getNodeTime(),offset);
}