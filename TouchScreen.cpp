// Touch screen library with X Y and Z (pressure) readings as well
// as oversampling to avoid 'bouncing'
// (c) ladyada / adafruit
// Code under MIT License

#include "pins_arduino.h"
#include "wiring_private.h"
#include <avr/pgmspace.h>
#include "TouchScreen.h"

// increase or decrease the touchscreen oversampling. This is a little different than you make think:
// 1 is no oversampling, whatever data we get is immediately returned
// 2 is double-sampling and we only return valid data if both points are the same
//	NOTE: this doesn't work on Due; use 3
// 3+ uses insert sort to get the median value.
// We found 2 is precise yet not too slow so we suggest sticking with it!

#define NUMSAMPLES 3

// Run output in regular (digitalWrite) or optimized (portOutputRegister) mode
//    0 = regular
//    >0 = optimized (NB: does not work, and doesn't seem needed on the Due)

#define TSFASTMODE 0

TSPoint::TSPoint(void) {
  x = y = 0;
}

TSPoint::TSPoint(int16_t x0, int16_t y0, int16_t z0) {
  x = x0;
  y = y0;
  z = z0;
}

bool TSPoint::operator==(TSPoint p1) {
  return  ((p1.x == x) && (p1.y == y) && (p1.z == z));
}

bool TSPoint::operator!=(TSPoint p1) {
  return  ((p1.x != x) || (p1.y != y) || (p1.z != z));
}

#if (NUMSAMPLES > 2)
static void insert_sort(int array[], uint8_t size) {
  uint8_t j;
  int save;
  
  for (int i = 1; i < size; i++) {
    save = array[i];
    for (j = i; j >= 1 && save < array[j - 1]; j--)
      array[j] = array[j - 1];
    array[j] = save; 
  }
}
#endif

TSPoint TouchScreen::getPoint(void) {
  int x, y, z;
  int samples[NUMSAMPLES];
  uint8_t i, valid;
  

#if TSFASTMODE > 0
  uint8_t xp_port = digitalPinToPort(_xp);
  uint8_t yp_port = digitalPinToPort(_yp);
  uint8_t xm_port = digitalPinToPort(_xm);
  uint8_t ym_port = digitalPinToPort(_ym);

  uint8_t xp_pin = digitalPinToBitMask(_xp);
  uint8_t yp_pin = digitalPinToBitMask(_yp);
  uint8_t xm_pin = digitalPinToBitMask(_xm);
  uint8_t ym_pin = digitalPinToBitMask(_ym);
#endif


  valid = 1;

// Setup to read X:
//   yp = input & analog sampled 
//   xm = LOW
//   ym = input
//   xp = HIGH

  pinMode(_yp, INPUT);
  pinMode(_ym, INPUT);
  
  // turn of input pull-ups
#if TSFASTMODE > 0
  *portOutputRegister(yp_port) &= ~yp_pin;
  *portOutputRegister(ym_port) &= ~ym_pin;
#else
  digitalWrite(_yp, LOW);
  digitalWrite(_ym, LOW);
#endif
   
  pinMode(_xp, OUTPUT);
  pinMode(_xm, OUTPUT);
#if TSFASTMODE > 0
  *portOutputRegister(xp_port) |= xp_pin;
  *portOutputRegister(xm_port) &= ~xm_pin;
#else
  digitalWrite(_xp, HIGH);
  digitalWrite(_xm, LOW);
#endif
   
   for (i=0; i<NUMSAMPLES; i++) {
     samples[i] = analogRead(_yp);
   }
#if NUMSAMPLES > 2
   insert_sort(samples, NUMSAMPLES);
#endif
#if NUMSAMPLES == 2
   if (samples[0] != samples[1]) { valid = 0; }
#endif
   x = (1023-samples[NUMSAMPLES/2]);

// Setup to read Y:
//   yp = HIGH
//   xm = input & analog sampled 
//   ym = LOW
//   xp = input

   pinMode(_xp, INPUT);
   pinMode(_xm, INPUT);

  // turn of input pull-ups
#if TSFASTMODE > 0
   *portOutputRegister(xp_port) &= ~xp_pin;
   *portOutputRegister(xm_port) &= ~xm_pin;
#else
   digitalWrite(_xp, LOW);
   digitalWrite(_xm, LOW);
#endif
   
   pinMode(_yp, OUTPUT);
   pinMode(_ym, OUTPUT);
#if TSFASTMODE > 0
   *portOutputRegister(yp_port) |= yp_pin;
   *portOutputRegister(ym_port) &= ~ym_pin;
#else
   digitalWrite(_yp, HIGH);
   digitalWrite(_ym, LOW);
#endif
  
   for (i=0; i<NUMSAMPLES; i++) {
     samples[i] = analogRead(_xm);
   }

#if NUMSAMPLES > 2
   insert_sort(samples, NUMSAMPLES);
#endif
#if NUMSAMPLES == 2
   if (samples[0] != samples[1]) { valid = 0; }
#endif

   y = (1023-samples[NUMSAMPLES/2]);

// Setup to read Z:
//   yp = input & analog sampled
//   xm = input & analog sampled
//   ym = HIGH
//   xp = LOW

   // Set X+ to ground
   // Set Y- to VCC
   pinMode(_xp, OUTPUT);
#if TSFASTMODE > 0
   *portOutputRegister(xp_port) &= ~xp_pin;
   *portOutputRegister(ym_port) |= ym_pin;
#else
   digitalWrite(_xp, LOW);
   digitalWrite(_ym, HIGH); 
#endif
  
   // turn of input pull-ups
   // Hi-Z X- and Y+
   pinMode(_xm, INPUT);
   pinMode(_yp, INPUT);
#if TSFASTMODE > 0
   *portOutputRegister(xm_port) &= ~xm_pin;
   *portOutputRegister(yp_port) &= ~yp_pin;
#else
   digitalWrite(_xm, LOW);
   digitalWrite(_yp, LOW);
#endif
  
   int z1 = analogRead(_xm); 
   int z2 = analogRead(_yp);

   if (_rxplate != 0) {
     // now read the x 
     float rtouch;
     rtouch = z2;
     rtouch /= z1;
     rtouch -= 1;
     rtouch *= x;
     rtouch *= _rxplate;
     rtouch /= 1024;
     
     z = rtouch;
   } else {
     z = (1023-(z2-z1));
   }

   if (! valid) {
     z = 0;
   }

   return TSPoint(x, y, z);
}

TouchScreen::TouchScreen(uint8_t xp, uint8_t yp, uint8_t xm, uint8_t ym) {
  _yp = yp;
  _xm = xm;
  _ym = ym;
  _xp = xp;
  _rxplate = 0;
  pressureThreshhold = 10;
}


TouchScreen::TouchScreen(uint8_t xp, uint8_t yp, uint8_t xm, uint8_t ym,
			 uint16_t rxplate) {
  _yp = yp;
  _xm = xm;
  _ym = ym;
  _xp = xp;
  _rxplate = rxplate;

  pressureThreshhold = 10;
}

int TouchScreen::readTouchX(void) {
   pinMode(_yp, INPUT);
   pinMode(_ym, INPUT);
   digitalWrite(_yp, LOW);
   digitalWrite(_ym, LOW);
   
   pinMode(_xp, OUTPUT);
   digitalWrite(_xp, HIGH);
   pinMode(_xm, OUTPUT);
   digitalWrite(_xm, LOW);
   
   return (1023-analogRead(_yp));
}


int TouchScreen::readTouchY(void) {
   pinMode(_xp, INPUT);
   pinMode(_xm, INPUT);
   digitalWrite(_xp, LOW);
   digitalWrite(_xm, LOW);
   
   pinMode(_yp, OUTPUT);
   digitalWrite(_yp, HIGH);
   pinMode(_ym, OUTPUT);
   digitalWrite(_ym, LOW);
   
   return (1023-analogRead(_xm));
}


uint16_t TouchScreen::pressure(void) {
  // Set X+ to ground
  pinMode(_xp, OUTPUT);
  digitalWrite(_xp, LOW);
  
  // Set Y- to VCC
  pinMode(_ym, OUTPUT);
  digitalWrite(_ym, HIGH); 
  
  // Hi-Z X- and Y+
  digitalWrite(_xm, LOW);
  pinMode(_xm, INPUT);
  digitalWrite(_yp, LOW);
  pinMode(_yp, INPUT);
  
  int z1 = analogRead(_xm); 
  int z2 = analogRead(_yp);

  if (_rxplate != 0) {
    // now read the x 
    float rtouch;
    rtouch = z2;
    rtouch /= z1;
    rtouch -= 1;
    rtouch *= readTouchX();
    rtouch *= _rxplate;
    rtouch /= 1024;
    
    return rtouch;
  } else {
    return (1023-(z2-z1));
  }
}
