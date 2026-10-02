// Adapted from FreeInk SDK (MIT); see LICENSE and UPSTREAM.md.
#include "XteinkDetect.h"
#include "Arduino.h"
#include "BoardConfig.h"
#include <string.h>
namespace freeink { namespace {
struct EpdProbePins {
  int8_t sclk;
  int8_t mosi;  // the controller's bidirectional SDA in half-duplex mode
  int8_t cs;
  int8_t dc;
  int8_t rst;
  int8_t busy;
};

// UC81xx read-capable registers (UC8179 / UC8279d datasheets, identical layout).
constexpr uint8_t UC81XX_CMD_VER = 0x70;   // reserved 0x00, CHIP_VER, LUT_VER[23:0]
constexpr uint8_t UC81XX_CMD_FLG = 0x71;   // status; BUSY_N (D0) = 1 when idle
constexpr uint8_t UC81XX_CMD_RMTP = 0xA2;  // bulk MTP read: 1 dummy byte, then MTP[0..n]

// Diagnostics snapshot of the last probe (see XteinkDisplayProbeDiag in the
// header). File-scope so locked-unit firmware can persist it after the fact.
XteinkDisplayProbeDiag g_probeDiag;

inline void epdClockDelay() { delayMicroseconds(1); }  // ~500 kHz, timing-safe

void epdWriteByte(const EpdProbePins& p, uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(p.mosi, (b & 0x80) ? HIGH : LOW);
    epdClockDelay();
    digitalWrite(p.sclk, HIGH);
    epdClockDelay();
    digitalWrite(p.sclk, LOW);
    b <<= 1;
  }
}

uint8_t epdReadByte(const EpdProbePins& p) {
  uint8_t b = 0;
  for (uint8_t i = 0; i < 8; i++) {
    // The controller shifts the next bit out on the SCL falling edge; sample
    // while the clock is low, then pulse.
    epdClockDelay();
    b = static_cast<uint8_t>((b << 1) | (digitalRead(p.mosi) == HIGH ? 1 : 0));
    digitalWrite(p.sclk, HIGH);
    epdClockDelay();
    digitalWrite(p.sclk, LOW);
  }
  return b;
}

// One command + N-byte half-duplex read: command with DC low, then SDA (our
// MOSI) released to input with DC high while the controller drives the reads.
void epdCmdRead(const EpdProbePins& p, uint8_t cmd, uint8_t* out, uint8_t len) {
  pinMode(p.mosi, OUTPUT);
  digitalWrite(p.dc, LOW);
  digitalWrite(p.cs, LOW);
  epdClockDelay();
  epdWriteByte(p, cmd);
  digitalWrite(p.dc, HIGH);
  pinMode(p.mosi, INPUT_PULLUP);
  epdClockDelay();
  for (uint8_t i = 0; i < len; i++) out[i] = epdReadByte(p);
  digitalWrite(p.cs, HIGH);
  pinMode(p.mosi, OUTPUT);
}

// The UC81xx VER signature: a leading reserved 0x00, then a CHIP_VER byte
// (datasheet default 0x03, MTP-programmed so not pinned to that exact value)
// that is neither a floating-low nor a floating-high bus. A released SDA reads
// back all-0x00 or all-0xFF through the pull-up, and neither the UC8253 nor the
// SSD1677 answers 0x70 with this shape (the SSD-family has no such read), so a
// additionally report idle (BUSY_N=1) without being a floating pattern.
//
// Match on the FLG status plus a non-uniform VER. A UC81xx drives 0x71 to a real
// status byte (BUSY_N=D0=1 when idle) and returns a structured VER; an SSD-family
// controller doesn't answer 0x70/0x71 at all, so the half-duplex line floats to a
// uniform level (all 0xFF via the pull-up, or all 0x00). We deliberately do NOT
// require any specific CHIP_VER value: a shipping X4 Pro UC8179 was observed
// returning VER=00 00 01 FF FF (CHIP_VER byte = 0x00) with FLG=0x13 — an earlier
// matcher that required ver[1] != 0 wrongly rejected it.
bool verIsFloating(const uint8_t ver[5]) {
  for (int i = 1; i < 5; i++)
    if (ver[i] != ver[0]) return false;  // any variation => a real, driven response
  return true;                           // all five bytes identical => floating bus
}

bool matchUc81xx(const uint8_t ver[5], uint8_t flg) {
  // FLG must be a real, non-floating status with BUSY_N (bit0) asserted (idle).
  if (flg == 0x00 || flg == 0xFF) return false;
  if ((flg & 0x01) != 0x01) return false;
  // VER must be an actually-driven (non-uniform) pattern, not a floating bus.
  return !verIsFloating(ver);
}

bool runDisplayProbePass(const EpdProbePins& p, uint8_t ver[5], uint8_t* flg, uint8_t rstLowMs) {
  pinMode(p.cs, OUTPUT);
  digitalWrite(p.cs, HIGH);
  pinMode(p.sclk, OUTPUT);
  digitalWrite(p.sclk, LOW);
  pinMode(p.dc, OUTPUT);
  digitalWrite(p.dc, LOW);
  pinMode(p.mosi, OUTPUT);
  if (p.busy >= 0) pinMode(p.busy, INPUT);

  // Hardware reset pulse (rstLowMs low, then a fixed settle). The vendor
  // identification path holds RST_N low for 50 ms (well beyond the datasheet's
  // 50 us minimum — the ID readback is less forgiving than normal operation),
  // but that cost is only paid on the CONFIRM pass: the screening pass uses a
  // short pulse so the common case — an SSD-family panel that will never answer
  // 0x70 — doesn't add ~100 ms to every boot and wake (see
  // probeDisplayController). We can't trust BUSY polarity here — the controller
  // (and therefore its idle level) is exactly what we're trying to identify —
  // so we don't gate on BUSY; a flat delay covers every UC81xx power-up. The
  // panel driver's own begin() resets again afterwards, so this leaves no state.
  if (p.rst >= 0) {
    // The sleep path holds RESET at a board-safe level and that per-pin hold
    // survives the wake reset. Controller detection runs before EpdBus::begin(),
    // so it must release the hold itself or every digitalWrite below silently
    // bounces off the retained latch and the probe can select the wrong driver.
    gpio_hold_dis(static_cast<gpio_num_t>(p.rst));
    pinMode(p.rst, OUTPUT);
    digitalWrite(p.rst, HIGH);
    delay(2);
    digitalWrite(p.rst, LOW);
    delay(rstLowMs);
    digitalWrite(p.rst, HIGH);
  }
  delay(30);

  uint8_t flgByte = 0;
  epdCmdRead(p, UC81XX_CMD_FLG, &flgByte, 1);
  epdCmdRead(p, UC81XX_CMD_VER, ver, 5);
  if (flg) *flg = flgByte;
  return matchUc81xx(ver, flgByte);
}

void releaseDisplayPins(const EpdProbePins& p) {
  // Leave everything released. RST_N has an internal pull-up, so INPUT keeps the
  // controller out of reset.
  pinMode(p.sclk, INPUT);
  pinMode(p.mosi, INPUT);
  pinMode(p.cs, INPUT_PULLUP);  // don't leave the panel selected
  pinMode(p.dc, INPUT);
  if (p.rst >= 0) pinMode(p.rst, INPUT);
}

#if FREEINK_DEVICE_X3 || FREEINK_DEVICE_X4CLASSIC
// X3 V6.3.15: 4200dafa (reset), 4200db40 (BUSY), 42009e4c/42009dba
// (read), 4200fb12 (selector). See docs/x3-v6.3.15-firmware-audit.md.
// Keep this separate from the X4-family fingerprint: stock X3 selects from
// VER byte 2 alone, without a FLG signature or readable/programmed MTP.
// The X4C uses the SAME protocol: its stock (V7.1.7 community build,
// FUN_4200a778/FUN_42007fcc) resets, waits BUSY bounded, writes 0x70 with DC
// low, flips SDA to input (no pull-up) and clocks 3 bytes back — no MISO
// involved — then selects the driver from ver[2] against a 5-entry table.
DisplayControllerVerdict probeX3DisplayController(const EpdProbePins& p, uint8_t verBytes[5], uint8_t* flg) {
  g_probeDiag = {};
  g_probeDiag.valid = true;
  g_probeDiag.verBytesRead = 3;
  pinMode(p.cs, OUTPUT);
  digitalWrite(p.cs, HIGH);
  pinMode(p.sclk, OUTPUT);
  digitalWrite(p.sclk, LOW);
  pinMode(p.dc, OUTPUT);
  digitalWrite(p.dc, HIGH);
  pinMode(p.mosi, OUTPUT);
  if (p.busy >= 0) pinMode(p.busy, INPUT);

  if (p.rst >= 0) {
    gpio_hold_dis(static_cast<gpio_num_t>(p.rst));
    pinMode(p.rst, OUTPUT);
    digitalWrite(p.rst, HIGH);
    delay(10);
    digitalWrite(p.rst, LOW);
    delay(50);
    digitalWrite(p.rst, HIGH);
  }
  delay(50);
  if (p.busy >= 0) {
    const unsigned long start = millis();
    do {
      delay(1);
      if (digitalRead(p.busy) == HIGH) break;
      if (millis() - start >= 300) {
        g_probeDiag.busyTimedOut = true;
        break;
      }
    } while (true);
  }

  // Stock still attempts VER after its bounded BUSY wait expires. A timeout
  // is diagnostic, not another condition that can reject a recognized ID.
  digitalWrite(p.cs, LOW);
  digitalWrite(p.dc, LOW);
  epdWriteByte(p, UC81XX_CMD_VER);
  digitalWrite(p.dc, HIGH);
  epdClockDelay();
  pinMode(p.mosi, INPUT);  // stock releases SDA without enabling a pull-up
  delayMicroseconds(2);
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t& b = g_probeDiag.ver[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      digitalWrite(p.sclk, LOW);
      epdClockDelay();
      digitalWrite(p.sclk, HIGH);
      epdClockDelay();
      b = static_cast<uint8_t>((b << 1) | (digitalRead(p.mosi) == HIGH ? 1 : 0));
    }
    digitalWrite(p.sclk, LOW);
    epdClockDelay();
  }
  digitalWrite(p.cs, HIGH);
  pinMode(p.mosi, OUTPUT);
  releaseDisplayPins(p);

  const uint8_t id = g_probeDiag.ver[2];
  const auto verdict = id == 0x66 ? DisplayControllerVerdict::Uc81xxConfirmed
                       : id == 0xFF ? DisplayControllerVerdict::PrimaryAssumed
                                    : DisplayControllerVerdict::Inconclusive;
  g_probeDiag.verdict = static_cast<uint8_t>(verdict);
  if (verBytes) memcpy(verBytes, g_probeDiag.ver, 5);
  if (flg) *flg = 0;  // not read by the stock X3 protocol
  return verdict;
}
#endif

// Two-pass probe with agreement, over an arbitrary pinout. Confirmed only when
// both passes match the UC81xx signature AND agree on the VER bytes — a floating
// bus can't produce the same stable non-trivial pattern twice. Disagreement is
// Inconclusive; both-fail is PrimaryAssumed (the profile's default controller).
//
// Reset budget: pass 1 screens with the short (1 ms) reset that every benched
// UC81xx answers fine; only if it matches does pass 2 confirm with the vendor
// identification timing (RST low 50 ms), which also makes pass 2's VER the one
// read under doc conditions. An SSD-family board (floating bus) therefore pays
// two cheap passes (~66 ms total, as before the doc-timing change) instead of
// two 50 ms resets on every boot and wake.
DisplayControllerVerdict probeDisplayController(const EpdProbePins& p, uint8_t verBytes[5], uint8_t* flg) {
  uint8_t ver1[5] = {0};
  uint8_t ver2[5] = {0};
  uint8_t flg1 = 0;
  const bool pass1 = runDisplayProbePass(p, ver1, &flg1, /*rstLowMs=*/1);
  delay(2);
  const bool pass2 = runDisplayProbePass(p, ver2, nullptr, /*rstLowMs=*/pass1 ? 50 : 1);

  const bool verAgree = memcmp(ver1, ver2, 5) == 0;
  bool confirmed = pass1 && pass2 && verAgree;
  // FLG is a driven idle status (not a floating 0xFF / dead 0x00, BUSY_N set).
  const bool flgDriven = flg1 != 0x00 && flg1 != 0xFF && (flg1 & 0x01) == 0x01;

  // Diagnostics snapshot for locked units (persisted by firmware, e.g. to SD).
  g_probeDiag = {};
  g_probeDiag.valid = true;
  g_probeDiag.verBytesRead = 5;
  memcpy(g_probeDiag.ver, pass1 && pass2 ? ver2 : ver1, 5);
  g_probeDiag.flg = flg1;
  g_probeDiag.promoted = false;
  g_probeDiag.mtpValid = false;

  // Ground-truth dump of the module's factory configuration: RMTP (0xA2)
  // returns a dummy byte then MTP[0..n] — the 0xA5 refresh-enable key, the
  // Command Default Setting block (real PSR/TRES/GSST/CDI/TCON), product ID
  // and LUT version. Read whenever SOMETHING is driving the status line: on a
  // confirmed part it's diagnostics; on the fallback path below it is the
  // discriminator itself. A part without RMTP (UC8253, SSD-family) floats the
  // line and reads uniform garbage here.
  if (confirmed || flgDriven) {
    uint8_t raw[sizeof(g_probeDiag.mtp) + 1] = {0};
    epdCmdRead(p, UC81XX_CMD_RMTP, raw, sizeof(raw));
    memcpy(g_probeDiag.mtp, raw + 1, sizeof(g_probeDiag.mtp));
    g_probeDiag.mtpValid = true;
  }

  // Fallback match — FIELD-OBSERVED UC8279d signature: new X3 units return
  // VER = FF FF FF FF FF (blank/unreadable LUT_VER area) with FLG = 0x13 (the
  // datasheet's idle default), which the uniform-VER floating-bus test wrongly
  // rejects. A pulled-up floating bus also reads FF — so require POSITIVE
  // evidence from RMTP. Two acceptable shapes:
  //   * mtp[0] == 0xA5: a programmed MTP's refresh-enable key. Unambiguous.
  //   * a NON-UNIFORM dump that repeats byte-for-byte on a second read: the
  //     field UC8279d modules ship a BLANK MTP (all zeros except the LUT
  //     version stamp at 0x01A — never 0xA5), but the silicon still DRIVES
  //     the RMTP readback. A UC8253 has no 0xA2 command, so its read floats
  //     to a uniform pull-up pattern (field-confirmed FF); floating garbage
  //     can be non-uniform once but cannot repeat 48 bytes exactly.
  if (!confirmed && flgDriven && verAgree && verIsFloating(ver1) && ver1[0] == 0xFF && g_probeDiag.mtpValid) {
    if (g_probeDiag.mtp[0] == 0xA5) {
      confirmed = true;
    } else {
      bool uniform = true;
      for (size_t i = 1; i < sizeof(g_probeDiag.mtp); i++) {
        if (g_probeDiag.mtp[i] != g_probeDiag.mtp[0]) {
          uniform = false;
          break;
        }
      }
      if (!uniform) {
        uint8_t raw2[sizeof(g_probeDiag.mtp) + 1] = {0};
        epdCmdRead(p, UC81XX_CMD_RMTP, raw2, sizeof(raw2));
        if (memcmp(g_probeDiag.mtp, raw2 + 1, sizeof(g_probeDiag.mtp)) == 0) confirmed = true;
      }
    }
  }
  releaseDisplayPins(p);

  if (verBytes) memcpy(verBytes, g_probeDiag.ver, 5);
  if (flg) *flg = flg1;
  DisplayControllerVerdict v = DisplayControllerVerdict::Inconclusive;
  if (confirmed) v = DisplayControllerVerdict::Uc81xxConfirmed;
  else if (!pass1 && !pass2) v = DisplayControllerVerdict::PrimaryAssumed;
  g_probeDiag.verdict = static_cast<uint8_t>(v);
  return v;
}

}  // namespace

DisplayControllerVerdict detectXteinkDisplayController(uint8_t verBytes[5], uint8_t* flg) {
  const auto& d = BoardConfig::ACTIVE.display;
  const EpdProbePins p{d.sclk, d.mosi, d.cs, d.dc, d.rst, d.busy};
#if FREEINK_DEVICE_X3
  if (BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3 ||
      BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3Uc8279) {
    return probeX3DisplayController(p, verBytes, flg);
  }
#endif
  return probeDisplayController(p, verBytes, flg);
}

const XteinkDisplayProbeDiag& getXteinkDisplayProbeDiag() { return g_probeDiag; }

} // namespace freeink
