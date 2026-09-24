"""Build an offline MAC vendor index from the IEEE MA-L public CSV listing."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
import plistlib


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", type=Path, help="IEEE oui.csv downloaded from standards-oui.ieee.org")
    args = parser.parse_args()
    vendors: dict[str, str] = {}
    with args.csv.open(encoding="utf-8-sig", newline="") as source:
        for row in csv.DictReader(source):
            prefix = row["Assignment"].upper()
            if row["Registry"] == "MA-L" and len(prefix) == 6:
                vendors[prefix] = row["Organization Name"].strip()
    if len(vendors) < 30_000:
        raise ValueError(f"IEEE MA-L listing looks incomplete: {len(vendors)} entries")
    output = Path(__file__).resolve().parents[1] / "build" / "oui_vendors.plist"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(plistlib.dumps(vendors, fmt=plistlib.FMT_BINARY))
    print(f"{len(vendors)} vendor prefixes -> {output}")


if __name__ == "__main__":
    main()

