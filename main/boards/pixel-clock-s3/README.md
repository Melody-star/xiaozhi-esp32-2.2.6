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
