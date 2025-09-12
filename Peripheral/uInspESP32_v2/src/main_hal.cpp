// main_hal.cpp - HAL implementation module for ESP32
// This file provides HAL implementations but does not contain Arduino setup/loop functions
#include "main.hpp"
#include "hal/HAL.hpp"
#include "hal/esp32/ESP32HAL.hpp"  // Include ESP32 HAL implementation
#include "StepperController.hpp"
#include "SimpPacketParse.hpp"
#include "Pipeline.hpp"
#include "Scheduler.hpp"
#include "GateSensor.hpp"
#include "StateMachine.hpp"
#include "MessageBus.hpp"
#include "Diagnostics.hpp"
#include "SerialTransport.hpp"

// Global variables
static hw_timer_t* timer = nullptr;
static uint32_t SYS_STEP_COUNT = 0;
static uint32_t SYS_TAR_FREQ = 0;
static uint32_t SYS_CUR_FREQ = 0;
static uint32_t SYS_FREQ_ADV_STEP = 5;
static bool SYS_FREQ_STABLE = false;
static bool SYS_STEPPER_DISABLED = true;
static uint32_t SEL1_ACT_COUNTDOWN = 0;

// External global instances from SyncTask.cpp
extern GateSensor gateSensor;
extern StateMachine stateMachine;

// HAL module instances
SerialTransport serialTransport(Serial); // Initialize with Serial
StepperController* stepperController = nullptr;

// Get HAL instance
IHAL& hal = getHAL();

// Forward declarations
void hal_onTimer();
extern int Run_ACTS(uint32_t cur_pulse);
// AUX_task is defined in SyncTask.cpp
extern "C" void AUX_task(void *pvParameter);

// Helper function declarations
static void readSerialIntoProtocol();
static void flushOutboundMessages(ITransport& transport);

// HAL module function declarations (exported)
extern "C" void custom_hal_init();
extern "C" void custom_hal_update();

// External variables
extern int minWidth;
extern int maxWidth;

// Pin definitions
#define PIN_LED 2  // Based on SyncTask.cpp definition

// AUX Task related definitions
#define AUX_COUNT 5

enum AUX_TASK_INFO_TYPE{
  AUX_DELAY=1,
  AUX_IO_CTRL=2,
  AUX_WAIT_FOR_ENC=3,
  AUX_WAIT_FOR_FINISH=1000,
};

struct AUX_TASK_INFO_WAIT_FOR_FINISH{
  int cmd_id;
};

struct AUX_TASK_INFO_WAIT_FOR_ENC{
  int value;
};

struct AUX_TASK_INFO_DELAY{
  int time;
};

struct AUX_TASK_INFO_IO_CTRL{
  int pin;
  int state;
  char CID[50];
  char TTAG[100];
  int TID;
};

struct AUX_TASK_INFO {
  AUX_TASK_INFO(){}
  ~AUX_TASK_INFO(){}
  AUX_TASK_INFO_TYPE type;
  
  union {
    AUX_TASK_INFO_DELAY delayInfo;
    AUX_TASK_INFO_IO_CTRL ioCtrl;
    AUX_TASK_INFO_WAIT_FOR_ENC wait_enc;
    AUX_TASK_INFO_WAIT_FOR_FINISH wait_fin;
  };
};

// FreeRTOS resources
static SemaphoreHandle_t AUX2Comm_Lock;
static QueueHandle_t AUXTaskQueue[AUX_COUNT];

// External functions from SyncTask.cpp
extern int Run_ACTS(uint32_t cur_pulse);

// External variables
extern int minWidth;
extern int maxWidth;

// Define missing constants
#define PIN_LED 2  // Assuming LED is on GPIO2, adjust if needed

// External FreeRTOS task is already declared at the top of the file

// HAL initialization function to be called from the main setup
void custom_hal_init() {
  // Initialize Serial communication
  Serial.begin(115200);
  Serial.setRxBufferSize(500);
  
  // Initialize HAL components
  hal.logger().setLogLevel(ILogger::LogLevel::INFO);
  hal.logger().log(ILogger::LogLevel::INFO, "System initializing...");
  
  // Initialize modules
  MessageBus::init();
  Diagnostics::init();
  
  // Initialize FreeRTOS resources
  AUX2Comm_Lock = xSemaphoreCreateMutex();
  for(int i = 0; i < AUX_COUNT; i++) {
    AUXTaskQueue[i] = xQueueCreate(20, sizeof(AUX_TASK_INFO));
    xTaskCreatePinnedToCore(&AUX_task, "AUX_task", 2048, (void*)&AUXTaskQueue[i], 1, NULL, 0);
  }
  
  // Initialize GPIO pins using HAL
  hal.gpio().setPinMode(PIN_LED, IGpio::PinMode::PIN_OUTPUT);
  
  // Initialize stepper controller
  stepperController = new StepperController(
    hal.stepperDriver(),
    hal.timerTickSource(),
    hal.clock()
  );
  
  // Initialize stepper with pins from main.hpp
  stepperController->init(
    STEPPER_PLS_PIN,
    STEPPER_DIR_PIN,
    STEPPER_EN_PIN,
    STEPPER_EN_ACTIVATION == 0, // Convert to bool
    0.0f, // Initial frequency
    SYS_FREQ_ADV_STEP * 10.0f // Max acceleration
  );
  
  // Initialize output pins
  hal.gpio().setPinMode(PIN_O_L1A, IGpio::PinMode::PIN_OUTPUT);
  hal.gpio().setPinMode(PIN_O_CAM1, IGpio::PinMode::PIN_OUTPUT);
  hal.gpio().setPinMode(PIN_O_L2A, IGpio::PinMode::PIN_OUTPUT);
  hal.gpio().setPinMode(PIN_O_CAM2, IGpio::PinMode::PIN_OUTPUT);
  hal.gpio().setPinMode(PIN_O_SEL1, IGpio::PinMode::PIN_OUTPUT);
  hal.gpio().setPinMode(PIN_O_SEL2, IGpio::PinMode::PIN_OUTPUT);
  hal.gpio().setPinMode(PIN_O_SEL3, IGpio::PinMode::PIN_OUTPUT);
  
  // Initialize input pins
  hal.gpio().setPinMode(PIN_I_GATE, IGpio::PinMode::PIN_INPUT_PULLUP);
  
  // Initialize GateSensor
  gateSensor.init(PIN_I_GATE, true, minWidth, maxWidth, 1);
  
  // Initialize state machine
  stateMachine.init(SYS_STATE::INIT);
  
  // Register timer tick callback
  hal.timerTickSource().registerTickCallback(hal_onTimer);
  
  // Apply initial action to state machine
  stateMachine.applyAction(SYS_STATE_ACT::INIT_OK);
  
  hal.logger().log(ILogger::LogLevel::INFO, "System initialized");
}

// Timer ISR callback - minimal and platform-neutral
void hal_onTimer() {
  SYS_STEP_COUNT++;
  
  // Step the stepper motor (now handled by StepperController)
  // The StepperController is already registered with the timer tick source
  
  // Process gate sensor (ISR-safe)
  gateSensor.tick(SYS_STEP_COUNT);
  
  // Run scheduled actions (ISR-safe, minimal logic only)
  Run_ACTS(SYS_STEP_COUNT);
}

// HAL update function to be called from the main loop
void custom_hal_update() {
  // High-level orchestration following refactor plan:
  // 1. Read serial into protocol
  readSerialIntoProtocol();
  
  // 2. Flush outbound messages through transport
  flushOutboundMessages(serialTransport);
  
  // 3. Update stepper controller (handles frequency ramping)
  stepperController->update();
  
  // 4. Pump state machine (execute current state loop)
  stateMachine.pump();
  
  // Yield to other tasks
  yield();
}

// Helper function to read serial data into protocol
static void readSerialIntoProtocol() {
  if (serialTransport.available() > 0) {
    // Read and process commands
    uint8_t buffer[256];
    size_t bytesRead = serialTransport.read(buffer, sizeof(buffer));
    if (bytesRead > 0) {
      // Process the received data
      // (Command handling code would go here)
    }
  }
}

// Helper function to flush outbound messages
static void flushOutboundMessages(ITransport& transport) {
  // Process messages from queues and send them through transport
  while (MessageBus::hasMainMessages()) {
    Message msg;
    if (MessageBus::getNextMainMessage(msg)) {
      // Process and send message (implementation depends on message format)
      // For now, just flush the transport
      transport.flush();
    }
  }
  
  // Also process auxiliary messages
  while (MessageBus::hasAuxMessages()) {
    Message msg;
    if (MessageBus::getNextAuxMessage(msg)) {
      // Process and send message
      transport.flush();
    }
  }
}