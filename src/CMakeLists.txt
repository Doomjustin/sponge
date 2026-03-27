spg_add_module(common common
    PUBLIC
        Microsoft.GSL::GSL
        Boost::asio
)

spg_add_module(redis redis
    PUBLIC
        sponge::common
        fmt::fmt
        Microsoft.GSL::GSL
        Boost::unordered
        Boost::asio
)

spg_add_module(leveldb leveldb
    PUBLIC
        sponge::common
        fmt::fmt
        Microsoft.GSL::GSL
)

spg_add_module(http http
    PUBLIC
        fmt::fmt
        spdlog::spdlog
        Microsoft.GSL::GSL
        Boost::asio
        Boost::beast
        glaze::glaze
        ctre::ctre
)

add_executable(sponge.redis-server redis_server.cpp)
target_compile_features(sponge.redis-server PRIVATE cxx_std_23)
target_link_libraries(sponge.redis-server PRIVATE sponge::redis)

add_executable(sponge.http-server http_server.cpp)
target_compile_features(sponge.http-server PRIVATE cxx_std_23)
target_link_libraries(sponge.http-server PRIVATE sponge::http)
