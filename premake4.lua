solution "SxC"
  location ("build/" .. os.get() .. (_ACTION and ("/" .. _ACTION) or ""))
  targetdir ("bin/" .. os.get())
  kind "SharedLib"
  language "C"
  configurations { "Debug", "Release" }
  configuration "Debug"
    flags { "Symbols", "ExtraWarnings", "FatalWarnings" }
  configuration "Release"
    flags { "OptimizeSpeed" }

  project "sxc"
    files { "src/*.h", "src/*.c" }

  project "lua51_sxc"
    links { "sxc", "lua5.1" }
    files { "src/lua51/*.h", "src/lua51/*.c" }
    targetprefix ""

