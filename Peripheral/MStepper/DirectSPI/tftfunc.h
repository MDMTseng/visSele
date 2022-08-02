/*
 * 
 * HIGH SPEED LOW LEVEL DISPLAY FUNCTIONS USING DIRECT TRANSFER MODE
 *
*/

#ifndef _TFTFUNC_H_
#define _TFTFUNC_H_


// Define ESP32 SPI pins to which the display is attached
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
// Reset and backlit pins are not used
//#define PIN_NUM_RST  18
//#define PIN_NUM_BCKL 5

// Display command/data pin
#define PIN_NUM_DC   26

#endif