# DESIGN.md — the cyd-busybar design system

How this firmware looks and behaves, and why. Read this before changing
anything visual.

**Where it comes from.** The system is the one specified in the repository-root
`DESIGN.md`, which was written for a different product (a Home Assistant
controller on the same panel). That document is the **source** for the visual
system: the tokens, the typography roles, the spacing scale, square corners,
primitive icons, the `vis`-byte component model, dirty-region rendering and the
night palette. Its *functional* constraints — devices, scenes, the three-page
structure — do not apply here, and this file records how the visual system was
applied to a BUSY Bar clone and **where it deviates, with reasons**. Section
numbers below refer to the root document.

The source of truth for values is `include/config.h` (geometry), `src/ui/theme.h`
(colour) and `src/ui/gfx.h` (type). This file describes them.

---

## 1. The medium, and what the product adds to it

Every rule in the source document traces to a property of a 2.8" 320×240
ILI9341 with a resistive digitiser driven by one blocking Arduino task. All of
those still hold. This product adds two constraints of its own:

| Constraint | What it forces |
|---|---|
| **The panel is the product** | The body is one raster and nothing competes with it. At 2× it is exactly the screen's width, so it is full-bleed and there is no card around it. |
| **Panel content is 160×80** | The chrome's faces cannot be used inside it — they do not scale by integers. A second, smaller font system exists purely for the framebuffer. |
| **Content arrives from the network** | Anything drawn on a panel is a caller's, not ours. Ownership has to be legible without a label, and the palette has to survive arbitrary colours (see §3.3). |

---

## 2. Principles

The source document's seven, unchanged in priority order. Three do most of the
work here:

1. **Never let an unreachable device read as a normal state.** A clock that has
   never synced shows `--:--`, not `00:00`. Showing midnight would be a
   calm-looking lie, and this is the one thing the UI must never be.
3. **Derive, don't store.** Panel ownership, the connectivity glyph, the active
   theme chip and every settings value are computed from live state each
   render. Nothing latches, so nothing can fall out of sync.
6. **Never signal with colour alone.** The palette collapses to one hue at
   night, so Wi-Fi down changes the bars from filled to hollow *and* reddens
   them, and a status theme carries its effect as a **shape** as well as a
   colour — after dark every theme is the same hue, and one that differed only
   in colour would be indistinguishable from the rest.

---

## 3. Colour

### 3.1 Fifteen tokens, not seventeen

The source table's `C_WARM` and `C_COOL` describe colour temperature, which
this product has no concept of. Keeping them would contradict *every element
earns its pixels*, so they are gone. The remaining fifteen are unchanged in
value and role.

Dropping them **improved** the night ladder rather than thinning it: `C_WARM`
held level 14, so the source document's one-step `NEUTRAL(13) → WARM(14) →
TEXT3(16)` crowd is now a clear three-step gap from `NEUTRAL` to `TEXT3`. The
only remaining one-step neighbour is `DISABLED(12)`/`NEUTRAL(13)`, still
justified on role separation: `NEUTRAL` is only ever a chip *fill*, `DISABLED`
only ever a *label* on an unfilled control, so the two never appear as the same
kind of mark.

Measured ladder, all fifteen distinct:

```
BG 0 < SURFACE 2 < DIVIDER 3 < ELEVATED 4 < BORDER 6 < DIM 7 < SUCCESS 10
     < DISABLED 12 < NEUTRAL 13 < TEXT3 16 < ACCENT 18 < TEXT2 20
     < WARNING 22 < TEXT 25 < ERROR 31
```

`simulator.html` asserts no collisions plus a minimum separation across an
explicit `MUST_DIFFER` list.

### 3.2 `C_BG` is no longer pinned by an asset

The source document pins `C_BG` to `0x0041` because a generated logo header
bakes it. This firmware has no baked bitmap — the splash mark is drawn from
primitives — so the pin is gone. The value is unchanged because it is still the
right colour, but changing it now costs nothing.

### 3.3 Night mode reaches the panels too

`themeSetNight()` maps the chrome to red only, as in the source. Two additions:

- **The panel stores real colour** and is pushed through `themeMap()` at
  present time. Canvas content arrives from the network in whatever colours the
  caller chose; at night it must not paint green and blue onto a red-only
  screen. That is a luminance map, so a caller's *relative* contrast survives.
- **This is what the one-panel change bought.** The old back panel was one bit
  deep, so every status theme rendered identically in white and the *effect*
  had to carry the entire difference between BUSY and LUNCH. They now show
  their own colours, and the effect is a second channel rather than the only
  one.

---

## 4. Typography

**Three roles, not four.** `F_TITLE` and `F_BODY` over Font 2, `F_MICRO` over
GLCD. The source document's fourth role, `F_NUM` over Font 4, existed for one
large adjustable number, and this firmware's chrome has none — the panels carry
the large content, in their own faces. The source document already names Font 4
as the first thing to trim if flash matters; with no role using it, keeping it
would have been 8.5 KB of glyph data and a table row that can drift.

| Role | Face | Ink (rel. `cy`) | Baseline | Cap | Used for |
|---|---|---|---|---|---|
| `F_TITLE` | Font 2 (16px box) | −7 … +7 | +5 | 10 | Card titles, the clock |
| `F_BODY` | Font 2 (16px box) | −7 … +7 | +5 | 10 | Live values, chip labels, tabs, captions |
| `F_MICRO` | Font 1, GLCD 6×8 | −4 … +3 | +3 | 7 | Last-resort fit only |

`F_TITLE` and `F_BODY` are the same face at the same size. The built-in set has
no bold, so **hierarchy is colour tier alone**. They stay separate roles
because they express intent and because a future face could distinguish them.

**The ink extents differ from the source document's table**, and the difference
is the measurement, not a change: `python3 scripts/font_metrics.py` decodes the
real glyph data and prints these numbers, and `--check` verifies `gfx.cpp`
against them. Font 2's ink runs −7…+7 rather than −5…+7 because these roles
draw the full printable set — brackets and braces reach two rows above the cap
line, and an IP address is not all caps. `F_BODY_INK_BOT` is a `constexpr` in
`gfx.h` so `screen.cpp` can `static_assert` the card's row geometry against it.

**A second font system, for the panel.** `src/canvas/vfont.inc` carries a 3×5
`VF_TINY` and a 5×7 `VF_SMALL`, generated by `scripts/gen_vfont.py`. The
built-in faces cannot be used here: panel text is drawn at 1× through 4× and
they do not scale. Both are column-major, both scale by integers only, and the
lowercase slots of the 3×5 face map to the uppercase forms — at 3px wide there
is no room for an x-height distinction, and a caller asking for TINY is asking
for legibility, not for case.

---

## 5. Spacing and corners

Unchanged. Five spacing values (4, 8, 12, 16, 24) and **square corners
everywhere, no radius token**. A partial repaint cannot strand a corner arc,
and a lone rounded control on a square page reads as a rendering fault.

---

## 6. Iconography

Drawn from primitives, never stored, on a 15×15 grid with a 1px stroke —
unchanged, and for the same first reason: night mode swaps the palette at
runtime, so a baked sprite would render in day colours over a red-only UI.

Four icons, and **every one carries state**:

| Icon | Carries |
|---|---|
| `icoWifi` | The whole connectivity readout: 3 filled bars, or 0 — drawn hollow — in `C_ERROR` |
| `icoBadge` | An overlay dot: the time-not-synced badge |
| `icoSun` / `icoMoon` / `icoClock` | The three settings that are otherwise identical rows of text |

`icoChevron` — a stepper glyph for the old left/right theme control — is gone.
Picking a theme by tapping its own tab needs no direction icon, and DESIGN.md's
own rule is that an icon with no job does not get to stay: "no icon exists
because a row looked bare."

`icoMoon` takes the colour **behind** it: a crescent is a disc minus a disc, and
with no alpha the bite must be painted in the card's own fill.

**The Network card reuses `icoWifi` rather than inventing a fourth glyph**, and
draws it from live state. That is what earns it an icon: it is a readout, not a
label for the row.

---

## 7. Components

The `vis` byte and `ctlColour()` table are unchanged, including
`BV_ACTIVE_OFF`/`C_NEUTRAL` — every selected control is a solid fill with dark
text, and only *which* fill depends on what was selected.

Components: `wCard`, `wChip`, `wToggle`, `wTab`, `wPip`. Two notes:

- **`wTab` draws a 2px underline, not a filled pill.** The source document's
  `wPill` fills the selected cell; its §12 table specifies "label + 2px
  underline". The underline is the minimal form of the same rule — selected
  gets a mark, unselected gets nothing — and it keeps the header quiet, which
  matters more here because the body below it is showing a caller's content and
  should be the loudest thing on screen.
- **There is no `wValue` and no `wStepBtn`.** Neither has a job: no large
  readout, and no stepper. The theme picker used to step through five themes
  with two chevrons; it now picks one directly from a row of `wChip`, which is
  the same component the settings chips already use — a control with a current
  state, not a direction affordance, because a theme genuinely has one.

---

## 8. Layout

### 8.1 Header — y 0..31

Unchanged from the source, and it tiles exactly:

```
26 + 3 × 81 + 51 = 320
│    │          └── clock,   x 269..319   (minute-rate, exactly 5 characters)
│    └───────────── tabs,    x  26..268   (Clock / Status / Settings)
└────────────────── Wi-Fi,   x   0..25
divider at y=31; every region clears 31px tall, so the rule is painted once
```

**Three tabs is not a coincidence, it is the constraint.** The header tiles at
`3 × 81`; a fourth tab does not divide 243. The brief's fourth mode ("Apps")
had no content once the JS app platform went out of scope, so the strip is
Clock / Status / Settings and the arithmetic holds.

The connectivity glyph carries three states, and the second is remapped for
this product — there is no Home Assistant to be unreachable, but there is a
clock that may not have synced, which is a real degraded state:

| State | Rendering |
|---|---|
| Online, time synced | 3 bars in `C_TEXT3` |
| Online, no time yet | 3 bars **plus an amber badge** |
| No network | **0 bars — the same shapes hollow** — in `C_ERROR` |

### 8.2 Body — the display page

```
32 + 160 + 48 = 240, tiling exactly

panel   y  32..191    160 × 80 at ×2 = 320 × 160, full-bleed
strip   y 192..239    a chip OR five theme tabs, depending on mode
```

**There is one panel, because there is one screen.** The earlier build emulated
the BUSY Bar's two displays and split the body between them. That was inventing
a constraint the hardware does not have, and both panels paid for it: the front
got 64px of height, the back got a non-integer 3:2 scale because 64 + 160 would
not fit under a 32px header.

What survives is the **back panel's geometry** — 160×80, the richer layout and
the real BUSY Bar resolution, so draw coordinates from the original API still
land where the caller meant them — with the **front panel's rendering**, full
RGB565. At ×2 that is exactly 320×160: a flat integer scale, no fixed-pattern
compromise, and the 3:2 scaler is gone.

**The raster is full-bleed, so there is no card.** It is exactly the screen's
width; a card would need margins it does not have, and it does not need one —
the raster *is* the content, the way a photograph is, and the screen edge is
its bezel.

**The strip exists because full-bleed costs two things.** It takes away the
side gutters a theme control would have sat in, and it takes away the card
border that signalled ownership. The strip replaces both, and the ownership
half is strictly better than a border: a `BV_ACTIVE` chip carrying the
**owning application's name** says who took the panel, where a border could
only say that somebody had.

**The strip has two mutually exclusive modes, and picks between them the same
way the whole page decides what to draw.**

```
CHIP MODE  — Clock tab, Wi-Fi setup, or a remote application holding the panel
[                        Clock                        ]
 one indicator, full content width, x 16..303

TABS MODE  — the Status tab, and nothing else claiming the panel or the tab
[ BUSY ] [MEETING] [ DND ] [CODING] [LUNCH]
 5 × 54 + 4 × 4 = 286, x 16..301 -- the settings brightness row's own pitch,
 reused rather than a second chip size invented for the strip
```

**Tabs replaced a stepper.** The Status tab used to hold two chevrons that
stepped the current theme forward or back, one at a time, in the space a
full-bleed raster left for them. Naming all five instead removes the guessing:
every option is visible without cycling through the others, and up to four
taps to land on a theme becomes exactly one. The chevrons are gone from
`icons.cpp` entirely rather than left unused — DESIGN.md's own rule for an
icon with no job.

**The chip's label is derived every render**, from the owning application when
the canvas holds the panel, to `WI-FI SETUP` while the AP is up, to `CLOCK`
otherwise. Local sources are `BV_INACTIVE`; a remote one is `BV_ACTIVE` — a
fill and a foreground change, not colour alone.

**A theme tab's label is the folder name, not the JSON's.** `busy` becomes
`BUSY` by swapping any underscore for a space and letting `wChip` uppercase
it -- a one-word folder name like this one needs no swap at all — the same string `themeNameAt()` already returns, so nothing needs
re-parsing five `theme.json` files every render this page is up. If a theme's
own `"label"` field is ever set to something that does not match its folder
name, the tab shows the folder's name and the panel itself still shows the
JSON's — a small, accepted seam, and cheaper than a per-theme label cache kept
in step by hand.

**The setup AP outranks both panel apps, and the strip's mode with them.**
While the AP is up, a panel the canvas does not own shows the SSID and address
instead of the clock or a theme, and the strip shows the `WI-FI SETUP` chip —
never the tab row, even on the Status tab, because there is no theme to pick
while the panel is showing something else. The one gap: a canvas application
holding the panel hides all of it, though the strip then names that
application and the Settings card still shows the AP.

### 8.3 Body — the settings page

```
32 + 4 × 52 = 240, tiling exactly
card    x   8..311 (304 wide), h 48, square corners
content x  16..303 (288 wide)

  1        pad
  1..19    line 1 dirty rect  (icon + title + value, datum at cy = 8)
  20..45   control row        (CTL_DY 20, CTL_H 26)
  2        pad
```

Four cards, each with an icon so the four are distinguishable without reading,
and each naming its value on its own identity line — a segmented control shows
*which* of five is selected but not what the selection *means*.

```
brightness  [AUTO][ 25 ][ 50 ][ 75 ][100]   5 × 54 + 4 × 4 = 286, x 16..301
time        [12H ][24H ][SYNC]              same pitch, three slots
night mode                        [ ▉  ]    a toggle; the whole row is the target
network     [ AP MODE ][RECONNECT][ FORGET ]
                                   3 × 92 + 2 × 4 = 284, x 16..299
```

**The Network row sets its own chip pitch (92px, not 54).** `"RECONNECT"` needs
72px of label and does not fit a 54px chip. Per the source system each row's
pitch is fixed by that row tiling its card, not shared between rows — the
simulator caught the first attempt at 88px chips *plus* a static
`cyd-busybar.local` caption, which needed 116px in the 104px that was left,
which is why the pitch is 92 rather than the original 88: a third chip needed
the room a leftover address readout used to occupy.

**The readout that used to fill the space to the right of the chips is gone,
and `FORGET` took its place.** It showed the setup AP's own address while the
AP was up — a fixed constant, `192.168.4.1`, since `softAP()` always hands out
the same subnet — and that information is already shown, large, on the panel
itself the moment the AP comes up (`setupRender()`, §8.2). Once a third chip
needed the row's remaining width, keeping a static, always-identical, already
-duplicated readout stopped earning its pixels; dropping it is what makes room
for `FORGET` at a pitch wide enough that `RECONNECT` still needs no fallback
face.

**`FORGET` is a genuine reversal of an earlier decision in this document**,
made because it was asked for directly rather than because the reasoning
against it turned out wrong — see §12's note on why the reasoning still holds
and what makes this the exception to it.

`btnRect()` is the **only** function that knows where a control is; the
renderer and the hit test both go through it, which is what stops the drawn
rect and the tappable rect drifting apart.

Two rules carried over verbatim, both load-bearing:

- **Every dirty rect inside a card starts at `CARD_IN_X0`, never `CARD_X`.**
  Filling x 8..15 paints over the card's own border column and erases the
  outline, one repaint at a time.
- **Line 1's dirty rect runs to y+19**, past the cap box its datum centres on,
  because it must cover descenders. Clear only the cap box and a value that
  gets shorter leaves a `g`'s tail on the card forever.

---

## 9. Information hierarchy

On the settings page, the source document's order applies unchanged: what is
this (title, `F_TITLE`, `C_TEXT`) → what is it doing (icon first, then the
value, `C_TEXT2`) → what can I do (the chip row) → chrome.

**On the display page the hierarchy inverts, on purpose.** The panel is the
content and everything around it is frame. The chrome adds no icon, no title
and no state text, because the panel says all of that already. The one thing
the strip adds is **who is drawing** — which is precisely the thing the panel
content cannot say about itself.

---

## 10. Touch and feedback

Targets are generous, and the visual control is frequently smaller than the
thing you can hit:

| Target | Tappable | Drawn |
|---|---|---|
| Settings chip | 54 × 52 (the full row band) | 54 × 26 |
| Settings toggle | the whole row, any x | 44 × 24 track, 18 × 18 knob |
| Theme tab | 54 × 48 (the full strip band) | 54 × 26 |
| Network chip | 92 × 52 (the full row band) | 92 × 26 |
| Tab | 81 × 32 | label + 2px underline |
| The raster | **nothing** | the panel itself |

**The raster is a dead target, deliberately.** It shows a caller's content;
there is nothing there to act on, and accepting a tap would make a phantom
touch look like it did something. Every tappable thing on this page is in the
48px strip.

**The theme tabs appear exactly when the action exists.** They are drawn on the
Status tab, and only while neither the canvas nor the setup AP is claiming the
panel; otherwise the strip falls back to the single chip and nothing in that
space is a theme control. A control that looks tappable must be tappable, and
one that isn't must not — the touch handler tests the identical condition the
renderer does, so the two cannot drift apart.

**The press flash is keyed by kind as well as index**, and expires by *time*,
so every snapshot carries a per-item `vis` byte — a page that skipped it would
leave the tapped control inverted forever.

---

## 11. Performance

Dirty-region rendering is structural. Two rules this product adds:

- **Every panel app gates its own redraw.** The clock keys off the half-second
  colon phase; a theme keys off an `ANIM_TICK_MS` bucket; a countdown off the
  second. Without those, the panel is cleared and re-blitted on every pass of
  `loop()`.
- **`present()` only upscales the dirty box**, tracked in *source* coordinates,
  so a clock tick repaints five glyphs' worth of panel rather than 320 × 160.

Deliberately absent, unchanged from the source: shadows, gradients, glass, and
any alpha-composited effect. RGB565 has no alpha, and a soft edge means a
region can no longer be cleared by filling its own rect.

Measured after this build: **RAM 100,040 B (30.5%), flash 1,000,201 B (31.8%)**.

RAM rose ~21 KB against the two-panel build, and that is the framebuffer: one
160×80 RGB565 buffer is 25,600 B where a 72×16 RGB565 plus a 160×80 one-bit
buffer together came to ~3,900 B. It is the price of the panel being both
larger and in colour, and at 30.5% there is room for it.

---

## 12. Deliberately not built

- **Dialogs, menus and confirmations.** Every on-device action is one tap and
  reversible — including the Wi-Fi controls, and that was the design problem
  they posed. Raising the setup AP *by erasing credentials and rebooting* would
  have been destructive, unreversible and one phantom tap away, so it could not
  go on the device without the confirmation step this UI does not have.
  Making AP mode **additive instead** — it runs `WIFI_AP_STA`, disconnects
  nothing and touches no stored credentials — removes the problem rather than
  guarding it: the tap is now reversible by tapping again, and the rule holds
  unchanged.

  **Erasing credentials, on the other hand, IS now an on-device action** —
  `FORGET`, §8.3 — which reverses what this section used to say here. What
  changed is not the principle but which side of it applies: `wifiForget()`
  does not strand the device, it returns it to the fallback AP a
  never-configured device already boots into, reachable by anyone standing at
  the panel. A tap that returns you to square one rather than to nothing is
  reversible in the sense principle 4 (one tap acts, everywhere) cares about,
  even though the specific credentials it erases are gone for good.
- **A fourth tab.** The header tiles at `3 × 81`. Adding one means re-cutting
  the bar.
- **Charts, forms and text entry.** Nothing here is typed or historical.

---

## 13. Verification

`simulator.html` is the primary tool, and three things about it are
load-bearing rather than conveniences:

1. **It draws the panel's actual glyphs** — both font systems. The chrome's
   blob is the real byte data decoded out of `Font16.c` and `glcdfont.c`, and
   the panel's is the same table the firmware compiles in, so `textW()` is
   byte-identical to `tft.textWidth()`. Regenerate with
   `scripts/gen_sim_fonts.py` and `scripts/gen_vfont.py`.
2. **It re-checks every `static_assert` from `screen.cpp`**, plus things the
   compiler cannot see: that line-1 descenders stop before the control row,
   that the dirty rect covers them, that the strip's chip fills its content
   width, that the five theme tabs tile the strip at the settings row's own
   pitch, that every tab label fits its budget through the same fallback and
   truncation path a settings chip uses, that the chip holds an
   `application_name` arriving from the network, that every theme label fits
   the panel, and the night ladder.
3. **State is deep-linkable**: `?app=2&night=1&conn=2&ap=1&owned=1&theme=dnd`.
   `ap` takes 0 (off), 1 (on, AP+STA) and 2 (fallback — no credentials, so the
   chip is disabled), because those three render differently.

`node scripts/sim_check.js` runs the sweep headlessly — 61 checks across 540
states, with identical failures collapsed to one line so a single layout bug
does not print 540 times — and exits non-zero on any failure. It also runs a **stress case**
with a deliberately overlong value and *requires* the truncation warning to
fire: a degrade path that only runs when a real label overflows is one nobody
finds out is broken until a label does.

**Current status: 540 states clean, truncation verified, `font_metrics.py
--check` matching.**

**Not verified, and worth a look on the panel:** whether Font 2's 10px caps are
readable at arm's length; whether the 5×7 panel face holds up at ×1 for the
date and SSID lines now that the panel is the whole screen; whether `MEETING`
reading in the 6×8 fallback face beside four `BUSY`/`DND`/`CODING`/`LUNCH`
tabs in Font 2 reads as an inconsistency or goes unnoticed at a glance; the LDR
polarity, which varies by board revision; and touch accuracy on the strip,
which sits in the extrapolated band of the calibration fit.
