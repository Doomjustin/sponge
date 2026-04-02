spg_add_module(common common
    PUBLIC
        Microsoft.GSL::GSL
        fmt::fmt
        spdlog::spdlog
        Boost::asio
        PkgConfig::LIBURING
)

spg_add_module(sync sync
    PUBLIC
        sponge::common
        Microsoft.GSL::GSL
)

spg_add_module(redis redis
    PUBLIC
        sponge::common
        fmt::fmt
        Microsoft.GSL::GSL
        Boost::unordered
        Boost::beast
        Boost::asio
        PkgConfig::LIBURING
)

spg_add_module(leveldb leveldb
    PUBLIC
        sponge::common
        fmt::fmt
        Microsoft.GSL::GSL
        Crc32c::crc32c
)

spg_add_module(http http
    PUBLIC
        Microsoft.GSL::GSL
        fmt::fmt
        spdlog::spdlog
        Boost::asio
        Boost::beast
        glaze::glaze
        ctre::ctre
        PkgConfig::LIBURING
)


add_executable(sponge.redis-server redis_server.cpp)
target_compile_features(sponge.redis-server PRIVATE cxx_std_23)
target_link_libraries(sponge.redis-server PRIVATE sponge::redis)

find_library(JEMALLOC_LIB jemalloc)

if(JEMALLOC_LIB)
    message(STATUS "Found jemalloc: ${JEMALLOC_LIB}, using it as default allocator!")
    target_link_libraries(sponge.redis-server PRIVATE ${JEMALLOC_LIB})
else()
    message(WARNING "jemalloc not found, falling back to glibc allocator.")
endif()

add_executable(sponge.http-server http_server.cpp)
target_compile_features(sponge.http-server PRIVATE cxx_std_23)
target_link_libraries(sponge.http-server PRIVATE sponge::http)
