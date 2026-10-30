#!/usr/bin/env python3
"""panel.py: writes res/Mouth.svg and src/layout.hpp from one layout table.

Labels become paths (DejaVu Sans from the Rack install, or the font named by RACK_FONT), so the SVG has no <text>,
as the VCV panel guide asks. The widgets read the same table through layout.hpp, so art and components cannot drift.
House style as in My lil Looper. Run from anywhere: py -3.11 tools/panel.py   (needs fontTools).
Author: Ari Klopfer / A_Development. MIT
"""
import math
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

H = 128.5
W = 5.08 * 18
BAND = 8.6
INK, INK2 = '#16161a', '#202027'
LIME, MAGENTA, BLUE, WHITE, DIM = '#c8ff3a', '#ff3d9a', '#5ac8e6', '#f2f2f4', '#8d8d96'
FONT = os.environ.get('RACK_FONT', r'C:\Program Files\VCV\Rack2Free\res\fonts\DejaVuSans.ttf')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Round parts: radius in mm, from the Rack component SVGs at 75 dpi: knob Rogan1PSWhite, button VCVLightButton,
# bezel VCVLightBezel, lamp MediumLight. Jacks by role, ring coloured: seq = gate / trigger (blue), play = pitch and
# audio (lime), cv = modulation (magenta). Inputs get a ring, outputs a dark plate with the ring colour.
RADIUS = {'knob': 6.7, 'button': 3.1, 'bezel': 3.6, 'lamp': 1.7, 'play': 4.0, 'out_play': 4.0, 'out_seq': 4.0, 'out_cv': 4.0}
LABEL_SIZE = {'knob': 2.6, 'button': 2.4, 'bezel': 2.4, 'lamp': 2.0, 'play': 2.4, 'out_play': 2.6, 'out_seq': 2.6, 'out_cv': 2.6}
RING_COLOR = {'play': LIME}
OUT_COLOR = {'out_play': LIME, 'out_seq': BLUE, 'out_cv': MAGENTA}
PAD_R = 4.8                     # dark plate behind an output jack
RING_R = RADIUS['play'] + 0.7   # ring around an input jack

font = TTFont(FONT)
glyphs, cmap, upm = font.getGlyphSet(), font.getBestCmap(), font['head'].unitsPerEm


def width(s, size):
    """Advance width of the string in mm at this em size."""
    return sum(glyphs[cmap[ord(c)]].width for c in s) * size / upm


def text(s, x, y, size, fill, anchor='middle', stroke=0.0):
    """One <path> for the string: baseline at y, em size in mm, optional stroke for a bolder face."""
    k = size / upm
    w = width(s, size)
    pen_x = x - w / 2 if anchor == 'middle' else x - w if anchor == 'end' else x
    d = []
    for c in s:
        g = glyphs[cmap[ord(c)]]
        pen = SVGPathPen(glyphs)
        g.draw(TransformPen(pen, (k, 0, 0, -k, pen_x, y)))
        d.append(pen.getCommands())
        pen_x += g.width * k
    bold = ' stroke="%s" stroke-width="%g" stroke-linejoin="round"' % (fill, stroke) if stroke else ''
    return '<path fill="%s"%s d="%s"/>' % (fill, bold, ' '.join(d))


# ---- the title band, as in My lil Looper: Rack's screws sit in its corners (5.08 mm squares at x 5.08 and
# W - 10.16), so the title starts right of the left one and the face sits left of the right one
TITLE, TITLE_X, TITLE_Y, TITLE_SIZE = 'MY LIL MOUTH', 11.2, 6.3, 4.3
MASCOT_X = W - 14.46
MASCOT_R = BAND * 0.36


def mascot(cx, cy, r):
    """The My lil face: magenta disc, dot eyes, a flat mouth (src/TalkingFace.hpp paints the live mouth over it)."""
    e = r * 0.37
    return ''.join([
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx, cy, r, MAGENTA),
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx - e, cy - r * 0.32, r * 0.145, INK),
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx + e, cy - r * 0.32, r * 0.145, INK),
        '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="%g" stroke-linecap="round"/>'
        % (cx - r * 0.58, cy + r * 0.45, cx + r * 0.58, cy + r * 0.45, INK, r * 0.16),
    ])


def band_boxes():
    return [('title', (TITLE_X, TITLE_Y - 0.73 * TITLE_SIZE, TITLE_X + width(TITLE, TITLE_SIZE), TITLE_Y)),
            ('mascot', (MASCOT_X - MASCOT_R, BAND / 2 - MASCOT_R, MASCOT_X + MASCOT_R, BAND / 2 + MASCOT_R))]


# ---- layout: five columns 16.5 mm apart; controls on top, then the 12 note lamps, the trigger row, the jack zone
X = (12.5, 29.0, 45.5, 62.0, 78.5)
ROW_TOP, ROW_LAMP, ROW_TRAIN = 24.0, 43.0, 60.0
ROW_J = (89.0, 102.15, 115.3)
LAMP_X0, LAMP_DX = 9.97, 6.5
NOTES = ('C', '', 'D', '', 'E', 'F', '', 'G', '', 'A', '', 'B')
ZONE_X, ZONE_Y = 2.5, 82.0

MOUTH = [
    ('GAIN_PARAM', 'knob', X[0], ROW_TOP, 'GAIN'),
    ('THRESH_PARAM', 'knob', X[1], ROW_TOP, 'THRESH'),
    ('CAL_PARAM', 'button', X[2], ROW_TOP, 'CAL'),
    ('LOCK_PARAM', 'knob', X[3], ROW_TOP, 'LOCK'),
    ('GLIDE_PARAM', 'knob', X[4], ROW_TOP, 'GLIDE'),
] + [('NOTE%d_LIGHT' % i, 'lamp', LAMP_X0 + LAMP_DX * i, ROW_LAMP, n) for i, n in enumerate(NOTES)] + [
    ('TRAIN%d_PARAM' % i, 'bezel', X[i], ROW_TRAIN, 'TRAIN %d' % (i + 1)) for i in range(4)] + [
    ('SENS_PARAM', 'knob', X[4], ROW_TRAIN, 'SENS'),
    ('NOTE_OUTPUT', 'out_play', X[0], ROW_J[0], 'NOTE'),
    ('RAW_OUTPUT', 'out_play', X[1], ROW_J[0], 'RAW'),
    ('GATE_OUTPUT', 'out_seq', X[2], ROW_J[0], 'GATE'),
    ('VEL_OUTPUT', 'out_cv', X[3], ROW_J[0], 'VEL'),
    ('IN_INPUT', 'play', X[4], ROW_J[0], 'IN'),
] + [('TRIG%d_OUTPUT' % i, 'out_seq', X[i], ROW_J[1], 'TRIG %d' % (i + 1)) for i in range(4)] + [
    ('X_OUTPUT', 'out_cv', X[0], ROW_J[2], 'X'),
    ('Y_OUTPUT', 'out_cv', X[1], ROW_J[2], 'Y'),
    ('BRIGHT_OUTPUT', 'out_cv', X[2], ROW_J[2], 'BRIGHT'),
    ('ENV_OUTPUT', 'out_cv', X[3], ROW_J[2], 'ENV'),
]


def extras():
    return ['<rect x="%g" y="%g" width="%g" height="%g" rx="2" fill="%s"/>' % (ZONE_X, ZONE_Y, W - 2 * ZONE_X, 123.7 - ZONE_Y, INK2)]


def is_out(kind):
    return kind in OUT_COLOR


def reach(kind):
    return RING_R if kind in RING_COLOR else PAD_R if is_out(kind) else RADIUS[kind]


def label_at(kind, y, row_bottom):
    """Baseline, size, colour and stroke of the label under a part. Labels in a row share the row's lowest part."""
    colour, stroke = (OUT_COLOR[kind], 0.25) if is_out(kind) else (LIME, 0.2) if kind == 'lamp' else (WHITE, 0.18)
    low = y + PAD_R if is_out(kind) else row_bottom[y]
    return low + 2.8, LABEL_SIZE[kind], colour, stroke


def row_bottoms(layout):
    rb = {}
    for _, kind, x, y, _ in layout:
        if not is_out(kind):
            rb[y] = max(rb.get(y, 0), y + reach(kind))
    return rb


def label_box(x, base, size, label):
    w = width(label, size)
    return x - w / 2, base - 0.73 * size, x + w / 2, base


SCREW = 5.08


def screws():
    return [(x, y, x + SCREW, y + SCREW) for x in (SCREW, W - 2 * SCREW) for y in (0, H - SCREW)]


def box_hits_circle(b, cx, cy, r):
    nx, ny = min(max(cx, b[0]), b[2]), min(max(cy, b[1]), b[3])
    return math.hypot(cx - nx, cy - ny) < r


def boxes_hit(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def problems(layout):
    """Everything on the panel that touches something else or leaves the panel: parts (rings, plates), their labels,
    the band's title and face, Rack's four screws. Parts below the jack zone's top stay inside its side margin.
    Must be empty."""
    bad = []
    parts = [(n, x, y, reach(k)) for n, k, x, y, _ in layout]
    rb = row_bottoms(layout)
    labels = []
    for n, k, x, y, label in layout:
        if label:
            base, size, _, _ = label_at(k, y, rb)
            labels.append((n + ' label', label_box(x, base, size, label)))
    for j, (a, x, y, r) in enumerate(parts):
        for b, x2, y2, r2 in parts[j + 1:]:
            if math.hypot(x - x2, y - y2) < r + r2:
                bad.append('%s touches %s' % (a, b))
        if x - r < 0 or x + r > W or y - r < BAND or y + r > H:
            bad.append('%s leaves the panel or enters the band' % a)
        if y + r > ZONE_Y and (x - r < ZONE_X or x + r > W - ZONE_X):
            bad.append('%s leaves the jack zone' % a)
        if y - r < ZONE_Y < y + r:
            bad.append('%s straddles the jack zone edge' % a)
        for name, box in labels:
            if name != a + ' label' and box_hits_circle(box, x, y, r):
                bad.append('%s covers %s' % (a, name))
        for s in screws():
            if box_hits_circle(s, x, y, r):
                bad.append('%s under a screw' % a)
    boxes = labels + band_boxes()
    for j, (a, box) in enumerate(boxes):
        if box[0] < 0 or box[2] > W or box[3] > H:
            bad.append('%s leaves the panel' % a)
        if box[1] < ZONE_Y < box[3]:
            bad.append('%s straddles the jack zone edge' % a)
        for b, other in boxes[j + 1:]:
            if boxes_hit(box, other):
                bad.append('%s overlaps %s' % (a, b))
        for s in screws():
            if boxes_hit(box, s):
                bad.append('%s under a screw' % a)
    return bad


def panel(layout):
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="%gmm" height="%gmm" viewBox="0 0 %g %g">' % (W, H, W, H),
        '<rect width="%g" height="%g" fill="%s"/>' % (W, H, INK),
        '<rect width="%g" height="%g" fill="%s"/>' % (W, BAND, LIME),
        text(TITLE, TITLE_X, TITLE_Y, TITLE_SIZE, INK, 'start', 0.39), mascot(MASCOT_X, BAND / 2, MASCOT_R),
    ] + extras()
    rb = row_bottoms(layout)
    for _, kind, x, y, label in layout:
        if kind in RING_COLOR:
            out.append('<circle cx="%g" cy="%g" r="%g" fill="none" stroke="%s" stroke-width="0.5"/>' % (x, y, RING_R, RING_COLOR[kind]))
        elif is_out(kind):
            out.append('<circle cx="%g" cy="%g" r="%g" fill="%s" stroke="%s" stroke-width="0.5"/>' % (x, y, PAD_R, INK, OUT_COLOR[kind]))
        if label:
            base, size, colour, stroke = label_at(kind, y, rb)
            out.append(text(label, x, base, size, colour, stroke=stroke))
    out.append(text('A_DEVELOPMENT', W / 2, 126.4, 2.4, DIM))
    out.append('</svg>')
    return '\n'.join(out) + '\n'


def header(layout):
    lines = ['// layout.hpp: generated by tools/panel.py, do not edit. Component centres in mm.', '#pragma once',
             'namespace layout {', 'namespace mouth {']
    lines += ['constexpr float %s[2] = {%.2ff, %.2ff};' % (i, x, y) for i, _, x, y, _ in layout]
    lines += ['constexpr float FACE[3] = {%.2ff, %.2ff, %.3ff};   // mascot centre x, y, radius (TalkingFace)' % (MASCOT_X, BAND / 2, MASCOT_R)]
    lines += ['}', '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    bad = problems(MOUTH)
    if bad:
        raise SystemExit('panel problems: ' + '; '.join(bad))
    files = {os.path.join('res', 'Mouth.svg'): panel(MOUTH), os.path.join('src', 'layout.hpp'): header(MOUTH)}
    for rel, body in files.items():
        os.makedirs(os.path.join(ROOT, os.path.dirname(rel)), exist_ok=True)
        with open(os.path.join(ROOT, rel), 'w', encoding='utf-8', newline='\n') as f:
            f.write(body)
    print('wrote', ', '.join(files), '(%d components)' % len(MOUTH))
