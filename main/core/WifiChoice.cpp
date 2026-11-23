#include "WifiChoice.h"
#include <cstdio>
#include <cstring>
namespace gauge {
bool wifiWeb(WifiState s) { return s == WifiState::WebOnUdpOff || s == WifiState::WebOnUdpOn; }
bool wifiUdp(WifiState s) { return s == WifiState::WebOnUdpOn || s == WifiState::WebOffUdpOn; }

bool wifiCredentialsOk(const char *ssid, const char *password) {
  if (!ssid || !password) return false;
  const size_t s = std::strlen(ssid), p = std::strlen(password);
  return s >= 1 && s <= 32 && p >= 8 && p <= 63;
}

WifiLink wifiLinkFor(bool network_found) { return network_found ? WifiLink::Joined : WifiLink::Hosting; }

bool wifiAvailable(WifiState s, WifiBuild b) {
  if (s == WifiState::Off) return true;
  return b.credentials && (!wifiWeb(s) || b.web) && (!wifiUdp(s) || b.udp);
}

WifiState nextWifiState(WifiState s, WifiBuild b) {
  static const WifiState kCycle[] = {WifiState::Off, WifiState::WebOnUdpOff, WifiState::WebOnUdpOn, WifiState::WebOffUdpOn};
  constexpr int n = sizeof(kCycle) / sizeof(kCycle[0]);
  int i = 0;
  while (i < n && kCycle[i] != s) ++i;
  for (int k = 1; k <= n; ++k) {
    const WifiState next = kCycle[(i + k) % n];
    if (wifiAvailable(next, b)) return next;
  }
  return WifiState::Off;
}

WifiState decodeWifiState(bool found, uint8_t stored, WifiBuild b) {
  if (!found || stored > static_cast<uint8_t>(WifiState::WebOffUdpOn)) return WifiState::Off;
  const WifiState s = static_cast<WifiState>(stored);
  return wifiAvailable(s, b) ? s : WifiState::Off;
}

uint8_t encodeWifiState(WifiState s) { return static_cast<uint8_t>(s); }

const char *wifiStateText(WifiState s) {
  switch (s) {
    case WifiState::WebOnUdpOff: return "Wifi - Web On, UDP Off";
    case WifiState::WebOnUdpOn: return "Wifi - Web On, UDP On";
    case WifiState::WebOffUdpOn: return "Wifi - Web Off, UDP On";
    default: return "Wifi Off";
  }
}

void wifiPanelText(WifiState s, const WifiStatus &st, WifiBuild b, char *line1, size_t size1, char *line2, size_t size2,
                   bool &warn) {
  const char *ssid = st.ssid ? st.ssid : "";
  warn = false;
  std::snprintf(line2, size2, "%s", "");
  if (!b.web && !b.udp) {
    std::snprintf(line1, size1, "Wifi not built");
  } else if (!b.credentials) {
    warn = true;
    std::snprintf(line1, size1, "Wifi failed - no settings");
    std::snprintf(line2, size2, "Add main/wifi_secrets.h");
  } else if (st.phase == WifiPhase::Failed) {
    warn = true;
    std::snprintf(line1, size1, "Wifi failed");
    if (st.failure == WifiFailure::Join) std::snprintf(line2, size2, "Can't join %s", ssid);
  } else if (s != WifiState::Off && st.phase == WifiPhase::Starting) {
    std::snprintf(line1, size1, "Wifi starting...");
    std::snprintf(line2, size2, "Looking for %s", ssid);
  } else {
    std::snprintf(line1, size1, "%s", wifiStateText(s));
    if (s != WifiState::Off && st.phase == WifiPhase::On && st.link != WifiLink::None)
      std::snprintf(line2, size2, "%s %s  %s", st.link == WifiLink::Joined ? "Joined" : "Hosting", ssid, st.ip ? st.ip : "");
  }
}

bool ShortPress::update(bool down, uint32_t ms) {
  if (!started_) {  // held at start (e.g. since power-up): its release never counts as a press
    started_ = true;
    stable_ = raw_ = down;
    raw_since_ = ms;
    down_since_ = ms - kButtonLongMs;
    return false;
  }
  if (down != raw_) { raw_ = down; raw_since_ = ms; return false; }
  if (raw_ == stable_ || uint32_t(ms - raw_since_) < kButtonDebounceMs) return false;
  stable_ = raw_;
  if (stable_) { down_since_ = raw_since_; return false; }
  return uint32_t(raw_since_ - down_since_) < kButtonLongMs;  // released: held from down_since_ to raw_since_
}
} // namespace gauge
