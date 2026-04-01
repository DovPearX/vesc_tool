#include "displaycanvas.h"
#include <QPainter>
#include <QPaintEvent>
#include <cstring>

DisplayCanvas *DisplayCanvas::s_instance = nullptr;

DisplayCanvas::DisplayCanvas(int width, int height, QWidget *parent)
    : QWidget(parent), mWidth(width), mHeight(height), mFormat(RGB888)
{
    s_instance = this;
    mBuffer = QImage(width, height, QImage::Format_RGB32);
    mBuffer.fill(Qt::black);
    setFixedSize(width, height);
    setStyleSheet("background-color: black;");
    
    mPalette.resize(256);
    for (int i = 0; i < 256; i++) {
        mPalette[i] = qRgb(i, i, i);
    }
}

DisplayCanvas::~DisplayCanvas()
{
}

void DisplayCanvas::setImageData(const uint8_t *data, int width, int height, ColorFormat format)
{
    mWidth = width;
    mHeight = height;
    mFormat = format;
    setFixedSize(width, height);
    
    int dataSize = width * height;
    
    switch (format) {
        case Indexed2:
        case Indexed4:
        case Indexed16:
            dataSize = (width * height * format) / 8;
            break;
        case RGB332:
            dataSize = width * height;
            break;
        case RGB565:
            dataSize = width * height * 2;
            break;
        case RGB888:
            dataSize = width * height * 3;
            break;
    }
    
    mRawData.resize(dataSize);
    std::memcpy(mRawData.data(), data, dataSize);
    
    mBuffer = convertToRGB32();
    update();
}

void DisplayCanvas::setPalette(const uint32_t *palette, int paletteSize)
{
    mPalette.resize(paletteSize);
    std::memcpy(mPalette.data(), palette, paletteSize * sizeof(uint32_t));
}

void DisplayCanvas::clearScreen()
{
    mBuffer.fill(Qt::black);
    update();
}

uint32_t DisplayCanvas::convertColor(uint32_t color)
{
    switch (mFormat) {
        case RGB332: {
            uint8_t r3 = (color >> 5) & 0x7;
            uint8_t g3 = (color >> 2) & 0x7;
            uint8_t b2 = (color >> 0) & 0x3;
            uint8_t r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
            uint8_t g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
            uint8_t b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
            return qRgb(r, g, b);
        }
        case RGB565: {
            uint16_t c = color;
            uint8_t r5 = (c >> 11) & 0x1F;
            uint8_t g6 = (c >> 5) & 0x3F;
            uint8_t b5 = (c >> 0) & 0x1F;
            uint8_t r = (r5 << 3) | (r5 >> 2);
            uint8_t g = (g6 << 2) | (g6 >> 4);
            uint8_t b = (b5 << 3) | (b5 >> 2);
            return qRgb(r, g, b);
        }
        case RGB888: {
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = (color >> 0) & 0xFF;
            return qRgb(r, g, b);
        }
        case Indexed2:
        case Indexed4:
        case Indexed16: {
            if (color < (uint32_t)mPalette.size()) {
                return mPalette[color];
            }
            return qRgb(0, 0, 0);
        }
        default:
            return qRgb(0, 0, 0);
    }
}

QImage DisplayCanvas::convertToRGB32()
{
    QImage img(mWidth, mHeight, QImage::Format_RGB32);
    
    if (mRawData.isEmpty()) {
        img.fill(Qt::black);
        return img;
    }
    
    const uint8_t *data = mRawData.data();
    int idx = 0;
    
    for (int y = 0; y < mHeight; y++) {
        for (int x = 0; x < mWidth; x++) {
            uint32_t color = 0;
            
            switch (mFormat) {
                case Indexed2: {
                    int byteIdx = (y * mWidth + x) / 4;
                    int bitIdx = (y * mWidth + x) % 4;
                    color = (data[byteIdx] >> (bitIdx * 2)) & 0x3;
                    break;
                }
                case Indexed4: {
                    int byteIdx = (y * mWidth + x) / 2;
                    int nibble = (y * mWidth + x) % 2;
                    color = (data[byteIdx] >> (nibble * 4)) & 0xF;
                    break;
                }
                case Indexed16: {
                    int byteIdx = y * mWidth + x;
                    color = data[byteIdx];
                    break;
                }
                case RGB332: {
                    int byteIdx = y * mWidth + x;
                    color = data[byteIdx];
                    break;
                }
                case RGB565: {
                    int byteIdx = (y * mWidth + x) * 2;
                    color = (data[byteIdx] << 8) | data[byteIdx + 1];
                    break;
                }
                case RGB888: {
                    int byteIdx = (y * mWidth + x) * 3;
                    color = (data[byteIdx] << 16) | (data[byteIdx + 1] << 8) | data[byteIdx + 2];
                    break;
                }
            }
            
            img.setPixelColor(x, y, convertColor(color));
        }
    }
    
    return img;
}

void DisplayCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (mBuffer.isNull() || mBuffer.width() <= 0 || mBuffer.height() <= 0) {
        return;
    }

    painter.drawImage(QPoint(0, 0), mBuffer);
}
