#ifndef STATE_MACHINE_HPP
#define STATE_MACHINE_HPP

#include <cstdint>
#include <functional>

// Forward declarations
enum class SYS_STATE;
enum class SYS_STATE_ACT;
struct SYS_INFO;

/**
 * @brief State machine abstraction for system state management
 * 
 * This module encapsulates the system state machine logic including state transitions,
 * lifecycle management, and callback registration for state enter/exit events.
 * It provides a clean interface for state management that can be unit tested.
 */
class StateMachine {
public:
    /**
     * @brief Callback function type for state lifecycle events
     * @param state The state that triggered the event
     * @param phase The lifecycle phase (0=enter, 1=loop, 2=exit)
     */
    using StateLifecycleCallback = std::function<void(SYS_STATE state, int phase)>;

    /**
     * @brief Initialize the state machine
     * @param initialState Initial state to start with
     */
    void init(SYS_STATE initialState);

    /**
     * @brief Apply a state action (trigger state transition)
     * @param action The action to apply
     * @param extraCode Additional code for the action
     */
    void applyAction(SYS_STATE_ACT action, int extraCode = 0);

    /**
     * @brief Get the current state
     * @return Current system state
     */
    SYS_STATE currentState() const { return current_state_; }

    /**
     * @brief Get the previous state
     * @return Previous system state
     */
    SYS_STATE previousState() const { return previous_state_; }

    /**
     * @brief Get the extra code from the last action
     * @return Extra code value
     */
    int extraCode() const { return extra_code_; }

    /**
     * @brief Register callback for state lifecycle events
     * @param callback Function to call on state lifecycle events
     */
    void setStateLifecycleCallback(StateLifecycleCallback callback);

    /**
     * @brief Check if the state machine is in a specific state
     * @param state State to check
     * @return true if currently in the specified state
     */
    bool isInState(SYS_STATE state) const { return current_state_ == state; }

    /**
     * @brief Check if the state machine is in inspection mode
     * @return true if in any inspection mode state
     */
    bool isInInspectionMode() const;

    /**
     * @brief Check if the state machine is in error state
     * @return true if in any error state
     */
    bool isInErrorState() const;

    /**
     * @brief Get system info structure (for compatibility)
     * @return Pointer to system info
     */
    SYS_INFO* getSystemInfo();

    /**
     * @brief Pump the state machine (execute current state loop)
     * 
     * This method should be called regularly from the main loop to
     * execute the current state's loop logic.
     */
    void pump();

private:
    // State
    SYS_STATE current_state_;
    SYS_STATE previous_state_;
    int extra_code_;

    // Callback
    StateLifecycleCallback lifecycle_callback_;

    /**
     * @brief Execute state lifecycle (enter/loop/exit)
     * @param from_state Previous state
     * @param to_state New state
     */
    void executeLifecycle(SYS_STATE from_state, SYS_STATE to_state);

    /**
     * @brief Handle state transition logic
     * @param action Action to process
     * @return New state after transition
     */
    SYS_STATE processTransition(SYS_STATE_ACT action);

    /**
     * @brief Send state change notification
     * @param from_state Previous state
     * @param to_state New state
     */
    void notifyStateChange(SYS_STATE from_state, SYS_STATE to_state);
};

#endif // STATE_MACHINE_HPP
