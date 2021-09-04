#!/bin/sh
EXPORT_PATH=${1:-"../../release_export/"}
echo $EXPORT_PATH
cp  -r ./release-builds $EXPORT_PATH
