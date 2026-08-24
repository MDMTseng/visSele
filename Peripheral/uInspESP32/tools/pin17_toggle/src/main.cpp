// Toggle the camera trigger pin, and nothing else.
//
// WHY THIS EXISTS (2026-08-20): the machine stopped producing frames. The board
// reported firing (`cam_trig` acked), the core reported "grabbing STARTED", the
// camera passed a free-run test 5/5 at 816x528 -- and the camera's own
// LineStatus registers showed Line0/1/2 NOT MOVING while GPIO17 was driven high
// and low. Every layer said "fine" and no frame existed. The remaining question
// is purely electrical, and the machine firmware is too big a thing to ask it
// with: it has states, a plate, a gate, polarity config in NVS, and a host link
// that all have to be right before a pulse comes out.
//
// This is the smallest possible asker: one pin, one level, no state machine.
//
// PIN 17 MUST DRAW CURRENT. The trigger input is not a voltage the camera
// samples politely -- it is (opto) current. An ESP32 pin sourcing into a load
// that expects to SINK will read "high" on a meter and still deliver nothing,
// which is exactly the failure this was written to chase. So:
//   * drive strength is set to the ESP32 maximum (~40mA sink/source);
//   * both polarities are testable (`h`/`l`), because which one carries the
//     current depends on how the opto is wired, and the machine firmware's own
//     IO_INV_MASK can be flipped from NVS config;
//   * `o` selects OPEN_DRAIN, the correct mode if the line is pulled up on the
//     camera side and the board is only meant to pull it DOWN. In that wiring a
//     push-pull HIGH fights the pull-up and can source nothing useful.
//
// Serial commands (115200, newline optional):
//   h  drive HIGH            l  drive LOW           t  toggle 1Hz on/off
//   p  one 100us pulse (what trig_cam_pulse emits)
//   b  burst: 20 pulses at 10Hz
//   o  OPEN_DRAIN mode       d  push-pull mode (default)
//   ?  print state
//
// Flashing this REPLACES the machine firmware. Reflash the real one afterwards.

#include <Arduino.h>
#include "driver/gpio.h"

static const int CAM_PIN = 17;   // PIN_O_CAM1 in config/HardwareConfig.hpp
static bool blinking = false;
static bool openDrain = false;
static uint32_t lastToggle = 0;
static bool level = false;

static void applyMode() {
  if (openDrain) {
    pinMode(CAM_PIN, OUTPUT_OPEN_DRAIN);
  } else {
    pinMode(CAM_PIN, OUTPUT);
  }
  // Maximum drive. The default is ~10mA-ish; an opto input can want more, and a
  // pin that is "high" but current-starved looks identical to a working one
  // until something downstream needs the current.
  gpio_set_drive_capability((gpio_num_t)CAM_PIN, GPIO_DRIVE_CAP_3);
}

static void setLevel(bool hi) {
  level = hi;
  digitalWrite(CAM_PIN, hi ? HIGH : LOW);
  Serial.printf("pin %d = %s  (mode %s, readback %d)\n", CAM_PIN,
                hi ? "HIGH" : "LOW", openDrain ? "OPEN_DRAIN" : "push-pull",
                digitalRead(CAM_PIN));
}

static void pulse100us() {
  // The machine firmware's pulse, to the microsecond: assert, 100us, release.
  // If a wide manual level works and THIS does not, the fault is pulse width or
  // an input debouncer -- not the wiring.
  digitalWrite(CAM_PIN, HIGH);
  delayMicroseconds(100);
  digitalWrite(CAM_PIN, LOW);
  Serial.println("100us pulse fired (HIGH-going)");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  applyMode();
  setLevel(false);
  Serial.println();
  Serial.println("pin17_toggle -- camera trigger pin test");
  Serial.println("h=HIGH  l=LOW  t=1Hz toggle  p=100us pulse  b=burst20");
  Serial.println("o=OPEN_DRAIN  d=push-pull  ?=state");
  Serial.println("watch the camera side: MVS LineStatus, a meter, or");
  Serial.println("Peripheral/uInspESP32/tools/cam_hwtrig_win.py");
}

void loop() {
  if (Serial.available()) {
    int c = Serial.read();
    switch (c) {
      case 'h': blinking = false; setLevel(true); break;
      case 'l': blinking = false; setLevel(false); break;
      case 't': blinking = !blinking;
                Serial.printf("1Hz toggle %s\n", blinking ? "ON" : "OFF"); break;
      case 'p': blinking = false; pulse100us(); break;
      case 'b': blinking = false;
                for (int i = 0; i < 20; i++) { pulse100us(); delay(100); }
                Serial.println("burst done (20 pulses at 10Hz)"); break;
      case 'o': openDrain = true;  applyMode(); setLevel(level); break;
      case 'd': openDrain = false; applyMode(); setLevel(level); break;
      case '?': Serial.printf("pin %d level=%d mode=%s blinking=%d\n", CAM_PIN,
                              digitalRead(CAM_PIN),
                              openDrain ? "OPEN_DRAIN" : "push-pull", blinking); break;
      default: break;
    }
  }
  if (blinking && millis() - lastToggle >= 500) {
    lastToggle = millis();
    setLevel(!level);
  }
}
