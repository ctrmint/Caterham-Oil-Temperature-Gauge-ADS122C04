#pragma once
#include "core/DisplayText.h"
#include "core/WebLog.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>

// What the web page (#32) reads, shared between the main loop (which writes it) and the web server (which reads it). The
// main loop's writes are one row a second and a small status copy, each under a short spinlock: it never waits on the
// server. The server copies rows out in small chunks, so the lock is never held for long.
class WebSource {
 public:
  bool begin(uint32_t capacity);                          // the log's ring, in PSRAM; false if it can't be had
  void append(const gauge::Reading& r, uint32_t uptime_ms);  // main loop, once a second
  void setStatus(const gauge::WebStatus& s);               // main loop
  void head(gauge::WebStatus& s, uint32_t& first, uint32_t& next);  // server
  uint32_t copy(uint32_t since, gauge::LogRow* out, uint32_t max);   // server
 private:
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  gauge::LogRow* rows_ = nullptr;
  gauge::LogBuffer log_{nullptr, 0};
  gauge::WebStatus status_ = {};
};

// The read-only web server: GET / (the page, built into the firmware), GET /scale and GET /log?since=N. Anything else is
// 404. It runs in its own task at the lowest priority, below the main loop.
class WebServer {
 public:
  bool start(WebSource* source, gauge::Unit unit);
  void stop();
  bool running() const { return server_ != nullptr; }
 private:
  static esp_err_t onPage(httpd_req_t* req);
  static esp_err_t onScale(httpd_req_t* req);
  static esp_err_t onLog(httpd_req_t* req);
  httpd_handle_t server_ = nullptr;
  WebSource* source_ = nullptr;
  gauge::Unit unit_ = gauge::Unit::Celsius;
  gauge::LogRow* batch_ = nullptr;  // kLogBatchRows rows, in PSRAM, made once
};
