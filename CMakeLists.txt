cmake_minimum_required(VERSION 3.25)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(AeroTerminal LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable folder views in IDEs like Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)

# 1. Fetch GLFW
message(STATUS "Fetching GLFW...")
FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.3.8.zip
)
# Disable GLFW documentation, tests and examples to speed up build
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_INSTALL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(glfw)

# 2. Fetch Dear ImGui (Docking branch)
message(STATUS "Fetching Dear ImGui (docking branch)...")
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/heads/docking.zip
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# Define ImGui static library since it doesn't provide its own CMakeLists.txt
set(IMGUI_SRCS
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${IMGUI_SRCS})
target_include_directories(imgui PUBLIC 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)
target_link_libraries(imgui PRIVATE glfw)

# Find OpenGL
find_package(OpenGL REQUIRED)

# 3. Fetch MbedTLS
message(STATUS "Fetching MbedTLS...")
FetchContent_Declare(
    mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/mbedtls-2.28.10.zip
)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)

# Set MbedTLS variables for IXWebSocket FindMbedTLS.cmake
set(MBEDTLS_INCLUDE_DIRS "${mbedtls_SOURCE_DIR}/include" CACHE PATH "" FORCE)
set(MBEDTLS_LIBRARY mbedtls CACHE FILEPATH "" FORCE)
set(MBEDX509_LIBRARY mbedx509 CACHE FILEPATH "" FORCE)
set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE FILEPATH "" FORCE)

# 4. Fetch IXWebSocket
message(STATUS "Fetching IXWebSocket...")
FetchContent_Declare(
    ixwebsocket
    URL https://github.com/machinezone/IXWebSocket/archive/refs/tags/v11.4.4.zip
)
set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_OPEN_SSL OFF CACHE BOOL "" FORCE)
set(USE_MBED_TLS ON CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(USE_WS OFF CACHE BOOL "" FORCE)
set(USE_TEST OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

# 5. Setup AeroTerminal Executable
set(PROJECT_SRCS
    src/app/main.cpp
    src/app/WindowFrame.cpp
    src/core/MarketDataService.cpp
    src/ui/widgets/PanelChart.cpp
    src/ui/widgets/PanelExecution.cpp
    src/ui/widgets/PanelSaathi.cpp
    src/ui/widgets/PanelJournal.cpp
    src/ui/widgets/PanelCalendar.cpp
    src/ui/widgets/PanelWhiteboard.cpp
    src/ui/widgets/PanelOrderBook.cpp
    src/ui/widgets/PanelTrades.cpp
    src/ui/widgets/PanelScreener.cpp
    src/core/indicators/IndicatorManager.cpp
)

add_executable(AeroTerminal WIN32 ${PROJECT_SRCS})

# Target include directories
target_include_directories(AeroTerminal PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Set compile definitions for Win32 to use Unicode
if(WIN32)
    target_compile_definitions(AeroTerminal PRIVATE UNICODE _UNICODE)
endif()

# Copy fonts to build directory so they are easily accessible by the executable
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/fonts" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

# Link libraries
target_link_libraries(AeroTerminal PRIVATE
    imgui
    glfw
    OpenGL::GL
    ixwebsocket
)

if(WIN32)
    target_link_libraries(AeroTerminal PRIVATE
        ole32
        comctl32
        oleaut32
        uuid
        version
        shlwapi
    )
endif()

# Output build locations
message(STATUS "AeroTerminal configured successfully!")
