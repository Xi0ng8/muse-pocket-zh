#pragma once
#include "Arduino.h"
#include "driver/spi_master.h"
#include <atomic>
namespace freeink {
enum class BusyPolarity : uint8_t { ActiveHigh, ActiveLow, X3TwoPhase, UcIdleHigh };
struct EpdPins { int8_t sclk, mosi, cs, dc, rst, busy; int8_t powerEnable = -1; };
class EpdBus {
public:
    void begin(const EpdPins&, uint32_t, BusyPolarity, int8_t = -1, int8_t = -1);
    void reset(uint16_t extraSettleMs = 0);
    void cmd(uint8_t); void data(uint8_t); void data(const uint8_t*, uint16_t);
    void cmdData(uint8_t, const uint8_t*, uint16_t); void cmdData2(uint8_t, uint8_t, uint8_t);
    void beginTxn(); void endTxn(); void rawCmd(uint8_t); void rawData(uint8_t);
    void rawWriteBytes(const uint8_t*, uint16_t);
    void waitBusy(const char* = nullptr); void waitBusy(BusyPolarity, const char* = nullptr);
    void waitRefreshComplete(const char* = nullptr);
    void sendPlaneFlipped(uint8_t, const uint8_t*, uint16_t, uint16_t);
    void sendPlaneFlippedInverted(uint8_t, const uint8_t*, uint16_t, uint16_t);
    void fillPlane(uint8_t, uint8_t, uint16_t, uint16_t);
    bool isBusy() const;
    const EpdPins& pins() const { return pins_; }
    bool healthy() const { return healthy_; }
private:
    void write(const uint8_t*, size_t);
    EpdPins pins_{};
    BusyPolarity polarity_ = BusyPolarity::ActiveHigh;
    spi_device_handle_t spi_ = nullptr;
    std::atomic<bool> healthy_{true};
};
}
