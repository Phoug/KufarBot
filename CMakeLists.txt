cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project(
    Kufar-Telegram-Notifier
    VERSION 2.6
    LANGUAGES CXX
)

find_package(CURL 8.23 REQUIRED)
find_package(Threads REQUIRED)

include_directories(include)

add_executable(
    Kufar-Telegram-Notifier
    
    src/kufar.cpp
    src/helperfunctions.cpp
    src/telegram.cpp
    src/networking.cpp
    src/main.cpp
)

target_link_libraries(Kufar-Telegram-Notifier PRIVATE CURL::libcurl Threads::Threads)
if(MSVC)
    target_compile_options(Kufar-Telegram-Notifier PRIVATE /W4 /utf-8)
else()
    target_compile_options(Kufar-Telegram-Notifier PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.tests/regression.cpp")
    add_executable(regression-tests .tests/regression.cpp src/helperfunctions.cpp src/kufar.cpp src/telegram.cpp)
    target_compile_definitions(regression-tests PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(regression-tests PRIVATE Threads::Threads)
    if(MSVC)
        target_compile_options(regression-tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(regression-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME regression COMMAND regression-tests)
endif()

# Explicit opt-in: contacts public APIs, never sends Telegram messages.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.tests/api-smoke.cpp")
    add_executable(api-smoke EXCLUDE_FROM_ALL .tests/api-smoke.cpp src/networking.cpp src/helperfunctions.cpp src/kufar.cpp)
    target_link_libraries(api-smoke PRIVATE CURL::libcurl Threads::Threads)
    if(MSVC)
        target_compile_options(api-smoke PRIVATE /W4 /utf-8)
    else()
        target_compile_options(api-smoke PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
