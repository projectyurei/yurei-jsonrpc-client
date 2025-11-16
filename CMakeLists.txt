cmake_minimum_required(VERSION 3.16)
project(yurei-jsonrpc-client VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_search_module(CJSON REQUIRED cjson libcjson cJSON)
pkg_check_modules(LIBPQ REQUIRED libpq)

add_executable(yurei-jsonrpc-client
    src/main.c
    src/config.c
    src/logging.c
    src/event_queue.c
    src/parser.c
    src/websocket_client.c
    src/http_poller.c
    src/db_writer.c)

# Project include tree
file(GLOB PROJECT_HEADERS "include/*.h")
target_sources(yurei-jsonrpc-client PRIVATE ${PROJECT_HEADERS})

target_include_directories(yurei-jsonrpc-client
    PRIVATE
        include
        ${LIBWEBSOCKETS_INCLUDE_DIRS}
        ${LIBCURL_INCLUDE_DIRS}
        ${CJSON_INCLUDE_DIRS}
        ${LIBPQ_INCLUDE_DIRS})

# Propagate pkg-config compile definitions/flags
foreach(flag ${LIBWEBSOCKETS_CFLAGS_OTHER} ${LIBCURL_CFLAGS_OTHER} ${CJSON_CFLAGS_OTHER} ${LIBPQ_CFLAGS_OTHER})
    target_compile_options(yurei-jsonrpc-client PRIVATE ${flag})
endforeach()

# Link external dependencies
set(THIRD_PARTY_LIBS
    ${LIBWEBSOCKETS_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${LIBPQ_LIBRARIES})

# Handle pkg-config library directories at link time
foreach(dir ${LIBWEBSOCKETS_LIBRARY_DIRS} ${LIBCURL_LIBRARY_DIRS} ${CJSON_LIBRARY_DIRS} ${LIBPQ_LIBRARY_DIRS})
    target_link_directories(yurei-jsonrpc-client PRIVATE ${dir})
endforeach()

# Ensure pthread + rt on Linux
find_package(Threads REQUIRED)
target_link_libraries(yurei-jsonrpc-client PRIVATE ${THIRD_PARTY_LIBS} Threads::Threads)

install(TARGETS yurei-jsonrpc-client RUNTIME DESTINATION bin)
