#include <sponge/redis/string.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <string>
#include <unordered_map>
#include <utility>

#include <catch2/catch_test_macros.hpp>

namespace {

class CountingResource : public std::pmr::memory_resource {
public:
	[[nodiscard]]
	auto allocations() const noexcept -> std::size_t { return alloc_count_; }

	[[nodiscard]]
	auto deallocations() const noexcept -> std::size_t { return dealloc_count_; }

	[[nodiscard]]
	auto has_size_mismatch() const noexcept -> bool { return size_mismatch_; }

	[[nodiscard]]
	auto outstanding() const noexcept -> std::size_t { return allocated_.size(); }

private:
	auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override
	{
		auto* ptr = upstream_.allocate(bytes, alignment);
		allocated_[ptr] = bytes;
		++alloc_count_;
		return ptr;
	}

	void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) override
	{
		auto it = allocated_.find(ptr);
		if (it == allocated_.end() || it->second != bytes)
			size_mismatch_ = true;
		else
			allocated_.erase(it);

		++dealloc_count_;
		upstream_.deallocate(ptr, bytes, alignment);
	}

	[[nodiscard]]
	auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override
	{
		return this == &other;
	}

	std::pmr::memory_resource& upstream_{ *std::pmr::new_delete_resource() };
	std::unordered_map<void*, std::size_t> allocated_{};
	std::size_t alloc_count_{ 0 };
	std::size_t dealloc_count_{ 0 };
	bool size_mismatch_{ false };
};

} // namespace

TEST_CASE("String allocates and frees memory", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "hello", &resource };
		(void)str;
		REQUIRE(resource.allocations() == 1);
		REQUIRE(resource.deallocations() == 0);
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String handles empty string", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "", &resource };
		(void)str;
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String move constructor transfers ownership", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String first{ "abc", &resource };
		spg::redis::String second{ std::move(first) };
		(void)second;
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String move assignment releases old allocation", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String first{ "first", &resource };
		spg::redis::String second{ "second", &resource };

		REQUIRE(resource.allocations() == 2);
		REQUIRE(resource.deallocations() == 0);

		second = std::move(first);

		REQUIRE(resource.allocations() == 2);
		REQUIRE(resource.deallocations() == 1);
	}

	REQUIRE(resource.allocations() == 2);
	REQUIRE(resource.deallocations() == 2);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String self move assignment is safe", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "self", &resource };
		str = std::move(str);
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String default constructor creates empty string", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ &resource };

	REQUIRE(str.size() == 0);
	REQUIRE(str.capacity() == 0);
	REQUIRE(str.available() == 0);
	REQUIRE(str.view().empty());
	REQUIRE(str.begin() == str.end());
}

TEST_CASE("String exposes view indexing and iterators", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ "hello", &resource };

	REQUIRE(str.size() == 5);
	REQUIRE(str.capacity() == 5);
	REQUIRE(str.available() == 0);
	REQUIRE(str.view() == "hello");
	REQUIRE(str[1] == 'e');

	str[1] = 'a';
	REQUIRE(str.view() == "hallo");

	std::string copy{ str.begin(), str.end() };
	REQUIRE(copy == "hallo");
	REQUIRE(*str.end() == '\0');
}

TEST_CASE("String copy constructor performs deep copy", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String original{ "hello", &resource };
	spg::redis::String copy{ original };

	REQUIRE(resource.allocations() == 2);
	REQUIRE(copy.view() == "hello");
	REQUIRE(copy.capacity() == original.capacity());

	copy[0] = 'y';
	REQUIRE(copy.view() == "yello");
	REQUIRE(original.view() == "hello");
}

TEST_CASE("String copy assignment releases old storage and deep copies", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String original{ "hello", &resource };
	spg::redis::String target{ "x", &resource };

	REQUIRE(resource.allocations() == 2);
	REQUIRE(resource.deallocations() == 0);

	target = original;

	REQUIRE(resource.allocations() == 3);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE(target.view() == "hello");
	REQUIRE(target.capacity() >= original.capacity());
	REQUIRE(target.capacity() >= target.size());

	target[0] = 'j';
	REQUIRE(target.view() == "jello");
	REQUIRE(original.view() == "hello");
}

TEST_CASE("String copy assignment between different resources keeps target resource", "[redis][string]")
{
	CountingResource src_resource{};
	CountingResource dst_resource{};

	{
		spg::redis::String original{ "hello", &src_resource };
		spg::redis::String target{ "x", &dst_resource };

		target = original;

		REQUIRE(target.view() == "hello");
		REQUIRE(src_resource.allocations() == 1);
		REQUIRE(src_resource.deallocations() == 0);
		REQUIRE(dst_resource.allocations() == 2);
		REQUIRE(dst_resource.deallocations() == 1);
	}

	REQUIRE(src_resource.outstanding() == 0);
	REQUIRE(dst_resource.outstanding() == 0);
	REQUIRE_FALSE(src_resource.has_size_mismatch());
	REQUIRE_FALSE(dst_resource.has_size_mismatch());
}

TEST_CASE("String append grows storage and preserves content", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ "ab", &resource };

	str.append("c");

	REQUIRE(str.view() == "abc");
	REQUIRE(str.size() == 3);
	REQUIRE(str.capacity() >= str.size());
	REQUIRE(str.available() == str.capacity() - str.size());
	REQUIRE(resource.allocations() == 2);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String append can reuse available space", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ "ab", &resource };

	str.append("c");
	auto allocations_after_growth = resource.allocations();
	auto deallocations_after_growth = resource.deallocations();

	str.append("d");

	REQUIRE(str.view() == "abcd");
	REQUIRE(str.size() == 4);
	REQUIRE(str.capacity() >= 4);
	REQUIRE(resource.allocations() == allocations_after_growth);
	REQUIRE(resource.deallocations() == deallocations_after_growth);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String append empty string keeps content unchanged", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ "hello", &resource };

	auto allocations_before = resource.allocations();
	auto deallocations_before = resource.deallocations();
	auto size_before = str.size();
	auto capacity_before = str.capacity();

	str.append("");

	REQUIRE(str.view() == "hello");
	REQUIRE(str.size() == size_before);
	REQUIRE(str.capacity() == capacity_before);
	REQUIRE(resource.allocations() == allocations_before);
	REQUIRE(resource.deallocations() == deallocations_before);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String supports multiple consecutive appends", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ &resource };

	str.append("ab");
	str.append("cd");
	str.append("efg");

	REQUIRE(str.view() == "abcdefg");
	REQUIRE(str.size() == 7);
	REQUIRE(str.capacity() >= 7);
	REQUIRE(str.available() == str.capacity() - str.size());
	REQUIRE(*str.end() == '\0');
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String append crossing 255 capacity preserves content", "[redis][string]")
{
	CountingResource resource{};
	std::string initial(255, 'a');
	spg::redis::String str{ initial, &resource };

	str.append("b");

	REQUIRE(str.size() == 256);
	REQUIRE(str.capacity() >= 256);
	REQUIRE(str.view().substr(0, 255) == initial);
	REQUIRE(str[255] == 'b');
	REQUIRE(*str.end() == '\0');
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String reserve is no-op when capacity is sufficient", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ "hello", &resource };

	const auto allocs_before = resource.allocations();
	const auto cap_before = str.capacity();

	str.reserve(3);

	REQUIRE(str.view() == "hello");
	REQUIRE(str.size() == 5);
	REQUIRE(str.capacity() == cap_before);
	REQUIRE(resource.allocations() == allocs_before);
	REQUIRE(resource.outstanding() == 1);
}

TEST_CASE("String reserve expands capacity and preserves content", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "hello", &resource };
		str.reserve(20);

		REQUIRE(str.capacity() >= 20);
		REQUIRE(str.size() == 5);
		REQUIRE(str.view() == "hello");
		REQUIRE(resource.allocations() == 2);
		REQUIRE(resource.deallocations() == 1);
		REQUIRE(resource.outstanding() == 1);
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String greedy reserve doubles capacity below prealloc limit", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "hi", &resource };
		str.reserve(10, spg::redis::greedy);

		REQUIRE(str.capacity() == 20);
		REQUIRE(str.size() == 2);
		REQUIRE(str.view() == "hi");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String resize extends length with zero bytes", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "ab", &resource };
		str.resize(5);

		REQUIRE(str.size() == 5);
		REQUIRE(str.capacity() >= 5);
		REQUIRE(str[0] == 'a');
		REQUIRE(str[1] == 'b');
		REQUIRE(str[2] == '\0');
		REQUIRE(str[3] == '\0');
		REQUIRE(str[4] == '\0');
		REQUIRE(*str.end() == '\0');
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String resize can shrink without reallocation", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ "hello", &resource };

	const auto allocs_before = resource.allocations();
	const auto deallocs_before = resource.deallocations();

	str.resize(3);

	REQUIRE(str.size() == 3);
	REQUIRE(str.view() == "hel");
	REQUIRE(*str.end() == '\0');
	REQUIRE(resource.allocations() == allocs_before);
	REQUIRE(resource.deallocations() == deallocs_before);
}

TEST_CASE("String clear resets size without deallocating buffer", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String str{ "hello", &resource };

	const auto allocs_before = resource.allocations();
	const auto deallocs_before = resource.deallocations();
	const auto cap_before = str.capacity();

	str.clear();

	REQUIRE(str.size() == 0);
	REQUIRE(str.capacity() == cap_before);
	REQUIRE(str.view().empty());
	REQUIRE(str.begin() == str.end());
	REQUIRE(*str.end() == '\0');
	REQUIRE(resource.allocations() == allocs_before);
	REQUIRE(resource.deallocations() == deallocs_before);
	REQUIRE(resource.outstanding() == 1);
}

TEST_CASE("String clear allows reuse of buffer without reallocating", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ &resource };
		str.reserve(20);

		const auto allocs_after_reserve = resource.allocations();

		str.assign("first");
		str.clear();
		str.assign("second");

		REQUIRE(str.view() == "second");
		REQUIRE(str.size() == 6);
		REQUIRE(str.capacity() >= 20);
		REQUIRE(resource.allocations() == allocs_after_reserve);
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String assign replaces content within existing capacity", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "hello", &resource };
		str.reserve(20);

		const auto allocs_before = resource.allocations();
		str.assign("world!");

		REQUIRE(str.view() == "world!");
		REQUIRE(str.size() == 6);
		REQUIRE(str.capacity() >= 20);
		REQUIRE(resource.allocations() == allocs_before);
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String assign reallocates when content exceeds capacity", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "hi", &resource };
		str.assign("much longer string");

		REQUIRE(str.view() == "much longer string");
		REQUIRE(str.size() == 18);
		REQUIRE(str.capacity() >= 18);
		REQUIRE(resource.allocations() == 2);
		REQUIRE(resource.deallocations() == 1);
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String self copy assignment is safe", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String str{ "hello", &resource };
		const auto* const ptr_before = str.begin();

		str = str;

		REQUIRE(str.view() == "hello");
		REQUIRE(str.begin() == ptr_before);
		REQUIRE(resource.allocations() == 1);
		REQUIRE(resource.deallocations() == 0);
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("string_cast converts signed positive and zero", "[redis][string]")
{
	CountingResource resource{};

	{
		auto zero = spg::redis::string_cast(0, &resource);
		auto pos = spg::redis::string_cast(12345, &resource);

		REQUIRE(zero.view() == "0");
		REQUIRE(pos.view() == "12345");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("string_cast converts signed negative values", "[redis][string]")
{
	CountingResource resource{};

	{
		auto neg = spg::redis::string_cast(-9876, &resource);
		REQUIRE(neg.view() == "-9876");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("string_cast handles signed limits", "[redis][string]")
{
	CountingResource resource{};

	{
		auto min_v = spg::redis::string_cast(std::numeric_limits<std::int64_t>::min(),
			&resource);
		auto max_v = spg::redis::string_cast(std::numeric_limits<std::int64_t>::max(),
			&resource);

		REQUIRE(min_v.view() == "-9223372036854775808");
		REQUIRE(max_v.view() == "9223372036854775807");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("string_cast handles unsigned values and limits", "[redis][string]")
{
	CountingResource resource{};

	{
		auto u = spg::redis::string_cast(42u, &resource);
		auto umax = spg::redis::string_cast(std::numeric_limits<std::uint64_t>::max(),
			&resource);

		REQUIRE(u.view() == "42");
		REQUIRE(umax.view() == "18446744073709551615");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("format uses provided memory resource", "[redis][string]")
{
	CountingResource resource{};

	{
		auto str = spg::redis::format(
			&resource, "user:{} score:{}", 7, 99);

		REQUIRE(str.view() == "user:7 score:99");
		REQUIRE(resource.allocations() == 1);
		REQUIRE(resource.deallocations() == 0);
		REQUIRE(resource.outstanding() == 1);
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("format can format String argument via format_as", "[redis][string]")
{
	CountingResource resource{};

	{
		spg::redis::String name{ "alice", &resource };
		auto str = spg::redis::format(
			&resource, "name={}", name);

		REQUIRE(str.view() == "name=alice");
		REQUIRE(resource.allocations() == 2);
		REQUIRE(resource.deallocations() == 0);
		REQUIRE(resource.outstanding() == 2);
	}

	REQUIRE(resource.allocations() == 2);
	REQUIRE(resource.deallocations() == 2);
	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("format supports escaped braces and mixed argument types", "[redis][string]")
{
	CountingResource resource{};
	spg::redis::String user{ "bob", &resource };

	auto out = spg::redis::format(&resource, "{{user}}={} active={} score={}", user, true, 42);

	REQUIRE(out.view() == "{user}=bob active=true score=42");
	REQUIRE(resource.allocations() >= 2);
	REQUIRE(resource.outstanding() >= 1);
}

TEST_CASE("format handles long content growth", "[redis][string]")
{
	CountingResource resource{};
	std::string payload(600, 'x');

	{
		auto out = spg::redis::format(&resource, "prefix:{}:suffix", payload);

		REQUIRE(out.size() == payload.size() + 14);
		REQUIRE(out.view().substr(0, 7) == "prefix:");
		REQUIRE(out.view().substr(out.size() - 7) == ":suffix");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("string_cast default overload converts integral values", "[redis][string]")
{
	auto a = spg::redis::string_cast(12345);
	auto b = spg::redis::string_cast(-808);

	REQUIRE(a.view() == "12345");
	REQUIRE(b.view() == "-808");
}

TEST_CASE("format default overload works with String via format_as", "[redis][string]")
{
	spg::redis::String level{ "info" };
	auto line = spg::redis::format("[{}] code={}", level, 200);

	REQUIRE(line.view() == "[info] code=200");
}

