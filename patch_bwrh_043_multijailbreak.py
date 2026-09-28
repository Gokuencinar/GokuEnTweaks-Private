from pathlib import Path
import plistlib
import re

root = Path("BetterWiFi-RH")

# Remove build-time RootHide pinning from compatibility builds only.
for makefile in root.rglob("Makefile"):
    text = makefile.read_text()
    text, scheme_count = re.subn(
        r'(?m)^\s*(?:export\s+)?THEOS_PACKAGE_SCHEME\s*[:?+]?=\s*roothide\s*$',
        '',
        text,
    )
    text, arch_count = re.subn(
        r'(?m)^\s*(?:export\s+)?PACKAGE_ARCH\s*[:?+]?=\s*iphoneos-arm64e\s*$',
        '',
        text,
    )
    # Lower the deployment target for the compatibility variants. Command-line
    # TARGET still takes precedence in CI, but keeping the project metadata at
    # 15.0 makes local builds consistent too.
    text, target_count = re.subn(
        r'(?m)^(\s*TARGET\s*[:?+]?=\s*iphone:clang:[^:\\s]+:)16(?:\.0)?\s*
# Enable the tweak constructor on iOS 15+.
p = root / "Tweak.xm"
s = p.read_text()
s2, count = re.subn(
    r'@available\(iOS\s+16\.0\s*,\s*\*\)',
    '@available(iOS 15.0, *)',
    s,
)
if count < 1:
    raise SystemExit("expected at least one iOS 16 availability guard")
p.write_text(s2)

# Neutral package description for non-RootHide variants.
p = root / "control"
s = p.read_text()
s = re.sub(
    r'^Description:.*$',
    'Description: Advanced Wi-Fi enhancements for iOS 15+ jailbreaks with network details, live monitoring, diagnostics, filters, language selection and efficient on-demand scanning.',
    s,
    count=1,
    flags=re.M,
)
p.write_text(s)

# Neutral settings footer.
p = root / "prefs/Resources/Root.plist"
obj = plistlib.loads(p.read_bytes())

def walk(value):
    if isinstance(value, dict):
        return {k: walk(v) for k, v in value.items()}
    if isinstance(value, list):
        return [walk(v) for v in value]
    if isinstance(value, str):
        return value.replace(
            "Advanced Wi-Fi controls for iOS 16 and RootHide. No background daemon is used.",
            "Advanced Wi-Fi controls for supported iOS jailbreaks. No background daemon is used."
        )
    return value

p.write_bytes(plistlib.dumps(walk(obj), fmt=plistlib.FMT_XML, sort_keys=False))

p = root / "prefs/Resources/es.lproj/Root.strings"
if p.exists():
    s = p.read_text()
    s = s.replace(
        '"Advanced Wi-Fi controls for iOS 16 and RootHide. No background daemon is used." = "Controles Wi‑Fi avanzados para iOS 16 y RootHide. No utiliza ningún daemon en segundo plano.";',
        '"Advanced Wi-Fi controls for supported iOS jailbreaks. No background daemon is used." = "Controles Wi‑Fi avanzados para jailbreaks de iOS compatibles. No utiliza ningún daemon en segundo plano.";'
    )
    p.write_text(s)

print(f"Multi-jailbreak compatibility patch applied; availability guards changed={count}")
,
        r'\\g<1>15.0',
        text,
    )
    if scheme_count or arch_count or target_count:
        makefile.write_text(text)

# Enable the tweak constructor on iOS 15+.
p = root / "Tweak.xm"
s = p.read_text()
s2, count = re.subn(
    r'@available\(iOS\s+16\.0\s*,\s*\*\)',
    '@available(iOS 15.0, *)',
    s,
)
if count < 1:
    raise SystemExit("expected at least one iOS 16 availability guard")
p.write_text(s2)

# Neutral package description for non-RootHide variants.
p = root / "control"
s = p.read_text()
s = re.sub(
    r'^Description:.*$',
    'Description: Advanced Wi-Fi enhancements for iOS 15+ jailbreaks with network details, live monitoring, diagnostics, filters, language selection and efficient on-demand scanning.',
    s,
    count=1,
    flags=re.M,
)
p.write_text(s)

# Neutral settings footer.
p = root / "prefs/Resources/Root.plist"
obj = plistlib.loads(p.read_bytes())

def walk(value):
    if isinstance(value, dict):
        return {k: walk(v) for k, v in value.items()}
    if isinstance(value, list):
        return [walk(v) for v in value]
    if isinstance(value, str):
        return value.replace(
            "Advanced Wi-Fi controls for iOS 16 and RootHide. No background daemon is used.",
            "Advanced Wi-Fi controls for supported iOS jailbreaks. No background daemon is used."
        )
    return value

p.write_bytes(plistlib.dumps(walk(obj), fmt=plistlib.FMT_XML, sort_keys=False))

p = root / "prefs/Resources/es.lproj/Root.strings"
if p.exists():
    s = p.read_text()
    s = s.replace(
        '"Advanced Wi-Fi controls for iOS 16 and RootHide. No background daemon is used." = "Controles Wi‑Fi avanzados para iOS 16 y RootHide. No utiliza ningún daemon en segundo plano.";',
        '"Advanced Wi-Fi controls for supported iOS jailbreaks. No background daemon is used." = "Controles Wi‑Fi avanzados para jailbreaks de iOS compatibles. No utiliza ningún daemon en segundo plano.";'
    )
    p.write_text(s)

print(f"Multi-jailbreak compatibility patch applied; availability guards changed={count}")
