
#include <Adafruit_NeoPixel.h>
#include "images.cpp"
#include "colors.h"


// defined in images.cpp #define NUM_PIXELS 125     // The number of LEDs (pixels) on NeoPixel LED strip
#define PIN_NEO_PIXEL 16  // The ESP32 pin GPIO16 connected to NeoPixel
#define PIN_BUTTON 33 //gpio 33 for button
#define PIN_BUZZER_1 18
#define PIN_BUZZER_2 19
#define PWM1_Ch    0
#define PWM1_Res   8
#define PWM1_Freq  1000

bool debug = true;
bool serialControl = true;

Adafruit_NeoPixel NeoPixel(NUM_PIXELS, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800);

int image[4][5][25];  //row 0: image, row 1: R, row 2: G, row 3: B, row 4: dictionary
int dictionary[125];
long int startTime = 0;   //for controlling led blinks
long int startTimeButton = 0;   //tracking how long a button is pressed
long int startTimeBeep = 0;     //beep duration tracking
bool buttonStates[2] = {false, false}; //tracking where the button was and where the button now is(the missile knows
bool buttonState = false;
bool longPressed = false;
int blinkCount = 0;
double hOffset = 0;
double entranceAngle = 0;
double sensorAngle = 0;
int longPressTime = 500;     //how long a long press is
int debounce = 40;    //how long a button has to be read before it counts as a press
int pressType = 0;
int tone = 150;   //beep sound
int beeping = 0;    //which beep is currently happening
int page = 0;     //which page of the menu we are on
bool update = false;    //screen has an update to run

//inches range 3-36
//driveway angle -30 to 30
//degrees 0 to 90

//todo may need another or something to denote inches feet and deg

enum numberType {
  In,
  Deg,
  Feet,
  Base
};

numberType displayNumberType = Base;

//basic LED RF control
bool rf = false;    //desired LED state from rf - false is off
bool signState = false;   //state of LEDs - false is off



void beep(int _type, long int &_startTimeBeep, int &_beeping, int _tone, int _page) {
  if (_type == 1 && _beeping == 0 && _page != 0 && _page != 3) {  //short beep
    ledcWrite(PWM1_Ch, _tone);
    _beeping = _type;
    _startTimeBeep = millis();
  } else if (_type == 2 && _beeping == 0) {   //long beep
    ledcWrite(PWM1_Ch, _tone);
    _beeping = _type;
    _startTimeBeep = millis();
  }

  if (_beeping == 1 && millis() - _startTimeBeep > 50) {
    ledcWrite(PWM1_Ch, 0);
    _beeping = 0;
    _startTimeBeep = 0;
  } else if (_beeping == 2 && millis() - _startTimeBeep > 150) {
    ledcWrite(PWM1_Ch, 0);
    _beeping = 0;
    _startTimeBeep = 0;
  }
  
  // if (_beeping == 0 && _startTimeBeep == 0) {
  //   ledcWrite(PWM1_Ch, 0);
  // }
}

//returns the current button press type 0 for off 1 for short 2 for long
int checkButton(long int &_startTimeButton, int _longPressTime, int _debounce, bool &_buttonState, bool &_longPressed) {
  if (!digitalRead(PIN_BUTTON) && !_buttonState) {
    _startTimeButton = millis();
    _buttonState = true;
   // ledcWrite(PWM1_Ch, 0);
  }
  
  if (digitalRead(PIN_BUTTON) && millis() - _startTimeButton > _debounce && millis() - _startTimeButton < _longPressTime && buttonState == true) {
    _buttonState = false;
    _startTimeButton = 0;
   // ledcWrite(PWM1_Ch, 0);
    return 1;
  }

  if (!digitalRead(PIN_BUTTON) && millis() - _startTimeButton > _longPressTime && _longPressed == false && _buttonState == true) {
    _longPressed = true;
    return 2;
  }
  if (digitalRead(PIN_BUTTON) && millis() - _startTimeButton >  _longPressTime && _longPressed == true) {
    _buttonState = false;
    _longPressed = false;
    _startTimeButton = 0;
    //ledcWrite(PWM1_Ch, 0);
    //return 2;
  }

  return 0;
}



void menu(int _type, double &_hOffset, double &_entranceAngle, double &_sensorAngle, int &_page, bool &_update) {
  if (_type == 2) {
    _page++;
    _update = true;
    if (_page > 3) _page = 0;
  }

  if (_page == 1) { //inches input
    if (_type == 1) {
      _hOffset++;
      if (_hOffset > 36) _hOffset = 0;
      _update = true;
    }
  } else if (_page == 2) {
    if (_type == 1) {
      _entranceAngle++;
      if (_entranceAngle > 30) _entranceAngle = -30;
      _update = true;
    }

  } else if (_page == 3) {
    int _calculatedAngle = 0;
    //do calculation stuff
    if (_sensorAngle != _calculatedAngle) {
      _sensorAngle = 45;    // give the user the sensor angle they need
      _update = true;
    }
  }
}

void printDouble( double val, unsigned int precision){
// prints val with number of decimal places determine by precision
// NOTE: precision is 1 followed by the number of zeros for the desired number of decimal places
// example: printDouble( 3.1415, 100); // prints 3.14 (two decimal places)

    Serial.print (int(val));  //prints the int part
    Serial.print("."); // print the decimal point
    unsigned int frac;
    if(val >= 0)
        frac = (val - int(val)) * precision;
    else
        frac = (int(val)- val ) * precision;
    Serial.println(frac,DEC) ;
} 

void convertNumbers(int _image[][5][NUM_PIXELS / 5], double _number, int _rows, int _columns, numberType _numberType) {
  enum digitsType {
    OneDigit,   //no decimal
    TwoDigit,   //no decimal
    ThreeDigit, //no decimal
    TwoDigitDec,  //with decimal
    ThreeDigitDec,  //with decimal
    //FourDigitDec    //with decimal (if using hundredths)
  };
  
  int _nType = 0;
  double offset = abs(_number); //no negatives for calculation
  int hundreds = 0;
  int tens = 0;
  int ones = 0;
  int tenths = 0;
  int startIndexArray = 0;
  bool _decimal = false;

//arrays of starting positions for number types [#, ##, ###, #.#, ##,#][h,t,o,d,th,-][start,end]
 const int _startPositions [5][7][2] {
  //one Digit
  {{ 0,  0},    //hundreds start & end pos
   { 0,  0},    //tens start & end pos
   {11, 14},    //ones start & end pos
   { 0,  0},    //decimal start & end pos
   { 0,  0},    //tenths start & end pos
   { 7, 10},     //negative sign start & end pos
   {15, 18}     //type start & end pos
  },
  //two Digit
  {{ 0,  0},    //hundreds start & end pos
   { 9, 12},    //tens start & end pos
   {13, 16},    //ones start & end pos
   { 0,  0},    //decimal start & end pos
   { 0,  0},    //tenths start & end pos
   { 5,  8},     //negative sign start & end pos
   {17, 20}     //type start & end pos
  },
  //three Digit
  {{ 7, 10},    //hundreds start & end pos
   {11, 14},    //tens start & end pos
   {15, 18},    //ones start & end pos
   { 0,  0},    //decimal start & end pos
   { 0,  0},    //tenths start & end pos
   { 3,  6},     //negative sign start & end pos
   {19, 22}     //type start & end pos
  },
  //Two Digit Decimal
  {{ 0,  0},    //hundreds start & end pos
   { 0,  0},    //tens start & end pos
   { 8, 11},    //ones start & end pos
   {12, 12},    //decimal start & end pos
   {14, 17},    //tenths start & end pos
   { 4,  7},     //negative sign start & end pos
   {18, 21}     //type start & end pos
  },
  //Three Digit Decimal
  {{ 0,  0},    //hundreds start & end pos
   { 4,  7},    //tens start & end pos
   { 8, 11},    //ones start & end pos
   {12, 12},    //decimal start & end pos
   {14, 17},    //tenths start & end pos
   { 0,  3},     //negative sign start & end pos
   {18, 21}     //type start & end pos
  },
 };




//todo replace with new types
  switch (_numberType) {
    case In:
      _nType = 0;
      break;
    case Deg:
      _nType = 1;
      break;
    case Feet:
      _nType = 2;
      break;
    default:
      _nType = 3;
      //break;
  }


if (debug) printDouble(offset, 1000);

//could make one while with if elses
  while (offset >= 100) {
    hundreds++;
    offset = offset - 100;
    if (hundreds > 99) return;
  }

  while (offset >= 10) {
    tens++;
    offset = offset - 10;
  }

  while (offset >= 1) {
    ones++;
    offset = offset -1;
  }

  while (offset >= .1) {
    tenths++;
    offset = offset - .1;
  }

//
  if (hundreds) {
    startIndexArray = 2;
  } else if (tens) {
    startIndexArray = 1;
  } else if (ones) {
    startIndexArray = 0;
  } 
  //count and match index of _startpositions
  if (tenths) {
    startIndexArray = startIndexArray + 3;
    if (startIndexArray > 4) return;  //error out array bounds
    _decimal = true;
  }

  if (debug) {
    Serial.println("offset: ");
    Serial.println("hundreds: ");
    Serial.println(hundreds);
    Serial.println("tens: ");
    Serial.println(tens);
    Serial.println("ones: ");
    Serial.println(ones);
  }
  
//                                                         center
  //  0,  1,  2,  3,  4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24
  //  0,  1,  2,  3,  4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24
  //  0,  1,  2,  3,  4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24
  //  0,  1,  2,  3,  4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24
  //  0,  1,  2,  3,  4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24

//empty LEDs
for (int r = 0; r < _rows; r++) {
  for (int col = 0; col < _columns; col++) {
    _image[0][r][col] = 0;  //r
    _image[1][r][col] = 0;  //g
    _image[2][r][col] = 0;  //b
  }
}

//todo use array to pick correct starting positions for number based on enum
  if (hundreds > 0) {
    for (int r = 0; r < _rows; r++) {
      for (int col = _startPositions[startIndexArray][0][0]; col < _startPositions[startIndexArray][0][1]; col++) {
        _image[0][r][col] = led_numbers[hundreds][r][col - _startPositions[startIndexArray][0][0]];  //r
        _image[1][r][col] = led_numbers[hundreds][r][col - _startPositions[startIndexArray][0][0]];  //g
        _image[2][r][col] = led_numbers[hundreds][r][col - _startPositions[startIndexArray][0][0]];  //b
      }
    }
  }
//tens
  if (tens > 0 || hundreds > 0) {
    for (int r = 0; r < _rows; r++) {
      for (int col = _startPositions[startIndexArray][1][0]; col < _startPositions[startIndexArray][1][1]; col++) {
        _image[0][r][col] = led_numbers[tens][r][col - _startPositions[startIndexArray][1][0]];  //r
        _image[1][r][col] = led_numbers[tens][r][col - _startPositions[startIndexArray][1][0]];  //g
        _image[2][r][col] = led_numbers[tens][r][col - _startPositions[startIndexArray][1][0]];  //b
      }
    }
  }
//ones
  if(ones >= 0 || tens > 0 || hundreds > 0) {
    for (int r = 0; r < _rows; r++) {
      for (int col = _startPositions[startIndexArray][2][0]; col < _startPositions[startIndexArray][2][1]; col++) {
        _image[0][r][col] = led_numbers[ones][r][col - _startPositions[startIndexArray][2][0]];  //r
        _image[1][r][col] = led_numbers[ones][r][col - _startPositions[startIndexArray][2][0]];  //g
        _image[2][r][col] = led_numbers[ones][r][col - _startPositions[startIndexArray][2][0]];  //b
      }
    }
  }
//decimal
  if (_decimal) {
    for (int r = 0; r < _rows; r++) {
      _image[0][r][12] = led_Decimal[r][0];  //r
      _image[1][r][12] = led_Decimal[r][0];  //g
      _image[2][r][12] = led_Decimal[r][0];  //b
    }
  }

//tenths
  if(tenths > 0) {
    for (int r = 0; r < _rows; r++) {
      for (int col = _startPositions[startIndexArray][4][0]; col < _startPositions[startIndexArray][4][1]; col++) {
        _image[0][r][col] = led_numbers[tenths][r][col - _startPositions[startIndexArray][4][0]];  //r
        _image[1][r][col] = led_numbers[tenths][r][col - _startPositions[startIndexArray][4][0]];  //g
        _image[2][r][col] = led_numbers[tenths][r][col - _startPositions[startIndexArray][4][0]];  //b
      }
    }
  }

//negative
  if (_number < 0) {
    for (int r = 0; r < _rows; r++) {
      for (int col = _startPositions[startIndexArray][5][0]; col < _startPositions[startIndexArray][5][1]; col++) {
        _image[0][r][col] = number_Types[3][r][col - _startPositions[startIndexArray][5][0]];  //r
        _image[1][r][col] = number_Types[3][r][col - _startPositions[startIndexArray][5][0]];  //g
        _image[2][r][col] = number_Types[3][r][col - _startPositions[startIndexArray][5][0]];  //b
      }
    }
  }

  if(_numberType == Deg) {
    for (int r = 0; r < _rows; r++) {
      for (int col = _startPositions[startIndexArray][6][0]; col < _startPositions[startIndexArray][6][1]; col++) {
        _image[0][r][col] = number_Types[2][r][col - _startPositions[startIndexArray][6][0]];  //r
        _image[1][r][col] = number_Types[2][r][col - _startPositions[startIndexArray][6][0]];  //g
        _image[2][r][col] = number_Types[2][r][col - _startPositions[startIndexArray][6][0]];  //b
      }
    }
  }

  if(_numberType == Feet) {
    for (int r = 0; r < _rows; r++) {
      for (int col = _startPositions[startIndexArray][6][0]; col < _startPositions[startIndexArray][6][1]; col++) {
        _image[0][r][col] = number_Types[1][r][col - _startPositions[startIndexArray][6][0]];  //r
        _image[1][r][col] = number_Types[1][r][col - _startPositions[startIndexArray][6][0]];  //g
        _image[2][r][col] = number_Types[1][r][col - _startPositions[startIndexArray][6][0]];  //b
      }
    }
  }

  if(_numberType == In) {
    for (int r = 0; r < _rows; r++) {
      for (int col = _startPositions[startIndexArray][6][0]; col < _startPositions[startIndexArray][6][1]; col++) {
        _image[0][r][col] = number_Types[0][r][col - _startPositions[startIndexArray][6][0]];  //r
        _image[1][r][col] = number_Types[0][r][col - _startPositions[startIndexArray][6][0]];  //g
        _image[2][r][col] = number_Types[0][r][col - _startPositions[startIndexArray][6][0]];  //b
      }
    }
  }
}


//row column addressing
void led_display(const int _image[][5][NUM_PIXELS / 5], int _rows, int _columns) {
  NeoPixel.clear();
  if (debug) Serial.println("starting sign");

  for (int row = 0; row < _rows; row++) {
    for (int column = 0; column < _columns; column++) {
      if (_image[0][row][column] != 0 || _image[1][row][column] != 0 || _image[2][row][column] != 0) {
        NeoPixel.setPixelColor(_image[3][row][column], NeoPixel.Color(_image[0][row][column], _image[1][row][column], _image[2][row][column]));
      }
    }
  }

  NeoPixel.show();
  
}

//row column addressing color overrides
void led_display(const int _image[][5][NUM_PIXELS / 5], int _rows, int _columns, led_color _textColor, led_color _barColor) {
  NeoPixel.clear();
  if (debug) Serial.println("starting sign");

  for (int row = 0; row < _rows; row++) {
    for (int column = 0; column < _columns; column++) {
      if (column == 0 || column == 24) {// override the bar
        if (_barColor.red == none.red && _barColor.green == none.green && _barColor.blue == none.blue) {  //no override set
          NeoPixel.setPixelColor(_image[3][row][column], NeoPixel.Color(_image[0][row][column], _image[1][row][column], _image[2][row][column]));
        } else {
          NeoPixel.setPixelColor(_image[3][row][column], NeoPixel.Color(_barColor.red, _barColor.green, _barColor.blue));
        }
      } else {
        if (_image[0][row][column] != 0 || _image[1][row][column] != 0 || _image[2][row][column] != 0) {    //if pixel should be on
          if (_textColor.red == none.red && _textColor.green == none.green && _textColor.blue == none.blue) {  //no override set use default
            NeoPixel.setPixelColor(_image[3][row][column], NeoPixel.Color(_image[0][row][column], _image[1][row][column], _image[2][row][column]));
          } else {
            NeoPixel.setPixelColor(_image[3][row][column], NeoPixel.Color(_textColor.red, _textColor.green, _textColor.blue));
          }
        }
      }
    }
  }

  NeoPixel.show();
  
}



//#############################
//OLD LED SYSTEM
//OLD!!! uses dictionary bundled with image
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


//##############################
//OLD LED SYSTEM
//OLD!!! using a override color for bars and text - none will use the stored value
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

// //uses an override dictionary
// void led_display( int _image[][NUM_PIXELS], int _dictionary[], int _numPixels) {
//   for (int pixel = 0; pixel++; pixel < _numPixels) {
//       if (_image[0][pixel] == 1) {
//         if (_image[1][pixel] == 0 && _image[2][pixel] == 0 && _image[3][pixel] == 0) {
//           NeoPixel.setPixelColor(_image[4][pixel], NeoPixel.Color(255, 255, 255));
//         } else {
//         //looks up the pixel ID at index(pixel) and sets it to the specified RGB value at that same index
//           NeoPixel.setPixelColor(_dictionary[pixel], NeoPixel.Color(_image[1][pixel], _image[2][pixel], _image[3][pixel]));
//         }
//       }
//   }


// }

//alternates between the first and second image, numToRun times, displaying each for _#ImageTime, with optional pre "delay" before starting
void led_flash(long int _startTime, int _blinkCount, int _numToRun, int _firstImageTime, int _secondImageTime, int _preDelay,  int _firstImage[][NUM_PIXELS],  int _secondImage[][NUM_PIXELS], int _numPixels) {
  //if (_blinkCount < _numToRun)
  

}

//alternates between the first and second image, using the text and bar override colors specified, _count times, displaying each for _#ImageTime, with optional pre "delay" before starting
void led_flash(int _count, int _firstImageTime, int _secondImageTime, int _preDelay,  int _firstImage[][NUM_PIXELS],  int _secondImage[][NUM_PIXELS], int _numPixels, led_color _firstTextColor, led_color _firstBarColor, led_color _secondTextColor, led_color _secondBarColor) {

}


void setup() {
  NeoPixel.begin();  // initialize NeoPixel strip object (REQUIRED)
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  // pinMode(PIN_BUZZER_1, OUTPUT);
  // pinMode(PIN_BUZZER_2, OUTPUT);

  ledcAttachPin(PIN_BUZZER_1, PWM1_Ch);
  ledcSetup(PWM1_Ch, PWM1_Freq, PWM1_Res);

  if (debug || serialControl) Serial.begin(9600);
  Serial.setTimeout(10);
  if (serialControl) Serial.println("serial control on");
  if (debug) Serial.println("debug on");  

//fill pixel map
  for (int r = 0; r < 5; r++) {
    for (int col = 0; col < 25; col++) {
      image[3][r][col] = stop_image[3][r][col];
    }
  }

  NeoPixel.clear();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called

  //startup demo
  for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {           // for each pixel
    NeoPixel.setPixelColor(pixel, NeoPixel.Color(200, 0, 0));  // it only takes effect if pixels.show() is called
  }
  NeoPixel.show();  // update to the NeoPixel Led Strip
  delay(200);   

  NeoPixel.clear();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called
  NeoPixel.show();  // update to the NeoPixel Led Strip

  if (debug) {
    // for (double d = -200; d < 200; d = d + 1) {
    //   convertNumbers(image, d, 5, 25, In);
    //   led_display(image, 5, 25, purple, blue);
    //   delay(10);
    // }

    // for (double d = -200; d < 200; d = d + 1) {
    //   convertNumbers(image, d, 5, 25, Deg);
    //   led_display(image, 5, 25, cyan, blue);
    //   delay(10);
    // }

  }
}

void loop() {
  //NeoPixel.clear();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called

  //startup demo
  // for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {           // for each pixel
  //   NeoPixel.setPixelColor(pixel, NeoPixel.Color(255, 255, 255));  // it only takes effect if pixels.show() is called
  // }
  // NeoPixel.show();  // update to the NeoPixel Led Strip
  // delay(500);     

  // NeoPixel.clear();
  // NeoPixel.show();  // update to the NeoPixel Led Strip
  // delay(250);     

  pressType = checkButton(startTimeButton, longPressTime, debounce, buttonState, longPressed);

  menu(pressType, hOffset, entranceAngle, sensorAngle, page, update);
  beep(pressType, startTimeBeep, beeping, tone, page);

  if(update) {
    switch (page) {
      case 1:
        displayNumberType = In;
        convertNumbers(image, hOffset, 5, 25, displayNumberType);
        led_display(image, 5, 25, purple, blue);
        update = false;
        break;
      case 2:
        displayNumberType = Deg;
        convertNumbers(image, entranceAngle, 5, 25, displayNumberType);
        led_display(image, 5, 25, green, blue);
        update = false;
        break;
      case 3:
        displayNumberType = Deg;
        convertNumbers(image, sensorAngle, 5, 25, displayNumberType);
        led_display(image, 5, 25, yellow, blue);
        update = false;
        break;
      default:
        displayNumberType = Base;
        convertNumbers(image, entranceAngle, 5, 25, displayNumberType);
        led_display(image, 5, 25, off, off);
        update = false;
        break;
    }

  }

  if (serialControl) {
    if (Serial.available() > 0) {
      String str = Serial.readString();
      if (str == "on") {
        rf = true;
        convertNumbers(image, 12.5, 5, 25, Feet);
        Serial.println ("turned on sign");
      } else if (str == "off"){
        rf = false;
        Serial.println ("turned off sign");
      }
    }
  }


//basic LED RF control
  if (rf && !signState) {   //rf wants on, and sign is off
    signState = true;
    led_display(stop_image, 5, 25);
    //led_display(image, 5, 25, purple, blue);
    NeoPixel.show();  // update to the NeoPixel Led Strip
    //delay(250);
  } else if (!rf && signState) {  //rf wants off, and sign is on
    signState = false;
    NeoPixel.clear();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called
    NeoPixel.show();  // update to the NeoPixel Led Strip
  }





// //using color overrides for text and bars
//   led_display(stop_image, 125, red, red);

//   delay(250);

//   led_display(stop_image, 125, orange, redDim);

//   delay(250);

//   led_display(stop_image, 125, red, red);

//   delay(250);

//   led_display(stop_image, 125, orange, redDim);

//   delay(250);

//   led_display(stop_image, 125, red, red);

//   delay(250);

//   led_display(stop_image, 125, orange, redDim);

//   delay(250);

//   led_display(stop_image, 125, red, red);

//   delay(1000);



//   led_display(over_image, 125, purple, red);

//   delay(1000);

//   led_display(height_image, 125, purple, red);

//   delay(2000);


//   //standard predefined text
//   led_display(stop_image, 125);

//   delay(1000);

//   led_display(over_image, 125);

//   delay(1000);

//   led_display(height_image, 125);

//   delay(1000);
}
