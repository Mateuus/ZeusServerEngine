cmake_minimum_required(VERSION 3.20)
project(ZeusServerEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ZEUS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(ZEUS_CORE_PUBLIC ${ZEUS_ROOT}/Source/ZeusCore/Public)
set(ZEUS_PLATFORM_PUBLIC ${ZEUS_ROOT}/Source/ZeusPlatform/Public)
set(ZEUS_RUNTIME_PUBLIC ${ZEUS_ROOT}/Source/ZeusRuntime/Public)
set(ZEUS_APP_PUBLIC ${ZEUS_ROOT}/Source/ZeusApp/Public)

add_library(ZeusCore STATIC
    Source/ZeusCore/Private/ZeusCoreStub.cpp
)
target_include_directories(ZeusCore PUBLIC ${ZEUS_CORE_PUBLIC})

add_library(ZeusPlatform STATIC
    Source/ZeusPlatform/Private/ConsoleIo.cpp
    Source/ZeusPlatform/Private/PlatformConsole.cpp
    Source/ZeusPlatform/Private/PlatformConsoleLive.cpp
    Source/ZeusPlatform/Private/PlatformTime.cpp
    Source/ZeusPlatform/Private/ZeusLog.cpp
)
target_include_directories(ZeusPlatform PUBLIC
    ${ZEUS_PLATFORM_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
)
target_link_libraries(ZeusPlatform PUBLIC ZeusCore)

add_library(ZeusRuntime STATIC
    Source/ZeusRuntime/Private/EngineLoop.cpp
    Source/ZeusRuntime/Private/ServerClock.cpp
)
target_include_directories(ZeusRuntime PUBLIC
    ${ZEUS_RUNTIME_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
)
target_link_libraries(ZeusRuntime PUBLIC ZeusPlatform)

add_executable(ZeusServer
    Source/ZeusApp/Private/main.cpp
    Source/ZeusApp/Private/CoreServerApp.cpp
)
target_include_directories(ZeusServer PRIVATE
    ${ZEUS_APP_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
    ${ZEUS_RUNTIME_PUBLIC}
)
target_link_libraries(ZeusServer PRIVATE ZeusRuntime)

if(UNIX AND NOT APPLE)
    target_compile_definitions(ZeusPlatform PRIVATE _GNU_SOURCE)
endif()
