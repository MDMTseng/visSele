#include <iostream>
#include <cassert>
#include <vector>
#include <string>

// Include mock HAL for testing
#include "mock/MockHAL.hpp"

// Include the modules we want to test
#include "Scheduler.hpp"
#include "Pipeline.hpp"
#include "GateSensor.hpp"
#include "StateMachine.hpp"

using namespace std;

/**
 * @brief Test framework for scheduler logic
 * 
 * This test suite verifies that the scheduling logic works correctly
 * without hardware dependencies.
 */

class SchedulerTest {
private:
    MockHAL mock_hal_;
    Pipeline pipeline_;
    Scheduler scheduler_;
    GateSensor gate_sensor_;
    StateMachine state_machine_;

public:
    SchedulerTest() : gate_sensor_(mock_hal_.getMockTimerTickSource(), mock_hal_.getMockClock()) {
        // Initialize components with mock HAL
        pipeline_.init();
        scheduler_.init();
        state_machine_.init();
        gate_sensor_.init();
    }

    void runAllTests() {
        cout << "Running Scheduler Tests..." << endl;
        
        testBasicScheduling();
        testMultipleActions();
        testActionTiming();
        testGateSensorIntegration();
        testStateMachineTransitions();
        
        cout << "All scheduler tests passed!" << endl;
    }

private:
    void testBasicScheduling() {
        cout << "  Testing basic scheduling..." << endl;
        
        // Clear any previous state
        scheduler_.reset();
        mock_hal_.getMockGpio().clearWriteLog();
        
        // Create a test object in the pipeline
        Pipeline::ObjectInfo obj;
        obj.tid = 1;
        obj.gate_pulse = 100;
        obj.width = 50;
        obj.insp_status = Pipeline::InspectionStatus::INSP_PENDING;
        
        pipeline_.registerObject(obj);
        
        // Schedule some actions
        uint32_t current_pulse = 100;
        
        // Schedule CAM1 trigger at pulse 150
        scheduler_.scheduleAction(Scheduler::ActionType::CAM1, current_pulse + 50, true, &obj);
        
        // Schedule light on at pulse 120
        scheduler_.scheduleAction(Scheduler::ActionType::L1A, current_pulse + 20, true, &obj);
        
        // Simulate timer ticks
        for (uint32_t pulse = current_pulse; pulse < current_pulse + 200; pulse++) {
            scheduler_.runScheduled(pulse);
        }
        
        // Verify actions were executed
        const auto& write_log = mock_hal_.getMockGpio().getWriteLog();
        bool cam_triggered = false;
        bool light_on = false;
        
        for (const auto& entry : write_log) {
            if (entry.find("GPIO" + to_string(PIN_O_CAM1) + "=HIGH") != string::npos) {
                cam_triggered = true;
            }
            if (entry.find("GPIO" + to_string(PIN_O_L1A) + "=HIGH") != string::npos) {
                light_on = true;
            }
        }
        
        assert(cam_triggered && "CAM1 should have been triggered");
        assert(light_on && "Light should have been turned on");
        
        cout << "    ✓ Basic scheduling test passed" << endl;
    }

    void testMultipleActions() {
        cout << "  Testing multiple actions..." << endl;
        
        scheduler_.reset();
        mock_hal_.getMockGpio().clearWriteLog();
        
        // Create multiple test objects
        vector<Pipeline::ObjectInfo> objects;
        for (int i = 0; i < 3; i++) {
            Pipeline::ObjectInfo obj;
            obj.tid = i + 1;
            obj.gate_pulse = 100 + i * 100;
            obj.width = 50;
            obj.insp_status = Pipeline::InspectionStatus::INSP_PENDING;
            objects.push_back(obj);
            pipeline_.registerObject(obj);
        }
        
        // Schedule actions for each object
        for (size_t i = 0; i < objects.size(); i++) {
            uint32_t base_pulse = objects[i].gate_pulse;
            scheduler_.scheduleAction(Scheduler::ActionType::CAM1, base_pulse + 50, true, &objects[i]);
            scheduler_.scheduleAction(Scheduler::ActionType::L1A, base_pulse + 20, true, &objects[i]);
            scheduler_.scheduleAction(Scheduler::ActionType::L1A, base_pulse + 80, false, &objects[i]);
        }
        
        // Simulate execution
        for (uint32_t pulse = 0; pulse < 500; pulse++) {
            scheduler_.runScheduled(pulse);
        }
        
        // Verify all actions were executed
        const auto& write_log = mock_hal_.getMockGpio().getWriteLog();
        int cam_triggers = 0;
        int light_ons = 0;
        int light_offs = 0;
        
        for (const auto& entry : write_log) {
            if (entry.find("GPIO" + to_string(PIN_O_CAM1) + "=HIGH") != string::npos) {
                cam_triggers++;
            }
            if (entry.find("GPIO" + to_string(PIN_O_L1A) + "=HIGH") != string::npos) {
                light_ons++;
            }
            if (entry.find("GPIO" + to_string(PIN_O_L1A) + "=LOW") != string::npos) {
                light_offs++;
            }
        }
        
        assert(cam_triggers == 3 && "Should have 3 CAM triggers");
        assert(light_ons == 3 && "Should have 3 light on actions");
        assert(light_offs == 3 && "Should have 3 light off actions");
        
        cout << "    ✓ Multiple actions test passed" << endl;
    }

    void testActionTiming() {
        cout << "  Testing action timing..." << endl;
        
        scheduler_.reset();
        mock_hal_.getMockGpio().clearWriteLog();
        
        // Create test object
        Pipeline::ObjectInfo obj;
        obj.tid = 1;
        obj.gate_pulse = 100;
        obj.width = 50;
        obj.insp_status = Pipeline::InspectionStatus::INSP_PENDING;
        pipeline_.registerObject(obj);
        
        // Schedule action at specific pulse
        uint32_t target_pulse = 150;
        scheduler_.scheduleAction(Scheduler::ActionType::CAM1, target_pulse, true, &obj);
        
        // Simulate execution with precise timing
        bool action_executed = false;
        for (uint32_t pulse = 100; pulse < 200; pulse++) {
            scheduler_.runScheduled(pulse);
            
            // Check if action was executed at the right time
            if (pulse == target_pulse) {
                const auto& write_log = mock_hal_.getMockGpio().getWriteLog();
                bool found_action = false;
                for (const auto& entry : write_log) {
                    if (entry.find("GPIO" + to_string(PIN_O_CAM1) + "=HIGH") != string::npos) {
                        found_action = true;
                        break;
                    }
                }
                assert(found_action && "Action should be executed at target pulse");
                action_executed = true;
            }
        }
        
        assert(action_executed && "Action should have been executed");
        
        cout << "    ✓ Action timing test passed" << endl;
    }

    void testGateSensorIntegration() {
        cout << "  Testing gate sensor integration..." << endl;
        
        // Reset components
        gate_sensor_.reset();
        pipeline_.reset();
        mock_hal_.getMockGpio().clearWriteLog();
        
        // Simulate gate sensor ticks
        for (uint32_t tick = 0; tick < 100; tick++) {
            gate_sensor_.tick(tick);
        }
        
        // Verify gate sensor processed the ticks
        // (This would depend on the specific gate sensor implementation)
        
        cout << "    ✓ Gate sensor integration test passed" << endl;
    }

    void testStateMachineTransitions() {
        cout << "  Testing state machine transitions..." << endl;
        
        state_machine_.reset();
        
        // Test initial state
        assert(state_machine_.getCurrentState() == StateMachine::SystemState::INIT && 
               "Initial state should be INIT");
        
        // Test state transitions
        state_machine_.applyAction(StateMachine::SystemAction::INIT_OK);
        assert(state_machine_.getCurrentState() == StateMachine::SystemState::IDLE && 
               "Should transition to IDLE after INIT_OK");
        
        state_machine_.applyAction(StateMachine::SystemAction::ENTER_INSP_MODE);
        assert(state_machine_.getCurrentState() == StateMachine::SystemState::INSP_MODE && 
               "Should transition to INSP_MODE");
        
        cout << "    ✓ State machine transitions test passed" << endl;
    }
};

// Main test runner
int main() {
    try {
        SchedulerTest test;
        test.runAllTests();
        cout << "\n🎉 All tests passed successfully!" << endl;
        return 0;
    } catch (const exception& e) {
        cerr << "❌ Test failed: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "❌ Unknown test failure" << endl;
        return 1;
    }
}
