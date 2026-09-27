workspace "Macaw"
    location ""
    configurations { "Debug", "Release", "Viewer" }
    platforms { "x64" }
    defaultplatform "x64"
    startproject "Macaw"

filter "platforms:x64"
    architecture "x86_64"

filter "system:windows"
    systemversion "latest"

filter "configurations:Debug"
    defines { "_DEBUG" }
    symbols "On"
    runtime "Debug"

filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "Speed"
    runtime "Release"

filter "configurations:Viewer"
    defines { "NDEBUG", "OBJ_VIEWER" }
    optimize "Speed"
    runtime "Release"

filter {}

function ConfigureProject(ProjectName, ProjectKind)
    project(ProjectName)
        kind(ProjectKind)
        language "C++"
        cppdialect "C++20"
        characterset "Unicode"
        staticruntime "Off"
        toolset "msc-v145"
        targetdir "bin/%{cfg.buildcfg}/%{cfg.platform}"
        objdir "bin-int/%{prj.name}/%{cfg.buildcfg}/%{cfg.platform}"
        includedirs { ".", "range_v_3", "Externals/Include" }
        defines { "NOMINMAX" }
        buildoptions { "/utf-8" }
        pchheader "pch.h"
        pchsource "pch.cpp"
        files { "pch.cpp" }
end

function ConfigureExecutableLinks()
    links {
        "Editor",
        "Render",
        "World",
        "Asset",
        "Serialization",
        "Core",
        "Math",
        "DirectXTex",
        "d3d11",
        "dxgi",
        "d3dcompiler",
        "dwmapi",
        "gdi32",
        "imm32",
        "shell32",
        "user32",
        "kernel32",
    }

    filter "configurations:Debug"
        libdirs { "Externals/bin/debug" }

    filter "configurations:Release"
        libdirs { "Externals/bin/release" }

    filter "configurations:Viewer"
        libdirs { "Externals/bin/release" }

    filter {}
end

ConfigureProject("Math", "StaticLib")
    files { "Math/**.h", "Math/**.cpp" }
    removefiles { "pch.cpp" }
    enablepch "Off"

ConfigureProject("Core", "StaticLib")
    files { "Core/**.h", "Core/**.cpp", "Core/**.cc" }
    dependson { "Math" }

ConfigureProject("Serialization", "StaticLib")
    files { "Serialization/**.h", "Serialization/**.cpp" }
    dependson { "Core" }

ConfigureProject("Asset", "StaticLib")
    files { "Asset/**.h", "Asset/**.cpp", "Asset/**.cc" }
    dependson { "Serialization" }

ConfigureProject("World", "StaticLib")
    files { "World/**.h", "World/**.cpp" }
    dependson { "Asset" }

ConfigureProject("Render", "StaticLib")
    files { "Render/**.h", "Render/**.cpp", "ImGui/**.h", "ImGui/**.cpp" }
    dependson { "Asset" }

ConfigureProject("Editor", "StaticLib")
    files { "Editor/**.h", "Editor/**.cpp" }
    dependson { "World", "Render" }

ConfigureProject("Macaw", "WindowedApp")
    files { "Application/**.h", "Application/**.cpp", "Macaw.cpp", "Macaw.h", "framework.h", "targetver.h", "Resource.h", "Macaw.rc", "Scripts/GenerateGizmoTorus.cpp" }
    filter "configurations:Viewer"
        removefiles { "Application/FEditorApplication.cpp" }
    filter "configurations:Debug or Release"
        removefiles { "Application/FViewApplication.cpp" }
    filter {}
    ConfigureExecutableLinks()

project "Render"
filter "files:ImGui/**.cpp"
    enablepch "Off"

project "Math"
filter "files:Math/SimpleMath/SimpleMath.cpp"
    enablepch "Off"

project "Macaw"
filter {}
