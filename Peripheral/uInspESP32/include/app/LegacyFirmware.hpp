#pragma once

/**
 * Firmware entry points for the legacy inspection controller.
 * These wrap the former Arduino `setup` and `loop` functions so the
 * application can be driven from `src/main.cpp`.
 */
void firmwareSetup();
void firmwareLoop();

