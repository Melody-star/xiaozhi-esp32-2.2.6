#include "web_ota_server.h"
#include "system_info.h"
#include "board.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_http_server.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_heap_caps.h>
#include <cJSON.h>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "WebOta"

// --- Log buffer for web log viewer ---
#define LOG_BUFFER_CAPACITY 256
#define LOG_LINE_MAX 384

static std::mutex s_log_mutex;
static char s_log_buffer[LOG_BUFFER_CAPACITY][LOG_LINE_MAX];
static size_t s_log_write_idx = 0;
static bool s_log_hook_installed = false;
static vprintf_like_t s_orig_vprintf = nullptr;

static int log_hook(const char *fmt, va_list args) {
    char buf[LOG_LINE_MAX];
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    if (len > 0) {
        if (buf[len - 1] == '\n') buf[--len] = '\0';
        std::lock_guard<std::mutex> lock(s_log_mutex);
        size_t idx = s_log_write_idx % LOG_BUFFER_CAPACITY;
        size_t copy_len = std::min((size_t)len, sizeof(s_log_buffer[idx]) - 1);
        memcpy(s_log_buffer[idx], buf, copy_len);
        s_log_buffer[idx][copy_len] = '\0';
        s_log_write_idx++;
    }
    return s_orig_vprintf ? s_orig_vprintf(fmt, args) : len;
}

static void ensure_log_hook() {
    if (!s_log_hook_installed) {
        s_orig_vprintf = esp_log_set_vprintf(log_hook);
        s_log_hook_installed = true;
    }
}

WebOtaServer* WebOtaServer::instance_ = nullptr;

static std::mutex s_ota_mutex;
static bool s_ota_in_progress = false;
static int s_ota_progress = 0;
static size_t s_ota_speed = 0;
static std::string s_ota_status = "idle";
static std::string s_ota_error;

WebOtaServer::WebOtaServer() : server_handle_(nullptr) {
    instance_ = this;
}

WebOtaServer::~WebOtaServer() {
    Stop();
    instance_ = nullptr;
}

std::string WebOtaServer::GetHtmlPage() {
    return R"(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>小智固件升级</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: -apple-system, sans-serif; background: #f5f5f5; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 16px; }
  .card { background: #fff; border-radius: 16px; padding: 32px; box-shadow: 0 4px 24px rgba(0,0,0,0.1); width: 100%; max-width: 480px; }
  h1 { font-size: 24px; text-align: center; margin-bottom: 8px; color: #333; }
  .subtitle { text-align: center; color: #888; font-size: 14px; margin-bottom: 24px; }
  .drop-zone { border: 2px dashed #ccc; border-radius: 12px; padding: 40px; text-align: center; cursor: pointer; transition: all 0.3s; }
  .drop-zone:hover, .drop-zone.drag-over { border-color: #2196F3; background: #f0f8ff; }
  .drop-zone.has-file { border-color: #4CAF50; background: #f1f8e9; }
  .drop-zone .icon { font-size: 48px; margin-bottom: 12px; }
  .drop-zone .text { color: #666; font-size: 16px; }
  .drop-zone .hint { color: #999; font-size: 13px; margin-top: 8px; }
  .file-info { display: none; margin-top: 16px; padding: 12px; background: #f9f9f9; border-radius: 8px; font-size: 14px; color: #333; }
  .file-info.show { display: block; }
  .progress-container { display: none; margin-top: 20px; }
  .progress-container.show { display: block; }
  .progress-bar { width: 100%; height: 24px; background: #e0e0e0; border-radius: 12px; overflow: hidden; }
  .progress-fill { height: 100%; background: linear-gradient(90deg, #2196F3, #4CAF50); border-radius: 12px; transition: width 0.3s; width: 0%; }
  .progress-text { text-align: center; margin-top: 8px; font-size: 14px; color: #666; }
  .status { display: none; margin-top: 16px; padding: 12px; border-radius: 8px; text-align: center; font-size: 14px; }
  .status.error { display: block; background: #ffebee; color: #c62828; }
  .status.success { display: block; background: #e8f5e9; color: #2e7d32; }
  .btn { display: block; width: 100%; margin-top: 20px; padding: 14px; border: none; border-radius: 10px; font-size: 16px; cursor: pointer; transition: opacity 0.3s; }
  .btn:disabled { opacity: 0.5; cursor: not-allowed; }
  .btn-primary { background: #2196F3; color: #fff; }
  .btn-primary:hover:not(:disabled) { background: #1976D2; }
  .btn-secondary { background: #e0e0e0; color: #333; }
  .btn-secondary:hover:not(:disabled) { background: #bdbdbd; }
</style>
</head>
<body>
<div class="card">
  <h1>固件升级</h1>
  <p class="subtitle" id="deviceInfo">正在获取设备信息...</p>

  <div class="drop-zone" id="dropZone">
    <div class="icon">&#128190;</div>
    <div class="text">点击或拖拽固件文件到此处</div>
    <div class="hint">支持 .bin 格式的固件文件</div>
  </div>

  <div class="file-info" id="fileInfo">
    <div><strong>文件名：</strong><span id="fileName"></span></div>
    <div><strong>大小：</strong><span id="fileSize"></span></div>
  </div>

  <div class="progress-container" id="progressContainer">
    <div class="progress-bar">
      <div class="progress-fill" id="progressFill"></div>
    </div>
    <div class="progress-text" id="progressText">0%</div>
  </div>

  <div class="status" id="status"></div>

  <button class="btn btn-primary" id="uploadBtn" disabled>开始升级</button>
  <a href="/logs" style="display:block;text-align:center;margin-top:12px;color:#888;font-size:13px;text-decoration:none;">📋 查看实时日志</a>
</div>

<script>
const dropZone = document.getElementById('dropZone');
const fileInput = document.createElement('input');
fileInput.type = 'file';
fileInput.accept = '.bin,.elf';

let selectedFile = null;
let pollInterval = null;
let uploadCompleted = false;

fetch('/api/status')
  .then(r => r.json())
  .then(data => {
    document.getElementById('deviceInfo').textContent =
      '当前版本: ' + (data.version || 'unknown') + ' | 设备: ' + (data.device_id || '-');
  })
  .catch(() => {
    document.getElementById('deviceInfo').textContent = '设备信息获取失败';
  });

dropZone.addEventListener('click', () => fileInput.click());
dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('drag-over'); });
dropZone.addEventListener('dragleave', () => { dropZone.classList.remove('drag-over'); });
dropZone.addEventListener('drop', (e) => {
  e.preventDefault();
  dropZone.classList.remove('drag-over');
  if (e.dataTransfer.files.length > 0) {
    handleFile(e.dataTransfer.files[0]);
  }
});

fileInput.addEventListener('change', () => {
  if (fileInput.files.length > 0) {
    handleFile(fileInput.files[0]);
  }
});

function handleFile(file) {
  selectedFile = file;
  document.getElementById('fileName').textContent = file.name;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  let size = file.size;
  let unit = 0;
  while (size >= 1024 && unit < sizes.length - 1) { size /= 1024; unit++; }
  document.getElementById('fileSize').textContent = size.toFixed(1) + ' ' + sizes[unit];
  document.getElementById('fileInfo').classList.add('show');
  dropZone.classList.add('has-file');
  document.getElementById('uploadBtn').disabled = false;
  document.getElementById('status').className = 'status';
  document.getElementById('status').textContent = '';
}

document.getElementById('uploadBtn').addEventListener('click', () => {
  if (!selectedFile) return;

  const btn = document.getElementById('uploadBtn');
  btn.disabled = true;
  btn.textContent = '正在上传...';
  document.getElementById('status').className = 'status';
  document.getElementById('progressContainer').classList.add('show');

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/upload', true);
  xhr.setRequestHeader('Content-Type', 'application/octet-stream');

  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round(e.loaded * 100 / e.total);
      document.getElementById('progressFill').style.width = pct + '%';
      document.getElementById('progressText').textContent = '上传中: ' + pct + '%';
    }
  };

  xhr.onload = function() {
    if (xhr.status === 200) {
      uploadCompleted = true;
      document.getElementById('progressText').textContent = '上传完成，正在刷写固件...';
      pollInterval = setInterval(pollProgress, 500);
      // 兜底：10秒后无论轮询结果如何都刷新页面
      setTimeout(() => { location.reload(); }, 10000);
    } else {
      let msg = '上传失败';
      try { const r = JSON.parse(xhr.responseText); msg = r.error || msg; } catch(e) {}
      showStatus(msg, 'error');
      resetUI();
    }
  };

  xhr.onerror = () => {
    showStatus('网络错误，上传失败', 'error');
    resetUI();
  };

  xhr.send(selectedFile);
});

function pollProgress() {
  fetch('/api/ota_progress')
    .then(r => r.json())
    .then(data => {
      if (data.state === 'flashing') {
        document.getElementById('progressFill').style.width = data.progress + '%';
        const speed = (data.speed / 1024).toFixed(1);
        document.getElementById('progressText').textContent =
          '刷写中: ' + data.progress + '% (' + speed + ' KB/s)';
      } else if (data.state === 'success') {
        clearInterval(pollInterval);
        document.getElementById('progressFill').style.width = '100%';
        document.getElementById('progressText').textContent = '升级成功，设备正在重启...';
        showStatus('升级成功！设备即将重启。', 'success');
        document.getElementById('uploadBtn').textContent = '升级成功';
        setTimeout(() => { location.reload(); }, 3000);
      } else if (data.state === 'failed') {
        clearInterval(pollInterval);
        showStatus('升级失败: ' + (data.error || '未知错误'), 'error');
        resetUI();
      } else if (data.state === 'idle' && uploadCompleted) {
        // 设备已重启，新固件服务器已上线
        clearInterval(pollInterval);
        document.getElementById('progressFill').style.width = '100%';
        document.getElementById('progressText').textContent = '升级成功';
        showStatus('升级成功！', 'success');
        setTimeout(() => { location.reload(); }, 2000);
      }
    })
    .catch(() => {});
}

function showStatus(msg, type) {
  const el = document.getElementById('status');
  el.className = 'status ' + type;
  el.textContent = msg;
}

function resetUI() {
  document.getElementById('uploadBtn').disabled = false;
  document.getElementById('uploadBtn').textContent = '开始升级';
}
</script>
</body>
</html>
)";
}

esp_err_t WebOtaServer::index_handler(httpd_req_t *req) {
    std::string html = GetHtmlPage();
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebOtaServer::log_page_handler(httpd_req_t *req) {
    const char* html = R"(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>实时日志</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: #1e1e1e; color: #d4d4d4; font-family: 'Consolas','Courier New',monospace; font-size: 13px; }
  #toolbar { position: sticky; top: 0; background: #252526; padding: 8px 12px; border-bottom: 1px solid #333; display: flex; align-items: center; gap: 10px; z-index: 10; }
  #toolbar h1 { font-size: 14px; font-weight: 600; color: #ccc; }
  #toolbar .badge { font-size: 11px; color: #888; }
  #toolbar button { background: #0e639c; color: #fff; border: none; padding: 4px 12px; border-radius: 3px; cursor: pointer; font-size: 12px; }
  #toolbar button:hover { background: #1177bb; }
  #toolbar .controls { margin-left: auto; display: flex; gap: 6px; align-items: center; }
  #log { padding: 8px 12px; white-space: pre-wrap; word-break: break-all; line-height: 1.5; height: calc(100vh - 45px); overflow-y: auto; }
  .line { padding: 1px 0; }
  .line:hover { background: #2a2d2e; }
  .I { color: #6a9955; }
  .W { color: #dcdcaa; }
  .E { color: #f48771; font-weight: bold; background: rgba(244,135,113,0.08); }
</style>
</head>
<body>
<div id="toolbar">
  <h1>📋 实时日志</h1>
  <span class="badge" id="status">连接中...</span>
  <div class="controls">
    <label><input type="checkbox" id="autoScroll" checked> 自动滚动</label>
    <button id="clearBtn">清空</button>
  </div>
</div>
<div id="log"></div>
<script>
const logEl = document.getElementById('log');
const statusEl = document.getElementById('status');
const autoScrollChk = document.getElementById('autoScroll');
let since = 0;
let polling = true;

async function poll() {
  while (polling) {
    try {
      const r = await fetch('/api/logs?since=' + since);
      const data = await r.json();
      if (data.lines && data.lines.length > 0) {
        for (const line of data.lines) {
          const div = document.createElement('div');
          div.className = 'line ' + (line.charAt(0) === 'E' ? 'E' : line.charAt(0) === 'W' ? 'W' : 'I');
          div.textContent = line;
          logEl.appendChild(div);
        }
        since = data.since;
        if (autoScrollChk.checked) logEl.scrollTop = logEl.scrollHeight;
      }
      statusEl.textContent = '已连接 (' + logEl.children.length + ' 条)';
    } catch(e) {
      statusEl.textContent = '连接断开';
      await new Promise(r => setTimeout(r, 2000));
    }
    await new Promise(r => setTimeout(r, 300));
  }
}
poll();

document.getElementById('clearBtn').onclick = () => {
  logEl.innerHTML = '';
  since = 0;
};
</script>
</body>
</html>
)";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

esp_err_t WebOtaServer::ota_status_handler(httpd_req_t *req) {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", app_desc->version);
    cJSON_AddStringToObject(root, "device_id", SystemInfo::GetMacAddress().c_str());
    cJSON_AddStringToObject(root, "board_type", board.GetBoardType().c_str());
    cJSON_AddNumberToObject(root, "heap_free", esp_get_free_heap_size());

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t WebOtaServer::progress_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();

    std::lock_guard<std::mutex> lock(s_ota_mutex);
    if (s_ota_in_progress) {
        cJSON_AddStringToObject(root, "state", "flashing");
        cJSON_AddNumberToObject(root, "progress", s_ota_progress);
        cJSON_AddNumberToObject(root, "speed", s_ota_speed);
    } else if (s_ota_status == "success") {
        cJSON_AddStringToObject(root, "state", "success");
        cJSON_AddNumberToObject(root, "progress", 100);
    } else if (s_ota_status == "failed") {
        cJSON_AddStringToObject(root, "state", "failed");
        cJSON_AddStringToObject(root, "error", s_ota_error.c_str());
    } else {
        cJSON_AddStringToObject(root, "state", "idle");
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t WebOtaServer::logs_handler(httpd_req_t *req) {
    char query[128];
    size_t since = 0;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[32];
        if (httpd_query_key_value(query, "since", val, sizeof(val)) == ESP_OK) {
            since = atoi(val);
        }
    }

    std::lock_guard<std::mutex> lock(s_log_mutex);
    size_t current = s_log_write_idx;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "since", current);

    cJSON *lines = cJSON_AddArrayToObject(root, "lines");

    size_t available = current - since;
    if (available > 100) available = 100;
    size_t start = (since < current && current - since <= LOG_BUFFER_CAPACITY) ? since : (current > available ? current - available : 0);

    for (size_t i = start; i < current; i++) {
        cJSON_AddItemToArray(lines, cJSON_CreateString(s_log_buffer[i % LOG_BUFFER_CAPACITY]));
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t WebOtaServer::upload_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "Method not allowed", 18);
        return ESP_OK;
    }

    {
        std::lock_guard<std::mutex> lock(s_ota_mutex);
        if (s_ota_in_progress) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "OTA already in progress");
            char *json = cJSON_PrintUnformatted(err);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_status(req, "429 Too Many Requests");
            httpd_resp_send(req, json, strlen(json));
            cJSON_free(json);
            cJSON_Delete(err);
            return ESP_OK;
        }
        s_ota_in_progress = true;
        s_ota_progress = 0;
        s_ota_status = "flashing";
    }

    esp_ota_handle_t update_handle = 0;
    auto update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "No OTA partition available", 26);
        std::lock_guard<std::mutex> lock(s_ota_mutex);
        s_ota_in_progress = false;
        s_ota_status = "failed";
        s_ota_error = "No OTA partition available";
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Writing OTA to partition %s at 0x%lx", update_partition->label, update_partition->address);

    bool image_header_checked = false;
    std::vector<char> image_header;
    constexpr size_t PAGE_SIZE = 4096;
    char* buffer = (char*)heap_caps_malloc(PAGE_SIZE, MALLOC_CAP_INTERNAL);
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate OTA buffer");
        httpd_resp_send(req, "Memory allocation failed", 23);
        std::lock_guard<std::mutex> lock(s_ota_mutex);
        s_ota_in_progress = false;
        s_ota_status = "failed";
        s_ota_error = "Memory allocation failed";
        return ESP_OK;
    }

    int buffer_offset = 0;
    size_t total_read = 0, recent_read = 0;
    auto last_calc_time = esp_timer_get_time();

    int content_len = req->content_len;
    if (content_len <= 0) {
        content_len = 32 * 1024 * 1024;
    }

    while (true) {
        int ret = httpd_req_recv(req, buffer + buffer_offset, PAGE_SIZE - buffer_offset);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (ret < 0) {
            ESP_LOGE(TAG, "HTTP recv error: %d", ret);
            if (image_header_checked) {
                esp_ota_abort(update_handle);
            }
            heap_caps_free(buffer);
            std::lock_guard<std::mutex> lock(s_ota_mutex);
            s_ota_in_progress = false;
            s_ota_status = "failed";
            s_ota_error = "HTTP receive error";
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Read error", 10);
            return ESP_OK;
        }
        if (ret == 0) {
            break;
        }

        recent_read += ret;
        total_read += ret;
        buffer_offset += ret;

        auto now = esp_timer_get_time();
        if (now - last_calc_time >= 500000) {
            s_ota_progress = content_len > 0 ? total_read * 100 / content_len : 0;
            uint32_t elapsed_ms = (now - last_calc_time) / 1000;
            s_ota_speed = recent_read * 1000 / (elapsed_ms > 0 ? elapsed_ms : 1);
            ESP_LOGI(TAG, "OTA progress: %d%% (%u/%u)", s_ota_progress, total_read, content_len);
            last_calc_time = now;
            recent_read = 0;
        }

        if (!image_header_checked) {
            image_header.insert(image_header.end(), buffer, buffer + buffer_offset);
            if (image_header.size() >= (int)(sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))) {
                if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle) != ESP_OK) {
                    esp_ota_abort(update_handle);
                    ESP_LOGE(TAG, "esp_ota_begin failed");
                    heap_caps_free(buffer);
                    std::lock_guard<std::mutex> lock(s_ota_mutex);
                    s_ota_in_progress = false;
                    s_ota_status = "failed";
                    s_ota_error = "esp_ota_begin failed";
                    httpd_resp_set_status(req, "500 Internal Server Error");
                    httpd_resp_send(req, "OTA begin failed", 16);
                    return ESP_OK;
                }
                image_header_checked = true;
                image_header.clear();
                image_header.shrink_to_fit();
            }
        }

        if (buffer_offset == PAGE_SIZE) {
            if (image_header_checked) {
                auto err = esp_ota_write(update_handle, buffer, buffer_offset);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
                    esp_ota_abort(update_handle);
                    heap_caps_free(buffer);
                    std::lock_guard<std::mutex> lock(s_ota_mutex);
                    s_ota_in_progress = false;
                    s_ota_status = "failed";
                    s_ota_error = "esp_ota_write failed";
                    httpd_resp_set_status(req, "500 Internal Server Error");
                    httpd_resp_send(req, "OTA write failed", 16);
                    return ESP_OK;
                }
            }
            buffer_offset = 0;
        }
    }

    // Write any remaining data after the loop ends (ret == 0)
    if (buffer_offset > 0 && image_header_checked) {
        auto err = esp_ota_write(update_handle, buffer, buffer_offset);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            heap_caps_free(buffer);
            std::lock_guard<std::mutex> lock(s_ota_mutex);
            s_ota_in_progress = false;
            s_ota_status = "failed";
            s_ota_error = "esp_ota_write failed";
            httpd_resp_send(req, "OTA write failed", 16);
            return ESP_OK;
        }
        buffer_offset = 0;
    }

    heap_caps_free(buffer);

    if (!image_header_checked) {
        ESP_LOGE(TAG, "No valid firmware image header received");
        std::lock_guard<std::mutex> lock(s_ota_mutex);
        s_ota_in_progress = false;
        s_ota_status = "failed";
        s_ota_error = "Not a valid firmware image";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Not a valid firmware image", 25);
        return ESP_OK;
    }

    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        std::lock_guard<std::mutex> lock(s_ota_mutex);
        s_ota_in_progress = false;
        s_ota_status = "failed";
        s_ota_error = "esp_ota_end failed";
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "OTA validation failed", 20);
        return ESP_OK;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        std::lock_guard<std::mutex> lock(s_ota_mutex);
        s_ota_in_progress = false;
        s_ota_status = "failed";
        s_ota_error = "esp_ota_set_boot_partition failed";
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Failed to set boot partition", 27);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA firmware upgrade successful!");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "success");
    cJSON_AddStringToObject(resp, "message", "Firmware upgrade successful, rebooting...");
    char *json = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    cJSON_free(json);
    cJSON_Delete(resp);

    {
        std::lock_guard<std::mutex> lock(s_ota_mutex);
        s_ota_status = "success";
        s_ota_in_progress = false;
    }

    esp_timer_create_args_t timer_args = {
        .callback = [](void*) { esp_restart(); },
        .name = "ota_reboot"
    };
    esp_timer_handle_t reboot_timer;
    if (esp_timer_create(&timer_args, &reboot_timer) == ESP_OK) {
        esp_timer_start_once(reboot_timer, 3000000);
    }

    return ESP_OK;
}

bool WebOtaServer::Start(int port) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = nullptr
    };

    httpd_uri_t upload_uri = {
        .uri = "/api/upload",
        .method = HTTP_POST,
        .handler = upload_handler,
        .user_ctx = nullptr
    };

    httpd_uri_t progress_uri = {
        .uri = "/api/ota_progress",
        .method = HTTP_GET,
        .handler = progress_handler,
        .user_ctx = nullptr
    };

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = ota_status_handler,
        .user_ctx = nullptr
    };

    httpd_uri_t logs_api_uri = {
        .uri = "/api/logs",
        .method = HTTP_GET,
        .handler = logs_handler,
        .user_ctx = nullptr
    };

    httpd_uri_t log_page_uri = {
        .uri = "/logs",
        .method = HTTP_GET,
        .handler = log_page_handler,
        .user_ctx = nullptr
    };

    ensure_log_hook();

    if (httpd_start(&server_handle_, &config) == ESP_OK) {
        httpd_register_uri_handler(server_handle_, &index_uri);
        httpd_register_uri_handler(server_handle_, &upload_uri);
        httpd_register_uri_handler(server_handle_, &progress_uri);
        httpd_register_uri_handler(server_handle_, &status_uri);
        httpd_register_uri_handler(server_handle_, &logs_api_uri);
        httpd_register_uri_handler(server_handle_, &log_page_uri);
        ESP_LOGI(TAG, "Web OTA server started on port %d", port);
        return true;
    }

    ESP_LOGE(TAG, "Failed to start Web OTA server");
    return false;
}

void WebOtaServer::Stop() {
    if (server_handle_) {
        httpd_stop(server_handle_);
        server_handle_ = nullptr;
        ESP_LOGI(TAG, "Web OTA server stopped");
    }
}