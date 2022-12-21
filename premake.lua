PATH_ROOT  = path.getabsolute("")
PATH_BUILD = path.join(PATH_ROOT,"build")
PATH_SRC   = path.join(PATH_ROOT,"src")

solution "pbr-utils"
    language       ( "C++" )
    location       ( PATH_BUILD )

    configurations { "Debug", "Release" }
    platforms      { "x64" }
    flags          { "NoPCH"} -- "FatalWarnings",
    buildoptions   { "-Wno-switch", "-ObjC" } -- todo: not use -ObjC for non-osx

    configuration "Debug"
        defines { "DEBUG" }
        flags   { "Symbols" }

    configuration "Release"
        defines { "NDEBUG" }
        flags   { "Optimize" }

project "pbr-utils"
    objdir      ( PATH_BUILD )
    kind        ( "ConsoleApp" )
    targetname  ( "pbr-utils" )
    targetdir   ( PATH_BUILD )
    files       { path.join(PATH_SRC, "**.cpp") }
    includedirs { PATH_SRC, PATH_BUILD }
    links       {
        "CoreFoundation.framework",
        "CoreGraphics.framework",
        "IOSurface.framework",
        "IOKit.framework",
        "Cocoa.framework",
        "QuartzCore.framework",
        "Metal.framework",
        "MetalKit.framework",
        "OpenGL.framework"
    }
