
EXPORT_PATH=${1:-"../../release_export/"}

mkdir -p $EXPORT_PATH/WebUI/

cp  -r index.html bundle.js *.ttf resource $EXPORT_PATH/WebUI/