#pragma once

#include <Arduino.h>
#include <string>

#include "util/Util.hpp"
#include "util/RingBuf.hpp"
#include "util/SimpPacketParse.hpp"
#include <ArduinoJson.h>

#include "config/HardwareConfig.hpp"
#include "core/FirmwareTypes.hpp"

using std::string;

void RESET_GateSensing();
void setup_comm();
void loop_comm();
void G_LOG(char* str);
