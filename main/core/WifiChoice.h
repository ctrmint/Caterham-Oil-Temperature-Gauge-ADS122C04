#pragma once
#include <stddef.h>
#include <stdint.h>
// Wi-Fi control from the BOOT button (#34): the state cycle, its stored value, the button's short press and the
// confirmation panel's timing and text. Pure logic, so the host tests cover it.
namespace gauge {
// The stored value is the enum's value, so existing values never change meaning. The cycle, as the user worded it:
// Wifi Off -> Wifi - Web On, UDP Off -> Wifi - Web On, UDP On -> Wifi - Web Off, UDP On -> Wifi Off (one change a press).
enum class WifiState : uint8_t { Off = 0, WebOnUdpOff = 1, WebOnUdpOn = 2, WebOffUdpOn = 3 };
bool wifiWeb(WifiState s);  // the web page (#32) runs
bool wifiUdp(WifiState s);  // the UDP stream (#33) runs
// What this firmware can do: each feature built in (menuconfig), and usable credentials in main/wifi_secrets.h.
struct WifiBuild { bool web; bool udp; bool credentials; };
// An SSID of 1 to 32 bytes and a WPA2 password of 8 to 63.
bool wifiCredentialsOk(const char *ssid, const char *password);
// A state is available when every feature it turns on is built in and the credentials are usable. Off always is.
bool wifiAvailable(WifiState s, WifiBuild b);
// The next available state in the cycle; Off when nothing else is.
WifiState nextWifiState(WifiState s, WifiBuild b);
// The stored value; anything missing, unknown or no longer available is Off.
WifiState decodeWifiState(bool found, uint8_t stored, WifiBuild b);
uint8_t encodeWifiState(WifiState s);
const char *wifiStateText(WifiState s);

// The network's progress, as the network task reports it.
enum class WifiPhase : uint8_t { Off, Starting, On, Failed };
// How the gauge is on the network: it joined the configured network as a client (it was there), or it's hosting that
// network itself as the access point (it wasn't).
enum class WifiLink : uint8_t { None, Joined, Hosting };
// Why it failed: the network was there but couldn't be joined (a wrong password, say), or Wi-Fi didn't start.
enum class WifiFailure : uint8_t { None, Join, Start };
// Joined if the configured network was found, else Hosting. A network that's there is never hosted a second time.
WifiLink wifiLinkFor(bool network_found);
struct WifiStatus {
  WifiPhase phase;
  WifiLink link;
  WifiFailure failure;
  const char *ssid;  // the configured network
  const char *ip;    // the gauge's address once up, e.g. "192.168.1.57" or "192.168.4.1"
};
// The confirmation panel: line 1 the state (or what went wrong), line 2 how it's on the network once it's up.
// warn: line 1 is a failure, shown in amber.
constexpr size_t kWifiPanelTextSize = 64;
void wifiPanelText(WifiState s, const WifiStatus &st, WifiBuild b, char *line1, size_t size1, char *line2, size_t size2,
                   bool &warn);

// The BOOT button: debounced; a short press counts on release, a long one (kButtonLongMs or more) never does.
constexpr uint32_t kButtonDebounceMs = 30, kButtonLongMs = 1000;
class ShortPress {
 public:
  bool update(bool down, uint32_t ms);  // true once per short press
 private:
  bool stable_ = false;    // the debounced level
  bool raw_ = false;       // the last raw level
  uint32_t raw_since_ = 0;
  uint32_t down_since_ = 0;
  bool started_ = false;
};

// The panel shows for kWifiPanelMs after each start(), restarted by another.
constexpr uint32_t kWifiPanelMs = 3000;
class PanelTimer {
 public:
  void start(uint32_t ms) { on_ = true; since_ = ms; }
  bool visible(uint32_t ms) const { return on_ && uint32_t(ms - since_) < kWifiPanelMs; }
 private:
  bool on_ = false;
  uint32_t since_ = 0;
};
} // namespace gauge
