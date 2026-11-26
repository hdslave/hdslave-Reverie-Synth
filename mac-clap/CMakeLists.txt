cmake_minimum_required(VERSION 3.20)
project(reverie_mac_clap LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CLAP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../third_party/clap/include")
set(DESKTOP_DIR "${CMAKE_SOURCE_DIR}/desktop")
set(TARGET_NAME "Reverie")
set(SLUG "reverie")

# Compile inner plugin DSP/screen with clap_entry renamed to hdslave_inner_clap_entry
add_library(${SLUG}_inner OBJECT
    "${CMAKE_SOURCE_DIR}/src/reverie.cpp"
    "${CMAKE_SOURCE_DIR}/src/screen.cpp")
target_compile_definitions(${SLUG}_inner PRIVATE clap_entry=hdslave_inner_clap_entry)
target_include_directories(${SLUG}_inner PRIVATE
    "${CLAP_INCLUDE_DIR}"
    "${CMAKE_SOURCE_DIR}/src"
    "${DESKTOP_DIR}"
)
set_target_properties(${SLUG}_inner PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MINGW)
    target_compile_definitions(${SLUG}_inner PRIVATE _USE_MATH_DEFINES)
endif()

if(MSVC)
    target_include_directories(${SLUG}_inner PRIVATE "${DESKTOP_DIR}/win32/compat")
    target_compile_options(${SLUG}_inner PRIVATE /O2 /EHsc /W2 /FI"${DESKTOP_DIR}/win32/compat/msvc_compat.h" /wd4244 /wd4267 /wd4305 /wd4996)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${SLUG}_inner PRIVATE -Wno-c++11-narrowing -Wno-deprecated-declarations -Wno-unused-command-line-argument)
    endif()
else()
    target_compile_options(${SLUG}_inner PRIVATE -O3 -ffp-contract=fast -fno-math-errno -fno-trapping-math -fvisibility=hidden)
endif()

# Desktop panel and native window
set(SHELL_SRCS
    "${DESKTOP_DIR}/desktop_wrapper.cpp"
    "${DESKTOP_DIR}/panel_ui.cpp"
)
if(APPLE)
    list(APPEND SHELL_SRCS "${DESKTOP_DIR}/gui_macos.mm")
elseif(WIN32)
    list(APPEND SHELL_SRCS "${DESKTOP_DIR}/gui_win32.cpp")
endif()

# Hosts may load both formats in one process. Give each editor its own native class.
function(add_reverie_editor target view_class)
    add_library(${target} MODULE $<TARGET_OBJECTS:${SLUG}_inner> ${SHELL_SRCS})
    target_include_directories(${target} PRIVATE "${CLAP_INCLUDE_DIR}" "${DESKTOP_DIR}")
    if(APPLE)
        target_compile_definitions(${target} PRIVATE HDSLAVE_OBJC_VIEW_CLASS=${view_class})
        target_compile_options(${target} PRIVATE -O3 -fvisibility=hidden)
        target_link_libraries(${target} PRIVATE "-framework Cocoa" "-framework CoreGraphics" "-framework QuartzCore")
    elseif(WIN32)
        target_compile_definitions(${target} PRIVATE HDSLAVE_WIN32_CLASS_NAME=L"${view_class}" UNICODE _UNICODE)
        if(MSVC)
            target_compile_options(${target} PRIVATE /O2 /EHsc /W3 /wd4244 /wd4267 /wd4996)
        else()
            target_compile_options(${target} PRIVATE -O3)
        endif()
        target_link_libraries(${target} PRIVATE user32 gdi32)
    endif()
endfunction()

if(APPLE)
    add_reverie_editor(${TARGET_NAME} HdslavePanelView_${SLUG})
else()
    add_reverie_editor(${TARGET_NAME} HdslaveDesktopClapView_${SLUG})
endif()

if(APPLE)
    set(BUNDLE_NAME "${TARGET_NAME}")
    set(BUNDLE_SLUG "${SLUG}")
    set(PLIST_OUT "${CMAKE_BINARY_DIR}/plists/${TARGET_NAME}-Info.plist")
    configure_file("${DESKTOP_DIR}/macos/Info.plist.in" "${PLIST_OUT}" @ONLY)

    set_target_properties(${TARGET_NAME} PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
        MACOSX_BUNDLE_INFO_PLIST "${PLIST_OUT}"
        PREFIX ""
        OUTPUT_NAME "${TARGET_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap"
    )
elseif(WIN32)
    target_sources(${TARGET_NAME} PRIVATE "${DESKTOP_DIR}/win32/clap.def")
    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".clap"
        OUTPUT_NAME "${TARGET_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap"
    )
endif()

option(REVERIE_BUILD_VST3 "Build the self-contained VST3 instrument" OFF)
if(REVERIE_BUILD_VST3)
    include(cmake/ReverieVST3.cmake)
endif()

add_executable(host_test "${CMAKE_SOURCE_DIR}/tests/host_test.cpp")
target_include_directories(host_test PRIVATE "${CLAP_INCLUDE_DIR}")
if(APPLE)
    target_sources(host_test PRIVATE tests/editor_focus_macos.mm)
    target_link_libraries(host_test PRIVATE dl "-framework Cocoa")
elseif(WIN32)
    target_sources(host_test PRIVATE tests/editor_focus_win32.cpp)
    target_link_libraries(host_test PRIVATE user32)
endif()
