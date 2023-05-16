PATH_ROOT  = path.getabsolute("")
PATH_BUILD = path.join(PATH_ROOT,"build")
PATH_SRC   = path.join(PATH_ROOT,"src")

function get_platform_config()
    local my_os = os.get()
    if my_os == "windows" then
        return {
            buildoptions = function() end,
            linkoptions = function()
                linkoptions { "-static" }
            end,
            links = function()

                links {
                    "user32",
                    "gdi32",
                }

            end
        }
    elseif my_os == "macosx" then
        return {
            buildoptions = function()
                buildoptions {"-ObjC"}
            end,
            linkoptions = function() end,
            links = function()
                links {
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
            end
        }
    else
        return {
            buildoptions = function() end,
            linkoptions = function()
            end,
            links = function()
            end
        }
    end
end


local platform = get_platform_config()


solution "pbr-utils"
    language       ( "C++" )
    location       ( PATH_BUILD )

    configurations { "Debug", "Release" }
    platforms      { "x64" }
    flags          { "NoPCH"} -- "FatalWarnings", "StaticRuntime"
    buildoptions   { "-Wno-switch"}

    platform.buildoptions()

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

    platform.linkoptions()
    platform.links()
