#include "net/WebServer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstring>

namespace {
constexpr const char* TAG = "oil_web";
constexpr uint32_t kChunkRows = 32;  // rows copied per hold of the lock
extern "C" const char kPageStart[] asm("_binary_index_html_start");
extern "C" const char kPageEnd[] asm("_binary_index_html_end");

void noStore(httpd_req_t* req) { httpd_resp_set_hdr(req, "Cache-Control", "no-store"); }
} // namespace

bool WebSource::begin(uint32_t capacity) {
  rows_ = static_cast<gauge::LogRow*>(heap_caps_malloc(capacity * sizeof(gauge::LogRow), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!rows_) {
    ESP_LOGW(TAG, "No PSRAM for the web log: the web page is unavailable");
    return false;
  }
  log_ = gauge::LogBuffer(rows_, capacity);
  return true;
}

void WebSource::append(const gauge::Reading& r, uint32_t uptime_ms) {
  portENTER_CRITICAL(&mux_);
  log_.append(r, uptime_ms);
  portEXIT_CRITICAL(&mux_);
}

void WebSource::setStatus(const gauge::WebStatus& s) {
  portENTER_CRITICAL(&mux_);
  status_ = s;
  portEXIT_CRITICAL(&mux_);
}

void WebSource::head(gauge::WebStatus& s, uint32_t& first, uint32_t& next) {
  portENTER_CRITICAL(&mux_);
  s = status_;
  first = log_.first();
  next = log_.next();
  portEXIT_CRITICAL(&mux_);
}

uint32_t WebSource::copy(uint32_t since, gauge::LogRow* out, uint32_t max) {
  uint32_t n = 0;
  while (n < max) {
    const uint32_t want = max - n < kChunkRows ? max - n : kChunkRows;
    portENTER_CRITICAL(&mux_);
    const uint32_t got = log_.copy(since, out + n, want);
    portEXIT_CRITICAL(&mux_);
    if (got == 0) break;
    since = out[n + got - 1].seq + 1;
    n += got;
  }
  return n;
}

bool WebServer::start(WebSource* source, gauge::Unit unit) {
  if (server_) return true;
  if (!source) return false;
  source_ = source;
  unit_ = unit;
  if (!batch_) {
    batch_ = static_cast<gauge::LogRow*>(
        heap_caps_malloc(gauge::kLogBatchRows * sizeof(gauge::LogRow), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!batch_) { ESP_LOGW(TAG, "No PSRAM for the web server"); return false; }
  }
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.task_priority = tskIDLE_PRIORITY;  // below the main loop: a busy phone can never delay a reading
  cfg.stack_size = 6144;
  cfg.max_open_sockets = 4;
  cfg.lru_purge_enable = true;
  cfg.max_uri_handlers = 4;
  cfg.max_req_hdr_len = 2048;  // a phone browser's headers, with room to spare
  if (httpd_start(&server_, &cfg) != ESP_OK) {
    ESP_LOGW(TAG, "Web server didn't start");
    server_ = nullptr;
    return false;
  }
  const httpd_uri_t page = {"/", HTTP_GET, &WebServer::onPage, this};
  const httpd_uri_t scale = {"/scale", HTTP_GET, &WebServer::onScale, this};
  const httpd_uri_t log = {"/log", HTTP_GET, &WebServer::onLog, this};
  if (httpd_register_uri_handler(server_, &page) != ESP_OK || httpd_register_uri_handler(server_, &scale) != ESP_OK ||
      httpd_register_uri_handler(server_, &log) != ESP_OK) {
    ESP_LOGW(TAG, "Web pages not registered");
    stop();
    return false;
  }
  ESP_LOGI(TAG, "Web page up on port 80");
  return true;
}

void WebServer::stop() {
  if (server_ && httpd_stop(server_) != ESP_OK) ESP_LOGW(TAG, "Web server didn't stop cleanly");
  server_ = nullptr;
}

esp_err_t WebServer::onPage(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  noStore(req);
  return httpd_resp_send(req, kPageStart, kPageEnd - kPageStart - 1);  // EMBED_TXTFILES adds a NUL
}

esp_err_t WebServer::onScale(httpd_req_t* req) {
  const WebServer* self = static_cast<const WebServer*>(req->user_ctx);
  char body[2048];
  const size_t n = gauge::webScale(self->unit_, body, sizeof(body));
  if (n == 0) return httpd_resp_send_500(req);
  httpd_resp_set_type(req, "application/json");
  noStore(req);
  return httpd_resp_send(req, body, n);
}

esp_err_t WebServer::onLog(httpd_req_t* req) {
  const WebServer* self = static_cast<const WebServer*>(req->user_ctx);
  char query[64];
  const uint32_t since =
      httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK ? gauge::parseSince(query) : 0;
  gauge::WebStatus status;
  uint32_t first = 0, next = 0;
  self->source_->head(status, first, next);
  const uint32_t n = self->source_->copy(since, self->batch_, gauge::kLogBatchRows);
  httpd_resp_set_type(req, "application/json");
  noStore(req);
  char out[1024];
  size_t used = gauge::webLogHead(status, first, next, out, sizeof(out));
  if (used == 0) return httpd_resp_send_500(req);
  for (uint32_t i = 0; i < n; ++i) {
    size_t k = gauge::webLogRow(self->batch_[i], i > 0, out + used, sizeof(out) - used);
    if (k == 0) {  // full: send what's there and start the next chunk
      if (httpd_resp_send_chunk(req, out, used) != ESP_OK) return ESP_FAIL;
      used = 0;
      k = gauge::webLogRow(self->batch_[i], i > 0, out, sizeof(out));
      if (k == 0) return ESP_FAIL;
    }
    used += k;
  }
  if (used + 2 >= sizeof(out)) {
    if (httpd_resp_send_chunk(req, out, used) != ESP_OK) return ESP_FAIL;
    used = 0;
  }
  std::memcpy(out + used, "]}", 2);
  used += 2;
  if (httpd_resp_send_chunk(req, out, used) != ESP_OK) return ESP_FAIL;
  return httpd_resp_send_chunk(req, nullptr, 0);
}
