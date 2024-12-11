#!/bin/bash

SOURCE_DIR="etc"
DEST_DIR="ramulator2"

# Copy the files as specified
mkdir $DEST_DIR/configs
cp "$SOURCE_DIR/CMakeLists.txt" "$DEST_DIR/CMakeLists.txt"
cp "$SOURCE_DIR/CMakeLists2.txt" "$DEST_DIR/src/CMakeLists.txt"
cp "$SOURCE_DIR/request.h" "$DEST_DIR/src/base/request.h"
cp "$SOURCE_DIR/request.cpp" "$DEST_DIR/src/base/request.cpp"
cp "$SOURCE_DIR/generic_dram_controller.cpp" "$DEST_DIR/src/dram_controller/impl/generic_dram_controller.cpp"
cp "$SOURCE_DIR/DDR5_3200C.yaml" "$DEST_DIR/configs/DDR5_3200C.yaml"
cp "$SOURCE_DIR/Bridge.cpp" "$DEST_DIR/src/Bridge.cpp"
cp "$SOURCE_DIR/Bridge.h" "$DEST_DIR/src/Bridge.h"

echo "Files copied successfully!"