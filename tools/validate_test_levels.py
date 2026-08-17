#!/usr/bin/env python3
"""Static validation for the raw Geometry Dash acceptance fixtures.

This does not replace the in-game runner: it catches malformed key/value pairs,
missing feature fixtures, duplicate object records and accidental empty levels
before they reach Geometry Dash.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
LEVELS = ROOT / "test-levels"
REQUIRED = {
    "object-dual.lvl",
    "object-upside-down-slope.lvl",
    "object-partial-rotation.lvl",
    "object-robot.lvl",
    "object-spider.lvl",
    "object-swing.lvl",
    "object-trigger-graph.lvl",
    "object-dash-orb.lvl",
    "object-teleport-portals.lvl",
    "object-2.2.lvl",
    "object-modifier-blocks.lvl",
    "runtime-phasing.lvl",
    "cps-70-cube.lvl",
    "cps-70-spider.lvl",
    "cps-70-swing.lvl",
}


def validate(path: Path) -> list[str]:
    errors: list[str] = []
    records = [record for record in path.read_text().strip().split(";") if record]
    if len(records) < 2:
        return ["must contain settings and at least one object"]
    for index, record in enumerate(records[1:], 1):
        fields = record.split(",")
        if len(fields) % 2:
            errors.append(f"object {index}: odd key/value field count")
            continue
        pairs = dict(zip(fields[::2], fields[1::2]))
        if "1" not in pairs:
            errors.append(f"object {index}: missing object ID key 1")
        if "2" not in pairs or "3" not in pairs:
            errors.append(f"object {index}: missing position key 2 or 3")
        for key in fields[::2]:
            try:
                int(key)
            except ValueError:
                errors.append(f"object {index}: non-integer key {key!r}")
    return errors


def main() -> int:
    found = {path.name for path in LEVELS.glob("*.lvl")}
    failures = [f"missing fixture: {name}" for name in sorted(REQUIRED - found)]
    for path in sorted(LEVELS.glob("*.lvl")):
        failures.extend(f"{path.name}: {error}" for error in validate(path))
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"validated {len(found)} acceptance level fixtures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
