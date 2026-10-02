// Native ESP-IDF bus for the MIT FreeInk panel drivers.
#include "EpdBus.h"
#include "esp_log.h"
#include <cstring>
namespace freeink {
void EpdBus::begin(const EpdPins& p, uint32_t hz, BusyPolarity b, int8_t miso, int8_t) {
    pins_ = p; polarity_ = b;
    pinMode(p.cs, OUTPUT); digitalWrite(p.cs, HIGH);
    pinMode(p.dc, OUTPUT); digitalWrite(p.dc, HIGH);
    gpio_hold_dis(static_cast<gpio_num_t>(p.rst));
    pinMode(p.rst, OUTPUT); digitalWrite(p.rst, HIGH);
    pinMode(p.busy, b == BusyPolarity::ActiveHigh ? INPUT : INPUT_PULLUP);
    spi_bus_config_t bus = {};
    bus.mosi_io_num = p.mosi; bus.miso_io_num = miso; bus.sclk_io_num = p.sclk;
    bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = 4096;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t dev = {};
    dev.clock_speed_hz = hz; dev.spics_io_num = -1; dev.queue_size = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &spi_));
}
void EpdBus::reset(uint16_t extra) {
    digitalWrite(pins_.rst, HIGH); delay(10);
    digitalWrite(pins_.rst, LOW); delay(10);
    digitalWrite(pins_.rst, HIGH); delay(10 + extra);
}
void EpdBus::write(const uint8_t* p, size_t len) {
    // Copy PSRAM/unaligned input to a DMA-compatible internal scratch buffer.
    alignas(4) uint8_t scratch[512];
    while (len && healthy_) {
        size_t n = std::min(len, sizeof(scratch));
        memcpy(scratch, p, n);
        spi_transaction_t t = {}; t.length = n * 8; t.tx_buffer = scratch;
        if (spi_device_polling_transmit(spi_, &t) != ESP_OK) healthy_ = false;
        p += n; len -= n;
    }
}
void EpdBus::beginTxn() { spi_device_acquire_bus(spi_, portMAX_DELAY); digitalWrite(pins_.cs, LOW); }
void EpdBus::endTxn() { digitalWrite(pins_.cs, HIGH); spi_device_release_bus(spi_); }
void EpdBus::rawCmd(uint8_t c) { digitalWrite(pins_.dc, LOW); write(&c, 1); digitalWrite(pins_.dc, HIGH); }
void EpdBus::rawData(uint8_t d) { digitalWrite(pins_.dc, HIGH); write(&d, 1); }
void EpdBus::rawWriteBytes(const uint8_t* d, uint16_t n) { digitalWrite(pins_.dc, HIGH); write(d, n); }
void EpdBus::cmd(uint8_t c) { beginTxn(); rawCmd(c); endTxn(); }
void EpdBus::data(uint8_t d) { beginTxn(); rawData(d); endTxn(); }
void EpdBus::data(const uint8_t* d, uint16_t n) { beginTxn(); rawWriteBytes(d, n); endTxn(); }
void EpdBus::cmdData(uint8_t c, const uint8_t* d, uint16_t n) { beginTxn(); rawCmd(c); if (n) rawWriteBytes(d, n); endTxn(); }
void EpdBus::cmdData2(uint8_t c, uint8_t a, uint8_t b) { uint8_t d[] = {a,b}; cmdData(c,d,2); }
bool EpdBus::isBusy() const { return digitalRead(pins_.busy) == (polarity_ == BusyPolarity::ActiveHigh ? HIGH : LOW); }
void EpdBus::waitBusy(const char* tag) { waitBusy(polarity_,tag); }
void EpdBus::waitBusy(BusyPolarity p, const char* tag) {
    delay(1);
    unsigned long start = millis();
    int active = p == BusyPolarity::ActiveHigh ? HIGH : LOW;
    while (healthy_ && digitalRead(pins_.busy) == active) {
        if (millis() - start > 30000) {
            // Stop all later bus writes after timeout; never issue commands mid-waveform.
            healthy_ = false;
            ESP_LOGE("pocket.epd", "panel timeout: %s", tag ? tag : "wait");
            break;
        }
        delay(1);
    }
}
void EpdBus::waitRefreshComplete(const char* tag) {
    if (polarity_ == BusyPolarity::ActiveHigh) {
        unsigned long start = millis();
        while (!isBusy() && millis()-start < 20) delayMicroseconds(200);
    }
    waitBusy(tag);
}
void EpdBus::sendPlaneFlipped(uint8_t cmd_, const uint8_t* plane, uint16_t h, uint16_t wb) {
    cmd(cmd_); beginTxn();
    for (int y=h-1; y>=0; --y) rawWriteBytes(plane+y*wb,wb);
    endTxn();
}
void EpdBus::sendPlaneFlippedInverted(uint8_t cmd_, const uint8_t* plane, uint16_t h, uint16_t wb) {
    uint8_t row[128]; cmd(cmd_); beginTxn();
    for (int y=h-1; y>=0; --y) for (uint16_t x=0; x<wb; x+=sizeof(row)) {
        uint16_t n = std::min<uint16_t>(wb-x,sizeof(row));
        for (uint16_t i=0;i<n;++i) row[i]=~plane[y*wb+x+i];
        rawWriteBytes(row,n);
    }
    endTxn();
}
void EpdBus::fillPlane(uint8_t cmd_, uint8_t value, uint16_t h, uint16_t wb) {
    uint8_t row[128]; memset(row,value,sizeof(row)); cmd(cmd_); beginTxn();
    for (uint16_t y=0;y<h;++y) for (uint16_t x=0;x<wb;x+=sizeof(row)) rawWriteBytes(row,std::min<uint16_t>(wb-x,sizeof(row)));
    endTxn();
}
}
