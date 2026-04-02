#include <cstdlib>

#include "application.h"

int main(int argc, char* argv[])
{
    using namespace spg::redis;

    Application app{};
    app.execute();

    return EXIT_SUCCESS;
}