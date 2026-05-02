cmake_minimum_required(VERSION 3.20)
project(WinPublicIP VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Статическая линковка CRT — убирает зависимость от VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Все исходники
set(SOURCES
    src/main.cpp
    src/TrayApp.cpp
    src/SettingsDialog.cpp
    src/Config/AppSettings.cpp
    src/Services/HttpHelper.cpp
    src/Services/IpProvider.cpp
    src/Services/GeoProvider.cpp
    src/Services/VpnDetector.cpp
    src/Rendering/FlagLoader.cpp
    src/Rendering/TrayRenderer.cpp
    resources/resources.rc
)

# WIN32 — без консольного окна
add_executable(WinPublicIP WIN32 ${SOURCES})

target_include_directories(WinPublicIP PRIVATE
    src
    third_party
)

target_compile_definitions(WinPublicIP PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Пути для компилятора ресурсов (rc.exe запускается из build/, нужен корень проекта)
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_SOURCE_DIR}\"")

target_compile_options(WinPublicIP PRIVATE
    /W4       # предупреждения уровня 4
    /WX-      # предупреждения не являются ошибками
    /utf-8    # исходники в UTF-8
)

target_link_libraries(WinPublicIP PRIVATE
    winhttp.lib
    gdiplus.lib
    iphlpapi.lib
    shell32.lib
    advapi32.lib
    user32.lib
    gdi32.lib
    ole32.lib
    shlwapi.lib
)
