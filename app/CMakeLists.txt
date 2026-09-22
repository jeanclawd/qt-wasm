cmake_minimum_required(VERSION 3.19)
project(qt-wasm-demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt6's wasm target finalization (the step that emits <target>.html and
# qtloader.js next to the .wasm) lives in QtPublicWasmToolchainHelpers.cmake,
# which `qt-cmake` normally injects through Qt's own toolchain file.
# emscripten-forge drives cmake with Emscripten.cmake directly, so pull the
# helpers in by hand. Same workaround as the upstream
# qt-calculator-experimental recipe.
if(EMSCRIPTEN)
    find_file(QT_WASM_HELPERS
        NAMES QtPublicWasmToolchainHelpers.cmake
        PATHS "$ENV{PREFIX}/lib/cmake/Qt6"
              "$ENV{PREFIX}/lib/cmake/Qt6Core"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    include("${QT_WASM_HELPERS}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(qt-wasm-demo
    main.cpp
)

target_link_libraries(qt-wasm-demo PRIVATE Qt6::Widgets)

if(EMSCRIPTEN)
    # qt6-main[build=jspi*] is built with -feature-wasm-jspi and propagates
    # -sJSPI through its INTERFACE link options. Repeating it is
    # belt-and-braces; JSPI is what lets QApplication::exec() block without
    # freezing the browser's main thread.
    target_link_options(qt-wasm-demo PRIVATE -sJSPI=1)

    # emscripten-forge convention: everything on the channel is built with
    # wasm exceptions, so the ABI has to match.
    target_compile_options(qt-wasm-demo PRIVATE -fwasm-exceptions)
    target_link_options(qt-wasm-demo PRIVATE -fwasm-exceptions)
endif()
