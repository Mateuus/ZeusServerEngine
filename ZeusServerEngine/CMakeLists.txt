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
set(ZEUS_COLLISION_PUBLIC ${ZEUS_ROOT}/Source/ZeusCollision/Public)
set(ZEUS_GAME_PUBLIC ${ZEUS_ROOT}/Source/ZeusGame/Public)
set(ZEUS_APP_PUBLIC ${ZEUS_ROOT}/Source/ZeusApp/Public)

option(ZEUS_USE_JOLT "Enable Jolt Physics integration in ZeusCollision." ON)

if(ZEUS_USE_JOLT)
    include(FetchContent)

    # Disable Jolt's own samples / tests / viewer / hello world to keep the build
    # focused on the static lib. Keep float precision (DOUBLE_PRECISION=OFF) to
    # avoid contaminating ZeusMath; cm <-> m happens in JoltConversion.
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
    set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
    set(OBJECT_LAYER_BITS 16 CACHE STRING "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
    set(USE_AVX2 OFF CACHE BOOL "" FORCE)
    set(USE_AVX OFF CACHE BOOL "" FORCE)
    set(USE_SSE4_2 OFF CACHE BOOL "" FORCE)
    set(USE_SSE4_1 OFF CACHE BOOL "" FORCE)
    set(USE_LZCNT OFF CACHE BOOL "" FORCE)
    set(USE_TZCNT OFF CACHE BOOL "" FORCE)
    set(USE_F16C OFF CACHE BOOL "" FORCE)
    set(USE_FMADD OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.5.0
        SOURCE_SUBDIR Build
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JoltPhysics)
endif()

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

# ---- ZeusCollision -----------------------------------------------------------
add_library(ZeusCollision STATIC
    Source/ZeusCollision/Private/CollisionAsset.cpp
    Source/ZeusCollision/Private/ZsmCollisionLoader.cpp
    Source/ZeusCollision/Private/ZsmDynamicLoader.cpp
    Source/ZeusCollision/Private/ZsmTerrainLoader.cpp
    Source/ZeusCollision/Private/CollisionDebug.cpp
    Source/ZeusCollision/Private/CollisionWorld.cpp
    Source/ZeusCollision/Private/DynamicCollisionWorld.cpp
    Source/ZeusCollision/Private/CollisionTestScene.cpp
)
target_include_directories(ZeusCollision PUBLIC
    ${ZEUS_COLLISION_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_MATH_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
)
target_link_libraries(ZeusCollision PUBLIC ZeusCore ZeusMath ZeusPlatform)

add_library(ZeusWorld STATIC
    Source/ZeusWorld/Private/Actor.cpp
    Source/ZeusWorld/Private/ActorComponent.cpp
    Source/ZeusWorld/Private/DebugPlayerActor.cpp
    Source/ZeusWorld/Private/EntityManager.cpp
    Source/ZeusWorld/Private/Level.cpp
    Source/ZeusWorld/Private/MapInstance.cpp
    Source/ZeusWorld/Private/RegionSystemTests.cpp
    Source/ZeusWorld/Private/World.cpp
    Source/ZeusWorld/Private/WorldRuntime.cpp
    Source/ZeusWorld/Private/WorldStats.cpp
    Source/ZeusWorld/Private/ZeusRegionSystem.cpp
)
target_include_directories(ZeusWorld PUBLIC
    ${ZEUS_WORLD_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_MATH_PUBLIC}
    ${ZEUS_RUNTIME_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
    ${ZEUS_COLLISION_PUBLIC}
)
target_link_libraries(ZeusWorld PUBLIC ZeusCore ZeusMath ZeusRuntime ZeusCollision)

# ---- ZeusGame (gameplay subsystems: Entities + Movement) --------------------
add_library(ZeusGame STATIC
    Source/ZeusGame/Private/Entities/CharacterActor.cpp
    Source/ZeusGame/Private/Movement/MovementComponent.cpp
    Source/ZeusGame/Private/Movement/MovementSystem.cpp
    Source/ZeusGame/Private/Movement/MovementTests.cpp
)
target_include_directories(ZeusGame PUBLIC
    ${ZEUS_GAME_PUBLIC}
    ${ZEUS_CORE_PUBLIC}
    ${ZEUS_MATH_PUBLIC}
    ${ZEUS_PLATFORM_PUBLIC}
    ${ZEUS_COLLISION_PUBLIC}
    ${ZEUS_WORLD_PUBLIC}
)
target_link_libraries(ZeusGame PUBLIC ZeusCore ZeusMath ZeusPlatform ZeusCollision ZeusWorld)

if(ZEUS_USE_JOLT)
    target_sources(ZeusCollision PRIVATE
        Source/ZeusCollision/Private/JoltConversion.cpp
        Source/ZeusCollision/Private/JoltShapeFactory.cpp
        Source/ZeusCollision/Private/PhysicsWorld.cpp
    )
    target_link_libraries(ZeusCollision PUBLIC Jolt)
    target_compile_definitions(ZeusCollision PUBLIC ZEUS_HAS_JOLT=1)
else()
    target_compile_definitions(ZeusCollision PUBLIC ZEUS_HAS_JOLT=0)
endif()

add_executable(ZeusServer
    Source/ZeusApp/Private/main.cpp
    Source/ZeusApp/Private/CoreServerApp.cpp
    Source/ZeusApp/Private/ServerPaths.cpp
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
    ${ZEUS_COLLISION_PUBLIC}
    ${ZEUS_GAME_PUBLIC}
)
target_link_libraries(ZeusServer PRIVATE ZeusGame ZeusCollision ZeusWorld ZeusMath ZeusRuntime ZeusSession ZeusNet ZeusProtocol)

# Mantém o layout de runtime no output do build (build/Debug, build/Release, etc.)
# copiando Config/ e Data/ da raiz do motor a cada build do ZeusServer.
add_custom_command(TARGET ZeusServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ZeusServer>/Config"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ZeusServer>/Data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ZEUS_ROOT}/Config"
        "$<TARGET_FILE_DIR:ZeusServer>/Config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ZEUS_ROOT}/Data"
        "$<TARGET_FILE_DIR:ZeusServer>/Data"
    COMMENT "Copying Config and Data folders to ZeusServer output"
)

if(UNIX AND NOT APPLE)
    target_compile_definitions(ZeusPlatform PRIVATE _GNU_SOURCE)
endif()
