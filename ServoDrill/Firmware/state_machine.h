#pragma once
// Simple state machine abstraction

#include <cstdint>

extern uint32_t milliseconds();

namespace state_machine {

class StateMachine;
enum SMEvent { EV_INIT, EV_ENTRY, EV_EXIT, EV_TIMER, EV_USER };

typedef void (*State)(StateMachine *, int);

class StateMachine {
public:
  StateMachine(State initial) : currentState_(initial) { }

  void initialize() { dispatch(EV_INIT); }

  void changeStateTo(State nextState) {
    if (nextState == currentState_) {
      return;
    }
    dispatch(EV_EXIT);
    stopTimer();
    currentState_ = nextState;
    dispatch(EV_ENTRY);
  }

  void dispatch(int ev) {
    if (currentState_) {
      currentState_(this, ev);
    }
  }

  // check timeouts
  virtual void tick() {
    if (!timeout_) {
      return;
    }
    if (milliseconds() >= timeout_) {
      dispatch(EV_TIMER);
      timeout_ = 0;
    }
  }

  void setTimer(uint32_t ticks) { timeout_ = milliseconds() + ticks; }

  void stopTimer() { timeout_ = 0; }

protected:
  uint32_t timeout_;
  State currentState_;
};

} // namespace state_machine
