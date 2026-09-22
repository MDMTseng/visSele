# Draw the synthetic part the SBM fixture is built from.
#
#   python make_sbm_fixture_image.py [out.png]
#
# WHY SYNTHETIC, AND WHY IT IS CHECKED IN AS A SCRIPT
#
# This repository is public. A fixture made from a real frame publishes a
# customer's part -- its outline, its finish, its hole pattern -- to anyone who
# clones it. A drawn part cannot leak anything, and it is also a better fixture:
# every pixel is decided here, so two people on two machines get byte-identical
# input, and a test that fails means the code changed rather than the camera did.
#
# WHAT THE SHAPE HAS TO SATISFY, none of it arbitrary:
#
#   * it must NOT touch the border. trainShapeMatcher takes the largest
#     connected component that does not reach the edge -- the backlit field
#     spans the frame and touches it, so a part that also touches it can lose
#     to the field and collapse the origin to the image centre.
#   * it must be ASYMMETRIC under rotation. A shape with n-fold symmetry has n
#     equally good poses, the matcher picks one, and a rotation sweep then
#     reports a residual of 360/n degrees that looks like a catastrophic
#     localization error and is not one.
#   * it needs interior detail. line2Dup matches gradients; a plain blob puts
#     every feature on one closed contour, which locates position well and
#     angle badly.
#   * the edges want to be soft, not a hard step. A one-pixel step is a
#     gradient no real lens produces, and features extracted from it move
#     differently under scaling than features from a real part would.
#
# Pure stdlib: no numpy, no PIL. The blur is two separable running-sum passes,
# which is why this is fast enough to be worth doing honestly rather than
# leaving hard edges.
#
# THE PART IS DEFINED IN MILLIMETRES, NOT IN PIXELS.
#
# A fixture that is a fixed grid of pixels can only ever test one instrument.
# Point the same bytes at a machine with a finer lens and you have not modelled
# that machine -- you have modelled a part that shrank to match it, which is the
# one thing that never happens in the field. What actually changes with the lens
# is how many PIXELS a part of unchanged physical size covers.
#
# So the shape below is millimetres, and --mmpp renders it for a given
# instrument: the same object, photographed through a different lens. Every
# output carries a sidecar .json naming the scale it was drawn at and the part's
# true size, so a test can assert what it should assert -- that the measured
# size comes back the same on every bench -- instead of comparing pixel counts
# that are meant to differ.
#
# SIZE CONSTRAINT: the part must fit the sensor at the FINEST mmpp any profile
# uses (5.2 um/px today -> 12.7 x 10.6 mm of field). It must also not touch the
# border, see above. 4 mm of base radius clears both with room to spare.
import sys
import json
import zlib
import struct
import math

W, H = 2448, 2048          # the real sensor geometry, so scale behaves as it does live
BG, FG = 26, 232           # dark field, bright part -- the backlit polarity
BLUR = 3                   # box radius; 3 gives an edge a few pixels wide

CX, CY = W / 2.0, H / 2.0

# The shape is authored in the arbitrary units below (base radius 620) because
# that is how it was drawn and tuned; PART_R_MM says what that base radius is in
# millimetres, and every other number scales with it.
SHAPE_R_UNITS = 620.0
PART_R_MM = 4.0
MMPP_DEFAULT = 0.01388594314      # this bench's telecentric lens


def upp(mmpp, part_r_mm=PART_R_MM):
    """Pixels per shape unit at this instrument's scale."""
    return (part_r_mm / SHAPE_R_UNITS) / mmpp


def part_polygon(k):
    """An asymmetric rounded wedge. Deliberately NOT a regular polygon.

    `k` is pixels per shape unit, so the same outline comes out physically
    identical on every instrument and merely lands on more or fewer pixels."""
    # A closed outline in polar form: a base radius modulated so no rotation
    # maps it onto itself. The 1.0 harmonic is what breaks every symmetry.
    pts = []
    n = 720
    for i in range(n):
        t = 2.0 * math.pi * i / n
        r = (620.0
             + 150.0 * math.cos(t)             # one lobe: kills all symmetry
             + 70.0 * math.cos(3.0 * t + 0.7)  # three-fold texture on top of it
             + 34.0 * math.cos(5.0 * t + 2.1))
        pts.append((CX + k * r * math.cos(t), CY + k * r * math.sin(t) * 0.82))
    return pts


# Interior features. Different sizes and an off-centre placement, so the
# extractor has something to key on other than the outline.
HOLES_U = [(-250, -150, 105),
           (300, 120, 70),
           (40, -330, 45),
           (-60, 300, 45)]
SLOT_U = (-470, 210, -170, 300)                   # x0,y0,x1,y1, in shape units


def render(k):
    poly = part_polygon(k)
    HOLES = [(CX + hx * k, CY + hy * k, hr * k) for (hx, hy, hr) in HOLES_U]
    SLOT = (CX + SLOT_U[0] * k, CY + SLOT_U[1] * k,
            CX + SLOT_U[2] * k, CY + SLOT_U[3] * k)
    img = bytearray([BG]) * (W * H)

    # Scanline fill of the outline. Row spans, not per-pixel tests: 2448x2048
    # per-pixel in Python is minutes, this is under a second.
    for y in range(H):
        yc = y + 0.5
        xs = []
        for i in range(len(poly)):
            x0, y0 = poly[i]
            x1, y1 = poly[(i + 1) % len(poly)]
            if (y0 <= yc < y1) or (y1 <= yc < y0):
                xs.append(x0 + (yc - y0) * (x1 - x0) / (y1 - y0))
        xs.sort()
        row = y * W
        for i in range(0, len(xs) - 1, 2):
            a = max(0, int(xs[i] + 0.5))
            b = min(W, int(xs[i + 1] + 0.5))
            if b > a:
                img[row + a:row + b] = bytearray([FG]) * (b - a)

    # Punch the interior detail back out to the field value.
    for (hx, hy, hr) in HOLES:
        r2 = hr * hr
        for y in range(max(0, int(hy - hr)), min(H, int(hy + hr) + 1)):
            dy = y - hy
            dx = math.sqrt(max(0.0, r2 - dy * dy))
            a, b = max(0, int(hx - dx)), min(W, int(hx + dx))
            if b > a:
                img[y * W + a:y * W + b] = bytearray([BG]) * (b - a)
    x0, y0, x1, y1 = (int(v) for v in SLOT)
    for y in range(max(0, y0), min(H, y1)):
        img[y * W + max(0, x0):y * W + min(W, x1)] = bytearray([BG]) * (min(W, x1) - max(0, x0))
    return img


def box_blur(img, radius):
    """Two separable running-sum passes. Softens the step edge into a ramp."""
    k = 2 * radius + 1
    tmp = bytearray(W * H)
    for y in range(H):
        row = y * W
        acc = img[row] * (radius + 1)
        for i in range(radius):
            acc += img[row + min(i, W - 1)]
        for x in range(W):
            tmp[row + x] = acc // k
            acc += img[row + min(x + radius + 1, W - 1)] - img[row + max(x - radius, 0)]
    out = bytearray(W * H)
    for x in range(W):
        acc = tmp[x] * (radius + 1)
        for i in range(radius):
            acc += tmp[min(i, H - 1) * W + x]
        for y in range(H):
            out[y * W + x] = acc // k
            acc += tmp[min(y + radius + 1, H - 1) * W + x] - tmp[max(y - radius, 0) * W + x]
    return out


def write_png(path, img):
    """8-bit greyscale PNG. Hand-rolled so this needs no image library."""
    raw = bytearray()
    for y in range(H):
        raw.append(0)                       # filter type 0 (None)
        raw += img[y * W:(y + 1) * W]
    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 0, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(bytes(raw), 9))
           + chunk(b'IEND', b''))
    with open(path, 'wb') as fh:
        fh.write(png)
    return len(png)


def _part_area_mm2(k, mmpp):
    """Shoelace on the outline, minus the interior detail, converted to mm^2."""
    poly = part_polygon(k)
    a = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        a += x0 * y1 - x1 * y0
    px2 = abs(a) / 2.0
    for (_, _, hr) in HOLES_U:
        px2 -= math.pi * (hr * k) ** 2
    px2 -= abs((SLOT_U[2] - SLOT_U[0]) * k) * abs((SLOT_U[3] - SLOT_U[1]) * k)
    return px2 * mmpp * mmpp


def sidecar(path, mmpp, part_r_mm):
    """What this picture IS, next to the picture.

    The settings travel with the frame rather than living in whichever test
    happens to load it: a folder of frames is then self-describing, and a test
    that wants the part's true size asks the fixture instead of hard-coding a
    number that silently stops matching when the fixture is regenerated."""
    k = upp(mmpp, part_r_mm)
    meta = {
        'source_mmpp': mmpp,
        # What this frame was "shot" at. 1 ms is full brightness by definition,
        # so a fixture drawn at full contrast says 1 -- and a fake camera set to
        # half that must return half the light.
        'source_exposure_ms': 1.0,
        'levels': {'field': BG, 'part': FG},
        'image': {'width': W, 'height': H,
                  'fov_mm': [W * mmpp, H * mmpp]},
        'part': {
            'base_radius_mm': part_r_mm,
            # The outline's extremes, in mm, from the same harmonics the
            # polygon uses -- what a correct measurement of this part returns.
            'max_radius_mm': (SHAPE_R_UNITS + 150.0 + 70.0 + 34.0) * k * mmpp,
            'min_radius_mm': (SHAPE_R_UNITS - 150.0 - 70.0 - 34.0) * k * mmpp * 0.82,
            'centre_px': [CX, CY],
            # The outline's true area in mm^2, less the holes and the slot --
            # what a correct frame of this part covers, whatever the lens. A
            # test can compare it against the bright area it actually received
            # and catch a fake camera that ignored the scale.
            'area_mm2': _part_area_mm2(k, mmpp),
        },
        'note': 'generated by make_sbm_fixture_image.py -- the part is defined in mm '
                'and rendered for this mmpp, so it is the same object on every bench',
    }
    with open(path, 'w', encoding='utf-8') as fh:
        json.dump(meta, fh, indent=2)
    return meta


if __name__ == '__main__':
    args = sys.argv[1:]
    out, mmpp, part_r_mm = 'sbm_synth.png', MMPP_DEFAULT, PART_R_MM
    i = 0
    while i < len(args):
        if args[i] == '--mmpp':
            mmpp = float(args[i + 1]); i += 2
        elif args[i] == '--part-mm':
            part_r_mm = float(args[i + 1]); i += 2
        else:
            out = args[i]; i += 1
    k = upp(mmpp, part_r_mm)
    n = write_png(out, box_blur(render(k), BLUR))
    meta = sidecar(out.rsplit('.', 1)[0] + '.json', mmpp, part_r_mm)
    print('%s  %dx%d  %d bytes  mmpp=%g  part r=%gmm (%.0f px)'
          % (out, W, H, n, mmpp, part_r_mm, SHAPE_R_UNITS * k))
