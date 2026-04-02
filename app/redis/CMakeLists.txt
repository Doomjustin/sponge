spg_add_module(redis
    PUBLIC
        sponge::common
        fmt::fmt
        Microsoft.GSL::GSL
        Boost::unordered
        Boost::beast
        Boost::asio
        PkgConfig::LIBURING
)


add_executable(sponge.redis-server main.cpp)
target_compile_features(sponge.redis-server PRIVATE cxx_std_23)
target_link_libraries(sponge.redis-server PRIVATE sponge::redis)

find_library(JEMALLOC_LIB jemalloc)

if(JEMALLOC_LIB)
    message(STATUS "Found jemalloc: ${JEMALLOC_LIB}, using it as default allocator!")
    target_link_libraries(sponge.redis-server PRIVATE ${JEMALLOC_LIB})
else()
    message(WARNING "jemalloc not found, falling back to glibc allocator.")
endif()
