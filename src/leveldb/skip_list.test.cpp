#include <sponge/leveldb/skip_list.h>

#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace spg::leveldb;

namespace {

struct IntComparator {
	auto operator()(int lhs, int rhs) const noexcept -> int
	{
		if (lhs < rhs)
			return -1;
		if (lhs > rhs)
			return 1;
		return 0;
	}
};

} // namespace

TEST_CASE("SkipList 插入元素后 contains 应正确反映存在性", "[leveldb][skip_list]")
{
	SkipList<int, IntComparator> list{ IntComparator{} };

	list.insert(10);
	list.insert(30);
	list.insert(20);

	REQUIRE(list.contains(10));
	REQUIRE(list.contains(20));
	REQUIRE(list.contains(30));
	REQUIRE_FALSE(list.contains(40));
}

TEST_CASE("SkipList 迭代器从首元素开始应按升序遍历", "[leveldb][skip_list]")
{
	SkipList<int, IntComparator> list{ IntComparator{} };
	list.insert(3);
	list.insert(1);
	list.insert(2);

	auto it = list.iterator();
	it.seek(spg::to_first);

	std::vector<int> values;
	while (it.is_valid()) {
		values.push_back(it.value());
		it.next();
	}

	REQUIRE(values == std::vector<int>{ 1, 2, 3 });
}

TEST_CASE("SkipList seek(key) 应定位到第一个大于等于 key 的元素", "[leveldb][skip_list]")
{
	SkipList<int, IntComparator> list{ IntComparator{} };
	list.insert(10);
	list.insert(20);
	list.insert(30);

	auto it = list.iterator();

	it.seek(20);
	REQUIRE(it.is_valid());
	REQUIRE(it.value() == 20);

	it.seek(25);
	REQUIRE(it.is_valid());
	REQUIRE(it.value() == 30);

	it.seek(35);
	REQUIRE_FALSE(it.is_valid());
}

TEST_CASE("SkipList 迭代器 prev 在首元素上应变为无效", "[leveldb][skip_list]")
{
	SkipList<int, IntComparator> list{ IntComparator{} };
	list.insert(5);
	list.insert(15);

	auto it = list.iterator();
	it.seek(spg::to_first);
	REQUIRE(it.is_valid());
	REQUIRE(it.value() == 5);

	it.prev();
	REQUIRE_FALSE(it.is_valid());
}

TEST_CASE("SkipList 迭代器从末元素开始 prev 应逐步回退", "[leveldb][skip_list]")
{
	SkipList<int, IntComparator> list{ IntComparator{} };
	list.insert(4);
	list.insert(2);
	list.insert(6);

	auto it = list.iterator();
	it.seek(spg::to_last);

	REQUIRE(it.is_valid());
	REQUIRE(it.value() == 6);

	it.prev();
	REQUIRE(it.is_valid());
	REQUIRE(it.value() == 4);

	it.prev();
	REQUIRE(it.is_valid());
	REQUIRE(it.value() == 2);
}
