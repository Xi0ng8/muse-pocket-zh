#pragma once
#include <cstdint>
// Xteink X4 Pro pinout confirmed by FreeInk; see UPSTREAM.md.
namespace BoardConfig {
enum class Board { XteinkX4, XteinkX4Pro, Sticky, WsEpaper397 };
enum class DisplayController { SSD1677, UC8179, UC8279 };
struct ActiveProfile {
    uint16_t displayWidth = 800, displayHeight = 480;
    uint32_t displaySpiHz = 10000000;
    struct { bool mirrorX = false, mirrorY = false; } orientation;
    struct { int8_t sclk = 12, mosi = 11, cs = 13, dc = 18, rst = 14, busy = 6; } display;
    Board board = Board::XteinkX4Pro;
    DisplayController displayController = DisplayController::SSD1677;
    uint8_t displayControllerVariant = 0;
};
inline ActiveProfile ACTIVE;
constexpr uint32_t MAX_FRAMEBUFFER_BYTES = 48000;
inline bool isX4Classic() { return false; }
}
