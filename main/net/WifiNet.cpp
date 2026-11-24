#include "net/WifiNet.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace {
constexpr const char* TAG = "oil_wifi";
constexpr uint32_t kTaskStack = 6144;
constexpr UBaseType_t kTaskPriority = tskIDLE_PRIORITY;  // below the main loop (priority 1): it can never delay a reading
constexpr TickType_t kIdleWait = pdMS_TO_TICKS(200);      // check the requested state and the link at least this often
constexpr TickType_t kJoinWait = pdMS_TO_TICKS(15000);    // to connect and get an address
constexpr TickType_t kRejoinWait = pdMS_TO_TICKS(10000);  // after a joined network drops, before looking again
constexpr uint8_t kMaxStations = 4;
constexpr EventBits_t kGotIp = BIT0, kDisconnected = BIT1;
bool ok(esp_err_t err, const char* what) {
  if (err == ESP_OK) return true;
  ESP_LOGW(TAG, "%s failed (%s)", what, esp_err_to_name(err));
  return false;
}
void copyField(uint8_t* field, size_t field_size, const char* text) {  // an SSID or password field, NUL-padded
  const size_t n = std::strlen(text);
  std::memcpy(field, text, n < field_size ? n : field_size);
}
// Internal RAM after the network's start: what's left for everything else. The display no longer needs any per frame.
void logMemory(const char* what) {
  ESP_LOGI(TAG, "%s: internal RAM free %u, largest DMA block %u", what,
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
}
} // namespace

bool WifiNet::begin(const char* ssid, const char* password, const char* group, uint16_t port, WebSource* web,
                    gauge::Unit unit) {
  web_source_ = web;
  unit_ = unit;
  std::snprintf(ssid_, sizeof(ssid_), "%s", ssid ? ssid : "");
  std::snprintf(password_, sizeof(password_), "%s", password ? password : "");
  std::snprintf(group_, sizeof(group_), "%s", group ? group : "");
  port_ = port;
  events_ = xEventGroupCreate();
  if (!events_ || xTaskCreate(&WifiNet::taskMain, "oil_wifi", kTaskStack, this, kTaskPriority, &task_) != pdPASS) {
    ESP_LOGW(TAG, "Wi-Fi task not started: Wi-Fi unavailable");
    task_ = nullptr;
    return false;
  }
  return true;
}

void WifiNet::request(gauge::WifiState s) {
  requested_.store(gauge::encodeWifiState(s));
  if (task_) xTaskNotifyGive(task_);
}

void WifiNet::post(const gauge::UdpFields& f) {
  portENTER_CRITICAL(&mux_);
  mailbox_ = f;
  fresh_ = true;
  portEXIT_CRITICAL(&mux_);
  if (task_) xTaskNotifyGive(task_);
}

void WifiNet::address(char* out, size_t size) {
  portENTER_CRITICAL(&mux_);
  std::snprintf(out, size, "%s", ip_);
  portEXIT_CRITICAL(&mux_);
}

void WifiNet::setAddress(const char* ip) {
  portENTER_CRITICAL(&mux_);
  std::snprintf(ip_, sizeof(ip_), "%s", ip ? ip : "");
  portEXIT_CRITICAL(&mux_);
}

void WifiNet::taskMain(void* arg) { static_cast<WifiNet*>(arg)->run(); }

// In the default event loop's task: just note what happened, for the Wi-Fi task.
void WifiNet::onEvent(void* arg, esp_event_base_t base, int32_t id, void* data) {
  WifiNet* self = static_cast<WifiNet*>(arg);
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    const auto* got = static_cast<const ip_event_got_ip_t*>(data);
    char ip[16];
    std::snprintf(ip, sizeof(ip), IPSTR, IP2STR(&got->ip_info.ip));
    self->setAddress(ip);
    xEventGroupClearBits(self->events_, kDisconnected);
    xEventGroupSetBits(self->events_, kGotIp);
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(self->events_, kGotIp);
    xEventGroupSetBits(self->events_, kDisconnected);
  }
  if (self->task_) xTaskNotifyGive(self->task_);
}

void WifiNet::run() {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, kIdleWait);
    const auto want = static_cast<gauge::WifiState>(requested_.load());
    const bool want_on = want != gauge::WifiState::Off;
    if (want_on && link() == gauge::WifiLink::None) {
      start();
    } else if (!want_on && radio_on_) {
      closeSocket();
      stopRadio();
      phase_.store(static_cast<uint8_t>(gauge::WifiPhase::Off));
    }
    if (link() == gauge::WifiLink::Joined && (xEventGroupGetBits(events_) & kDisconnected)) lostLink();
    if (link() == gauge::WifiLink::None) continue;
    const bool want_udp = gauge::wifiUdp(want);
    if (want_udp && sock_ < 0 && !openSocket()) {
      closeSocket();
      stopRadio();
      fail(gauge::WifiFailure::Start);
      continue;
    }
    if (!want_udp && sock_ >= 0) closeSocket();
    // The web page (#32): its server runs while the link is up and the state asks for it.
    const bool want_web = gauge::wifiWeb(want) && web_source_;
    if (want_web && !web_.running()) {
      if (!web_.start(web_source_, unit_)) {
        closeSocket();
        stopRadio();
        fail(gauge::WifiFailure::Start);
        continue;
      }
      logMemory("Web page up");
    }
    if (!want_web && web_.running()) web_.stop();
    if (sock_ >= 0) sendLatest();
  }
}

bool WifiNet::init() {
  if (ready_) return true;
  if (!ok(esp_netif_init(), "esp_netif_init")) return false;
  const esp_err_t loop = esp_event_loop_create_default();
  if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE && !ok(loop, "esp_event_loop_create_default")) return false;
  if (!sta_netif_) sta_netif_ = esp_netif_create_default_wifi_sta();
  if (!ap_netif_) ap_netif_ = esp_netif_create_default_wifi_ap();
  if (!sta_netif_ || !ap_netif_) { ESP_LOGW(TAG, "Wi-Fi interfaces not created"); return false; }
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (!ok(esp_wifi_init(&cfg), "esp_wifi_init")) return false;
  if (!ok(esp_wifi_set_storage(WIFI_STORAGE_RAM), "esp_wifi_set_storage")) return false;
  if (!ok(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &WifiNet::onEvent, this, nullptr),
          "Wi-Fi event handler") ||
      !ok(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiNet::onEvent, this, nullptr),
          "IP event handler"))
    return false;
  ready_ = true;
  return true;
}

void WifiNet::start() {
  phase_.store(static_cast<uint8_t>(gauge::WifiPhase::Starting));
  failure_.store(static_cast<uint8_t>(gauge::WifiFailure::None));
  if (!init()) { fail(gauge::WifiFailure::Start); return; }
  const bool there = networkThere();
  const gauge::WifiLink want = gauge::wifiLinkFor(there);
  if (want == gauge::WifiLink::Joined ? join() : host()) {
    link_.store(static_cast<uint8_t>(want));
    phase_.store(static_cast<uint8_t>(gauge::WifiPhase::On));
    ESP_LOGI(TAG, "%s %s", want == gauge::WifiLink::Joined ? "Joined" : "Hosting", ssid_);
    logMemory("Wi-Fi up");
  } else {
    stopRadio();
    fail(there ? gauge::WifiFailure::Join : gauge::WifiFailure::Start);
  }
}

void WifiNet::fail(gauge::WifiFailure why) {
  requested_.store(gauge::encodeWifiState(gauge::WifiState::Off));
  link_.store(static_cast<uint8_t>(gauge::WifiLink::None));
  failure_.store(static_cast<uint8_t>(why));
  phase_.store(static_cast<uint8_t>(gauge::WifiPhase::Failed));
}

bool WifiNet::networkThere() {
  if (!ok(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode(STA)")) return false;
  if (!radio_on_) {
    if (!ok(esp_wifi_start(), "esp_wifi_start")) return false;
    radio_on_ = true;
  }
  wifi_scan_config_t scan = {};
  scan.ssid = reinterpret_cast<uint8_t*>(ssid_);
  scan.show_hidden = true;
  if (!ok(esp_wifi_scan_start(&scan, true), "esp_wifi_scan_start")) return false;  // blocks this task only
  uint16_t found = 0;
  const bool counted = ok(esp_wifi_scan_get_ap_num(&found), "esp_wifi_scan_get_ap_num");
  ok(esp_wifi_clear_ap_list(), "esp_wifi_clear_ap_list");
  return counted && found > 0;
}

bool WifiNet::join() {
  wifi_config_t cfg = {};
  copyField(cfg.sta.ssid, sizeof(cfg.sta.ssid), ssid_);
  copyField(cfg.sta.password, sizeof(cfg.sta.password), password_);
  cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;  // WPA2 or better
  if (!ok(esp_wifi_set_config(WIFI_IF_STA, &cfg), "esp_wifi_set_config(STA)")) return false;
  xEventGroupClearBits(events_, kGotIp | kDisconnected);
  if (!ok(esp_wifi_connect(), "esp_wifi_connect")) return false;
  const EventBits_t bits = xEventGroupWaitBits(events_, kGotIp, pdFALSE, pdFALSE, kJoinWait);
  if (!(bits & kGotIp)) { ESP_LOGW(TAG, "Couldn't join %s", ssid_); return false; }
  return true;
}

bool WifiNet::host() {
  if (radio_on_) {
    const esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) ok(err, "esp_wifi_stop");
    radio_on_ = false;
  }
  if (!ok(esp_wifi_set_mode(WIFI_MODE_AP), "esp_wifi_set_mode(AP)")) return false;
  wifi_config_t cfg = {};
  copyField(cfg.ap.ssid, sizeof(cfg.ap.ssid), ssid_);
  cfg.ap.ssid_len = static_cast<uint8_t>(std::strlen(ssid_));
  copyField(cfg.ap.password, sizeof(cfg.ap.password), password_);
  cfg.ap.channel = 1;
  cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
  cfg.ap.max_connection = kMaxStations;
  cfg.ap.dtim_period = 1;  // multicast isn't held back long for listeners that are saving power
  if (!ok(esp_wifi_set_config(WIFI_IF_AP, &cfg), "esp_wifi_set_config(AP)")) return false;
  if (!ok(esp_wifi_start(), "esp_wifi_start")) return false;
  radio_on_ = true;
  esp_netif_ip_info_t ip = {};
  char text[16] = "192.168.4.1";
  if (esp_netif_get_ip_info(static_cast<esp_netif_t*>(ap_netif_), &ip) == ESP_OK)
    std::snprintf(text, sizeof(text), IPSTR, IP2STR(&ip.ip));
  setAddress(text);
  return true;
}

void WifiNet::stopRadio() {
  web_.stop();
  if (radio_on_) {
    const esp_err_t dis = esp_wifi_disconnect();
    if (dis != ESP_OK && dis != ESP_ERR_WIFI_NOT_STARTED && dis != ESP_ERR_WIFI_NOT_CONNECT && dis != ESP_ERR_WIFI_MODE) {
      ok(dis, "esp_wifi_disconnect");
    }
    const esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) ok(err, "esp_wifi_stop");
  }
  radio_on_ = false;
  link_.store(static_cast<uint8_t>(gauge::WifiLink::None));
  setAddress("");
}

void WifiNet::lostLink() {
  ESP_LOGW(TAG, "Lost %s: rejoining", ssid_);
  closeSocket();  // reopened on the new link, with its own interface address
  setAddress("");
  xEventGroupClearBits(events_, kDisconnected);
  if (ok(esp_wifi_connect(), "esp_wifi_connect") &&
      (xEventGroupWaitBits(events_, kGotIp, pdFALSE, pdFALSE, kRejoinWait) & kGotIp)) {
    return;  // back on it
  }
  if (networkThere()) {  // still there: keep trying, a while apart
    xEventGroupSetBits(events_, kDisconnected);
    return;
  }
  if (host()) {
    link_.store(static_cast<uint8_t>(gauge::WifiLink::Hosting));
    ESP_LOGI(TAG, "%s gone: hosting it", ssid_);
  } else {
    stopRadio();
    fail(gauge::WifiFailure::Start);
  }
}

bool WifiNet::openSocket() {
  sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock_ < 0) { ESP_LOGW(TAG, "UDP socket failed (errno %d)", errno); return false; }
  const int flags = fcntl(sock_, F_GETFL, 0);
  if (flags < 0 || fcntl(sock_, F_SETFL, flags | O_NONBLOCK) < 0) { ESP_LOGW(TAG, "UDP non-blocking failed"); return false; }
  const uint8_t ttl = 1;  // stays on this network
  if (setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) { ESP_LOGW(TAG, "UDP TTL failed"); return false; }
  void* netif = link() == gauge::WifiLink::Joined ? sta_netif_ : ap_netif_;
  esp_netif_ip_info_t ip = {};
  if (netif && esp_netif_get_ip_info(static_cast<esp_netif_t*>(netif), &ip) == ESP_OK) {
    struct in_addr iface = {};
    iface.s_addr = ip.ip.addr;
    if (setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof(iface)) < 0) ESP_LOGW(TAG, "UDP interface not set");
  }
  ESP_LOGI(TAG, "UDP stream to %s:%u", group_, static_cast<unsigned>(port_));
  return true;
}

void WifiNet::closeSocket() {
  if (sock_ >= 0) close(sock_);
  sock_ = -1;
}

void WifiNet::sendLatest() {
  gauge::UdpFields f;
  portENTER_CRITICAL(&mux_);
  const bool fresh = fresh_;
  if (fresh) f = mailbox_;
  fresh_ = false;
  portEXIT_CRITICAL(&mux_);
  if (!fresh) return;
  gauge::encodeUdpPacket(f, packet_);
  struct sockaddr_in to = {};
  to.sin_family = AF_INET;
  to.sin_port = htons(port_);
  if (inet_aton(group_, &to.sin_addr) == 0) { dropped_.fetch_add(1); return; }
  // Non-blocking: if Wi-Fi is busy, this packet is lost (its sequence number shows it), never queued or waited on.
  if (sendto(sock_, packet_, sizeof(packet_), MSG_DONTWAIT, reinterpret_cast<struct sockaddr*>(&to), sizeof(to)) < 0)
    dropped_.fetch_add(1);
}
