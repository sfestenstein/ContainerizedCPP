cmake_minimum_required(VERSION 3.20)

project(PubSubLib CXX)

find_package(czmq QUIET)
find_package(zyre QUIET)
find_package(ZeroMQ QUIET)
find_package(PkgConfig REQUIRED)

# Apt packages often expose czmq/zyre/zmq via pkg-config rather than CMake config.
if(NOT TARGET czmq-static AND NOT TARGET czmq AND NOT TARGET libczmq)
    pkg_check_modules(CZMQ QUIET IMPORTED_TARGET libczmq)
    if(NOT CZMQ_FOUND)
        pkg_check_modules(CZMQ QUIET IMPORTED_TARGET czmq)
    endif()
endif()

if(NOT TARGET zyre-static AND NOT TARGET zyre AND NOT TARGET libzyre)
    pkg_check_modules(ZYRE QUIET IMPORTED_TARGET libzyre)
    if(NOT ZYRE_FOUND)
        pkg_check_modules(ZYRE QUIET IMPORTED_TARGET zyre)
    endif()
endif()

if(NOT TARGET libzmq-static AND NOT TARGET libzmq AND NOT TARGET ZeroMQ::ZeroMQ)
    pkg_check_modules(ZMQ QUIET IMPORTED_TARGET libzmq)
    if(NOT ZMQ_FOUND)
        pkg_check_modules(ZMQ QUIET IMPORTED_TARGET zmq)
    endif()
endif()

set(CZMQ_LINK_TARGET)
if(TARGET czmq-static)
    set(CZMQ_LINK_TARGET czmq-static)
elseif(TARGET czmq)
    set(CZMQ_LINK_TARGET czmq)
elseif(TARGET libczmq)
    set(CZMQ_LINK_TARGET libczmq)
elseif(TARGET PkgConfig::CZMQ)
    set(CZMQ_LINK_TARGET PkgConfig::CZMQ)
endif()

set(ZYRE_LINK_TARGET)
if(TARGET zyre-static)
    set(ZYRE_LINK_TARGET zyre-static)
elseif(TARGET zyre)
    set(ZYRE_LINK_TARGET zyre)
elseif(TARGET libzyre)
    set(ZYRE_LINK_TARGET libzyre)
elseif(TARGET PkgConfig::ZYRE)
    set(ZYRE_LINK_TARGET PkgConfig::ZYRE)
endif()

set(ZMQ_LINK_TARGET)
if(TARGET libzmq-static)
    set(ZMQ_LINK_TARGET libzmq-static)
elseif(TARGET libzmq)
    set(ZMQ_LINK_TARGET libzmq)
elseif(TARGET ZeroMQ::ZeroMQ)
    set(ZMQ_LINK_TARGET ZeroMQ::ZeroMQ)
elseif(TARGET PkgConfig::ZMQ)
    set(ZMQ_LINK_TARGET PkgConfig::ZMQ)
endif()

if(NOT CZMQ_LINK_TARGET OR NOT ZYRE_LINK_TARGET OR NOT ZMQ_LINK_TARGET)
    message(FATAL_ERROR
        "Could not resolve PubSub dependency targets: "
        "czmq=${CZMQ_LINK_TARGET}, zyre=${ZYRE_LINK_TARGET}, zmq=${ZMQ_LINK_TARGET}. "
        "Install libczmq/libzyre/libzmq development packages or provide CMake package configs."
    )
endif()

file(GLOB SOURCES     "*.cpp")
file(GLOB HEADERS     "*.h")

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${CZMQ_LINK_TARGET}
    ${ZYRE_LINK_TARGET}
    ${ZMQ_LINK_TARGET}
    CommonUtils
    protobuf::libprotobuf
)
