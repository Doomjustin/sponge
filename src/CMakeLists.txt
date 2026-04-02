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

add_executable(sponge.http-server http_server.cpp)
target_compile_features(sponge.http-server PRIVATE cxx_std_23)
target_link_libraries(sponge.http-server PRIVATE sponge::http)
