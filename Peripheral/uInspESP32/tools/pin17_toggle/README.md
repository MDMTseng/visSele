# pin17_toggle — camera trigger pin, on its own

One pin, one level, no state machine. For when the board says it fired, the
camera says it is healthy, and no frame exists.

    cd Peripheral/uInspESP32/tools/pin17_toggle
    pio run -t upload           # REPLACES the machine firmware
    pio device monitor -b 115200

Then, with the core stopped:

* `t` — 1 Hz toggle. Watch `LineStatus` follow (or not) from the camera side:

      python ../../../InspectionCore/tools/cam_check.py --status

* `h` / `l` — hold a level and measure with a meter at BOTH ends of the trigger
  wire. A level present at the board and absent at the camera is the wire.
* `o` — OPEN_DRAIN. Use this if the camera side pulls the line up and the board
  is only meant to pull it down; a push-pull HIGH fights that pull-up.
* `p` / `b` — the real 100 µs pulse. If a wide level works and this does not,
  suspect pulse width or an input debouncer, not the wiring.

**Pin 17 must draw current.** The trigger input is opto current, not a voltage
the camera reads politely — so drive strength is set to the ESP32 maximum
(`GPIO_DRIVE_CAP_3`) and both polarities are testable. A pin that measures
"high" but is current-starved looks exactly like a working one.

When you are done, reflash the machine firmware:

    cd ../..   &&   pio run -t upload
