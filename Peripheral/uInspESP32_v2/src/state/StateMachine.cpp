#include "StateMachine.hpp"
#include "main.hpp"
#include <cstdio>
#include <cstring>

// External dependencies (will be injected via callbacks)
extern void RESET_ALL_PIPELINE_QUEUE();
// Error logging now handled by Diagnostics module
extern bool blockNewDetectedObject;
extern uint32_t SYS_TAR_FREQ;
extern uint32_t SETUP_TAR_FREQ;
// FEEDER_PIN is defined as a macro in main.hpp

// Message handling now managed by MessageBus module

// Global SYS_INFO instance for compatibility
SYS_INFO global_sysinfo = {
    .pre_state = SYS_STATE::INIT,
    .state = SYS_STATE::INIT,
    .extra_code = 0,
    .status = 0,
    .PTSyncInfo = {.state = PulseTimeSyncInfo_State::INIT},
};

void StateMachine::init(SYS_STATE initialState) {
    current_state_ = initialState;
    previous_state_ = initialState;
    extra_code_ = 0;
    
    // Initialize global system info
    global_sysinfo.pre_state = initialState;
    global_sysinfo.state = initialState;
    global_sysinfo.extra_code = 0;
    global_sysinfo.status = 0;
    global_sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::INIT;
}

void StateMachine::applyAction(SYS_STATE_ACT action, int extraCode) {
    SYS_STATE new_state = processTransition(action);
    
    if (current_state_ != new_state) {
        // State changed
        notifyStateChange(current_state_, new_state);
        
        previous_state_ = current_state_;
        current_state_ = new_state;
        extra_code_ = extraCode;
        
        // Update global system info
        global_sysinfo.pre_state = previous_state_;
        global_sysinfo.state = current_state_;
        global_sysinfo.extra_code = extraCode;
        
        executeLifecycle(previous_state_, current_state_);
    } else {
        // No state change, but still execute lifecycle for loop processing
        executeLifecycle(current_state_, current_state_);
    }
}

bool StateMachine::isInInspectionMode() const {
    return current_state_ == SYS_STATE::INSPECTION_MODE_TEST ||
           current_state_ == SYS_STATE::INSPECTION_MODE_READY;
}

bool StateMachine::isInErrorState() const {
    return current_state_ == SYS_STATE::INSPECTION_MODE_ERROR ||
           current_state_ == SYS_STATE::INSPECTION_MODE_FATAL;
}

SYS_INFO* StateMachine::getSystemInfo() {
    return &global_sysinfo;
}

void StateMachine::pump() {
    // Execute the current state's loop logic
    executeLifecycle(current_state_, current_state_);
}

void StateMachine::setStateLifecycleCallback(StateLifecycleCallback callback) {
    lifecycle_callback_ = callback;
}

void StateMachine::executeLifecycle(SYS_STATE from_state, SYS_STATE to_state) {
    SYS_STATE states[3] = {SYS_STATE::NOP}; // 0: enter, 1:loop, 2:exit
    int i_from, i_to;
    
    if (from_state == to_state) {
        i_from = 1;
        i_to = 1;
        states[1] = to_state;
    } else {
        i_from = 2;
        i_to = 0;
        states[0] = to_state;  // enter
        states[2] = from_state; // exit
    }

    // Execute in reverse order: exit -> loop -> enter
    for (int i = i_from; i >= i_to; i--) {
        SYS_STATE state = states[i];
        
        // Call registered callback if available
        if (lifecycle_callback_) {
            lifecycle_callback_(state, i);
        }
        
        // Execute state-specific logic
        switch (state) {
        case SYS_STATE::INIT:
            if (i == 2) { // EXIT
                blockNewDetectedObject = true;
                SYS_TAR_FREQ = 0;
                pinMode(FEEDER_PIN, OUTPUT);
            }
            break;
            
        case SYS_STATE::IDLE:
            if (i == 0) { // ENTER
                blockNewDetectedObject = true;
                RESET_ALL_PIPELINE_QUEUE();
            } else if (i == 1) { // LOOP
                SYS_TAR_FREQ = SETUP_TAR_FREQ;
            }
            break;

        case SYS_STATE::INSPECTION_MODE_TEST:
            if (i == 0) { // ENTER
                blockNewDetectedObject = false;
            } else if (i == 1) { // LOOP
                SYS_TAR_FREQ = SETUP_TAR_FREQ;
            }
            break;

        case SYS_STATE::INSPECTION_MODE_READY:
            if (i == 0) { // ENTER
                blockNewDetectedObject = false;
            } else if (i == 1) { // LOOP
                SYS_TAR_FREQ = SETUP_TAR_FREQ;
            }
            break;

        case SYS_STATE::INSPECTION_MODE_ERROR:
            if (i == 0) { // ENTER
                blockNewDetectedObject = true;
                SYS_TAR_FREQ = 0;
                Diagnostics::pushError((GEN_ERROR_CODE)extra_code_);
            } else if (i == 1) { // LOOP
                // Error state loop logic could be added here
            }
            break;

        case SYS_STATE::INSPECTION_MODE_FATAL:
            if (i == 0) { // ENTER
                blockNewDetectedObject = true;
                SYS_TAR_FREQ = 0;
                Diagnostics::pushError((GEN_ERROR_CODE)extra_code_);
            } else if (i == 1) { // LOOP
                // Fatal error state loop logic could be added here
            }
            break;

        default:
            break;
        }
    }
}

SYS_STATE StateMachine::processTransition(SYS_STATE_ACT action) {
    SYS_STATE new_state = current_state_;
    
    // State transition logic using the macro-generated switch
    switch (current_state_) {
    case SYS_STATE::INIT:
        switch (action) {
        case SYS_STATE_ACT::INIT_OK:
            new_state = SYS_STATE::IDLE;
            break;
        default:
            break;
        }
        break;
        
    case SYS_STATE::IDLE:
        switch (action) {
        case SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE:
            new_state = SYS_STATE::INSPECTION_MODE_READY;
            break;
        case SYS_STATE_ACT::ENTER_INSPECTION_TEST_MODE:
            new_state = SYS_STATE::INSPECTION_MODE_TEST;
            break;
        default:
            break;
        }
        break;
        
    case SYS_STATE::INSPECTION_MODE_TEST:
        switch (action) {
        case SYS_STATE_ACT::EXIT_INSPECTION_MODE:
            new_state = SYS_STATE::IDLE;
            break;
        default:
            break;
        }
        break;
        
    case SYS_STATE::INSPECTION_MODE_READY:
        switch (action) {
        case SYS_STATE_ACT::EXIT_INSPECTION_MODE:
            new_state = SYS_STATE::IDLE;
            break;
        case SYS_STATE_ACT::INSPECTION_ERROR:
            new_state = SYS_STATE::INSPECTION_MODE_ERROR;
            break;
        default:
            break;
        }
        break;
        
    case SYS_STATE::INSPECTION_MODE_ERROR:
        switch (action) {
        case SYS_STATE_ACT::EXIT_INSPECTION_MODE:
            new_state = SYS_STATE::IDLE;
            break;
        case SYS_STATE_ACT::INSPECTION_ERROR_REDEEM:
            new_state = SYS_STATE::INSPECTION_MODE_READY;
            break;
        default:
            break;
        }
        break;
        
    default:
        break;
    }
    
    return new_state;
}

void StateMachine::notifyStateChange(SYS_STATE from_state, SYS_STATE to_state) {
    // Send state change notification to communication queue
    char numberStr[100];
    sprintf(numberStr, "State changed from %d to %d", (int)from_state, (int)to_state);
    Message msg = Message::createLog(numberStr);
    MessageBus::sendMessage(msg);
}
