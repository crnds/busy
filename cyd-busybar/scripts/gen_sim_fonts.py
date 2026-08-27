#!/usr/bin/env python3
"""Extract TFT_eSPI's real glyph data into simulator.html's GLYPHS blob.

This is what makes simulator.html WYSIWYG rather than an approximation: it
draws the panel's ACTUAL pixels and its textW() is byte-identical to
tft.textWidth(). Sizing an outline face to match cap height does not work here
-- Font 2's 'O' advances 8px at a 10px cap height where a scalable face needs
about 11, so glyphs would overlap and the reported layout would not be the
panel's.

Run after changing TFT_eSPI's version:
    python3 scripts/gen_sim_fonts.py
"""
import json, re, sys, pathlib

FONTS = pathlib.Path(".pio/libdeps/cyd/TFT_eSPI/Fonts")

def strip_comments(text):
    """A trailing // comment sits between the `=` and the `{` in these files."""
    return re.sub(r"//[^\n]*", "", text)

def carray(text, name):
    """Return the integers of a C array declaration called `name`."""
    m = re.search(re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        raise SystemExit(f"array {name} not found")
    return [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+|\d+", m.group(1))]

def pack(rows_bits, w, h):
    """Pack a list of h rows of w booleans into MSB-first bytes, as hex."""
    out = bytearray()
    for y in range(h):
        acc, n = 0, 0
        for x in range(w):
            acc = (acc << 1) | (1 if rows_bits[y][x] else 0)
            n += 1
            if n == 8:
                out.append(acc); acc, n = 0, 0
        if n:
            out.append(acc << (8 - n))
    return out.hex()

# ── FONT 2: one or more bytes per row, MSB left, uncompressed ─────────────
def font2():
    src = strip_comments((FONTS / "Font16.c").read_text())
    widths = carray(src, "widtbl_f16")
    height = 16
    glyphs = []
    for i in range(96):
        w = widths[i]
        # The width table carries a +1 spacing pixel; the bitmap does not.
        ink = w - 1
        name = "chr_f16_%02x" % (0x20 + i)
        m = re.search(re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
        if not m:
            glyphs.append(""); continue
        data = [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]
        per = (ink + 7) // 8 if ink > 0 else 1
        rows = []
        for y in range(height):
            bits = []
            for b in range(per):
                idx = y * per + b
                byte = data[idx] if idx < len(data) else 0
                for k in range(8):
                    bits.append((byte >> (7 - k)) & 1)
            rows.append(bits[:ink] if ink > 0 else [])
        glyphs.append(pack(rows, max(ink, 0), height))
    return {"w": widths, "h": height, "baseline": 13, "g": glyphs}

# ── FONT 4: 8-bit run-length encoded ──────────────────────────────────────
# TFT_eSPI's decoder: each byte is a run. Bit 7 set means a run of INK,
# clear means a run of background; the length is (b & 0x7F) + 1, scanned
# row-major across width x height.
def font4():
    src = strip_comments((FONTS / "Font32rle.c").read_text())
    widths = carray(src, "widtbl_f32")
    height = 26
    glyphs = []
    for i in range(96):
        # Font 2's width table carries a +1 spacing pixel and Font 4's does
        # NOT -- TFT_eSPI does `w = w+6; w /= 8` for font 2 but `w *= height`
        # straight off the table for an RLE face. So the RLE bitmap is the full
        # tabled width.
        w = widths[i]
        ink = w
        name = "chr_f32_%02x" % (0x20 + i)
        m = re.search(re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
        if not m:
            glyphs.append(""); continue
        data = [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]
        total = max(ink, 0) * height
        flat, p = [], 0
        while len(flat) < total and p < len(data):
            b = data[p]; p += 1
            if b & 0x80:
                flat.extend([1] * ((b & 0x7F) + 1))
            else:
                flat.extend([0] * (b + 1))
        flat = (flat + [0] * total)[:total]
        rows = [flat[y * ink:(y + 1) * ink] for y in range(height)] if ink > 0 \
               else [[] for _ in range(height)]
        glyphs.append(pack(rows, max(ink, 0), height))
    return {"w": widths, "h": height, "baseline": 19, "g": glyphs}

# ── FONT 1: GLCD, 5 columns per char, column-major, 6px advance ───────────
def font1():
    src = strip_comments((FONTS / "glcdfont.c").read_text())
    m = re.search(r"font\s*\[\]\s*(?:PROGMEM)?\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        m = re.search(r"glcdfont\s*\[\]\s*(?:PROGMEM)?\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        raise SystemExit("glcdfont table not found")
    data = [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]
    glyphs = []
    for i in range(96):
        c = 0x20 + i
        cols = data[c * 5:(c + 1) * 5]
        rows = [[(cols[x] >> y) & 1 for x in range(5)] for y in range(8)]
        glyphs.append(pack(rows, 5, 8))
    return {"w": [6] * 96, "h": 8, "baseline": 7, "g": glyphs}

def main():
    # Only the faces the firmware actually loads. Extracting Font 4 would let
    # the simulator draw something the panel cannot.
    blob = {"f1": font1(), "f2": font2()}
    js = "const GLYPHS = " + json.dumps(blob, separators=(",", ":")) + ";\n"
    out = pathlib.Path("scripts/sim_fonts.js")
    out.write_text(js)
    print(f"wrote {out} ({len(js)} bytes)")
    # A quick sanity render so a format change is caught here, not on the panel.
    for face, ch in (("f2", "A"), ("f1", "A")):
        f = blob[face]
        i = ord(ch) - 0x20
        w = 5 if face == "f1" else (f["w"][i] - 1 if face == "f2" else f["w"][i])
        hexs = f["g"][i]
        by = bytes.fromhex(hexs)
        per = (w + 7) // 8
        print(f"--- {face} {ch!r} w={w} h={f['h']} ---")
        for y in range(f["h"]):
            line = ""
            for x in range(w):
                b = by[y * per + (x >> 3)]
                line += "#" if (b >> (7 - (x & 7))) & 1 else "."
            print(line)

main()
