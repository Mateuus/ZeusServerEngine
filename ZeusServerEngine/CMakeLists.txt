cmake_minimum_required(VERSION 3.20)
project(ZeusServerEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ZEUS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(ZEUS_CORE_PUBLIC ${ZEUS_ROOT}/Source/ZeusCore/Public)
set(ZEUS_PROTOCOL_PUBLIC ${ZEUS_ROOT}/Source/ZeusProtocol/Public)
set(ZEUS_PLATFORM_PUBLIC ${ZEUS_ROOT}/Source/ZeusPlatform/Public)
set(ZEUS_NET_PUBLIC ${ZEUS_ROOT}/Source/ZeusNet/Public)
set(ZEUS_SESSION_PUBLIC ${ZEUS_ROOT}/Source/ZeusSession/Public)
set(ZEUS_RUNTIME_PUBLIC ${ZEUS_ROOT}/Source/ZeusRuntime/Public)
set(ZEUS_MATH_PUBLIC ${ZEUS_ROOT}/Source/ZeusMath/Public)
set(ZEUS_WORLD_PUBLIC ${ZEUS_ROOT}/Source/ZeusWorld/Public)
set(ZEUS_APP_PUBLIC ${ZEUS_ROOT}/Source/ZeusApp/Public)

add_library(ZeusCore STATIC
    Source/ZeusCore/Private/ZeusCoreStub.cpp
)
target_include_directories(ZeusCore PUBLIC ${ZEUS_CORE_PUBLIC})

add_library(ZeusProtocol STATIC
    Source/ZeusProtocol/Private/PacketBuilder.cpp
    Source/ZeusProtocol/Private/PacketHeaderWire.cpp
    Source/ZeusProtocol/Private/PacketParser.cpp
    Source/ZeusProtocol/Private/PacketReader.cpp
    Source/ZeusProtocol/Private/PacketWriter.cpp
    Source/ZeusProtocol/Private/SessionWire.cpp
)
target_include_directories(ZeusProtocol PUBLIC
    ${ZEUS_PROTOCOL_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
)
target_link_libraries(ZeusProtocol PUBLIC ZeusCore)

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

add_library(ZeusNet STATIC
    Source/ZeusNet/Private/ChannelConfig.cpp
    Source/ZeusNet/Private/NetConnection.cpp
    Source/ZeusNet/Private/NetConnectionManager.cpp
    Source/ZeusNet/Private/NetworkDiagnostics.cpp
    Source/ZeusNet/Private/NetworkSimulator.cpp
    Source/ZeusNet/Private/PacketAckTracker.cpp
    Source/ZeusNet/Private/SendQueue.cpp
    Source/ZeusNet/Private/UdpEndpoint.cpp
    Source/ZeusNet/Private/UdpServer.cpp
    Source/ZeusNet/Private/UdpSocket.cpp
)
target_include_directories(ZeusNet PUBLIC
    ${ZEUS_NET_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_PROTOCOL_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
)
target_link_libraries(ZeusNet PUBLIC ZeusProtocol ZeusPlatform ZeusCore)
if(WIN32)
    target_link_libraries(ZeusNet PUBLIC ws2_32)
endif()

add_library(ZeusSession STATIC
    Source/ZeusSession/Private/ClientSession.cpp
    Source/ZeusSession/Private/SessionManager.cpp
    Source/ZeusSession/Private/SessionOpcodeRules.cpp
    Source/ZeusSession/Private/SessionPacketHandler.cpp
)
target_include_directories(ZeusSession PUBLIC
    ${ZEUS_SESSION_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_NET_PUBLIC}
    ${ZEUS_PROTOCOL_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
)
target_link_libraries(ZeusSession PUBLIC ZeusNet ZeusProtocol ZeusPlatform)

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

add_library(ZeusMath STATIC
    Source/ZeusMath/Private/Bounds.cpp
    Source/ZeusMath/Private/MathUtils.cpp
    Source/ZeusMath/Private/Quaternion.cpp
    Source/ZeusMath/Private/Rotator.cpp
    Source/ZeusMath/Private/Transform.cpp
    Source/ZeusMath/Private/Vector3.cpp
)
target_include_directories(ZeusMath PUBLIC ${ZEUS_MATH_PUBLIC} ${ZEUS_CORE_PUBLIC})
target_link_libraries(ZeusMath PUBLIC ZeusCore)

add_library(ZeusWorld STATIC
    Source/ZeusWorld/Private/Actor.cpp
    Source/ZeusWorld/Private/ActorComponent.cpp
    Source/ZeusWorld/Private/EntityManager.cpp
    Source/ZeusWorld/Private/Level.cpp
    Source/ZeusWorld/Private/MapInstance.cpp
    Source/ZeusWorld/Private/World.cpp
    Source/ZeusWorld/Private/WorldRuntime.cpp
    Source/ZeusWorld/Private/WorldStats.cpp
)
target_include_directories(ZeusWorld PUBLIC
    ${ZEUS_WORLD_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_MATH_PUBLIC}
    ${ZEUS_RUNTIME_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
)
target_link_libraries(ZeusWorld PUBLIC ZeusCore ZeusMath ZeusRuntime)

add_executable(ZeusServer
    Source/ZeusApp/Private/main.cpp
    Source/ZeusApp/Private/CoreServerApp.cpp
)
target_include_directories(ZeusServer PRIVATE
    ${ZEUS_APP_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_PROTOCOL_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
    ${ZEUS_NET_PUBLIC}
    ${ZEUS_SESSION_PUBLIC}
    ${ZEUS_RUNTIME_PUBLIC}
    ${ZEUS_MATH_PUBLIC}
    ${ZEUS_WORLD_PUBLIC}
)
target_link_libraries(ZeusServer PRIVATE ZeusWorld ZeusMath ZeusRuntime ZeusSession ZeusNet ZeusProtocol)

if(UNIX AND NOT APPLE)
    target_compile_definitions(ZeusPlatform PRIVATE _GNU_SOURCE)
endif()
