cmake_minimum_required(VERSION 3.20)
project(command-deck-control CXX)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
include(CTest)

# ── Third-party ───────────────────────────────────────────────────────────────

FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)

# Must be set before FetchContent_MakeAvailable so LVGL's own CMakeLists picks
# it up and adds the parent directory to lvgl's PUBLIC include path.
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)

# Skip LVGL demos and examples — saves ~60 s of compile time
set(LV_CONF_BUILD_DISABLE_DEMOS    1 CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES 1 CACHE STRING "" FORCE)

FetchContent_MakeAvailable(lvgl nlohmann_json tomlplusplus)

# Exclude ARM-only assembly files — they won't assemble on x86 and aren't
# needed for the emulator build. TARGET_DIRECTORY requires CMake ≥ 3.18.
set_source_files_properties(
    ${lvgl_SOURCE_DIR}/src/draw/sw/blend/helium/lv_blend_helium.S
    ${lvgl_SOURCE_DIR}/src/draw/sw/blend/neon/lv_blend_neon.S
    TARGET_DIRECTORY lvgl
    PROPERTIES HEADER_FILE_ONLY TRUE
)

# ── Emulator (SDL2 window instead of framebuffer) ─────────────────────────────
option(EMULATOR "Build with SDL2 window instead of framebuffer" OFF)

# System libs
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
pkg_check_modules(LIBCURL       REQUIRED libcurl)
pkg_check_modules(SQLITE3       REQUIRED sqlite3)
pkg_check_modules(FFMPEG        REQUIRED libavcodec libavformat libavutil libswscale)

target_include_directories(lvgl PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(lvgl PUBLIC ${FFMPEG_LIBRARIES})

if(EMULATOR)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    message(STATUS "Building emulator target (SDL2 display)")
endif()

# ── Sources ───────────────────────────────────────────────────────────────────

file(GLOB_RECURSE SOURCES src/*.cpp src/**/*.cpp)

add_executable(command-deck ${SOURCES})

target_include_directories(command-deck PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/../shared
    ${lvgl_SOURCE_DIR}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)


target_link_libraries(command-deck PRIVATE
    lvgl
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    ${LIBWEBSOCKETS_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    pthread
)

if(BUILD_TESTING)
    add_executable(activity-store-test
        tests/activity_store_test.cpp
        src/activity_store.cpp
    )
    target_include_directories(activity-store-test PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/../shared
    )
    target_link_libraries(activity-store-test PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME activity-store COMMAND activity-store-test)

    add_executable(theme-profile-test
        tests/theme_profile_test.cpp
        src/ui/theme_profile.cpp
    )
    target_include_directories(theme-profile-test PRIVATE src ${lvgl_SOURCE_DIR})
    target_link_libraries(theme-profile-test PRIVATE tomlplusplus::tomlplusplus lvgl)
    add_test(NAME theme-profile COMMAND theme-profile-test)

    add_executable(power-core-test
        tests/power_core_test.cpp
        src/activity_store.cpp
        src/command_router.cpp
        src/config.cpp
        src/metrics_store.cpp
        src/pdu/pdu_store.cpp
        src/pdu/synlink_client.cpp
        src/power/power_history_store.cpp
        src/power/power_sequence_engine.cpp
        src/power/power_budget_controller.cpp
        src/wol.cpp
    )
    target_include_directories(power-core-test PRIVATE
        src ${CMAKE_SOURCE_DIR}/../shared ${SQLITE3_INCLUDE_DIRS}
    )
    target_link_libraries(power-core-test PRIVATE
        nlohmann_json::nlohmann_json tomlplusplus::tomlplusplus
        ${SQLITE3_LIBRARIES} ${LIBCURL_LIBRARIES}
    )
    add_test(NAME power-core COMMAND power-core-test)

    # Manual live-network diagnostic; intentionally excluded from CTest.
    add_executable(pdu-network-test
        tests/pdu_network_test.cpp
        src/config.cpp
        src/pdu/synlink_client.cpp
    )
    target_include_directories(pdu-network-test PRIVATE
        src ${CMAKE_SOURCE_DIR}/../shared
    )
    target_link_libraries(pdu-network-test PRIVATE
        nlohmann_json::nlohmann_json tomlplusplus::tomlplusplus ${LIBCURL_LIBRARIES}
    )
endif()

if(EMULATOR)
    target_compile_definitions(command-deck PRIVATE EMULATOR_BUILD)
    target_include_directories(command-deck PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(command-deck PRIVATE ${SDL2_LIBRARIES})
endif()

# ── Install ───────────────────────────────────────────────────────────────────

install(TARGETS command-deck DESTINATION /usr/local/bin)
install(FILES   config.toml  DESTINATION /etc/command-deck)
install(FILES   scripts/command-deck.service
    DESTINATION /etc/systemd/system)
install(PROGRAMS scripts/command-deck-update
    DESTINATION /usr/local/libexec)
