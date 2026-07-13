添加web更新固件功能；添加web查看实时日志功能；添加在menuconfig中选择OTA方式选项；添加mcp播报ip地址

- 新增 web_ota_server 支持浏览器上传固件升级（menuconfig OTA_METHOD_WEB_UPLOAD）
- Kconfig 增加 OTA 方式选择：远程 HTTP 自动升级 / Web 页面手动上传
- 新增 self.network.get_ip MCP 工具，支持语音查询设备 IP 地址

根据采集到的亮度自动调整屏幕亮度，亮度的范围为10%-100%，添加语音控制屏幕亮度，并且语音控制是否开启自动亮度


重构 LED 矩阵驱动框架（英文数字展示）；实现分层显示架构与状态机

1. 创建分层 LED 图形框架 (main/led/)
   - strip_driver.h/.cc — LedStripDriver 封装 led_strip，提供 Init/SetPixel/Refresh/Clear
   - frame_buffer.h/.cc — FrameBuffer 颜色缓冲，Clear/SetPixel/GetPixel
   - font.h/.cc — 3×5 比例字库（0-9 / : / A-Z / . / 空格），每字符按列存储，
     列字节 bit0=row0(顶) ~ bit4=row4(底)
   - graphics.h/.cc — Graphics 绘图引擎：DrawPixel/DrawLine(DDA)/DrawRect/FillRect/DrawCircle(中点圆)/FillCircle/DrawBitmap(单色)/DrawChar/DrawString/Clear/Refresh（遍历 FrameBuffer 经 MatrixMapper 写入 LedStripDriver）
   - display_manager.h/.cc — DisplayManager 状态机显示管理器

3. 实现 DisplayManager 状态机 (main/led/display_manager.h/.cc)
   - 四种显示模式：CLOCK / TEMPERATURE / HUMIDITY / DATE
   - 1 秒定时器驱动每秒刷新，非 CLOCK 模式 5 秒自动回时钟
   - DrawClock() — HH:MM:SS + 第 7 行星期指示条（当日红色，其余绿色）
   - DrawTemperature() — "T 25.5"（摄氏度）
   - DrawHumidity() — "HUM 60"（百分比）
   - DrawDate() — "11 JUL"（日+月缩写）
   - NextMode() 通过 Button 回调调用，同时读取 AHT20 传感器更新温湿度

4. 更新 pixel-clock-s3 板级代码
   - 开机显示静态居中 "START..."
   - 按钮单击改为循环切换显示模式 + 实时读取 AHT20 传感器
   - 读取初始温湿度值传入 DisplayManager