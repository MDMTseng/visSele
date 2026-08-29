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
import sys
import zlib
import struct
import math

W, H = 2448, 2048          # the real sensor geometry, so scale behaves as it does live
BG, FG = 26, 232           # dark field, bright part -- the backlit polarity
BLUR = 3                   # box radius; 3 gives an edge a few pixels wide

CX, CY = W / 2.0, H / 2.0


def part_polygon():
    """An asymmetric rounded wedge. Deliberately NOT a regular polygon."""
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
        pts.append((CX + r * math.cos(t), CY + r * math.sin(t) * 0.82))
    return pts


# Interior features. Different sizes and an off-centre placement, so the
# extractor has something to key on other than the outline.
HOLES = [(CX - 250, CY - 150, 105),
         (CX + 300, CY + 120, 70),
         (CX + 40, CY - 330, 45),
         (CX - 60, CY + 300, 45)]
SLOT = (CX - 470, CY + 210, CX - 170, CY + 300)   # x0,y0,x1,y1 rectangle


def render():
    poly = part_polygon()
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


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'sbm_synth.png'
    n = write_png(out, box_blur(render(), BLUR))
    print('%s  %dx%d  %d bytes' % (out, W, H, n))
