#include <cstdlib>
#include <string>

#include <sponge/http/server.h>

using namespace spg;

auto ping_message(const http::Request& request, std::string_view message) -> std::string
{
    return std::string{ message };
}

struct User {
    int id;
    std::string name;
};

struct NewUser {
    std::string name;
};

auto new_user(const http::Request& request, NewUser body) -> User
{
    return { .id = 1, .name = std::move(body.name) };
}

auto return_struct(const http::Request& request) -> User
{
    return { .id = 42, .name = "The answer" };
}

auto get_user(const http::Request& request, int id) -> User
{
    return { .id = id, .name = "The answer" };
}

auto ping(const http::Request& request) -> std::string { return "pong"; }

int main(int argc, char* argv[])
{
    http::Server server{ "127.0.0.1", "14444" };

    server.Post<"/ping/<str:message>">(ping_message);
    server.Post<"/ping">(ping);
    server.Get<"/user">(return_struct);
    server.Post<"/new_user">(new_user);
    server.Get<"/user/<int:id>">(get_user);

    server.run();
    return EXIT_SUCCESS;
}
