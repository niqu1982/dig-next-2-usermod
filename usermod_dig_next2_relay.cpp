/*
 * ============================================================
 *  WLED Usermod: QuinLED Dig-Next-2 — Multi-Relay + Buttons
 *  FINAL: relay logic decoupled from transient segment updates
 * ============================================================
 *
 *  Założenie:
 *  - BTN1 przełącza tylko kanał CH1
 *  - BTN2 przełącza tylko kanał CH2
 *  - Master relay jest ON gdy CH1 lub CH2 jest ON
 *  - onStateChange synchronizuje stany tylko dla zmian z app/API,
 *    ale ignoruje krótkie przejściowe zmiany po lokalnym buttonie
 *
 *  Mapowanie z configu:
 *  - Bus 0 / GPIO2   -> CH1
 *  - Bus 1 / GPIO4   -> CH2
 *  - BTN1 GPIO34
 *  - BTN2 GPIO35
 * ============================================================
 */

#include "wled.h"

#define DN2_RELAY_CH1_A     20
#define DN2_RELAY_CH1_B     22
#define DN2_RELAY_CH2       21
#define DN2_RELAY_MASTER     5

#define DN2_BTN1_PIN        34
#define DN2_BTN2_PIN        35

#define DN2_RELAY_ON      HIGH
#define DN2_RELAY_OFF     LOW

#define DN2_DEBOUNCE_MS     50
#define DN2_BTN_ACTIVE     LOW

#define DN2_BUS_CH1         0
#define DN2_BUS_CH2         1
#define DN2_SEGMENT_CH1_IDX 0
#define DN2_SEGMENT_CH2_IDX 1

#define DN2_LOCAL_GUARD_MS  500

#ifndef USERMOD_ID_DIG_NEXT2_RELAY
  #define USERMOD_ID_DIG_NEXT2_RELAY 0x4E32
#endif

class DigNext2RelayUsermod : public Usermod {
private:
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

  void setRelay(uint8_t pin, bool on) {
    digitalWrite(pin, on ? DN2_RELAY_ON : DN2_RELAY_OFF);
  }

  void applyRelays() {
    setRelay(DN2_RELAY_CH1_A, ch1Active);
    setRelay(DN2_RELAY_CH1_B, ch1Active);
    setRelay(DN2_RELAY_CH2, ch2Active);

    bool wantMaster = ch1Active || ch2Active;
    masterActive = wantMaster;
    setRelay(DN2_RELAY_MASTER, masterActive);
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

    DEBUG_PRINTF("[DN2] button ch=%u => CH1=%d CH2=%d MASTER=%d\n",
      channel, ch1Active, ch2Active, masterActive);
  }

  void readButtons() {
    uint32_t now = millis();

    bool r1 = (bool)digitalRead(DN2_BTN1_PIN);
    if (r1 != btn1Raw) {
      btn1Raw = r1;
      btn1Time = now;
    }
    if ((now - btn1Time) > DN2_DEBOUNCE_MS && btn1Stable != btn1Raw) {
      btn1Stable = btn1Raw;
      if (btn1Stable == DN2_BTN_ACTIVE) btn1Fired = true;
    }

    bool r2 = (bool)digitalRead(DN2_BTN2_PIN);
    if (r2 != btn2Raw) {
      btn2Raw = r2;
      btn2Time = now;
    }
    if ((now - btn2Time) > DN2_DEBOUNCE_MS && btn2Stable != btn2Raw) {
      btn2Stable = btn2Raw;
      if (btn2Stable == DN2_BTN_ACTIVE) btn2Fired = true;
    }
  }

public:
  void setup() override {
    pinMode(DN2_RELAY_CH1_A, OUTPUT);
    pinMode(DN2_RELAY_CH1_B, OUTPUT);
    pinMode(DN2_RELAY_CH2, OUTPUT);
    pinMode(DN2_RELAY_MASTER, OUTPUT);

    pinMode(DN2_BTN1_PIN, INPUT);
    pinMode(DN2_BTN2_PIN, INPUT);

    ch1Active = isSegmentLit(DN2_SEGMENT_CH1_IDX);
    ch2Active = isSegmentLit(DN2_SEGMENT_CH2_IDX);
    applyRelays();

    DEBUG_PRINTF("[DN2] setup CH1=%d CH2=%d MASTER=%d\n", ch1Active, ch2Active, masterActive);
  }

  void loop() override {
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
    if ((now - lastLocalButtonMs) < DN2_LOCAL_GUARD_MS) {
      DEBUG_PRINTF("[DN2] ignore transient state change mode=%u\n", mode);
      return;
    }

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

    DEBUG_PRINTF("[DN2] sync mode=%u => CH1=%d CH2=%d MASTER=%d\n",
      mode, ch1Active, ch2Active, masterActive);
  }

  uint16_t getId() override {
    return USERMOD_ID_DIG_NEXT2_RELAY;
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root[F("u")];
    if (user.isNull()) user = root.createNestedObject(F("u"));
    JsonArray arr = user.createNestedArray(F("Dig-Next-2 Relay"));
    arr.add(ch1Active ? F("CH1 ON") : F("CH1 OFF"));
    arr.add(ch2Active ? F("CH2 ON") : F("CH2 OFF"));
    arr.add(masterActive ? F("MST ON") : F("MST OFF"));
  }

  void addToJsonState(JsonObject& root) override {
    root[F("dn2ch1")] = ch1Active;
    root[F("dn2ch2")] = ch2Active;
    root[F("dn2master")] = masterActive;
  }
};

static DigNext2RelayUsermod dn2relayusermod;
REGISTER_USERMOD(dn2relayusermod);