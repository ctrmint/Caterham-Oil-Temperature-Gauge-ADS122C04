// Wi-Fi settings for the gauge (#33, #34). Copy this file to main/wifi_secrets.h and fill it in.
// main/wifi_secrets.h is in .gitignore: never commit it, and never share it.
//
// When Wi-Fi is switched on (the BOOT button), the gauge looks for this network:
//  - found: it joins it as a client, and the screen shows the address it was given;
//  - not found: it runs this network itself, with the same name and password, at 192.168.4.1.
// So a laptop set up for this network connects either way. A network the gauge can see but can't join (a wrong
// password, say) shows "Wifi failed"; it never hosts a second network with the same name.
//
// SSID: 1 to 32 characters. Password: 8 to 63 characters (WPA2). They're C strings: put \ before any " or \.
#define OIL_GAUGE_WIFI_SSID     "your-network"
#define OIL_GAUGE_WIFI_PASSWORD "your-password"
