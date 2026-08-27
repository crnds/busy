#!/usr/bin/env python3
"""Measure the built-in faces' real INK extents and print gfx.cpp's tables.

The font headers give the nominal box (Font 2: 16 tall / baseline 13; Font 4:
26 / 19), but every built-in glyph sits INSET inside that box. Centring on the
box therefore puts ink somewhere the nominal numbers do not predict, which is
what ROLE_DY corrects. Ask for ink, not for ascent.

    python3 scripts/font_metrics.py           # print the tables
    python3 scripts/font_metrics.py --check   # verify src/ui/gfx.cpp matches

Depends on scripts/sim_fonts.js; run gen_sim_fonts.py first.
"""
import json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# role -> (face key, ROLE_DY, the character set the role actually draws)
DIGITS = "0123456789-"
ALNUM  = "".join(chr(c) for c in range(0x21, 0x7F))
ROLES = [
    ("F_TITLE", "f2", 0, ALNUM),
    ("F_BODY",  "f2", 0, ALNUM),
    ("F_MICRO", "f1", 0, ALNUM),
]
# Cap height is measured off ONE clean flat-topped, flat-bottomed glyph. The
# union of A-Z and 0-9 overstates it -- 'Q' has a tail and 'J' can dip below
# the baseline, so the union measures the whole alphabet's extent, not a cap.
CAP_GLYPH = {"f2": "H", "f1": "H"}

def load():
    js = (ROOT / "scripts" / "sim_fonts.js").read_text()
    return json.loads(js[js.index("{"):js.rindex("}") + 1])

def ink_rows(face, ch):
    """Rows of `ch` that carry ink, as (set of rows, glyph width)."""
    i = ord(ch) - 0x20
    if not (0 <= i < 96):
        return set(), 0
    w = 5 if face["h"] == 8 else (face["w"][i] - 1 if face["h"] == 16 else face["w"][i])
    data = bytes.fromhex(face["g"][i])
    per = (w + 7) // 8 if w > 0 else 0
    rows = set()
    for y in range(face["h"]):
        for x in range(w):
            idx = y * per + (x >> 3)
            if idx < len(data) and (data[idx] >> (7 - (x & 7))) & 1:
                rows.add(y)
                break
    return rows, w

def measure(blob):
    out = []
    for role, key, dy, charset in ROLES:
        face = blob[key]
        H = face["h"]
        allrows = set()
        for ch in charset:
            r, _ = ink_rows(face, ch)
            allrows |= r
        caprows, _ = ink_rows(face, CAP_GLYPH[key])
        top, bot = min(allrows), max(allrows)
        ctop, cbot = min(caprows), max(caprows)
        out.append({
            "role": role, "face": key, "box": H, "dy": dy,
            "ink_top":  top - H // 2 + dy,
            "ink_bot":  bot - H // 2 + dy,
            "cap":      cbot - ctop + 1,
            "baseline": face["baseline"] - H // 2 + dy,
        })
    return out

def parse_gfx():
    src = (ROOT / "src" / "ui" / "gfx.cpp").read_text()
    hdr = (ROOT / "src" / "ui" / "gfx.h").read_text()
    # gfx.cpp's tables reference the constexpr extents from gfx.h, so that
    # screen.cpp can static_assert the row geometry against the same numbers.
    # Resolve those symbols before reading the tables.
    consts = {m[0]: int(m[1]) for m in
              re.findall(r"constexpr\s+int\s+(\w+)\s*=\s*(-?\d+)", hdr)}

    def arr(name):
        m = re.search(re.escape(name) + r"\[F_COUNT\]\s*=\s*\{([^}]*)\}", src)
        if not m:
            raise SystemExit(f"{name} not found in gfx.cpp")
        body = m.group(1)
        for k, v in consts.items():
            body = body.replace(k, str(v))
        vals = [int(v) for v in re.findall(r"-?\d+", body)]
        if len(vals) != len(ROLES):
            raise SystemExit(f"{name} has {len(vals)} entries, expected {len(ROLES)} "
                             f"(unresolved symbol in the table?)")
        return vals
    return {"ROLE_DY": arr("ROLE_DY"), "INK_TOP": arr("INK_TOP"),
            "INK_BOT": arr("INK_BOT"), "CAP_H": arr("CAP_H"),
            "BASELINE": arr("BASELINE")}

def main():
    blob = load()
    m = measure(blob)

    print(f"{'role':9s} {'face':5s} {'box':>4s} {'dy':>3s} "
          f"{'inkTop':>7s} {'inkBot':>7s} {'cap':>4s} {'base':>5s}")
    for r in m:
        print(f"{r['role']:9s} {r['face']:5s} {r['box']:4d} {r['dy']:3d} "
              f"{r['ink_top']:7d} {r['ink_bot']:7d} {r['cap']:4d} {r['baseline']:5d}")

    print("\n// paste into src/ui/gfx.cpp")
    for name, key in (("INK_TOP", "ink_top"), ("INK_BOT", "ink_bot"),
                      ("CAP_H", "cap"), ("BASELINE", "baseline")):
        vals = ", ".join(f"{r[key]:3d}" for r in m)
        print(f"static const int8_t {name}[F_COUNT]{' ' * (9 - len(name))}= {{ {vals} }};")

    if "--check" not in sys.argv:
        return 0

    got, bad = parse_gfx(), 0
    for i, r in enumerate(m):
        for name, key in (("ROLE_DY", "dy"), ("INK_TOP", "ink_top"),
                          ("INK_BOT", "ink_bot"), ("CAP_H", "cap"),
                          ("BASELINE", "baseline")):
            if got[name][i] != r[key]:
                print(f"MISMATCH {r['role']}.{name}: gfx.cpp has {got[name][i]}, "
                      f"measured {r[key]}")
                bad += 1
    print("\ngfx.cpp matches the measured metrics." if not bad
          else f"\n{bad} mismatch(es).")
    return 1 if bad else 0

sys.exit(main())
