#!/usr/bin/env python3
"""Zip the built WebUI as an overlay you drop onto an installed app.

    python tools/make_overlay.py [out.zip]

An overlay is not an update package: it carries no info.json, no boot.js and no
manifest, so the launcher will not install it and will not verify it. It is the
WebUI folder and nothing else, for the case where only the UI changed and the
core on the machine is already the right one. When the core changed too, build a
real package with UI/Launcher/tools/make_package.py instead -- an overlay would
leave that machine running new UI against old core.

The zip's single top-level entry is WebUI/, laid out exactly as the package lays
it out (dist/ contents at the root, plus resource/), so updating a machine is
"replace the WebUI folder" with no path guessing.
"""

import io
import os
import sys
import time
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
WEBUI = os.path.dirname(HERE)
DIST = os.path.join(WEBUI, 'dist')
RES = os.path.join(WEBUI, 'resource')


def add_tree(zf, src, prefix):
    n = 0
    for root, dirs, files in os.walk(src):
        dirs.sort()
        for name in sorted(files):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, src).replace(os.sep, '/')
            zf.write(full, prefix + '/' + rel)
            n += 1
    return n


def main():
    if not os.path.isdir(DIST):
        sys.exit('no dist/ -- run "npm run build" first')

    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        WEBUI, '..', '..', 'overlay_webui_%s.zip' % time.strftime('%Y%m%d_%H%M'))
    out = os.path.abspath(out)

    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as zf:
        n = add_tree(zf, DIST, 'WebUI')
        # The build's copy-resource step already drops resource/ inside dist/;
        # only fall back to the source copy if that step did not run.
        if os.path.isdir(RES) and not os.path.isdir(os.path.join(DIST, 'resource')):
            n += add_tree(zf, RES, 'WebUI/resource')
        zf.writestr('README.txt',
                    'WebUI overlay, built %s.\r\n\r\n'
                    'Replace the app package\'s WebUI folder with the WebUI\r\n'
                    'folder in this zip, then restart the launcher.\r\n\r\n'
                    'UI only -- the core is untouched. If this build also\r\n'
                    'needed a new core, this overlay is not enough.\r\n'
                    % time.strftime('%Y-%m-%d %H:%M'))

    print('%s  (%d files, %.1f MB)' % (out, n, os.path.getsize(out) / 1048576.0))


if __name__ == '__main__':
    main()
