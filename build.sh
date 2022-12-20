#!/bin/bash

BUILD_FOLDER=build

GENIE=${BUILD_FOLDER}/genie
mkdir -p ${BUILD_FOLDER}

if [ ! -f "$PWD/$GENIE" ]; then
	wget https://github.com/bkaradzic/bx/raw/master/tools/bin/darwin/genie -P ${BUILD_FOLDER}
	chmod +x $GENIE
fi

SOKOL_GFX_INCLUDE=sokol_gfx.h
SOKOL_APP_INCLUDE=sokol_app.h
SOKOL_GLUE_INCLUDE=sokol_glue.h

if [ ! -f "$PWD/${BUILD_FOLDER}/${SOKOL_GFX_INCLUDE}" ]; then
	wget https://raw.githubusercontent.com/floooh/sokol/master/${SOKOL_GFX_INCLUDE} -P ${BUILD_FOLDER}
fi

if [ ! -f "$PWD/${BUILD_FOLDER}/${SOKOL_APP_INCLUDE}" ]; then
	wget https://raw.githubusercontent.com/floooh/sokol/master/${SOKOL_APP_INCLUDE} -P ${BUILD_FOLDER}
fi

if [ ! -f "$PWD/${BUILD_FOLDER}/${SOKOL_GLUE_INCLUDE}" ]; then
	wget https://raw.githubusercontent.com/floooh/sokol/master/${SOKOL_GLUE_INCLUDE} -P ${BUILD_FOLDER}
fi


SOKOL_SDHC=sokol-shdc
SOKOL_SDHC_CMD=${BUILD_FOLDER}/sokol-shdc

if [ ! -f ${BUILD_FOLDER}/${SOKOL_SDHC} ]; then
    wget https://github.com/floooh/sokol-tools-bin/raw/master/bin/osx/sokol-shdc -O ${BUILD_FOLDER}/${SOKOL_SDHC}
    chmod +x ${BUILD_FOLDER}/${SOKOL_SDHC}
fi

STB_IMAGE=stb_image.h

if [ ! -f "$PWD/${BUILD_FOLDER}/${STB_IMAGE}" ]; then
	wget https://raw.githubusercontent.com/nothings/stb/master/${STB_IMAGE} -P ${BUILD_FOLDER}
fi

${SOKOL_SDHC_CMD} --input assets/shaders.glsl --output src/shaders.glsl.h --slang metal_macos

./${BUILD_FOLDER}/genie --file=premake.lua gmake

cd build
make all
