添加web更新固件功能；添加web查看实时日志功能；添加在menuconfig中选择OTA方式选项；添加mcp播报ip地址

- 新增 web_ota_server 支持浏览器上传固件升级（menuconfig OTA_METHOD_WEB_UPLOAD）
- Kconfig 增加 OTA 方式选择：远程 HTTP 自动升级 / Web 页面手动上传
- 新增 self.network.get_ip MCP 工具，支持语音查询设备 IP 地址