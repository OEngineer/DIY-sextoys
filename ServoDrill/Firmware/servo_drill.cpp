#include "./state_machine.h"
#include "./ticker.h"

#include <mbed.h>

/**
 * Control for servomotor actuating trigger of cordless drill.
 * Uses MG996R 180º servomotor.
 *
 * User interface:
 *   - 1 knob for position setting (ratiometric to VDDA/VSSA)
 *   - 1 RGB LED for state feedback
 *   - 1 pushbutton for changing states
 */

#define POT_PIN PTB0       // A0
#define PWM_PIN PTA5       // D5
#define BUTTON_PIN PTA4    // D4
#define RED_LED_PIN LED1   // PTB18
#define GREEN_LED_PIN LED2 // PTB19
#define BLUE_LED_PIN LED3  // PTD1
#define SERIAL_BPS 38400

// PWM range for servo typically 1-2ms
#define SERVO_PWM_MIN_PU 0.5 / 20.0 // fully CW
#define SERVO_PWM_MAX_PU 1.7 / 20.0 // fully CCW
#define SERVO_PWM_PERIOD_MS 20.0f
#define SERVO_PWM_SCALE (SERVO_PWM_MAX_PU - SERVO_PWM_MIN_PU)

#define POLL_PERIOD_MS 100

enum { // I/O states
  BUTTON_PRESSED = 0,
  BUTTON_RELEASED = 1,
  LED_ON = 0,
  LED_OFF = 1
};

enum LedColor {
  LC_BLACK = 0,
  LC_RED = 1,
  LC_GREEN = 2,
  LC_BLUE = 4,
  LC_YELLOW = LC_RED + LC_GREEN,
  LC_CYAN = LC_GREEN + LC_BLUE,
  LC_MAGENTA = LC_BLUE + LC_RED,
  LC_WHITE = LC_RED + LC_GREEN + LC_BLUE
};

// Global I/O
static AnalogIn potInput(POT_PIN);
static DigitalIn buttonInput(BUTTON_PIN, PullUp);
static DigitalOut redLed(RED_LED_PIN);     // RGB LED on board
static DigitalOut greenLed(GREEN_LED_PIN); // RGB LED on board
static DigitalOut blueLed(BLUE_LED_PIN);   // RGB LED on board
static PwmOut servo(PWM_PIN);
static BufferedSerial pc(USBTX, USBRX, SERIAL_BPS);

static LedColor currentLedColor;

/** Set the RGB LED to given color */
static void ledColor(LedColor color) {
  currentLedColor = color;
  redLed.write((color & LC_RED) != 0 ? LED_ON : LED_OFF);
  greenLed.write((color & LC_GREEN) != 0 ? LED_ON : LED_OFF);
  blueLed.write((color & LC_BLUE) != 0 ? LED_ON : LED_OFF);
}

static void errorFlash() {
  LedColor orig = currentLedColor;
  ledColor(LC_RED);
  ThisThread::sleep_for(400ms);
  ledColor(orig);
}

static unsigned percent(float pu) { return static_cast<unsigned>(pu * 100.0f); }
static unsigned permil(float pu) { return static_cast<unsigned>(pu * 1000.0f); }

class ServoPot {
public:
  ServoPot(AnalogIn &pot, PwmOut &servo)
      : pot_(pot), servo_(servo), servoMin_(0.0), servoMax_(1.0),
        currentPosition_(0.0) {
    servo.period_ms(SERVO_PWM_PERIOD_MS);
    reset();
  }

  static float servoPuToDuty(float pu) {
#if REVERSE_MOTION
    return ((1.0 - pu) * SERVO_PWM_SCALE) + SERVO_PWM_MIN_PU;
#else
    return (pu * SERVO_PWM_SCALE) + SERVO_PWM_MIN_PU;
#endif
  }

  static float servoDutyToPu(float duty) {
#if REVERSE_MOTION
    return 1.0 - ((duty - SERVO_PWM_MIN_PU) / SERVO_PWM_SCALE);
#else
    return ((duty - SERVO_PWM_MIN_PU) / SERVO_PWM_SCALE);
#endif
  }

  void printState() {
    printf("curpos=%u, servo=(%u,%u)\r\n", percent(currentPosition_),
           percent(servoMin_), percent(servoMax_));
  }

  void reset() {
    servoMin_ = 0.0;
    servoMax_ = servoRange_ = 1.0;
    currentPosition_ = 0.0;
    printState();
  }

  void setServoMinimum(float min) {
    servoMin_ = min;
    servoRange_ = servoMax_ - servoMin_;
    printState();
  }

  void setServoMaximum(float max) {
    servoMax_ = max;
    servoRange_ = servoMax_ - servoMin_;
    printState();
  }

  float servoPuPosition() { return servoDutyToPu(servo.read()); }

  // return pu scaled and offset to current servo PU range
  float relativePosition(float pu) { return pu * servoRange_ + servoMin_; }

  // position servo to PU position
  void positionServo(float where) {
    float duty = servoPuToDuty(relativePosition(where));
    servo.write(duty);
    if (where != currentPosition_) {
      currentPosition_ = where;
      printf("Servo %u (%u/1000 duty)\r\n", percent(currentPosition_),
             permil(duty));
    }
  }

  // return relative pot position
  float readPot() {
    return currentPot_ = pot_.read(); // 0.0-1.0
  }

  void setServoFromPot() {
    static float lastPot;
    float potRelative = readPot();
    if (potRelative != lastPot) {
      positionServo(potRelative);
      lastPot = potRelative;
    }
  }

protected:
  AnalogIn &pot_;
  PwmOut &servo_;

  float servoMin_;
  float servoMax_;
  float servoRange_;

  float currentPosition_;
  float currentPot_;
};

ServoPot servoPot(potInput, servo);

// Tell mbed that every file descriptor is our serial port
FileHandle *mbed::mbed_override_console(int fd) { return &pc; }

static void initializeIO() {
  initializeTickCounter();
  ledColor(LC_BLACK);
  servoPot.positionServo(0.0f);
}

using namespace state_machine;

#define CHANGE_STATE_TO(state)                                                 \
  do {                                                                         \
    me->changeStateTo(reinterpret_cast<State>(&state));                        \
    puts("=> " #state);                                                        \
  } while (0)

class SDSM : public StateMachine {
  using super = StateMachine;
  static constexpr uint32_t BUTTON_DEBOUNCE_TIMEOUT = 10;
  static constexpr uint32_t BUTTON_SHORT_TIMEOUT = 1000;

public:
  enum SDSM_Event {
    EV_POLL = EV_USER,
    EV_BUTTON_SHORT_PRESS,
    EV_BUTTON_LONG_PRESS,
  };

  SDSM()
      : StateMachine(reinterpret_cast<State>(&s_opening)), buttonDown_(0),
        buttonUp_(milliseconds()), lastButtonState_(BUTTON_RELEASED),
        buttonDebouncingEnd_(0) {}

  void readButton() {
    int buttonState = !buttonInput.read();
    if (buttonState != lastButtonState_) {
      if (buttonState == BUTTON_PRESSED) {
        buttonDown();
      } else {
        buttonUp();
      }
      lastButtonState_ = buttonState;
    }
  }

  void dispatch(SDSM_Event ev) {
    if (ev != EV_POLL) {
      printf("D %u\r\n", static_cast<unsigned>(ev));
    }
    super::dispatch(static_cast<SMEvent>(ev));
  }

protected:
  void buttonDown() {
    uint32_t now = milliseconds();
    if (now < buttonDebouncingEnd_) {
      return;
    }
    buttonDebouncingEnd_ = now + BUTTON_DEBOUNCE_TIMEOUT;
    buttonDown_ = now;
    buttonUp_ = 0;
    puts("button down");
  }

  void buttonUp() {
    uint32_t now = milliseconds();
    if (now < buttonDebouncingEnd_) {
      return;
    }
    buttonDebouncingEnd_ = now + BUTTON_DEBOUNCE_TIMEOUT;
    uint32_t downTime = now - buttonDown_;
    buttonDown_ = 0;
    buttonUp_ = now;
    if (downTime > BUTTON_SHORT_TIMEOUT) {
      puts("button long press");
      dispatch(EV_BUTTON_LONG_PRESS);
    } else {
      puts("button short press");
      dispatch(EV_BUTTON_SHORT_PRESS);
    }
    puts("button up");
  }

  static void s_opening(SDSM *, int);
  static void s_setting_minimum(SDSM *, int);
  static void s_setting_maximum(SDSM *, int);
  static void s_driving_to_minimum(SDSM *, int);
  static void s_operational(SDSM *, int);
  static void s_paused(SDSM *, int);

  uint32_t buttonDown_; // last button down timestamp or 0
  uint32_t buttonUp_;   // last button up timestamp or 0
  int lastButtonState_; // last button state
  uint32_t buttonDebouncingEnd_;
  float minimumPosition_; // user-set
  float maximumPosition_; // user-set
};

/** opening (initial):
 *   - running servo toward max CCW
 *   - transition to setting minimum after
 *     - knob is CCW enough and
 *     - button short-pressed
 *   - LED: BLUE
 */
void SDSM::s_opening(SDSM *me, int ev) {
  switch (ev) {
  case EV_INIT:
    // fall through
  case EV_ENTRY:
    puts("Opening");
    ledColor(LC_BLUE);
    servoPot.reset();
    servoPot.positionServo(0.0);
    break;
  case EV_EXIT:
    break;
  case EV_TIMER:
    break;
  case EV_BUTTON_SHORT_PRESS: {
    if (servoPot.readPot() < 0.1) {
      CHANGE_STATE_TO(s_setting_minimum);
    } else {
      errorFlash();
    }
  } break;
  case EV_BUTTON_LONG_PRESS:
    break;
  }
}

/** setting minimum:
 *   - entry: servo max CCW (looking down)
 *   - knob controls servo directly
 *   - LED: RED
 *   - transition to setting maximum, save minimum PWM when button
 * short-pressed
 */
void SDSM::s_setting_minimum(SDSM *me, int ev) {
  switch (ev) {
  case EV_ENTRY:
    ledColor(LC_RED);
    servoPot.positionServo(0.0);
    break;
  case EV_EXIT:
    break;
  case EV_POLL:
    servoPot.setServoFromPot();
    break;
  case EV_TIMER:
    break;
  case EV_BUTTON_SHORT_PRESS:
    me->minimumPosition_ = servoPot.servoPuPosition();
    CHANGE_STATE_TO(s_setting_maximum);
    break;
  case EV_BUTTON_LONG_PRESS:
    CHANGE_STATE_TO(s_opening);
    break;
  }
}

/** setting maximum:
 *   - entry: servo at minimum position
 *   - knob controls servo directly
 *   - LED: YELLOW
 *   - transition to driving to minimum, save maximum PWM when button
 * short-pressed
 */
void SDSM::s_setting_maximum(SDSM *me, int ev) {
  switch (ev) {
  case EV_ENTRY:
    ledColor(LC_YELLOW);
    break;
  case EV_EXIT:
    break;
  case EV_TIMER:
    break;
  case EV_POLL:
    servoPot.setServoFromPot();
    break;
  case EV_BUTTON_SHORT_PRESS:
    me->maximumPosition_ = servoPot.servoPuPosition();
    CHANGE_STATE_TO(s_driving_to_minimum);
    break;
  case EV_BUTTON_LONG_PRESS:
    CHANGE_STATE_TO(s_opening);
    break;
  }
}

/**
 *  driving to minimum:
 *  - entry: servo at maximum position
 *  - move servo position to minimum
 *  - transition to operational, save knob CCW reading when knob is CCW
 * enough and button short-pressed
 *  - LED: GREEN
 */
void SDSM::s_driving_to_minimum(SDSM *me, int ev) {
  switch (ev) {
  case EV_ENTRY:
    ledColor(LC_GREEN);
    servoPot.positionServo(me->minimumPosition_);
    break;
  case EV_EXIT:
    break;
  case EV_TIMER:
    break;
  case EV_BUTTON_SHORT_PRESS: {
    if (servoPot.readPot() < 0.1) {
      CHANGE_STATE_TO(s_operational);
    } else {
      errorFlash();
    }
  } break;
  case EV_BUTTON_LONG_PRESS:
    CHANGE_STATE_TO(s_opening);
    break;
  }
}

/**
 *  operational:
 *  - entry: servo at minimum position
 *  - full knob range drives servo between minimum and maximum PWM
 *  - transition to paused on button
 *  - LED: MAGENTA
 */
void SDSM::s_operational(SDSM *me, int ev) {
  switch (ev) {
  case EV_ENTRY:
    servoPot.setServoMinimum(me->minimumPosition_);
    servoPot.setServoMaximum(me->maximumPosition_);
    ledColor(LC_MAGENTA);
    break;
  case EV_EXIT:
    break;
  case EV_TIMER:
    break;
  case EV_POLL:
    servoPot.setServoFromPot();
    break;
  case EV_BUTTON_SHORT_PRESS:
    CHANGE_STATE_TO(s_paused);
    break;
  case EV_BUTTON_LONG_PRESS:
    CHANGE_STATE_TO(s_opening);
    break;
  }
}

/**
 *   paused:
 *   - PWM goes to (set) minimum
 *   - LED: WHITE
 *   - transition to operational on button short-press
 */
void SDSM::s_paused(SDSM *me, int ev) {
  switch (ev) {
  case EV_ENTRY:
    ledColor(LC_WHITE);
    servoPot.positionServo(0.0); // go to minimum
    break;
  case EV_EXIT:
    break;
  case EV_TIMER:
    break;
  case EV_BUTTON_SHORT_PRESS:
    servoPot.setServoFromPot();
    CHANGE_STATE_TO(s_operational);
    break;
  case EV_BUTTON_LONG_PRESS:
    CHANGE_STATE_TO(s_opening);
    break;
  }
}

SDSM stateMachine;

int main() {
  initializeIO();
  puts("Initialized");
  stateMachine.initialize();

  uint32_t nextPoll = 0;

  for (;;) {
    stateMachine.tick();
    stateMachine.readButton();

    uint32_t now = milliseconds();
    if (now >= nextPoll) {
      nextPoll += POLL_PERIOD_MS;
      stateMachine.dispatch(SDSM::EV_POLL);
    }
  }
}
