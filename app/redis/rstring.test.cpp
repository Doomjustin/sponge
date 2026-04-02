#include "rstring.h"

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

// RAII helper：将 CountingResource 设为默认 PMR 资源，析构时还原。
struct ResourceGuard {
	explicit ResourceGuard(CountingResource& res)
	  : prev_{ std::pmr::set_default_resource(&res) }
	{}

	~ResourceGuard()
	{
		std::pmr::set_default_resource(prev_);
	}

	ResourceGuard(const ResourceGuard&) = delete;
	auto operator=(const ResourceGuard&) -> ResourceGuard& = delete;

	std::pmr::memory_resource* prev_;
};

} // namespace

TEST_CASE("String分配并释放内存", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "hello" };
		(void)str;
		REQUIRE(resource.allocations() == 1);
		REQUIRE(resource.deallocations() == 0);
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String处理空字符串", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "" };
		(void)str;
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String移动构造转移所有权", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String first{ "abc" };
		spg::redis::String second{ std::move(first) };
		(void)second;
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String移动赋值释放旧内存", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String first{ "first" };
		spg::redis::String second{ "second" };

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

TEST_CASE("String自移动赋值安全", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "self" };
		str = std::move(str);
	}

	REQUIRE(resource.allocations() == 1);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
	REQUIRE(resource.outstanding() == 0);
}

TEST_CASE("String默认构造创建空字符串", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{};

	REQUIRE(str.size() == 0);
	REQUIRE(str.capacity() == 0);
	REQUIRE(str.available() == 0);
	REQUIRE(str.view().empty());
	REQUIRE(str.begin() == str.end());
}

TEST_CASE("String支持视图索引与迭代器", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{ "hello" };

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

TEST_CASE("String拷贝构造执行深拷贝", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String original{ "hello" };
	spg::redis::String copy{ original };

	REQUIRE(resource.allocations() == 2);
	REQUIRE(copy.view() == "hello");
	REQUIRE(copy.capacity() == original.capacity());

	copy[0] = 'y';
	REQUIRE(copy.view() == "yello");
	REQUIRE(original.view() == "hello");
}

TEST_CASE("String拷贝赋值释放旧存储并深拷贝", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String original{ "hello" };
	spg::redis::String target{ "x" };

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

TEST_CASE("String追加时扩容并保持内容", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{ "ab" };

	str.append("c");

	REQUIRE(str.view() == "abc");
	REQUIRE(str.size() == 3);
	REQUIRE(str.capacity() >= str.size());
	REQUIRE(str.available() == str.capacity() - str.size());
	REQUIRE(resource.allocations() == 2);
	REQUIRE(resource.deallocations() == 1);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String追加可复用可用空间", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{ "ab" };

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

TEST_CASE("String追加空字符串时应保持内容不变", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{ "hello" };

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

TEST_CASE("String连续多次追加", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{};

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

TEST_CASE("String跨越255容量追加仍保持内容", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	std::string initial(255, 'a');
	spg::redis::String str{ initial };

	str.append("b");

	REQUIRE(str.size() == 256);
	REQUIRE(str.capacity() >= 256);
	REQUIRE(str.view().substr(0, 255) == initial);
	REQUIRE(str[255] == 'b');
	REQUIRE(*str.end() == '\0');
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String容量充足时reserve无操作", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{ "hello" };

	const auto allocs_before = resource.allocations();
	const auto cap_before = str.capacity();

	str.reserve(3);

	REQUIRE(str.view() == "hello");
	REQUIRE(str.size() == 5);
	REQUIRE(str.capacity() == cap_before);
	REQUIRE(resource.allocations() == allocs_before);
	REQUIRE(resource.outstanding() == 1);
}

TEST_CASE("String reserve扩容并保持内容", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "hello" };
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

TEST_CASE("String贪婪reserve在预分配阈值下容量翻倍", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "hi" };
		str.reserve(10, spg::greedy);

		REQUIRE(str.capacity() == 20);
		REQUIRE(str.size() == 2);
		REQUIRE(str.view() == "hi");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("String resize扩展长度并补零字节", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "ab" };
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

TEST_CASE("String resize缩短长度且不重分配", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{ "hello" };

	const auto allocs_before = resource.allocations();
	const auto deallocs_before = resource.deallocations();

	str.resize(3);

	REQUIRE(str.size() == 3);
	REQUIRE(str.view() == "hel");
	REQUIRE(*str.end() == '\0');
	REQUIRE(resource.allocations() == allocs_before);
	REQUIRE(resource.deallocations() == deallocs_before);
}

TEST_CASE("String clear重置长度且不释放缓冲区", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String str{ "hello" };

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

TEST_CASE("String clear后可复用缓冲区且不重分配", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{};
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

TEST_CASE("String assign在现有容量内替换内容", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "hello" };
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

TEST_CASE("String assign在内容超容量时重分配", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "hi" };
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

TEST_CASE("String自拷贝赋值安全", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String str{ "hello" };
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

TEST_CASE("string_cast转换有符号正数与零", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		auto zero = spg::redis::string_cast(0);
		auto pos = spg::redis::string_cast(12345);

		REQUIRE(zero.view() == "0");
		REQUIRE(pos.view() == "12345");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("string_cast转换有符号负数", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		auto neg = spg::redis::string_cast(-9876);
		REQUIRE(neg.view() == "-9876");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("string_cast处理有符号边界值", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		auto min_v = spg::redis::string_cast(std::numeric_limits<std::int64_t>::min());
		auto max_v = spg::redis::string_cast(std::numeric_limits<std::int64_t>::max());

		REQUIRE(min_v.view() == "-9223372036854775808");
		REQUIRE(max_v.view() == "9223372036854775807");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("string_cast处理无符号值与边界值", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		auto u = spg::redis::string_cast(42u);
		auto umax = spg::redis::string_cast(std::numeric_limits<std::uint64_t>::max());

		REQUIRE(u.view() == "42");
		REQUIRE(umax.view() == "18446744073709551615");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("format通过默认资源分配", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		auto str = spg::redis::format("user:{} score:{}", 7, 99);

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

TEST_CASE("format可通过format_as格式化String参数", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };

	{
		spg::redis::String name{ "alice" };
		auto str = spg::redis::format("name={}", name);

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

TEST_CASE("format支持转义花括号", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	auto out = spg::redis::format("{{user}}={}", "bob");

	REQUIRE(out.view() == "{user}=bob");
	REQUIRE(resource.allocations() >= 1);
	REQUIRE(resource.outstanding() >= 1);
}

TEST_CASE("format支持混合参数类型", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	spg::redis::String user{ "bob" };

	auto out = spg::redis::format("user={} active={} score={}", user, true, 42);

	REQUIRE(out.view() == "user=bob active=true score=42");
	REQUIRE(resource.allocations() >= 2);
	REQUIRE(resource.outstanding() >= 1);
}

TEST_CASE("format处理长内容增长", "[redis][string]")
{
	CountingResource resource{};
	ResourceGuard guard{ resource };
	std::string payload(600, 'x');

	{
		auto out = spg::redis::format("prefix:{}:suffix", payload);

		REQUIRE(out.size() == payload.size() + 14);
		REQUIRE(out.view().substr(0, 7) == "prefix:");
		REQUIRE(out.view().substr(out.size() - 7) == ":suffix");
	}

	REQUIRE(resource.outstanding() == 0);
	REQUIRE_FALSE(resource.has_size_mismatch());
}

