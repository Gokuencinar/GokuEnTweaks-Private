from pathlib import Path
import plistlib
import re

root = Path("BetterWiFi-RH")

# Enable the tweak constructor on iOS 15 as well. The project uses runtime
# selector checks for the private Wi-Fi APIs, so optional pieces remain gated
# dynamically instead of hard-failing at load time.
p = root / "Tweak.xm"
s = p.read_text()
s2, count = re.subn(r'@available\(iOS\s+16\.0\s*,\s*\*\)', '@available(iOS 15.0, *)', s)
if count < 1:
    raise SystemExit("expected at least one iOS 16 availability guard")
p.write_text(s2)

# Make the package description neutral across RootHide/rootless/rootful.
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

# Remove RootHide/iOS16-specific wording from the settings footer where present.
p = root / "prefs/Resources/Root.plist"
obj = plistlib.loads(p.read_bytes())

def walk(value):
    if isinstance(value, dict):
        return {k: walk(v) for k, v in value.items()}
    if isinstance(value, list):
        return [walk(v) for v in value]
    if isinstance(value, str):
        value = value.replace(
            "Advanced Wi-Fi controls for iOS 16 and RootHide. No background daemon is used.",
            "Advanced Wi-Fi controls for supported iOS jailbreaks. No background daemon is used."
        )
        return value
    return value

obj = walk(obj)
p.write_bytes(plistlib.dumps(obj, fmt=plistlib.FMT_XML, sort_keys=False))

p = root / "prefs/Resources/es.lproj/Root.strings"
if p.exists():
    s = p.read_text()
    s = s.replace(
        '"Advanced Wi-Fi controls for iOS 16 and RootHide. No background daemon is used." = "Controles Wi‑Fi avanzados para iOS 16 y RootHide. No utiliza ningún daemon en segundo plano.";',
        '"Advanced Wi-Fi controls for supported iOS jailbreaks. No background daemon is used." = "Controles Wi‑Fi avanzados para jailbreaks de iOS compatibles. No utiliza ningún daemon en segundo plano.";'
    )
    p.write_text(s)

print(f"Multi-jailbreak compatibility patch applied; iOS16 guards changed={count}")
