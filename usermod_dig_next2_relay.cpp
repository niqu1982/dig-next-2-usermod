#include "wled.h"
#include "pin_manager.h"

#ifndef USERMOD_ID_DIG_NEXT2_RELAY
  #define USERMOD_ID_DIG_NEXT2_RELAY  0x4E32
#endif

#define DN2_RELAY_ON         HIGH
#define DN2_RELAY_OFF        LOW
#define DN2_DEBOUNCE_MS      50
#define DN2_BTN_ACTIVE       LOW
#define DN2_BUS_CH1          0
#define DN2_BUS_CH2          1
#define DN2_SEGMENT_CH1_IDX  0
#define DN2_SEGMENT_CH2_IDX  1
#define DN2_LOCAL_GUARD_MS   500

class DigNext2RelayUsermod : public Usermod {
private:
  int8_t pinRelayCh1A   = 20;
  int8_t pinRelayCh1B   = 22;
  int8_t pinRelayCh2    = 21;
  int8_t pinRelayMaster = 5;
  int8_t pinButton1     = 34;
  int8_t pinButton2     = 35;

  bool ch1Active = false;
  bool ch2Active = false;
  bool masterActive = false;

  bool btn1Raw = HIGH;
  bool btn1Stable = HIGH;
  uint32_t btn1Time = 0;
  bool btn1Fired = false;

  bool btn2Raw = HIGH;
  bool btn2Stable = HIGH;
  uint32_t btn2Time = 0;
  bool btn2Fired = false;

  uint32_t lastLocalButtonMs = 0;
  bool pinsInitialized = false;

  bool allocRelayCh1A = false;
  bool allocRelayCh1B = false;
  bool allocRelayCh2 = false;
  bool allocRelayMaster = false;
  bool allocButton1 = false;
  bool allocButton2 = false;

  bool isValidPin(int8_t pin) {
    return pin >= 0;
  }

  bool allocatePinIfValid(int8_t pin, bool output, const char* owner, bool &flag) {
    flag = false;
    if (!isValidPin(pin)) return true;

    PinOwner po = PinOwner::UM_Unspecified;
    if (owner != nullptr) {
      po = PinOwner::UM_Unspecified;
    }

    if (!PinManager::allocatePin(pin, output, PinOwner::UM_Unspecified)) {
      DEBUG_PRINTF("[DN2] failed to allocate pin %d\n", pin);
      return false;
    }

    flag = true;
    pinMode(pin, output ? OUTPUT : INPUT);
    return true;
  }

  void releasePinIfAllocated(int8_t pin, bool &flag, bool output) {
    if (!flag || !isValidPin(pin)) return;
    PinManager::deallocatePin(pin, output ? PinOwner::UM_Unspecified : PinOwner::UM_Unspecified);
    flag = false;
  }

  void releaseAllPins() {
    releasePinIfAllocated(pinRelayCh1A, allocRelayCh1A, true);
    releasePinIfAllocated(pinRelayCh1B, allocRelayCh1B, true);
    releasePinIfAllocated(pinRelayCh2, allocRelayCh2, true);
    releasePinIfAllocated(pinRelayMaster, allocRelayMaster, true);
    releasePinIfAllocated(pinButton1, allocButton1, false);
    releasePinIfAllocated(pinButton2, allocButton2, false);
    pinsInitialized = false;
  }

  bool initPins() {
    releaseAllPins();

    bool ok = true;
    ok &= allocatePinIfValid(pinRelayCh1A, true,  "Relay CH1 A", allocRelayCh1A);
    ok &= allocatePinIfValid(pinRelayCh1B, true,  "Relay CH1 B", allocRelayCh1B);
    ok &= allocatePinIfValid(pinRelayCh2, true,   "Relay CH2", allocRelayCh2);
    ok &= allocatePinIfValid(pinRelayMaster, true,"Relay Master", allocRelayMaster);
    ok &= allocatePinIfValid(pinButton1, false,   "Button 1", allocButton1);
    ok &= allocatePinIfValid(pinButton2, false,   "Button 2", allocButton2);

    pinsInitialized = ok;
    if (pinsInitialized) applyRelays();
    return pinsInitialized;
  }

  void writePinIfValid(int8_t pin, bool on) {
    if (isValidPin(pin)) digitalWrite(pin, on ? DN2_RELAY_ON : DN2_RELAY_OFF);
  }

  void applyRelays() {
    writePinIfValid(pinRelayCh1A, ch1Active);
    writePinIfValid(pinRelayCh1B, ch1Active);
    writePinIfValid(pinRelayCh2, ch2Active);

    masterActive = ch1Active || ch2Active;
    writePinIfValid(pinRelayMaster, masterActive);
  }

  bool isSegmentLit(uint8_t segIdx) {
    if (bri == 0) return false;
    if (segIdx >= strip.getSegmentsNum()) return false;

    const Segment& seg = strip.getSegment(segIdx);
    if (!seg.isActive()) return false;
    return seg.on && seg.opacity > 0;
  }

  bool isBusLit(uint8_t busIdx) {
    if (bri == 0) return false;
    if (busIdx >= BusManager::getNumBusses()) return false;

    const Bus* bus = BusManager::getBus(busIdx);
    if (!bus || !bus->isOk()) return false;

    uint16_t bStart = bus->getStart();
    uint16_t bEnd = bStart + bus->getLength();

    for (uint8_t s = 0; s < strip.getSegmentsNum(); s++) {
      const Segment& seg = strip.getSegment(s);
      if (!seg.isActive()) continue;
      if (seg.start < bEnd && seg.stop > bStart) {
        if (seg.on && seg.opacity > 0) return true;
      }
    }
    return false;
  }

  void setSegmentOn(uint8_t segIdx, bool on) {
    if (segIdx >= strip.getSegmentsNum()) return;
    Segment& seg = strip.getSegment(segIdx);
    if (!seg.isActive()) return;
    seg.on = on;
    seg.opacity = on ? 255 : 0;
  }

  void updateChannelFromButton(uint8_t channel) {
    if (channel == 1) {
      ch1Active = !ch1Active;
      setSegmentOn(DN2_SEGMENT_CH1_IDX, ch1Active);
    } else {
      ch2Active = !ch2Active;
      setSegmentOn(DN2_SEGMENT_CH2_IDX, ch2Active);
    }

    if ((ch1Active || ch2Active) && bri == 0) {
      bri = (briLast > 0) ? briLast : 128;
    }

    lastLocalButtonMs = millis();
    applyRelays();
    stateChanged = true;
    colorUpdated(CALL_MODE_BUTTON);
  }

  void readButtons() {
    uint32_t now = millis();

    if (allocButton1 && isValidPin(pinButton1)) {
      bool r1 = (bool)digitalRead(pinButton1);
      if (r1 != btn1Raw) {
        btn1Raw = r1;
        btn1Time = now;
      }
      if ((now - btn1Time) > DN2_DEBOUNCE_MS && btn1Stable != btn1Raw) {
        btn1Stable = btn1Raw;
        if (btn1Stable == DN2_BTN_ACTIVE) btn1Fired = true;
      }
    }

    if (allocButton2 && isValidPin(pinButton2)) {
      bool r2 = (bool)digitalRead(pinButton2);
      if (r2 != btn2Raw) {
        btn2Raw = r2;
        btn2Time = now;
      }
      if ((now - btn2Time) > DN2_DEBOUNCE_MS && btn2Stable != btn2Raw) {
        btn2Stable = btn2Raw;
        if (btn2Stable == DN2_BTN_ACTIVE) btn2Fired = true;
      }
    }
  }

public:
  ~DigNext2RelayUsermod() {
    releaseAllPins();
  }

  void setup() override {
    ch1Active = isSegmentLit(DN2_SEGMENT_CH1_IDX);
    ch2Active = isSegmentLit(DN2_SEGMENT_CH2_IDX);
    initPins();
    DEBUG_PRINTLN(F("[DN2] Usermod ready with PinManager"));
  }

  void loop() override {
    if (!pinsInitialized) return;

    readButtons();

    if (btn1Fired) {
      btn1Fired = false;
      updateChannelFromButton(1);
    }

    if (btn2Fired) {
      btn2Fired = false;
      updateChannelFromButton(2);
    }
  }

  void onStateChange(uint8_t mode) override {
    uint32_t now = millis();
    if ((now - lastLocalButtonMs) < DN2_LOCAL_GUARD_MS) return;

    bool newCh1 = isSegmentLit(DN2_SEGMENT_CH1_IDX);
    bool newCh2 = isSegmentLit(DN2_SEGMENT_CH2_IDX);

    if (DN2_SEGMENT_CH1_IDX >= strip.getSegmentsNum() || !strip.getSegment(DN2_SEGMENT_CH1_IDX).isActive()) {
      newCh1 = isBusLit(DN2_BUS_CH1);
    }
    if (DN2_SEGMENT_CH2_IDX >= strip.getSegmentsNum() || !strip.getSegment(DN2_SEGMENT_CH2_IDX).isActive()) {
      newCh2 = isBusLit(DN2_BUS_CH2);
    }

    ch1Active = newCh1;
    ch2Active = newCh2;
    applyRelays();
    (void)mode;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[F("DigNext2Relay")];
    if (top.isNull()) return false;

    pinRelayCh1A   = top[F("relayCh1A")]   | pinRelayCh1A;
    pinRelayCh1B   = top[F("relayCh1B")]   | pinRelayCh1B;
    pinRelayCh2    = top[F("relayCh2")]    | pinRelayCh2;
    pinRelayMaster = top[F("relayMaster")] | pinRelayMaster;
    pinButton1     = top[F("button1")]     | pinButton1;
    pinButton2     = top[F("button2")]     | pinButton2;

    if (pinsInitialized) initPins();
    return true;
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(F("DigNext2Relay"));
    top[F("relayCh1A")]   = pinRelayCh1A;
    top[F("relayCh1B")]   = pinRelayCh1B;
    top[F("relayCh2")]    = pinRelayCh2;
    top[F("relayMaster")] = pinRelayMaster;
    top[F("button1")]     = pinButton1;
    top[F("button2")]     = pinButton2;
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root[F("u")];
    if (user.isNull()) user = root.createNestedObject(F("u"));

    JsonArray arr = user.createNestedArray(F("DigNext2Relay"));
    arr.add(ch1Active ? F("CH1 ON") : F("CH1 OFF"));
    arr.add(ch2Active ? F("CH2 ON") : F("CH2 OFF"));
    arr.add(masterActive ? F("MASTER ON") : F("MASTER OFF"));
    arr.add(String(F("Relay CH1 A pin: ")) + pinRelayCh1A);
    arr.add(String(F("Relay CH1 B pin: ")) + pinRelayCh1B);
    arr.add(String(F("Relay CH2 pin: ")) + pinRelayCh2);
    arr.add(String(F("Master pin: ")) + pinRelayMaster);
    arr.add(String(F("Button 1 pin: ")) + pinButton1);
    arr.add(String(F("Button 2 pin: ")) + pinButton2);
  }

  void addToJsonState(JsonObject& root) override {
    root[F("dn2ch1")] = ch1Active;
    root[F("dn2ch2")] = ch2Active;
    root[F("dn2master")] = masterActive;
  }

  uint16_t getId() override {
    return USERMOD_ID_DIG_NEXT2_RELAY;
  }
};

static DigNext2RelayUsermod dn2relayusermod;
REGISTER_USERMOD(dn2relayusermod);
