#pragma once
#include "core/UdpPacket.h"
#include "core/WifiChoice.h"
#include "net/WebServer.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <atomic>
#include <stddef.h>
#include <stdint.h>

// Wi-Fi, the UDP data stream (#33) and the web page's server (#32), run by one task below the main loop's priority (#34).
// When asked to start, it looks for the network in main/wifi_secrets.h: if it's there the gauge joins it as a client, and
// if not the gauge hosts that network itself (same name and password, at 192.168.4.1). A joined network that disappears
// is rejoined, or hosted once it's gone. The main loop only calls request() and post(), which never block and never
// allocate: the task does all the Wi-Fi, socket and server work, and drops a packet rather than wait. If anything fails,
// the gauge carries on as without Wi-Fi.
class WifiNet {
 public:
  // Starts the task (no radio yet). ssid and password: usable credentials (wifiCredentialsOk); group and port: the stream;
  // web: the page's log and status (nullptr without the page), shown in unit.
  bool begin(const char* ssid, const char* password, const char* group, uint16_t port, WebSource* web, gauge::Unit unit);
  void request(gauge::WifiState s);  // the state to run; the task follows it
  gauge::WifiPhase phase() const { return static_cast<gauge::WifiPhase>(phase_.load()); }
  gauge::WifiLink link() const { return static_cast<gauge::WifiLink>(link_.load()); }
  gauge::WifiFailure failure() const { return static_cast<gauge::WifiFailure>(failure_.load()); }
  void address(char* out, size_t size);  // the gauge's IP address as text, "" when it has none
  const char* ssid() const { return ssid_; }
  void post(const gauge::UdpFields& f);  // the newest sample, for the stream; overwrites one not yet sent
  uint32_t dropped() const { return dropped_.load(); }
 private:
  static void taskMain(void* arg);
  static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
  void run();
  bool init();
  void start();              // look for the network, then join or host it
  bool networkThere();       // a scan for the configured SSID
  bool join();               // as a client: connect and wait for an address
  bool host();               // as the access point
  void stopRadio();
  void lostLink();           // a joined network dropped: rejoin, or host it once it's gone
  void fail(gauge::WifiFailure why);
  void setAddress(const char* ip);
  bool openSocket();
  void closeSocket();
  void sendLatest();
  TaskHandle_t task_ = nullptr;
  EventGroupHandle_t events_ = nullptr;
  std::atomic<uint8_t> requested_{0};
  std::atomic<uint8_t> phase_{0};
  std::atomic<uint8_t> link_{0};
  std::atomic<uint8_t> failure_{0};
  std::atomic<uint32_t> dropped_{0};
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;  // guards mailbox_, fresh_ and ip_
  gauge::UdpFields mailbox_;
  bool fresh_ = false;
  char ip_[16] = {};
  bool ready_ = false;   // esp_wifi_init done (once)
  bool radio_on_ = false;
  int sock_ = -1;
  void* sta_netif_ = nullptr;
  void* ap_netif_ = nullptr;
  char ssid_[33] = {};
  char password_[64] = {};
  char group_[16] = {};
  uint16_t port_ = 0;
  uint8_t packet_[gauge::kUdpPacketSize] = {};
  WebSource* web_source_ = nullptr;
  gauge::Unit unit_ = gauge::Unit::Celsius;
  WebServer web_;
};
