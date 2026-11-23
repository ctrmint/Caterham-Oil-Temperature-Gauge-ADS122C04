#include "core/WifiChoice.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

using gauge::WifiBuild; using gauge::WifiFailure; using gauge::WifiLink; using gauge::WifiPhase; using gauge::WifiState;

static bool panel(WifiState s, WifiPhase p, WifiLink l, WifiFailure f, WifiBuild b, const char *l1, const char *l2, bool warn) {
  char a[gauge::kWifiPanelTextSize], c[gauge::kWifiPanelTextSize]; bool w = !warn;
  const gauge::WifiStatus st{p, l, f, "TrackNet", l == WifiLink::Hosting ? "192.168.4.1" : "192.168.1.57"};
  gauge::wifiPanelText(s, st, b, a, sizeof(a), c, sizeof(c), w);
  if (std::strcmp(a, l1) != 0 || std::strcmp(c, l2) != 0 || w != warn) std::fprintf(stderr, "got '%s' / '%s'\n", a, c);
  return std::strcmp(a, l1) == 0 && std::strcmp(c, l2) == 0 && w == warn;
}

int main() {
  const WifiBuild all{true, true, true}, udp{false, true, true}, web{true, false, true}, none{false, false, true};
  const WifiBuild nocreds{true, true, false}, udp_nocreds{false, true, false};
  // The user's wording, and what each state turns on.
  assert(!std::strcmp(gauge::wifiStateText(WifiState::Off), "Wifi Off"));
  assert(!std::strcmp(gauge::wifiStateText(WifiState::WebOnUdpOff), "Wifi - Web On, UDP Off"));
  assert(!std::strcmp(gauge::wifiStateText(WifiState::WebOnUdpOn), "Wifi - Web On, UDP On"));
  assert(!std::strcmp(gauge::wifiStateText(WifiState::WebOffUdpOn), "Wifi - Web Off, UDP On"));
  assert(!gauge::wifiWeb(WifiState::Off) && !gauge::wifiUdp(WifiState::Off));
  assert(gauge::wifiWeb(WifiState::WebOnUdpOff) && !gauge::wifiUdp(WifiState::WebOnUdpOff));
  assert(gauge::wifiWeb(WifiState::WebOnUdpOn) && gauge::wifiUdp(WifiState::WebOnUdpOn));
  assert(!gauge::wifiWeb(WifiState::WebOffUdpOn) && gauge::wifiUdp(WifiState::WebOffUdpOn));

  // The credentials from main/wifi_secrets.h: an SSID of 1 to 32 bytes, a password of 8 to 63.
  assert(!gauge::wifiCredentialsOk(nullptr, "12345678") && !gauge::wifiCredentialsOk("Net", nullptr));
  assert(!gauge::wifiCredentialsOk("", "12345678") && !gauge::wifiCredentialsOk("Net", "") && !gauge::wifiCredentialsOk("Net", "1234567"));
  assert(gauge::wifiCredentialsOk("N", "12345678") && gauge::wifiCredentialsOk(std::string(32, 's').c_str(), std::string(63, 'x').c_str()));
  assert(!gauge::wifiCredentialsOk(std::string(33, 's').c_str(), "12345678"));
  assert(!gauge::wifiCredentialsOk("Net", std::string(64, 'x').c_str()));

  // Joined if the network is there, hosted if it isn't.
  assert(gauge::wifiLinkFor(true) == WifiLink::Joined && gauge::wifiLinkFor(false) == WifiLink::Hosting);

  // The cycle, one change a press, skipping states this build can't run.
  assert(gauge::nextWifiState(WifiState::Off, all) == WifiState::WebOnUdpOff);
  assert(gauge::nextWifiState(WifiState::WebOnUdpOff, all) == WifiState::WebOnUdpOn);
  assert(gauge::nextWifiState(WifiState::WebOnUdpOn, all) == WifiState::WebOffUdpOn);
  assert(gauge::nextWifiState(WifiState::WebOffUdpOn, all) == WifiState::Off);
  assert(gauge::nextWifiState(WifiState::Off, udp) == WifiState::WebOffUdpOn && gauge::nextWifiState(WifiState::WebOffUdpOn, udp) == WifiState::Off);
  assert(gauge::nextWifiState(WifiState::Off, web) == WifiState::WebOnUdpOff && gauge::nextWifiState(WifiState::WebOnUdpOff, web) == WifiState::Off);
  for (WifiBuild b : {none, nocreds, udp_nocreds}) for (uint8_t v = 0; v < 4; ++v) assert(gauge::nextWifiState(static_cast<WifiState>(v), b) == WifiState::Off);
  // From a state that's no longer available, the next available one.
  assert(gauge::nextWifiState(WifiState::WebOnUdpOn, udp) == WifiState::WebOffUdpOn);
  assert(gauge::nextWifiState(WifiState::WebOnUdpOff, udp) == WifiState::WebOffUdpOn);
  { WifiState s = WifiState::Off; for (int i = 0; i < 4; ++i) s = gauge::nextWifiState(s, all); assert(s == WifiState::Off); }
  for (WifiBuild b : {all, udp, web, none, nocreds}) for (uint8_t v = 0; v < 4; ++v)
    assert(gauge::wifiAvailable(gauge::nextWifiState(static_cast<WifiState>(v), b), b));
  assert(gauge::wifiAvailable(WifiState::Off, none) && !gauge::wifiAvailable(WifiState::WebOffUdpOn, udp_nocreds));
  assert(!gauge::wifiAvailable(WifiState::WebOnUdpOn, udp) && !gauge::wifiAvailable(WifiState::WebOnUdpOn, web));

  // The stored value: missing, unknown or unavailable is Off.
  for (uint8_t v = 0; v < 4; ++v) {
    assert(gauge::encodeWifiState(static_cast<WifiState>(v)) == v);
    assert(gauge::decodeWifiState(true, v, all) == static_cast<WifiState>(v));
    assert(gauge::decodeWifiState(false, v, all) == WifiState::Off);
    assert(gauge::decodeWifiState(true, v, nocreds) == WifiState::Off);
  }
  assert(gauge::decodeWifiState(true, 4, all) == WifiState::Off && gauge::decodeWifiState(true, 255, all) == WifiState::Off);
  assert(gauge::decodeWifiState(true, 3, udp) == WifiState::WebOffUdpOn && gauge::decodeWifiState(true, 1, udp) == WifiState::Off);
  assert(gauge::decodeWifiState(true, 2, udp) == WifiState::Off);

  // The panel's text.
  const WifiFailure nf = WifiFailure::None;
  assert(panel(WifiState::Off, WifiPhase::Off, WifiLink::None, nf, udp, "Wifi Off", "", false));
  assert(panel(WifiState::WebOffUdpOn, WifiPhase::Starting, WifiLink::None, nf, udp, "Wifi starting...", "Looking for TrackNet", false));
  assert(panel(WifiState::WebOffUdpOn, WifiPhase::On, WifiLink::Joined, nf, udp, "Wifi - Web Off, UDP On", "Joined TrackNet  192.168.1.57", false));
  assert(panel(WifiState::WebOffUdpOn, WifiPhase::On, WifiLink::Hosting, nf, udp, "Wifi - Web Off, UDP On", "Hosting TrackNet  192.168.4.1", false));
  assert(panel(WifiState::WebOnUdpOn, WifiPhase::On, WifiLink::Joined, nf, all, "Wifi - Web On, UDP On", "Joined TrackNet  192.168.1.57", false));
  assert(panel(WifiState::Off, WifiPhase::Failed, WifiLink::None, WifiFailure::Join, udp, "Wifi failed", "Can't join TrackNet", true));
  assert(panel(WifiState::Off, WifiPhase::Failed, WifiLink::None, WifiFailure::Start, udp, "Wifi failed", "", true));
  assert(panel(WifiState::Off, WifiPhase::Off, WifiLink::None, nf, nocreds, "Wifi failed - no settings", "Add main/wifi_secrets.h", true));
  assert(panel(WifiState::Off, WifiPhase::Off, WifiLink::None, nf, WifiBuild{false, false, false}, "Wifi not built", "", false));
  assert(panel(WifiState::WebOffUdpOn, WifiPhase::Off, WifiLink::None, nf, udp, "Wifi - Web Off, UDP On", "", false));  // not up yet
  assert(panel(WifiState::WebOffUdpOn, WifiPhase::On, WifiLink::None, nf, udp, "Wifi - Web Off, UDP On", "", false));   // no link: no address
  { char a[8], c[8]; bool w;  // never overruns a small buffer
    const gauge::WifiStatus st{WifiPhase::On, WifiLink::Joined, nf, "TrackNet", "192.168.1.57"};
    gauge::wifiPanelText(WifiState::WebOffUdpOn, st, udp, a, sizeof(a), c, sizeof(c), w);
    assert(std::strlen(a) == 7 && std::strlen(c) == 7); }
  { char a[gauge::kWifiPanelTextSize], c[gauge::kWifiPanelTextSize]; bool w;  // the longest line still fits, clipped at worst
    const std::string ssid(32, 'W');
    const gauge::WifiStatus st{WifiPhase::On, WifiLink::Hosting, nf, ssid.c_str(), "192.168.4.1"};
    gauge::wifiPanelText(WifiState::WebOnUdpOn, st, all, a, sizeof(a), c, sizeof(c), w);
    assert(std::strlen(c) == std::strlen("Hosting ") + 32 + std::strlen("  192.168.4.1")); }

  // The BOOT button: a short press counts once, on release after the debounce; bounces, long holds and a button held
  // from the start don't.
  { gauge::ShortPress p; uint32_t t = 1000; int presses = 0;
    auto run = [&](bool down, uint32_t ms) { for (uint32_t e = t + ms; t < e; t += 10) presses += p.update(down, t); };
    run(false, 100); assert(presses == 0);
    run(true, 200); assert(presses == 0);
    run(false, 100); assert(presses == 1);
    run(false, 500); assert(presses == 1);
    for (int i = 0; i < 6; ++i) { presses += p.update(true, t); t += 5; presses += p.update(false, t); t += 5; }  // bounce
    run(false, 100); assert(presses == 1);
    run(true, 1500); run(false, 100); assert(presses == 1);  // long: never
    run(true, 960); run(false, 100); assert(presses == 2);   // just under the limit
    run(true, 1010); run(false, 100); assert(presses == 2);  // just over
  }
  { gauge::ShortPress p; int presses = 0; uint32_t t = 0;  // held from the start
    for (; t < 300; t += 10) presses += p.update(true, t);
    for (; t < 500; t += 10) presses += p.update(false, t);
    assert(presses == 0);
    for (; t < 700; t += 10) presses += p.update(true, t);
    for (; t < 900; t += 10) presses += p.update(false, t);
    assert(presses == 1); }
  { gauge::ShortPress p; int presses = 0; uint32_t t = 0xFFFFFF00u;  // across the millisecond wrap
    for (int i = 0; i < 10; ++i, t += 10) presses += p.update(false, t);
    for (int i = 0; i < 20; ++i, t += 10) presses += p.update(true, t);
    for (int i = 0; i < 10; ++i, t += 10) presses += p.update(false, t);
    assert(t < 0x1000u && presses == 1); }

  // The panel's timer: 3 s from the last press, across the wrap.
  { gauge::PanelTimer pt; assert(!pt.visible(0) && !pt.visible(5000));
    pt.start(1000); assert(pt.visible(1000) && pt.visible(3999) && !pt.visible(4000));
    pt.start(3500); assert(pt.visible(6499) && !pt.visible(6500));
    pt.start(0xFFFFFF00u); assert(pt.visible(0x100u) && pt.visible(0xFFFFFF00u + 2999u) && !pt.visible(0xFFFFFF00u + 3000u)); }
  std::puts("Wifi choice tests passed");
  return 0;
}
