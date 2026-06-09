#!/bin/sh
# Export the Vite production build. `npm run build` (vite.config.prod.mjs) emits a
# self-contained dist/ (index.html + assets/), so the WebUI root = dist/ contents.
EXPORT_PATH=${1:-"../../release_export/"}
echo $EXPORT_PATH
mkdir -p "$EXPORT_PATH"
cp -r dist/. "$EXPORT_PATH"
cp -r resource "$EXPORT_PATH"
