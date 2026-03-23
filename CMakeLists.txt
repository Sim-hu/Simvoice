cmake_minimum_required(VERSION 3.22)
project(tts-bot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependencies ---

find_package(SQLite3 REQUIRED)
find_package(spdlog REQUIRED)

find_package(dpp QUIET)
if(NOT dpp_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        dpp
        GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
        GIT_TAG v10.0.35
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(dpp)
endif()

# --- VOICEVOX Core (optional) ---

option(ENABLE_VOICEVOX "Enable VOICEVOX TTS engine" ON)

if(ENABLE_VOICEVOX)
    find_library(VOICEVOX_CORE_LIB voicevox_core
        HINTS
            ${VOICEVOX_CORE_DIR}/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/lib
        PATHS
            /usr/local/lib
            /usr/lib
            /opt/voicevox_core/lib
    )

    if(VOICEVOX_CORE_LIB)
        message(STATUS "Found VOICEVOX Core: ${VOICEVOX_CORE_LIB}")
    else()
        message(WARNING
            "VOICEVOX Core not found. TTS will be disabled.\n"
            "  Set -DVOICEVOX_CORE_DIR=/path/to/voicevox_core or place libvoicevox_core.so in lib/")
        set(ENABLE_VOICEVOX OFF)
    endif()
endif()

# --- Main executable ---

add_executable(tts-bot
    src/main.cpp
    src/config/config.cpp
    src/guild/state.cpp
    src/bot/commands/join.cpp
    src/bot/commands/voice.cpp
    src/bot/commands/dict.cpp
    src/bot/commands/stats.cpp
    src/bot/handler.cpp
    src/db/database.cpp
    src/tts/voicevox.cpp
    src/tts/synthesizer.cpp
    src/tts/preprocessor.cpp
    src/audio/pipeline.cpp
    src/audio/cache.cpp
    src/guild/queue.cpp
)

target_include_directories(tts-bot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(tts-bot PRIVATE
    dpp
    SQLite::SQLite3
    spdlog::spdlog
)

if(ENABLE_VOICEVOX)
    target_compile_definitions(tts-bot PRIVATE ENABLE_VOICEVOX VOICEVOX_LOAD_ONNXRUNTIME)
    target_include_directories(tts-bot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(tts-bot PRIVATE ${VOICEVOX_CORE_LIB})
endif()
