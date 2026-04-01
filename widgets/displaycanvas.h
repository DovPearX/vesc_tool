#ifndef DISPLAYCANVAS_H
#define DISPLAYCANVAS_H

#include <QWidget>
#include <QImage>
#include <cstdint>

class DisplayCanvas : public QWidget
{
    Q_OBJECT

public:
    enum ColorFormat {
        Indexed2 = 1,
        Indexed4 = 2,
        Indexed16 = 4,
        RGB332 = 8,
        RGB565 = 16,
        RGB888 = 24
    };

    explicit DisplayCanvas(int width = 320, int height = 240, QWidget *parent = nullptr);
    ~DisplayCanvas();

    int width() const { return mWidth; }
    int height() const { return mHeight; }
    
    void setImageData(const uint8_t *data, int width, int height, ColorFormat format);
    void setPalette(const uint32_t *palette, int paletteSize);
    void clearScreen();
    
    static DisplayCanvas *instance() { return s_instance; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static DisplayCanvas *s_instance;
    
    int mWidth;
    int mHeight;
    ColorFormat mFormat;
    QImage mBuffer;
    QVector<uint32_t> mPalette;
    QVector<uint8_t> mRawData;

    QImage convertToRGB32();
    uint32_t convertColor(uint32_t color);
};

#endif // DISPLAYCANVAS_H
