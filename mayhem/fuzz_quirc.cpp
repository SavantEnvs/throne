// QR-code decoding as the app's QrDecoder (quirc) does it: input is [width][height] followed by
// that many 8-bit grayscale pixels (zero-padded when short).
#include <QImage>
#include <QString>
#include <QVector>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "3rdparty/QrDecoder.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 3) return -1;
    const int width = data[0];
    const int height = data[1];
    if (width == 0 || height == 0) return -1;

    QImage image(width, height, QImage::Format_Grayscale8);
    const uint8_t *pixels = data + 2;
    const size_t avail = size - 2;
    for (int y = 0; y < height; ++y) {
        uchar *line = image.scanLine(y);
        std::memset(line, 0, static_cast<size_t>(width));
        const size_t offset = static_cast<size_t>(y) * width;
        if (offset < avail) std::memcpy(line, pixels + offset, std::min(static_cast<size_t>(width), avail - offset));
    }

    QrDecoder decoder;
    const QVector<QString> codes = decoder.decode(image);
    (void) codes.size();
    return 0;
}
