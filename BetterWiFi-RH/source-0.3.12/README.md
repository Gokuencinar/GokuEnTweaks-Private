# BetterWiFi RH

BetterWiFi RH is a RootHide-native Wi-Fi enhancement tweak for iOS 16, designed around the private WiFiKit UI used by Settings.

## 0.3.1 highlights

- Extra information for the currently connected network and scanned networks.
- Live RSSI monitor with a short signal-history graph.
- Channel analyzer for nearby 2.4 GHz and 5 GHz access points.
- Quick on-demand diagnostics for local IP configuration, DNS resolution and Internet/captive-portal connectivity.
- Advanced filters for band, known/unknown networks, minimum RSSI and sorting.
- BetterWiFi-style pull-to-refresh and unfiltered scanning support.
- Efficient monitoring mode enabled by default.
- No daemon and no permanent background activity. Timers and active refreshes stop when the relevant screen closes.

## Connected network details

The BetterWiFi RH section inside the Wi-Fi network information page can show:

- RSSI in dBm and a human-readable quality label
- security type
- BSSID
- channel and band
- channel width
- IPv4 address
- router
- DNS servers
- optional private Wi-Fi address

It also exposes the signal monitor, channel analyzer and quick diagnostics screens.

## Battery behavior

BetterWiFi RH does not run a background service. The signal monitor actively refreshes Wi-Fi only while its graph is visible. Efficient mode uses a 3-second interval; disabling it uses a faster 1.5-second interval. Channel scans and diagnostics are otherwise triggered by the user or by the normal Settings Wi-Fi page.

## Important limitation

"Remove signal filtering" asks WiFiKit to support unfiltered scanning so weaker access points may become visible. It does not increase the RF transmit power or physical antenna range of the device.

## Target

- iOS 16.x
- RootHide / Dopamine 2
- arm64e
