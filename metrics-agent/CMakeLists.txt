cmake_minimum_required(VERSION 3.20)
project(command-deck-agent CXX)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(nlohmann_json tomlplusplus)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)

file(GLOB_RECURSE SOURCES src/*.cpp src/**/*.cpp)

add_executable(deck-agent ${SOURCES})

target_include_directories(deck-agent PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/../shared
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

# Conditionally compile GPIO support (Pi only)
option(ENABLE_GPIO "Enable GPIO control via libgpiod" OFF)
if(ENABLE_GPIO)
    pkg_check_modules(LIBGPIOD REQUIRED libgpiod)
    target_compile_definitions(deck-agent PRIVATE ENABLE_GPIO)
    target_include_directories(deck-agent PRIVATE ${LIBGPIOD_INCLUDE_DIRS})
    target_link_libraries(deck-agent PRIVATE ${LIBGPIOD_LIBRARIES})
endif()

target_link_libraries(deck-agent PRIVATE
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    ${LIBWEBSOCKETS_LIBRARIES}
    pthread
)

install(TARGETS deck-agent       DESTINATION /usr/local/bin)
install(FILES   agent.toml       DESTINATION /etc/command-deck)
install(FILES   scripts/agent.service
    DESTINATION /etc/systemd/system)
