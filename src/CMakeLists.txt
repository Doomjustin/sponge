spg_add_module(common common
    PUBLIC
        Microsoft.GSL::GSL
)

spg_add_module(redis redis
    PUBLIC
        sponge::common
    fmt::fmt
        Microsoft.GSL::GSL
)