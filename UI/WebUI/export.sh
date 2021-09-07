#!/bin/sh
EXPORT_PATH=${1:-"../../release_export/"}
echo $EXPORT_PATH
cp  -r dist $EXPORT_PATH
cp  -r index.html $EXPORT_PATH
cp  -r resource $EXPORT_PATH
