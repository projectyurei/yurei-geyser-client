cmake_minimum_required(VERSION 3.20)
project(yurei-geyser-client LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(ProtobufC REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(gRPC REQUIRED CONFIG)

set(PROTO_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/proto")
set(PROTO_SRCS
  ${PROTO_GEN_DIR}/geyser.pb-c.c
  ${PROTO_GEN_DIR}/solana-storage.pb-c.c
  ${PROTO_GEN_DIR}/google/protobuf/timestamp.pb-c.c
)

set(SRC_FILES
  src/base58.c
  src/base64.c
  src/db_writer.c
  src/event_queue.c
  src/geyser_client.c
  src/log.c
  src/metrics.c
  src/pumpfun_parser.c
  src/protocol_detector.c
  src/raydium_parser.c
  src/yurei_config.c
)

add_library(yurei_objs STATIC ${SRC_FILES} ${PROTO_SRCS})

target_include_directories(yurei_objs
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${PROTO_GEN_DIR}
)

# Compiler warnings and optimizations
target_compile_options(yurei_objs PRIVATE -Wall -Wextra -Wpedantic -Werror=vla)

# Release build optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(yurei_objs PRIVATE -O3 -flto)
  if(NOT APPLE)
    target_compile_options(yurei_objs PRIVATE -march=native)
  endif()
endif()

find_package(OpenSSL)
if(OPENSSL_FOUND)
  target_link_libraries(yurei_objs PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

target_link_libraries(yurei_objs
  PRIVATE
    ProtobufC::protobuf-c
    gRPC::grpc
    gRPC::gpr
    PostgreSQL::PostgreSQL
    Threads::Threads
    m
)

add_executable(yurei-geyser-client src/main.c)
target_link_libraries(yurei-geyser-client PRIVATE yurei_objs)
set_target_properties(yurei-geyser-client PROPERTIES OUTPUT_NAME "yurei-geyser-client")

enable_testing()
add_executable(test_pumpfun_parser tests/test_pumpfun_parser.c)
target_link_libraries(test_pumpfun_parser PRIVATE yurei_objs)
add_test(NAME pumpfun_parser COMMAND test_pumpfun_parser)

add_executable(test_protocol_detector tests/test_protocol_detector.c)
target_link_libraries(test_protocol_detector PRIVATE yurei_objs)
add_test(NAME protocol_detector COMMAND test_protocol_detector)
