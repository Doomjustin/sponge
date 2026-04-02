#include <sponge/lru_cache.h>

#include <catch2/catch_test_macros.hpp>

#include <sponge/tracking_resource.h>

using namespace spg;

TEST_CASE("在向容量为2的缓存插入单个元素后，应该能查询到对应值", "[common][lru_cache]")
{
    LRUCache<int, std::string> cache{2};
    cache.put(1, "one");

    auto result = cache.get(1);
    REQUIRE(result != nullptr);
    REQUIRE(*result == "one");
}

TEST_CASE("在查询不存在的键时，应该返回空指针", "[common][lru_cache]")
{
    LRUCache<int, std::string> cache{2};
    cache.put(1, "one");

    auto result = cache.get(2);
    REQUIRE(result == nullptr);
}

TEST_CASE("在容量内连续插入多个元素时，应该全部可查询", "[common][lru_cache]")
{
    LRUCache<int, std::string> cache{3};
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    REQUIRE(*cache.get(1) == "one");
    REQUIRE(*cache.get(2) == "two");
    REQUIRE(*cache.get(3) == "three");
}

TEST_CASE("当插入元素超过容量时，应该驱逐最久未使用的元素", "[common][lru_cache]")
{
    LRUCache<int, std::string> cache{2};
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    REQUIRE(cache.get(1) == nullptr);
    REQUIRE(*cache.get(2) == "two");
    REQUIRE(*cache.get(3) == "three");
}

TEST_CASE("在查询已存在键后再插入新元素时，应该驱逐最久未查询的元素", "[common][lru_cache]")
{
    LRUCache<int, std::string> cache{2};
    cache.put(1, "one");
    cache.put(2, "two");
    std::ignore = cache.get(1);
    cache.put(3, "three");

    REQUIRE(cache.get(1) != nullptr);
    REQUIRE(cache.get(2) == nullptr);
    REQUIRE(*cache.get(3) == "three");
}

TEST_CASE("在更新已存在的键时，应该返回更新后的值", "[common][lru_cache]")
{
    LRUCache<int, std::string> cache{2};
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(1, "ONE");

    REQUIRE(*cache.get(1) == "ONE");
}

TEST_CASE("在更新键后再插入新元素时，应该驱逐旧的键", "[common][lru_cache]")
{
    LRUCache<int, std::string> cache{2};
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(1, "ONE");
    cache.put(3, "three");

    REQUIRE(*cache.get(1) == "ONE");
    REQUIRE(cache.get(2) == nullptr);
    REQUIRE(*cache.get(3) == "three");
}

TEST_CASE("在容量为1的缓存中频繁插入时，应该持续驱逐前一个元素", "[common][lru_cache]")
{
    LRUCache<int, int> cache{1};
    cache.put(1, 10);
    REQUIRE(*cache.get(1) == 10);

    cache.put(2, 20);
    REQUIRE(cache.get(1) == nullptr);
    REQUIRE(*cache.get(2) == 20);

    cache.put(3, 30);
    REQUIRE(cache.get(2) == nullptr);
    REQUIRE(*cache.get(3) == 30);
}

TEST_CASE("在使用 string 键和 int 值时，应该返回对应缓存值", "[common][lru_cache]")
{
    LRUCache<std::string, int> cache{2};
    cache.put("a", 10);
    cache.put("b", 20);

    REQUIRE(*cache.get("a") == 10);
    REQUIRE(*cache.get("b") == 20);
}

TEST_CASE("在缓存中存储复杂对象时，应该取回完整对象内容", "[common][lru_cache]")
{
    struct Item {
        int id;
        std::string name;
        bool operator==(const Item& other) const 
        {
            return id == other.id && name == other.name;
        }
    };

    LRUCache<int, Item> cache{2};
    cache.put(1, Item{1, "item1"});
    cache.put(2, Item{2, "item2"});

    auto result = cache.get(1);
    REQUIRE(result != nullptr);
    REQUIRE(result->id == 1);
    REQUIRE(result->name == "item1");
}

TEST_CASE("在使用自定义内存资源插入新键时，应该通过该资源完成节点分配", "[common][lru_cache]")
{
    TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

    LRUCache<int, int> cache{ 2, &resource };
    REQUIRE(resource.used_memory() == 0);

    cache.put(1, 10);

    REQUIRE(resource.used_memory() > 0);
}

TEST_CASE("在使用自定义内存资源命中已有键时，应该不新增节点分配", "[common][lru_cache]")
{
    TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

    LRUCache<int, int> cache{ 2, &resource };
    cache.put(1, 10);
    const auto used_before_get = resource.used_memory();

    auto* result = cache.get(1);

    REQUIRE(result != nullptr);
    REQUIRE(*result == 10);
    REQUIRE(resource.used_memory() == used_before_get);
}

TEST_CASE("在使用自定义内存资源的缓存析构后，应该释放内部节点占用", "[common][lru_cache]")
{
    TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

    {
        LRUCache<int, int> cache{ 2, &resource };
        cache.put(1, 10);
        cache.put(2, 20);

        REQUIRE(resource.used_memory() > 0);
    }

    REQUIRE(resource.used_memory() == 0);
}
