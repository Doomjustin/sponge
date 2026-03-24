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
)

spg_add_module(leveldb leveldb
    PUBLIC
        sponge::common
        fmt::fmt
        Microsoft.GSL::GSL
)