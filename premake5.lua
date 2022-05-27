-- premake5.lua
workspace "KenshiCalc"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "KenshiCalc"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "Walnut/WalnutExternal.lua"

include "AppMain"