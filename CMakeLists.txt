cmake_minimum_required(VERSION 3.20)
project(dsnap LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/common.cpp
    src/config.cpp
    src/audio.cpp
    src/capture.cpp
    src/overlay.cpp
    src/recorder.cpp
    src/tray.cpp
    src/hotkey.cpp
    res/dsnap.rc
)

add_executable(dsnap WIN32 ${SOURCES})

target_include_directories(dsnap PRIVATE src res)

target_link_libraries(dsnap PRIVATE
    gdi32
    user32
    shell32
    advapi32
    gdiplus
    ole32
    oleaut32
    strmiids
    urlmon
)

if(MSVC)
    target_compile_options(dsnap PRIVATE /W4 /O2 /GS- /GL)
    target_link_options(dsnap PRIVATE /LTCG /SUBSYSTEM:WINDOWS)
else()
    target_compile_options(dsnap PRIVATE -Wall -Wextra -O3 -mwindows)
    target_link_options(dsnap PRIVATE -mwindows -s -static)
endif()
