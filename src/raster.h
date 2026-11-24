#pragma once
#include <stddef.h>
#include <stdint.h>

// Turning a display raster (4 bit per pixel, row by row, the left pixel of a byte in its upper
// half) by 90° or 270° clockwise, without holding the whole raster in memory.
//
// The turned raster is built in bands of rows. For one band the source is read in short pieces,
// one per source row, so a band of B rows costs (source rows) reads of B / 2 bytes.
// No hardware here: tested on the PC (test/test_raster, `pio test -e native`).
namespace Raster {

// Reads `len` bytes at `offset` of the source raster. false: not readable.
typedef bool (*ReadFn)(void *context, uint32_t offset, uint8_t *buf, uint32_t len);

// Rows y0 .. y0 + rows - 1 of the raster `src` (srcW x srcH pixels) turned by `degrees`
// (90 or 270, clockwise). The turned raster is srcH pixels wide and srcW high; `band` receives
// rows * srcH / 2 bytes. srcW, srcH, y0 and rows must be even.
bool turnBand(int srcW, int srcH, int degrees, int y0, int rows, ReadFn read, void *context, uint8_t *band);

// Nibble access, as the display raster stores it.
inline uint8_t pixel(const uint8_t *row, int x) { return (x & 1) ? (row[x >> 1] & 0x0F) : (row[x >> 1] >> 4); }
inline void setPixel(uint8_t *row, int x, uint8_t value) {
   uint8_t &b = row[x >> 1];
   b = (x & 1) ? (uint8_t)((b & 0xF0) | (value & 0x0F)) : (uint8_t)((b & 0x0F) | (value << 4));
}

} // namespace Raster
