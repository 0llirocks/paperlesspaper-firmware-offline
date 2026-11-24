// Unit tests of src/raster.cpp on the PC: pio test -e native
#include <string.h>
#include <unity.h>
#include <vector>

#include "raster.h"

void setUp() {}
void tearDown() {}

struct Source {
   std::vector<uint8_t> bytes;
   int reads = 0;
};

static bool readSource(void *context, uint32_t offset, uint8_t *buf, uint32_t len) {
   Source *s = (Source *)context;
   if (offset + len > s->bytes.size())
      return false;
   memcpy(buf, s->bytes.data() + offset, len);
   s->reads++;
   return true;
}

// a raster in which every pixel has its own value (mod 16) along a pattern that is not symmetric
static Source makeRaster(int w, int h) {
   Source s;
   s.bytes.assign((size_t)w * h / 2, 0);
   for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
         Raster::setPixel(s.bytes.data() + y * w / 2, x, (uint8_t)((x * 3 + y * 7 + x * y) % 16));
   return s;
}

// the turned raster pixel by pixel, as the definition says
static std::vector<uint8_t> turnedNaive(const Source &s, int w, int h, int degrees) {
   std::vector<uint8_t> out((size_t)w * h / 2, 0); // h wide, w high
   for (int y = 0; y < w; y++)
      for (int x = 0; x < h; x++) {
         const int sx = (degrees == 90) ? y : w - 1 - y;
         const int sy = (degrees == 90) ? h - 1 - x : x;
         Raster::setPixel(out.data() + y * h / 2, x, Raster::pixel(s.bytes.data() + sy * w / 2, sx));
      }
   return out;
}

static std::vector<uint8_t> turnedInBands(Source &s, int w, int h, int degrees, int bandRows) {
   std::vector<uint8_t> out((size_t)w * h / 2, 0);
   for (int y0 = 0; y0 < w; y0 += bandRows) {
      const int rows = (w - y0 < bandRows) ? w - y0 : bandRows;
      TEST_ASSERT_TRUE(Raster::turnBand(w, h, degrees, y0, rows, readSource, &s, out.data() + y0 * h / 2));
   }
   return out;
}

static void test_turn_matches_definition() {
   const int sizes[][2] = {{8, 4}, {6, 10}, {800, 12}, {12, 600}};
   const int bands[] = {2, 4, 6, 48, 600, 800};
   for (const auto &size : sizes) {
      for (int degrees : {90, 270}) {
         Source s = makeRaster(size[0], size[1]);
         const std::vector<uint8_t> expected = turnedNaive(s, size[0], size[1], degrees);
         for (int band : bands) {
            if (band > size[0])
               continue;
            const std::vector<uint8_t> got = turnedInBands(s, size[0], size[1], degrees, band);
            TEST_ASSERT_EQUAL_UINT8_ARRAY(expected.data(), got.data(), expected.size());
         }
      }
   }
}

static void test_known_corners() {
   // 4 x 2 raster:  1 2 3 4
   //                5 6 7 8
   Source s;
   s.bytes = {0x12, 0x34, 0x56, 0x78};
   std::vector<uint8_t> out(4, 0);
   TEST_ASSERT_TRUE(Raster::turnBand(4, 2, 90, 0, 4, readSource, &s, out.data()));
   // clockwise: 5 1 / 6 2 / 7 3 / 8 4
   const uint8_t cw[] = {0x51, 0x62, 0x73, 0x84};
   TEST_ASSERT_EQUAL_UINT8_ARRAY(cw, out.data(), 4);
   TEST_ASSERT_TRUE(Raster::turnBand(4, 2, 270, 0, 4, readSource, &s, out.data()));
   // counter-clockwise: 4 8 / 3 7 / 2 6 / 1 5
   const uint8_t ccw[] = {0x48, 0x37, 0x26, 0x15};
   TEST_ASSERT_EQUAL_UINT8_ARRAY(ccw, out.data(), 4);
}

static void test_turn_back_is_the_original() {
   Source s = makeRaster(20, 8);
   std::vector<uint8_t> once = turnedInBands(s, 20, 8, 90, 4);
   Source turned;
   turned.bytes = once;
   std::vector<uint8_t> back = turnedInBands(turned, 8, 20, 270, 2);
   TEST_ASSERT_EQUAL_UINT8_ARRAY(s.bytes.data(), back.data(), s.bytes.size());
}

static void test_reads_per_band() {
   // one read per source row and band (bands up to 256 columns)
   Source s = makeRaster(480, 800);
   std::vector<uint8_t> band(160 * 800 / 2);
   TEST_ASSERT_TRUE(Raster::turnBand(480, 800, 270, 0, 160, readSource, &s, band.data()));
   TEST_ASSERT_EQUAL(800, s.reads);
}

static void test_refuses_bad_input() {
   Source s = makeRaster(8, 4);
   std::vector<uint8_t> band(16);
   TEST_ASSERT_FALSE(Raster::turnBand(8, 4, 180, 0, 2, readSource, &s, band.data()));
   TEST_ASSERT_FALSE(Raster::turnBand(8, 4, 90, 1, 2, readSource, &s, band.data()));  // odd row
   TEST_ASSERT_FALSE(Raster::turnBand(8, 4, 90, 6, 4, readSource, &s, band.data()));  // past the end
   TEST_ASSERT_FALSE(Raster::turnBand(10, 4, 90, 8, 2, readSource, &s, band.data())); // source too short
}

int main(int, char **) {
   UNITY_BEGIN();
   RUN_TEST(test_known_corners);
   RUN_TEST(test_turn_matches_definition);
   RUN_TEST(test_turn_back_is_the_original);
   RUN_TEST(test_reads_per_band);
   RUN_TEST(test_refuses_bad_input);
   return UNITY_END();
}
