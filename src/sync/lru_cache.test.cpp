#include <sponge/sync/lru_cache.h>

#include <atomic>
#include <memory>
#include <memory_resource>
#include <string>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sponge/tracking_resource.h>

using namespace spg::sync;

TEST_CASE("在插入共享对象后查询命中时，应该返回同一份共享所有权", "[sync][lru_cache]")
{
	auto value = std::make_shared<std::string>("one");

	LRUCache<int, std::string> cache{2};
	cache.put(1, value);

	auto result = cache.get(1);
	REQUIRE(result != nullptr);
	REQUIRE(result == value);
	REQUIRE(*result == "one");
}

TEST_CASE("在查询不存在的键时，应该返回空共享指针", "[sync][lru_cache]")
{
	LRUCache<int, std::string> cache{2};
	cache.put(1, std::make_shared<std::string>("one"));

	REQUIRE(cache.get(2) == nullptr);
}

TEST_CASE("在命中旧键后插入新键时，应该保留最近访问的元素", "[sync][lru_cache]")
{
	LRUCache<int, std::string> cache{2};
	cache.put(1, std::make_shared<std::string>("one"));
	cache.put(2, std::make_shared<std::string>("two"));

	std::ignore = cache.get(1);
	cache.put(3, std::make_shared<std::string>("three"));

	REQUIRE(cache.get(1) != nullptr);
	REQUIRE(cache.get(2) == nullptr);
	REQUIRE(cache.get(3) != nullptr);
}

TEST_CASE("在更新已存在键时，应该返回新的共享对象", "[sync][lru_cache]")
{
	auto original = std::make_shared<std::string>("one");
	auto updated = std::make_shared<std::string>("ONE");

	LRUCache<int, std::string> cache{2};
	cache.put(1, original);
	cache.put(1, updated);

	auto result = cache.get(1);
	REQUIRE(result == updated);
	REQUIRE(result != original);
}

TEST_CASE("在条目被驱逐后仍持有外部共享指针时，应该保持对象存活", "[sync][lru_cache]")
{
	auto value = std::make_shared<std::string>("one");

	LRUCache<int, std::string> cache{1};
	cache.put(1, value);
	cache.put(2, std::make_shared<std::string>("two"));

	REQUIRE(cache.get(1) == nullptr);
	REQUIRE(*value == "one");
}

TEST_CASE("在多线程并发写入不同键时，应该保留所有已写入元素", "[sync][lru_cache]")
{
	constexpr int thread_count = 4;
	constexpr int values_per_thread = 16;

	LRUCache<int, int> cache{thread_count * values_per_thread};
	std::atomic<bool> start{false};
	std::vector<std::jthread> threads;
	threads.reserve(thread_count);

	for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
		threads.emplace_back([&, thread_index] {
			while (!start.load(std::memory_order_acquire)) {
				std::this_thread::yield();
			}

			for (int value_index = 0; value_index < values_per_thread; ++value_index) {
				const auto key = thread_index * values_per_thread + value_index;
				cache.put(key, std::make_shared<int>(key * 10));
			}
		});
	}

	start.store(true, std::memory_order_release);
	threads.clear();

	for (int key = 0; key < thread_count * values_per_thread; ++key) {
		auto result = cache.get(key);
		REQUIRE(result != nullptr);
		REQUIRE(*result == key * 10);
	}
}

TEST_CASE("在多线程并发读取已存在键时，应该始终返回有效共享指针", "[sync][lru_cache]")
{
	constexpr int thread_count = 4;
	constexpr int key_count = 8;
	constexpr int reads_per_thread = 64;

	LRUCache<int, int> cache{key_count};
	for (int key = 0; key < key_count; ++key) {
		cache.put(key, std::make_shared<int>(key * 100));
	}

	std::atomic<bool> start{false};
	std::atomic<int> null_hits{0};
	std::atomic<int> wrong_values{0};
	std::vector<std::jthread> threads;
	threads.reserve(thread_count);

	for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
		threads.emplace_back([&, thread_index] {
			while (!start.load(std::memory_order_acquire)) {
				std::this_thread::yield();
			}

			for (int read_index = 0; read_index < reads_per_thread; ++read_index) {
				const auto key = (thread_index + read_index) % key_count;
				auto result = cache.get(key);
				if (result == nullptr) {
					null_hits.fetch_add(1, std::memory_order_relaxed);
					continue;
				}

				if (*result != key * 100) {
					wrong_values.fetch_add(1, std::memory_order_relaxed);
				}
			}
		});
	}

	start.store(true, std::memory_order_release);
	threads.clear();

	REQUIRE(null_hits.load(std::memory_order_relaxed) == 0);
	REQUIRE(wrong_values.load(std::memory_order_relaxed) == 0);
}

TEST_CASE("在使用自定义内存资源插入共享对象时，应该通过该资源完成节点分配", "[sync][lru_cache]")
{
	spg::TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

	LRUCache<int, int> cache{ 2, &resource };
	REQUIRE(resource.used_memory() == 0);

	cache.put(1, std::make_shared<int>(10));

	REQUIRE(resource.used_memory() > 0);
}

TEST_CASE("在使用自定义内存资源命中已有键时，应该不新增节点分配", "[sync][lru_cache]")
{
	spg::TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

	LRUCache<int, int> cache{ 2, &resource };
	cache.put(1, std::make_shared<int>(10));
	const auto used_before_get = resource.used_memory();

	auto result = cache.get(1);

	REQUIRE(result != nullptr);
	REQUIRE(*result == 10);
	REQUIRE(resource.used_memory() == used_before_get);
}

TEST_CASE("在使用自定义内存资源的线程安全缓存析构后，应该释放内部节点占用", "[sync][lru_cache]")
{
	spg::TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

	{
		LRUCache<int, int> cache{ 2, &resource };
		cache.put(1, std::make_shared<int>(10));
		cache.put(2, std::make_shared<int>(20));

		REQUIRE(resource.used_memory() > 0);
	}

	REQUIRE(resource.used_memory() == 0);
}
