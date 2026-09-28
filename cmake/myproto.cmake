if (WIN32 OR APPLE)
    # Homebrew protobuf's CONFIG package carries absl::* and utf8_range in its interface;
    # CMake's FindProtobuf module does not, and ld64 does not resolve symbols through
    # indirect dylibs, so module mode leaves undefined absl:: symbols on macOS.
    find_package(Protobuf CONFIG REQUIRED)
else ()
    find_package(Protobuf REQUIRED)
endif ()

set(PROTO_FILES
        go/grpc_server/gen/libcore.proto
        )

add_library(myproto STATIC ${PROTO_FILES})
target_link_libraries(myproto
        PUBLIC
        protobuf::libprotobuf
        )
target_include_directories(myproto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate(TARGET myproto LANGUAGE cpp)
