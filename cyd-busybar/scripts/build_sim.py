#!/usr/bin/env python3
"""Assemble simulator.html from its template plus the generated glyph blobs.

    python3 scripts/gen_sim_fonts.py   # TFT_eSPI's faces, for the chrome
    python3 scripts/gen_vfont.py       # the two panel faces
    python3 scripts/build_sim.py
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
tpl = (ROOT / "scripts" / "simulator.tpl.html").read_text()
for token, src in (("/*__FONTS__*/", "sim_fonts.js"), ("/*__VFONT__*/", "sim_vfont.js")):
    path = ROOT / "scripts" / src
    if not path.exists():
        raise SystemExit(f"{src} missing -- run the generators first")
    tpl = tpl.replace(token, path.read_text().strip())
out = ROOT / "simulator.html"
out.write_text(tpl)
print(f"wrote {out.name} ({len(tpl)} bytes)")
