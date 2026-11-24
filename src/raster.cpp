#include "raster.h"

namespace Raster {

// Turned by 90° clockwise, pixel (x, y) of the result is source pixel (y, srcH - 1 - x): a row of
// the result is a source column, read from the bottom up. By 270°: source pixel (srcW - 1 - y, x).
bool turnBand(int srcW, int srcH, int degrees, int y0, int rows, ReadFn read, void *context, uint8_t *band) {
   if ((degrees != 90 && degrees != 270) || srcW <= 0 || srcH <= 0 || ((srcW | srcH | y0 | rows) & 1) || y0 < 0 || rows <= 0 ||
       y0 + rows > srcW)
      return false;

   const uint32_t srcRowBytes = (uint32_t)srcW / 2;
   const int dstRowBytes = srcH / 2;
   // the source columns this band is made of, as one piece per source row
   const int firstColumn = (degrees == 90) ? y0 : srcW - y0 - rows;
   uint8_t piece[128];
   for (int done = 0; done < rows; done += (int)sizeof(piece) * 2) { // columns in parts of 256
      const int count = (rows - done < (int)sizeof(piece) * 2) ? rows - done : (int)sizeof(piece) * 2;
      const int column = firstColumn + done;
      for (int r = 0; r < srcH; r++) {
         if (!read(context, (uint32_t)r * srcRowBytes + (uint32_t)column / 2, piece, (uint32_t)count / 2))
            return false;
         for (int k = 0; k < count; k++) {
            const uint8_t value = pixel(piece, k);
            const int c = done + k; // column inside the band's columns
            int x, y;               // in the result, y relative to y0
            if (degrees == 90) {
               x = srcH - 1 - r;
               y = c;
            }
            else {
               x = r;
               y = rows - 1 - c;
            }
            setPixel(band + y * dstRowBytes, x, value);
         }
      }
   }
   return true;
}

} // namespace Raster
