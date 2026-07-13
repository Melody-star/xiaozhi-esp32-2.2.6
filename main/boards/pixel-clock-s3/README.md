# Pixel Clock S3 (像素时钟)

一款基于 ESP32-S3 的 AI 像素时钟。

## 硬件

- **主控**: ESP32-S3
- **麦克风**: ICS-43434 (I2S, WS=13, SCK=12, SD=14)
- **扬声器**: MAX98357 (I2S, BCLK=39, LRC=40, DIN=38)
- **温湿度**: AHT20 (I2C, SDA=21, SCL=47)
- **环境光**: GL5528 (ADC, GPIO2)
- **按钮**: GPIO1

## 烧录

```bash
idf.py set-target esp32s3
idf.py menuconfig  # 选择 Board Type -> Pixel Clock S3
idf.py build flash monitor
```

## 屏幕

屏幕由 4 个 8×8 模块水平拼接组成（总尺寸 32×8）。

每个模块内部均采用按行连续编号（Row-major）：

1  2  3  4  5  6  7  8
9 10 11 12 13 14 15 16
17 ...
...
57 58 59 60 61 62 63 64

模块之间按从左到右依次拼接，组成一个逻辑上的 32×8 点阵。
