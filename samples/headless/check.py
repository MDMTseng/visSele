# Compare one --insp report against samples/headless/expect.json.
#
# A separate file rather than a heredoc inside run.sh: a script piped to a
# Windows python from an MSYS shell can fail with no output and a meaningless
# exit code, which is exactly what a port team does not need from the tool that
# is supposed to tell them whether their port works.
#
#   python3 check.py <expect.json> <report.json> [--bless]
import io
import json
import sys

expect_path, report_path = sys.argv[1], sys.argv[2]
bless = '--bless' in sys.argv[3:]

with io.open(expect_path, encoding='utf-8') as fh:
    exp = json.load(fh)
with io.open(report_path, encoding='utf-8') as fh:
    rep = json.load(fh)

objs = rep['reports'][0]['reports']
print('objects: %d (expected %d)' % (len(objs), exp['objects']))

if bless:
    if len(objs) != 1:
        print('refusing to bless a run that found %d object(s)' % len(objs))
        sys.exit(4)
    o = objs[0]
    exp['objects'] = len(objs)
    exp['object0'] = {'cx_mm': o['cx'], 'cy_mm': o['cy'], 'rotate_deg': o['rotate'],
                      'scale': o['scale'], 'similarity': o['similarity']}
    with io.open(expect_path, 'w', encoding='utf-8') as fh:
        fh.write(json.dumps(exp, indent=2, ensure_ascii=False) + '\n')
    print('blessed -- expect.json now records THIS run')
    sys.exit(0)

if len(objs) != exp['objects']:
    # THE FAILURE THAT READS LIKE SUCCESS. An empty report is not an error: it
    # means nothing was located, and --insp still exits 0. A port with a broken
    # locator therefore looks fine until something checks the count.
    print('FAIL: located %d object(s), expected %d' % (len(objs), exp['objects']))
    print('      an empty report is what a def that failed to train looks like;'
          ' run with the core log visible (INSP_LOG_KEEP_STDERR=1) to see why')
    sys.exit(1)

o, e, tol = objs[0], exp['object0'], exp['tolerance']
bad = []
for key, rkey in (('cx_mm', 'cx'), ('cy_mm', 'cy'),
                  ('rotate_deg', 'rotate'), ('scale', 'scale')):
    d = abs(o[rkey] - e[key])
    ok = d <= tol[key]
    print('  %-11s %-20s expected %-20s diff %-12.6g %s'
          % (key, repr(o[rkey]), repr(e[key]), d, 'ok' if ok else 'OUT OF TOLERANCE'))
    if not ok:
        bad.append(key)

s_ok = o['similarity'] >= tol['similarity_min']
print('  %-11s %-20s floor    %-20s %s'
      % ('similarity', repr(o['similarity']), repr(tol['similarity_min']),
         'ok' if s_ok else 'BELOW FLOOR'))
if not s_ok:
    bad.append('similarity')

if bad:
    print('FAIL: ' + ', '.join(bad))
    sys.exit(1)
print('PASS')
