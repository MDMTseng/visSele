
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H


class StateMachine {
public:
    static const int SM_NULL_STATE = -99999999;
    static const int SM_NULL_INPUT = -99999999;

    enum RunType {
        STATE_TRANSITION = -999,
        EXIT_STATE = -1,
        UPDATE = 0,
        ENTER_STATE = 1,
    };
    
    static const char* RunTypeToString(RunType type) {
        return type==STATE_TRANSITION?"STATE_TRANSITION":
        type==EXIT_STATE?"EXIT_STATE":
        type==UPDATE?"UPDATE":
        type==ENTER_STATE?"ENTER_STATE":
        "UNKNOWN";
    }

    StateMachine(int initState) 
        : currentState(initState), preState(SM_NULL_STATE), nextState(SM_NULL_STATE) {}

    void update() {
        runInternal(UPDATE, SM_NULL_INPUT);
    }

    void transition(int input) {
        runInternal(STATE_TRANSITION, input);
    }

    int getCurrentState(){
        return currentState;
    }

    int getPreState(){
        return preState;
    }
    
    
protected:
    int currentState;
    int preState;
    int nextState;

    void runInternal(RunType type, int input) {
        if (type == STATE_TRANSITION) {
            int newState = handleStateTransition(input);
            if (newState != SM_NULL_STATE && newState != currentState) {
                nextState = newState;
                handleExitState();
                preState = currentState;
                currentState = nextState;
                nextState = SM_NULL_STATE;
                handleEnterState();
            }
            return;
        }
        handleUpdate();
    }


    virtual void handleExitState() {
        handleStateEvent(StateMachine::EXIT_STATE, SM_NULL_INPUT);
    }

    virtual void handleEnterState() {
        handleStateEvent(StateMachine::ENTER_STATE, SM_NULL_INPUT);
    }

    virtual int handleStateTransition(int input) {
        return handleStateEvent(StateMachine::STATE_TRANSITION, input);
    }
    virtual void handleUpdate() {
        handleStateEvent(StateMachine::UPDATE, SM_NULL_INPUT);
    }

    virtual int handleStateEvent(RunType type, int input)
    {
        // if(type == STATE_TRANSITION){

        //     printf(">>>state current: %d, input: %d\n", currentState, input);
        //     switch (currentState)
        //     {
        //         case 0:
        //             return (input == 1) ? 1 : SM_NULL_STATE;
        //         case 1:
        //             return (input == 3) ? 2 : SM_NULL_STATE;
        //         case 2:
        //             return (input == 0) ? 0 : SM_NULL_STATE;
        //         case SM_NULL_STATE:
        //             return 0;
        //     }
        //     return SM_NULL_STATE;
        // }

        // printf("current state: %d->%d \t run_type: %s\n", currentState, nextState, 
        // type==StateMachine::UPDATE?"UPDATE":
        // type==StateMachine::ENTER_STATE?"ENTER_STATE":
        // type==StateMachine::EXIT_STATE?"EXIT_STATE":
        // "STATE_TRANSITION");

        return SM_NULL_STATE;

    }
};

#endif
