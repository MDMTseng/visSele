
EXPORT_PATH=${1:-"../../release_export/"}

mkdir -p $EXPORT_PATH/WebUI/

cp  -r index.html dist resource $EXPORT_PATH/WebUI/