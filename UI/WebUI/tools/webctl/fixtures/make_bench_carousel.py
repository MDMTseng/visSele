# Build a fake-camera carousel for one bench profile.
#
#   python make_bench_carousel.py this_bench_telecentric
#   python make_bench_carousel.py fine_5um
#   python make_bench_carousel.py --mmpp 0.008 custom_name
#
# The frames a machine would actually produce: the SAME physical part, drawn at
# the mm/px that bench's lens_calib.json says the instrument has. Point the core
# at the folder with FORCE_BMP_CAROUSEL and it is that machine, deterministically.
#
# The point of having several is that a scale bug is invisible on one bench. Any
# consistent-but-wrong mmpp measures a def to a plausible size and nothing looks
# amiss; it is only when the same part comes back the same size through two
# different instruments that the scale has actually been tested.
#
# frames.json is written next to the frames: the source mmpp and the part's true
# dimensions, so a test asks the fixture what the right answer is instead of
# carrying a hard-coded number that stops matching the day this is regenerated.
import io
import json
import os
import shutil
import sys

import make_sbm_fixture_image as gen

HERE = os.path.dirname(os.path.abspath(__file__))
FRAMES = 3          # enough for the carousel to advance; they are identical on
                    # purpose -- a moving part is a different fixture's job


def mmpp_of(bench):
    """The bench's own instrument scale, from the file the core writes."""
    p = os.path.join(HERE, 'benches', bench, 'lens_calib.json')
    with io.open(p, encoding='utf-8') as fh:
        d = json.load(fh)
    um = d.get('um_per_px')
    if isinstance(um, (int, float)) and um > 0:
        return um / 1000.0
    m = d.get('m')
    if isinstance(m, (int, float)) and m > 0:
        return 1.0 / m
    # No honest scale in the file. Refused rather than guessed: a carousel built
    # at an invented mmpp would make an uncalibrated machine look calibrated.
    raise SystemExit(
        'bench %s has no scale in lens_calib.json -- pass --mmpp explicitly '
        'if you mean to build frames for it anyway' % bench)


if __name__ == '__main__':
    args = sys.argv[1:]
    mmpp = None
    if '--mmpp' in args:
        i = args.index('--mmpp')
        mmpp = float(args[i + 1]); del args[i:i + 2]
    if not args:
        raise SystemExit('usage: make_bench_carousel.py [--mmpp X] <bench-name>')
    bench = args[0]
    if mmpp is None:
        mmpp = mmpp_of(bench)

    out = os.path.join(HERE, 'journey_carousel_' + bench)
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)

    first = os.path.join(out, 'frame_01.png')
    k = gen.upp(mmpp)
    gen.write_png(first, gen.box_blur(gen.render(k), gen.BLUR))
    for i in range(2, FRAMES + 1):
        shutil.copyfile(first, os.path.join(out, 'frame_%02d.png' % i))

    meta = gen.sidecar(os.path.join(out, 'frames.json'), mmpp, gen.PART_R_MM)
    meta['bench'] = bench
    meta['frames'] = FRAMES
    with io.open(os.path.join(out, 'frames.json'), 'w', encoding='utf-8') as fh:
        json.dump(meta, fh, indent=2)

    print('%s  %d frames  mmpp=%g  part r=%gmm (%.0f px)'
          % (out, FRAMES, mmpp, gen.PART_R_MM, gen.SHAPE_R_UNITS * k))
