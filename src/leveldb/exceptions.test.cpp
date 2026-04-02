#include <sponge/leveldb/exceptions.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("在构造 LevelDBException 时，应该保存传入的错误消息", "[leveldb][exceptions]")
{
    const spg::leveldb::LevelDBException ex{ "something went wrong" };

    REQUIRE(std::string{ex.what()} == "something went wrong");
}

TEST_CASE("在 catch std::runtime_error 时，应该能捕获 LevelDBException", "[leveldb][exceptions]")
{
    REQUIRE_THROWS_AS(
        []{ throw spg::leveldb::LevelDBException{ "base error" }; }(),
        std::runtime_error
    );
}

TEST_CASE("在构造 IOException 时，应该保存带前缀的格式化错误消息", "[leveldb][exceptions]")
{
    const spg::leveldb::IOException ex{ "failed to read file: {}", "data.sst" };

    REQUIRE(std::string{ex.what()} == "IO Exception: failed to read file: data.sst");
}

TEST_CASE("在 catch LevelDBException 时，应该能捕获 IOException", "[leveldb][exceptions]")
{
    REQUIRE_THROWS_AS(
        []{ throw spg::leveldb::IOException{ "io error" }; }(),
        spg::leveldb::LevelDBException
    );
}

TEST_CASE("在构造 InvalidArgumentException 时，应该保存带前缀的格式化错误消息", "[leveldb][exceptions]")
{
    const spg::leveldb::InvalidArgumentException ex{ "invalid key size: {}", 0 };

    REQUIRE(std::string{ex.what()} == "Invalid Argument Exception: invalid key size: 0");
}

TEST_CASE("在 catch LevelDBException 时，应该能捕获 InvalidArgumentException", "[leveldb][exceptions]")
{
    REQUIRE_THROWS_AS(
        []{ throw spg::leveldb::InvalidArgumentException{ "bad argument" }; }(),
        spg::leveldb::LevelDBException
    );
}

TEST_CASE("在构造 IOException 时传入多个参数，应该全部格式化到消息中", "[leveldb][exceptions]")
{
    const spg::leveldb::IOException ex{ "read {} bytes from {}, expected {}", 10, "block.sst", 64 };

    REQUIRE(std::string{ex.what()} == "IO Exception: read 10 bytes from block.sst, expected 64");
}

TEST_CASE("在构造 InvalidArgumentException 时传入多个参数，应该全部格式化到消息中", "[leveldb][exceptions]")
{
    const spg::leveldb::InvalidArgumentException ex{ "key length {} exceeds max {}", 512, 256 };

    REQUIRE(std::string{ex.what()} == "Invalid Argument Exception: key length 512 exceeds max 256");
}
