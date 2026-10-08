// Muse Pocket: allocation-free UTF-8 and pixel layout, also usable on a host.
#pragma once
#include <cstddef>
#include <cstdint>

namespace pocket_text {
struct Decoded { uint32_t codepoint; size_t bytes; bool valid; };
struct Line { size_t begin; size_t end; };
Decoded decode(const char* text, size_t length);
bool wide(uint32_t codepoint);
int advance(uint32_t codepoint);
int width(const char* text, size_t length);
// Copies complete Unicode scalars, replacing malformed sequences with '?'.
size_t truncate(char* destination, size_t capacity, const char* text, size_t length);
// Width is in unscaled font pixels. Returned spans never split UTF-8 scalars.
size_t wrap(const char* text, size_t length, int max_width, Line* lines, size_t max_lines);
}
