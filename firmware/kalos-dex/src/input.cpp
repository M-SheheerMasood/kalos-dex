#include "input.h"
#include "config.h"
#include "esp_timer.h"

// ---------------- knob: polled decoder (count a click when it left a rest position and came back via the far side)
static const uint8_t REST = 0b11;
static volatile int32_t clickCount = 0;
static volatile uint8_t lastAB = REST, leftVia = REST;

static void pollKnob(void *) {
  uint8_t ab = (digitalRead(PIN_KNOB_CLK) << 1) | digitalRead(PIN_KNOB_DT);
  if (ab == lastAB) return;
  if (lastAB == REST) {
    leftVia = ab;
  } else if (ab == REST) {
    uint8_t cameVia = lastAB;
    if (leftVia == 0b01 && cameVia != 0b01) clickCount++;
    else if (leftVia == 0b10 && cameVia != 0b10) clickCount--;
    else if (leftVia == 0b00 && cameVia != 0b00) clickCount += (cameVia == 0b10) ? 1 : -1;
    leftVia = REST;
  }
  lastAB = ab;
}

// ---------------- buttons
struct ButtonState {
  int pin;
  bool down = false, held = false, doubleOn = false;
  uint32_t changedAt = 0, releasedAt = 0;
  uint8_t pendingPresses = 0;
};
static ButtonState buttons[(int)Btn::COUNT];
static const char *NAMES[] = {"Knob", "Screen", "Feed", "Play"};

// ---------------- event queue (small ring buffer)
static InputEvent queue[16];
static uint8_t qHead = 0, qTail = 0;
static void push(const InputEvent &e) {
  uint8_t n = (qHead + 1) % 16;
  if (n == qTail) qTail = (qTail + 1) % 16;  // full: drop the oldest
  queue[qHead] = e;
  qHead = n;
}
static void pushButton(Ev t, Btn b) { InputEvent e; e.type = t; e.button = b; push(e); }

namespace input {

void begin() {
  const int pins[] = {PIN_KNOB_SW, PIN_BTN_SCREEN, PIN_BTN_FEED, PIN_BTN_PLAY};
  for (int i = 0; i < (int)Btn::COUNT; i++) {
    buttons[i].pin = pins[i];
    pinMode(pins[i], INPUT_PULLUP);
  }
  pinMode(PIN_KNOB_CLK, INPUT_PULLUP);
  pinMode(PIN_KNOB_DT, INPUT_PULLUP);
  lastAB = (digitalRead(PIN_KNOB_CLK) << 1) | digitalRead(PIN_KNOB_DT);
  esp_timer_create_args_t args = {};
  args.callback = &pollKnob;
  args.name = "knob";
  esp_timer_handle_t timer;
  esp_timer_create(&args, &timer);
  esp_timer_start_periodic(timer, 500);  // 2000 times a second
}

void update(uint32_t now) {
  // knob: report all clicks since the last call as one Turn event
  static int32_t reported = 0;
  int32_t c = clickCount;
  if (c != reported) {
    InputEvent e; e.type = Ev::Turn; e.steps = constrain(c - reported, -127, 127);
    reported += e.steps;
    push(e);
  }
  // buttons
  for (int i = 0; i < (int)Btn::COUNT; i++) {
    ButtonState &b = buttons[i];
    bool pressed = digitalRead(b.pin) == LOW;
    if (pressed != b.down && now - b.changedAt > DEBOUNCE_MS) {
      b.down = pressed;
      b.changedAt = now;
      if (!b.down) {  // released
        if (b.held) { b.held = false; }
        else if (!b.doubleOn) pushButton(Ev::Press, (Btn)i);
        else if (++b.pendingPresses == 2) { pushButton(Ev::Double, (Btn)i); b.pendingPresses = 0; }
        else b.releasedAt = now;
      }
    }
    if (b.down && !b.held && now - b.changedAt > LONG_PRESS_MS) {
      b.held = true;
      b.pendingPresses = 0;
      pushButton(Ev::Hold, (Btn)i);
    }
    // single press confirmed when no second press came in time
    if (b.doubleOn && b.pendingPresses == 1 && !b.down && now - b.releasedAt > DOUBLE_MS) {
      b.pendingPresses = 0;
      pushButton(Ev::Press, (Btn)i);
    }
  }
}

bool next(InputEvent &e) {
  if (qTail == qHead) return false;
  e = queue[qTail];
  qTail = (qTail + 1) % 16;
  return true;
}

void enableDouble(Btn b, bool on) { buttons[(int)b].doubleOn = on; }
bool isDown(Btn b) { return buttons[(int)b].down; }
const char *name(Btn b) { return NAMES[(int)b]; }
int32_t totalClicks() { return clickCount; }

}  // namespace input
