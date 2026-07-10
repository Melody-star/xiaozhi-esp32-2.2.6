#ifndef WEB_OTA_SERVER_H
#define WEB_OTA_SERVER_H

#include <esp_http_server.h>
#include <string>
#include <functional>

class WebOtaServer {
public:
    WebOtaServer();
    ~WebOtaServer();

    bool Start(int port = 80);
    void Stop();
    bool IsRunning() const { return server_handle_ != nullptr; }

private:
    httpd_handle_t server_handle_;

    static esp_err_t index_handler(httpd_req_t *req);
    static esp_err_t upload_handler(httpd_req_t *req);
    static esp_err_t progress_handler(httpd_req_t *req);
    static esp_err_t ota_status_handler(httpd_req_t *req);
    static esp_err_t logs_handler(httpd_req_t *req);
    static esp_err_t log_page_handler(httpd_req_t *req);

    static std::string GetHtmlPage();
    static WebOtaServer* instance_;
};

#endif