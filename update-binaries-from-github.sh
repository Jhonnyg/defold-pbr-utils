#!/bin/bash

if [ "" == "${DEFOLD_PBR_PATH}" ]; then
    echo "No path set for the defold pbr repository"
else
fi

curl https://github.com/bkaradzic/bx/raw/master/tools/bin/darwin/genie --output-dir ${BUILD_FOLDER} -o ${GENIE} -L
