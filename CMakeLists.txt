cmake_minimum_required(VERSION 3.10)

set(VERSION 0.9.24)
project(raft VERSION ${VERSION})

option(raft_static "build raft as static library" On)
option(raft_shared "build raft as shared library" On)

if(NOT raft_static AND NOT raft_shared)
    message(ERROR "raft should neither be build as static nor shard library - what do you want")
endif()

#first try to find the libuv via pkg-config
if(NOT TARGET libuv)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBUV IMPORTED_TARGET libuv>=1.8.0)
        if(LIBUV_FOUND)
            add_library(libuv INTERFACE)
            target_link_libraries(libuv INTERFACE PkgConfig::LIBUV)
        endif()
    endif()
endif()

#we did not find libuv via - we try to find the header and library -> mainly for non unixoides
if(NOT TARGET libuv)
    #You might manually set the variable LIBUV_INCLUDE_DIR to give a hint where to search for the include
    FIND_PATH(LIBUV_INCLUDE_DIR NAMES uv.h HINTS ${LIBUV_INCLUDE_DIR} ${LIBUV_DIR})
    #You might manually set the variable LIBUV_LIBRARY_DIR to give a hint where to search for the library
    FIND_LIBRARY(LIBUV_LIBRARY NAMES uv libuv HINTS ${LIBUV_LIBRARY_DIR} ${LIBUV_DIR})
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(LIBUV  DEFAULT_MSG
                                      LIBUV_LIBRARY LIBUV_INCLUDE_DIR)
    if(LIBUV_FOUND)
        add_library(libuv INTERFACE)
        target_include_directories(libuv INTERFACE ${LIBUV_INCLUDE_DIR})
        target_link_libraries(libuv INTERFACE ${LIBUV_LIBRARY})
    endif()
endif()

set(RAFT_SOURCES
    src/byte.c
    src/client.c
    src/configuration.c
    src/convert.c
    src/election.c
    src/entry.c
    src/err.c
    src/fixture.c
    src/heap.c
    src/log.c
    src/membership.c
    src/progress.c
    src/raft.c
    src/recv_append_entries.c
    src/recv_append_entries_result.c
    src/recv.c
    src/recv_install_snapshot.c
    src/recv_request_vote.c
    src/recv_request_vote_result.c
    src/recv_timeout_now.c
    src/replication.c
    src/snapshot.c
    src/start.c
    src/state.c
    src/syscall.c
    src/tick.c
    src/tracing.c
    )

set(RAFT_HEADERS
    src/array.h
    src/assert.h
    src/byte.h
    src/configuration.h
    src/convert.h
    src/election.h
    src/entry.h
    src/err.h
    src/heap.h
    src/log.h
    src/membership.h
    src/progress.h
    src/queue.h
    src/recv_append_entries.h
    src/recv_append_entries_result.h
    src/recv.h
    src/recv_install_snapshot.h
    src/recv_request_vote.h
    src/recv_request_vote_result.h
    src/recv_timeout_now.h
    src/replication.h
    src/request.h
    src/snapshot.h
    src/syscall.h
    src/tick.h
    src/tracing.h
    )

if(TARGET libuv)
    set(RAFT_SOURCES
        ${RAFT_SOURCES}
        src/uv_append.c
        src/uv.c
        src/uv_encoding.c
        src/uv_finalize.c
        src/uv_fs.c
        src/uv_ip.c
        src/uv_list.c
        src/uv_metadata.c
        src/uv_os.c
        src/uv_prepare.c
        src/uv_recv.c
        src/uv_segment.c
        src/uv_send.c
        src/uv_snapshot.c
        src/uv_tcp.c
        src/uv_tcp_connect.c
        src/uv_tcp_listen.c
        src/uv_truncate.c
        src/uv_writer.c
        )
    set(RAFT_HEADERS
        src/uv_encoding.h
        src/uv_fs.h
        src/uv.h
        src/uv_ip.h
        src/uv_os.h
        src/uv_tcp.h
        src/uv_writer.h
        )
endif()

if(NOT TARGET libuv)
    message(WARNING "libuv not found we continue without")
endif()

if(raft_shared)
    add_library(${PROJECT_NAME} SHARED ${RAFT_SOURCES} ${RAFT_HEADERS})
    set_property(TARGET ${PROJECT_NAME} PROPERTY VERSION${VERSION})
    set_property(TARGET ${PROJECT_NAME} PROPERTY SOVERSION 0.7)

    if(TARGET libuv)
        target_link_libraries(${PROJECT_NAME} libuv)
    endif()
endif()

if(raft_static)
    set(STATIC_LIB_NAME ${PROJECT_NAME}_static)
    add_library(${STATIC_LIB_NAME} STATIC ${RAFT_SOURCES} ${RAFT_HEADERS})
    set_property(TARGET ${STATIC_LIB_NAME} PROPERTY VERSION${VERSION})
    set_property(TARGET ${STATIC_LIB_NAME} PROPERTY SOVERSION 0.7)

    if(TARGET libuv)
        target_link_libraries(${STATIC_LIB_NAME} libuv)
    endif()
endif()
