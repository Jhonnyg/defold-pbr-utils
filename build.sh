#!/bin/bash

export PLATFORM_EXT=""

if [ "$(uname)" == "Darwin" ]; then
    export PLATFORM="macos"
elif [ "$(expr substr $(uname -s) 1 10)" == "MINGW64_NT" ] || [ "$(expr substr $(uname -s) 1 7)" == "MSYS_NT" ]; then
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

    if [ "$PLATFORM" == "linux" ]; then
        curl https://github.com/bkaradzic/bx/raw/master/tools/bin/linux/genie --output-dir ${BUILD_FOLDER} -o ${GENIE} -L
        chmod +x $GENIE_CMD
    fi
fi

# SOKOL GFX VERSION: https://github.com/floooh/sokol/blob/1eef04f44bd59553d7d9c095eac252e52f743ec1/sokol_gfx.h
# SOKOL GLUE VERSION: https://github.com/floooh/sokol/blob/6cca6e36d8e5fbe316a6b7b3f65ba79d9cb436de/sokol_glue.h
# SOKOL APP VERSION: https://github.com/floooh/sokol/blob/0d7d72e6740026c2cdd7cd0a5503596efa74cb0b/sokol_app.h

SOKOL_SDHC=sokol-shdc${PLATFORM_EXT}
SOKOL_SDHC_CMD=${BUILD_FOLDER}/${SOKOL_SDHC}

if [ ! -f ${BUILD_FOLDER}/${SOKOL_SDHC} ]; then
    if [ "$PLATFORM" == "macos" ]; then
        curl -L https://github.com/floooh/sokol-tools-bin/raw/e64ac04c971e54d4da4f5da087afe21aa27885bc/bin/osx_arm64/sokol-shdc --output-dir ${BUILD_FOLDER} -o ${SOKOL_SDHC}
        chmod +x ${BUILD_FOLDER}/${SOKOL_SDHC}
    fi

    if [ "$PLATFORM" == "windows" ]; then
        curl -L https://github.com/floooh/sokol-tools-bin/raw/e64ac04c971e54d4da4f5da087afe21aa27885bc/bin/win32/sokol-shdc.exe --output-dir ${BUILD_FOLDER} -o ${SOKOL_SDHC}
    fi

    if [ "$PLATFORM" == "linux" ]; then
        curl -L https://github.com/floooh/sokol-tools-bin/raw/e64ac04c971e54d4da4f5da087afe21aa27885bc/bin/linux/sokol-shdc --output-dir ${BUILD_FOLDER} -o ${SOKOL_SDHC}
        chmod +x ${BUILD_FOLDER}/${SOKOL_SDHC}
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

${SOKOL_SDHC_CMD} --input assets/shaders.glsl --output src/shaders.glsl.h --slang glsl330

./${GENIE_CMD} --file=premake.lua gmake

cd build
make all

if [ "$PLATFORM" != "windows" ]; then
	chmod +x pbr-utils
fi

