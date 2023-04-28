#!/bin/bash

export PLATFORM_EXT=""

if [ "$(uname)" == "Darwin" ]; then
    export PLATFORM="macos"
elif [ "$(expr substr $(uname -s) 1 10)" == "MINGW64_NT" ]; then
    export PLATFORM="windows"
    export PLATFORM_EXT=".exe"
else
    export PLATFORM="linux"
fi

BUILD_FOLDER=build
mkdir -p ${BUILD_FOLDER}


GENIE=genie${PLATFORM_EXT}
GENIE_CMD=${BUILD_FOLDER}/${GENIE}

if [ ! -f "$PWD/${BUILD_FOLDER}/$GENIE" ]; then

    if [ "$PLATFORM" == "macos" ]; then
        curl https://github.com/bkaradzic/bx/raw/master/tools/bin/darwin/genie --output-dir ${BUILD_FOLDER} -o ${GENIE} -L
        chmod +x $GENIE_CMD
    fi

    if [ "$PLATFORM" == "windows" ]; then
        curl https://github.com/bkaradzic/bx/raw/master/tools/bin/windows/genie.exe --output-dir ${BUILD_FOLDER} -o ${GENIE} -L
    fi
fi

SOKOL_GFX_INCLUDE=sokol_gfx.h
SOKOL_APP_INCLUDE=sokol_app.h
SOKOL_GLUE_INCLUDE=sokol_glue.h

if [ ! -f "$PWD/${BUILD_FOLDER}/${SOKOL_GFX_INCLUDE}" ]; then
    curl https://raw.githubusercontent.com/floooh/sokol/master/${SOKOL_GFX_INCLUDE} --output-dir ${BUILD_FOLDER} -o ${SOKOL_GFX_INCLUDE}
fi

if [ ! -f "$PWD/${BUILD_FOLDER}/${SOKOL_APP_INCLUDE}" ]; then
    curl https://raw.githubusercontent.com/floooh/sokol/master/${SOKOL_APP_INCLUDE} --output-dir ${BUILD_FOLDER} -o ${SOKOL_APP_INCLUDE}
fi

if [ ! -f "$PWD/${BUILD_FOLDER}/${SOKOL_GLUE_INCLUDE}" ]; then
    curl https://raw.githubusercontent.com/floooh/sokol/master/${SOKOL_GLUE_INCLUDE} --output-dir ${BUILD_FOLDER} -o ${SOKOL_GLUE_INCLUDE}
fi

SOKOL_SDHC=sokol-shdc${PLATFORM_EXT}
SOKOL_SDHC_CMD=${BUILD_FOLDER}/${SOKOL_SDHC}

if [ ! -f ${BUILD_FOLDER}/${SOKOL_SDHC} ]; then
    if [ "$PLATFORM" == "macos" ]; then
        curl https://raw.githubusercontent.com/floooh/sokol-tools-bin/master/bin/osx/sokol-shdc --output-dir ${BUILD_FOLDER} -o ${SOKOL_SDHC}
        chmod +x ${BUILD_FOLDER}/${SOKOL_SDHC}
    fi

    if [ "$PLATFORM" == "windows" ]; then
        curl  https://raw.githubusercontent.com/floooh/sokol-tools-bin/master/bin/win32/sokol-shdc.exe --output-dir ${BUILD_FOLDER} -o ${SOKOL_SDHC}
    fi
fi

STB_IMAGE=stb_image.h
STB_IMAGE_WRITE=stb_image_write.h

if [ ! -f "$PWD/${BUILD_FOLDER}/${STB_IMAGE}" ]; then
    curl https://raw.githubusercontent.com/nothings/stb/master/${STB_IMAGE} --output-dir ${BUILD_FOLDER} -o ${STB_IMAGE}
fi

if [ ! -f "$PWD/${BUILD_FOLDER}/${STB_IMAGE_WRITE}" ]; then
    curl https://raw.githubusercontent.com/nothings/stb/master/${STB_IMAGE_WRITE} --output-dir ${BUILD_FOLDER} -o ${STB_IMAGE_WRITE}
fi

LINMATH=linmath.h

if [ ! -f "$PWD/${BUILD_FOLDER}/${LINMATH}" ]; then
    curl https://raw.githubusercontent.com/datenwolf/linmath.h/master/${LINMATH} --output-dir ${BUILD_FOLDER} -o ${LINMATH}
fi

SJON=sjson.h

if [ ! -f "$PWD/${BUILD_FOLDER}/${SJON}" ]; then
    curl https://raw.githubusercontent.com/septag/sjson/master/${SJON} --output-dir ${BUILD_FOLDER} -o ${SJON}
fi

${SOKOL_SDHC_CMD} --input assets/shaders.glsl --output src/shaders.metal.h --slang metal_macos
${SOKOL_SDHC_CMD} --input assets/shaders.glsl --output src/shaders.glsl.h --slang glsl330

./${GENIE_CMD} --file=premake.lua gmake

cd build
make all
