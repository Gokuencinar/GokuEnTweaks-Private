# GokuEnREPO

Public APT repository for **jailbroken iOS**, with builds for **RootHide, Dopamine rootless and rootful** environments :).

## 📲 Add GokuEnREPO

Repository URL:

`https://raw.githubusercontent.com/Gokuencinar/GokuEnREPO/main/`

<table>
<tr>
<td align="center" width="25%">
<a href="https://gokuencinar.github.io/GokuEnREPO/add/sileo.html"><img src="https://getsileo.app/img/icon.png" width="56" height="56" alt="Sileo"></a><br>
<strong><a href="https://gokuencinar.github.io/GokuEnREPO/add/sileo.html">Add to Sileo</a></strong>
</td>
<td align="center" width="25%">
<a href="https://gokuencinar.github.io/GokuEnREPO/add/cydia.html"><img src="assets/package-managers/cydia.svg" width="56" height="56" alt="Cydia"></a><br>
<strong><a href="https://gokuencinar.github.io/GokuEnREPO/add/cydia.html">Add to Cydia</a></strong>
</td>
<td align="center" width="25%">
<a href="https://gokuencinar.github.io/GokuEnREPO/add/zebra.html"><img src="https://getzbra.com/assets/zeeb.svg" width="56" height="56" alt="Zebra"></a><br>
<strong><a href="https://gokuencinar.github.io/GokuEnREPO/add/zebra.html">Add to Zebra</a></strong>
</td>
<td align="center" width="25%">
<a href="https://gokuencinar.github.io/GokuEnREPO/add/installer.html"><img src="assets/package-managers/installer.svg" width="56" height="56" alt="Installer 5"></a><br>
<strong><a href="https://gokuencinar.github.io/GokuEnREPO/add/installer.html">Add to Installer 5</a></strong>
</td>
</tr>
</table>

> If your browser does not open the package manager automatically, copy the repository URL above and add it manually as a source.

## 📦 Available tweaks

### 📡 BandLock Global — LTE/4G + 5G NR Band Control

Standalone jailbreak app for manually controlling LTE/4G and supported 5G NR modem selections on iOS 16. It includes network modes, independent LTE/NR pending selections, Field Test access and an offline catalogue covering 160 countries and territories.

- **RootHide build:** `com.gokuencinar.bandlock` — `iphoneos-arm64e`
- **Dopamine rootless build:** `com.gokuencinar.bandlock.dopamine` — `iphoneos-arm64`
- **Rootful build:** `com.gokuencinar.bandlock.rootful` — `iphoneos-arm`
- Country profiles never modify the modem automatically; compatible bands are prepared for review before applying.

➡️ **[BandLock details](tweaks/BandLock/README.md)** · **[Changelog](tweaks/BandLock/CHANGELOG.md)**

### 📶 BetterWiFi RH — Wi-Fi Tools

Advanced Wi-Fi enhancement tweak for jailbroken iOS that expands Apple’s Wi-Fi settings with live diagnostics, signal analysis and network-management tools while keeping background activity minimal.

- Extended information about the connected Wi-Fi network.
- Live signal monitor and signal history.
- 2.4 GHz / 5 GHz channel analyzer.
- Classic and advanced network filters plus diagnostic tools.
- Shuffle / PreferenceLoader integration.
- Manual language selector: Automatic, Spanish or English.
- On-demand operation while the relevant Wi-Fi pages are open; no dedicated resident daemon.
- **0.3.12 builds:** RootHide (`iphoneos-arm64e`, iOS 16), Dopamine rootless (`iphoneos-arm64`, iOS 15–18) and rootful (`iphoneos-arm`, iOS 15–17).

➡️ **[BetterWiFi RH details](tweaks/BetterWiFi-RH/README.md)** · **[Changelog](tweaks/BetterWiFi-RH/CHANGELOG.md)**

### Nuke Wireless — In Development

Nuke Wireless is currently in development. Public package releases are unavailable.

➡️ **[Nuke Wireless details](tweaks/Nuke-Wireless/README.md)**

## Repository layout

Project-specific files live under [`tweaks/`](tweaks/)

## Current packages

| Package | Version | Architecture |
| --- | --- | --- |
| BandLock (RootHide) | 1.3 | `iphoneos-arm64e` |
| BandLock (Dopamine) | 1.3 | `iphoneos-arm64` |
| BandLock (Rootful) | 1.3 | `iphoneos-arm` |
| BetterWiFi RH (RootHide) | 0.3.12 | `iphoneos-arm64e` |
| BetterWiFi RH (Dopamine rootless) | 0.3.12 | `iphoneos-arm64` |
| BetterWiFi RH (Rootful) | 0.3.12 | `iphoneos-arm` |

The `Packages` and `Packages.gz` indexes are automatically regenerated when a package inside `tweaks/*/debs/` changes.

## Compatibility

**iOS 15–18 (package-dependent) · RootHide · Dopamine rootless · Rootful · Sileo / Cydia / Zebra / Installer 5**

## ❤️ Support development

If you enjoy my tweaks and would like to support their continued development, bug fixes and future projects:

☕ **[Buy Me a Coffee](https://buymeacoffee.com/GokuEn)**

Any support is greatly appreciated. Thank you! ❤️

---

**Developer:** Gokuencinar / GokuEn
