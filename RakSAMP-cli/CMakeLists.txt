cmake_minimum_required(VERSION 3.10)
project(RakSAMP C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

add_definitions(-DTIXML_USE_STL)

# Compiler options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-w -O2) # suppress legacy compiler warnings
    if(ANDROID)
        add_compile_options(-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-strict-aliasing)
    endif()
endif()

# Keep libgcc/libstdc++ static on Windows, but allow linking lua51.dll dynamically.
if(WIN32 OR MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/raknet
    ${CMAKE_CURRENT_SOURCE_DIR}/raknet/SAMP
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml
    ${CMAKE_CURRENT_SOURCE_DIR}/server/src
)

# Common static library
add_library(common STATIC
    common/common.cpp
    common/win_compat.cpp
)

# TinyXML static library
add_library(tinyxml STATIC
    tinyxml/tinystr.cpp
    tinyxml/tinyxml.cpp
    tinyxml/tinyxmlerror.cpp
    tinyxml/tinyxmlparser.cpp
)

# RakNet static library (exact source files required by RakSAMP)
set(RAKNET_SOURCES
    raknet/_findfirst.cpp
    raknet/AsynchronousFileIO.cpp
    raknet/BitStream.cpp
    raknet/BitStream_NoTemplate.cpp
    raknet/CheckSum.cpp
    raknet/DataBlockEncryptor.cpp
    raknet/DataCompressor.cpp
    raknet/DS_ByteQueue.cpp
    raknet/DS_HuffmanEncodingTree.cpp
    raknet/DS_Table.cpp
    raknet/EncodeClassName.cpp
    raknet/GetTime.cpp
    raknet/InternalPacketPool.cpp
    raknet/LinuxStrings.cpp
    raknet/NetworkIDGenerator.cpp
    raknet/NetworkTypes.cpp
    raknet/PluginInterface.cpp
    raknet/RakClient.cpp
    raknet/RakNetStatistics.cpp
    raknet/RakNetworkFactory.cpp
    raknet/RakPeer.cpp
    raknet/RakServer.cpp
    raknet/RakSleep.cpp
    raknet/Rand.cpp
    raknet/ReliabilityLayer.cpp
    raknet/rijndael.cpp
    raknet/RPCMap.cpp
    raknet/SHA1.cpp
    raknet/SimpleMutex.cpp
    raknet/SocketLayer.cpp
    raknet/StringCompressor.cpp
    raknet/StringTable.cpp
    raknet/SystemAddressList.cpp
    raknet/TableSerializer.cpp
    raknet/TCPInterface.cpp
    raknet/SAMP/SAMPRPC.cpp
    raknet/SAMP/samp_auth.cpp
    raknet/SAMP/samp_netencr.cpp
)
add_library(raknet STATIC ${RAKNET_SOURCES})

# Client build uses SA-MP UDP encryption on send; server build decrypts on receive.
add_library(raknet_client STATIC ${RAKNET_SOURCES})
target_compile_definitions(raknet_client PRIVATE RAKSAMP_CLIENT)

# Embedded Lua 5.1 library
file(GLOB LUA_SOURCES
    server/src/LUA/*.c
)
add_library(lua STATIC ${LUA_SOURCES})
if(UNIX)
    target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
endif()

if(NOT ANDROID)
    # Client uses LuaJIT (shared) for MoonLoader-compatible ffi + C modules (lua51.dll).
    include("${CMAKE_SOURCE_DIR}/cmake/LuaJIT.cmake")

    # RakSAMPServer executable
    file(GLOB SERVER_SOURCES
        server/src/*.cpp
    )
    add_executable(RakSAMPServer ${SERVER_SOURCES})
    target_include_directories(RakSAMPServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/server/src/LUA)
    if(WIN32)
        target_link_libraries(RakSAMPServer PRIVATE raknet tinyxml common lua ws2_32 winmm)
    else()
        target_link_libraries(RakSAMPServer PRIVATE raknet tinyxml common lua ${CMAKE_DL_LIBS} pthread)
    endif()
endif()

# RakSAMPClient executable
file(GLOB CLIENT_SOURCES
    client/src/*.cpp
)
add_executable(RakSAMPClient ${CLIENT_SOURCES})
target_compile_definitions(RakSAMPClient PRIVATE RAKSAMP_CLIENT)
if(ANDROID)
    target_include_directories(RakSAMPClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/server/src/LUA)
    target_link_libraries(RakSAMPClient PRIVATE raknet_client tinyxml common lua log)
elseif(WIN32)
    target_include_directories(RakSAMPClient PRIVATE ${LUAJIT_SRC_DIR})
    target_link_libraries(RakSAMPClient PRIVATE raknet_client tinyxml common luajit_shared ws2_32 winmm gdi32 user32 comctl32)
    # Ensure lua51.dll is found next to the exe at runtime when launched from build dir
    add_custom_command(TARGET RakSAMPClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/luajit/lua51.dll"
            "$<TARGET_FILE_DIR:RakSAMPClient>/lua51.dll"
        COMMENT "Copy lua51.dll next to RakSAMPClient"
    )
else()
    target_include_directories(RakSAMPClient PRIVATE ${LUAJIT_SRC_DIR})
    target_link_libraries(RakSAMPClient PRIVATE raknet_client tinyxml common luajit_shared ${CMAKE_DL_LIBS} pthread)
    # $ORIGIN so libluajit-5.1.so.2 next to the binary is found
    set_target_properties(RakSAMPClient PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
    add_custom_command(TARGET RakSAMPClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/luajit/libluajit-5.1.so.2"
            "$<TARGET_FILE_DIR:RakSAMPClient>/libluajit-5.1.so.2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/luajit/libluajit-5.1.so.2"
            "$<TARGET_FILE_DIR:RakSAMPClient>/libluajit-5.1.so"
        COMMENT "Copy libluajit next to RakSAMPClient"
    )
endif()
