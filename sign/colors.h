#ifndef colors_h
#define colors_h

struct led_color {
  int red;
  int green;
  int blue;
};

led_color none = {
  -1,
  -1,
  -1
};

led_color off = {
  0,    //red
  0,    //green
  0     //blue
};

led_color white = {
  255,  //red
  255,  //green
  255   //blue
};

led_color red = {
  255,  //red
  0,    //green
  0     //blue
};

led_color red50 = {
  255/2,  //red
  0,      //green
  0       //blue
};

led_color redDim = {
  25,   //red
  0,    //green
  0     //blue
};

led_color green = {
  0,  //red
  255,    //green
  0     //blue
};

led_color blue = {
  0,  //red
  0,    //green
  255     //blue
};

led_color darkPurple = {
  255,  //red
  0,    //green
  255     //blue
};

led_color purple = {
  100,  //red
  0,    //green
  255     //blue
};

led_color orange = {
  255,  //red
  30,    //green
  0     //blue
};

led_color yellow = {
  255,  //red
  255,    //green
  0     //blue
};

led_color cyan = {
  0,  //red
  255,    //green
  255     //blue
};

#endif
