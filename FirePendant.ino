//--------------------------------------------------------------------------
// Animated flame for Adafruit Feather M0.  Uses the following parts:
//   - Adafruit Feather M0
//   - Charlieplex LED Matrix Driver (2946)
//   - Charlieplex LED Matrix (2947, 2948, 2972, 2973 or 2974)
//   - 350 mAh LiPoly battery (2750)
//   - SPDT Slide Switch (805)
//
// This is NOT good "learn from" code for the IS31FL3731; it is "squeeze
// every last byte from the Pro Trinket" code.  If you're starting out,
// download the Adafruit_IS31FL3731 and Adafruit_GFX libraries, which
// provide functions for drawing pixels, lines, etc.  This sketch also
// uses some ATmega-specific tricks and will not run as-is on other chips.
//--------------------------------------------------------------------------

#include <Wire.h>           // For I2C communication
#include "data.h"           // Flame animation data

#define I2C_ADDR 0x74       // I2C address of Charlieplex matrix
#define GAMMA 2.5

uint8_t        page = 0;    // Front/back buffer control
const uint8_t *ptr  = anim; // Current pointer into animation data
uint8_t        img[9 * 16]; // Buffer for rendering image
uint8_t        gamma8[256]; // Gamma correction table


// UTILITY FUNCTIONS -------------------------------------------------------

// The full IS31FL3731 library is NOT used by this code.  Instead, 'raw'
// writes are made to the matrix driver.  This is to maximize the space
// available for animation data.  Use the Adafruit_IS31FL3731 and
// Adafruit_GFX libraries if you need to do actual graphics stuff.

// Begin I2C transmission and write register address (data then follows)
uint8_t writeRegister(uint8_t n) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(n);
  // Transmission is left open for additional writes
  return 2;
}

// Select one of eight IS31FL3731 pages, or Function Registers
void pageSelect(uint8_t n) {
  writeRegister(0xFD); // Command Register
  Wire.write(n);       // Page number (or 0xB = Function Registers)
  Wire.endTransmission();
}

// SETUP FUNCTION - RUNS ONCE AT STARTUP -----------------------------------

void setup() {
  uint16_t i;
  uint8_t  p, bytes;

  Wire.begin();
  Wire.setClock(400000L);

  // Initialize IS31FL3731 directly (no library)
  pageSelect(0x0B);                        // Access the Function Registers
  writeRegister(0);                        // Starting from first...
  for(i=0; i<13; i++) Wire.write(10 == i); // Clear all except Shutdown
  Wire.endTransmission();
  for(p=0; p<2; p++) {                     // For each page used (0 & 1)...
    pageSelect(p);                         // Access the Frame Registers
    for(bytes=i=0; i<180; i++) {           // For each register...
      if(!bytes) bytes = writeRegister(i); // Buf empty? Start xfer @ reg i
      Wire.write(0xFF * (i < 18));         // 0-17 = enable, 18+ = blink+PWM
      if(++bytes >= SERIAL_BUFFER_SIZE) bytes = Wire.endTransmission();
    }
    if(bytes) Wire.endTransmission();      // Write any data left in buffer
  }
 
  for(i=0; i<256; i++) // Initialize gamma-correction table:
    gamma8[i] = (uint8_t)(pow(((float)i / 255.0), GAMMA) * 255.0 + 0.5);
}

// LOOP FUNCTION - RUNS EVERY FRAME ----------------------------------------

void loop() {
  uint8_t  a, x1, y1, x2, y2, x, y;

  // Display frame rendered on prior pass.  This is done at function start
  // (rather than after rendering) to ensire more uniform animation timing.
  pageSelect(0x0B);    // Function registers
  writeRegister(0x01); // Picture Display reg
  Wire.write(page);    // Page #
  Wire.endTransmission();

  page ^= 1; // Flip front/back buffer index

  // Then render NEXT frame.  Start by getting bounding rect for new frame:
  a = pgm_read_byte(ptr++);     // New frame X1/Y1
  if(a >= 0x90) {               // EOD marker? (valid X1 never exceeds 8)
    ptr = anim;                 // Reset animation data pointer to start
    a   = pgm_read_byte(ptr++); // and take first value
  }
  x1 = a >> 4;                  // X1 = high 4 bits
  y1 = a & 0x0F;                // Y1 = low 4 bits
  a  = pgm_read_byte(ptr++);    // New frame X2/Y2
  x2 = a >> 4;                  // X2 = high 4 bits
  y2 = a & 0x0F;                // Y2 = low 4 bits

  // Read rectangle of data from anim[] into portion of img[] buffer
  for(x=x1; x<=x2; x++) { // Column-major
    for(y=y1; y<=y2; y++) img[(x << 4) + y] = pgm_read_byte(ptr++);
  }

  // Write img[] to matrix (not actually displayed until next pass)
  pageSelect(page);    // Select background buffer
  writeRegister(0x24); // First byte of PWM data
  uint8_t i = 0, byteCounter = 1;
  for(uint8_t x=0; x<9; x++) {
    for(uint8_t y=0; y<16; y++) {
      Wire.write(img[i++]);      // Write each byte to matrix
      if(++byteCounter >= 32) {  // Every 32 bytes...
        Wire.endTransmission();  // end transmission and
        writeRegister(0x24 + i); // start a new one (Wire lib limits)
      }
    }
  }
  Wire.endTransmission();
}
