cmake_minimum_required (VERSION 2.6)
project (lthread)
#set(CMAKE_VERBOSE_MAKEFILE ON)
SET(CMAKE_BUILD_TYPE release)
SET (CMAKE_C_FLAGS_RELEASE "-Werror -g -std=gnu99 -Wall")

set(LTHREAD_VERSION_MAJOR 1)
set(LTHREAD_VERSION_MINOR 0)
set(LTHREAD_VERSION_PATCH 0)
set(LTHREAD_VERSION_STRING ${LTHREAD_VERSION_MAJOR}.${LTHREAD_VERSION_MINOR}.${LTHREAD_VERSION_PATCH})

set(LTHREAD_FILES src/lthread.c src/lthread_socket.c
                  src/lthread_sched.c src/lthread_io.c
                  src/lthread_poller.c src/lthread_compute.c
                  src/chan.c src/fifo.c)

add_library(lthread  ${LTHREAD_FILES})
set_property(SOURCE ${LTHREAD_FILES} PROPERTY COMPILE_FLAGS "-O2")

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/klib)
include_directories(${PROJECT_SOURCE_DIR}/src/dns)
include_directories(${PROJECT_SOURCE_DIR}/src/mux)

file(GLOB_RECURSE COMMON ${PROJECT_SOURCE_DIR}/src/common/*.c)
file(GLOB_RECURSE KLIB ${PROJECT_SOURCE_DIR}/src/klib/*.c)
file(GLOB_RECURSE CDNS ${PROJECT_SOURCE_DIR}/src/dns/*.c)
file(GLOB_RECURSE MUX ${PROJECT_SOURCE_DIR}/src/mux/*.c)

add_executable(mutex_test ${COMMON} ${KLIB} tests/mutex_test.c)
target_link_libraries(mutex_test z rt lthread pthread)

add_executable(chan_test ${COMMON} ${KLIB} ${LTHREAD_FILES})
target_link_libraries(chan_test z rt pthread)
set_target_properties(chan_test PROPERTIES COMPILE_FLAGS " -DCHAN_TEST ")

add_executable(conn_test ${COMMON} ${KLIB} ${LTHREAD_FILES} ${CDNS} ${MUX} tests/tcp_conn_test.c)
target_link_libraries(conn_test z rt pthread)
set_target_properties(conn_test PROPERTIES COMPILE_FLAGS " -DCONN_TEST ")

add_executable(ring_test ${KLIB})
target_link_libraries(ring_test z rt lthread pthread)
set_target_properties(ring_test PROPERTIES COMPILE_FLAGS " -DRING_TEST ")
