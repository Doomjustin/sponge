#include <sponge/redis/list_pack.h>

#include <cstdint>
#include <limits>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

// header 6 字节 + EOF 1 字节
static constexpr std::size_t EMPTY_SIZE = 7;

namespace {

auto collect_ints(ListPack& lp) -> std::vector<int64_t>
{
    std::vector<int64_t> values;
    for (auto it = lp.begin(); it != lp.end(); ++it) {
        auto elem = *it;
        REQUIRE(std::holds_alternative<int64_t>(elem));
        values.push_back(std::get<int64_t>(elem));
    }

    return values;
}

auto collect_strings(ListPack& lp) -> std::vector<std::string_view>
{
    std::vector<std::string_view> values;
    for (auto it = lp.begin(); it != lp.end(); ++it) {
        auto elem = *it;
        REQUIRE(std::holds_alternative<std::string_view>(elem));
        values.push_back(std::get<std::string_view>(elem));
    }

    return values;
}

void check_int_sequence(ListPack& lp, const std::vector<int64_t>& expected)
{
    const auto actual = collect_ints(lp);
    CHECK(actual == expected);
}

void check_string_sequence(ListPack& lp, const std::vector<std::string_view>& expected)
{
    const auto actual = collect_strings(lp);
    CHECK(actual == expected);
}

} // namespace

TEST_CASE("ListPack 初始状态", "[list_pack]")
{
    ListPack lp{ 64 };

    REQUIRE(lp.empty());
    REQUIRE(lp.size() == 0);
    REQUIRE(lp.byte_size() == EMPTY_SIZE);
}

TEST_CASE("ListPack append int64 — int7 (0~127)", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(0);
    lp.push_back(127);

    CHECK(lp.size() == 2);
    // int7: 1 encoding 字节 + 1 backlen 字节 = 2 字节 per entry
    CHECK(lp.byte_size() == EMPTY_SIZE + 2 * 2);
}

TEST_CASE("ListPack append int64 — int13 (-4096~4095, 排除 int7)", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(128);   // 超过 int7 上界
    lp.push_back(-1);    // 负数
    lp.push_back(4095);  // int13 上界

    CHECK(lp.size() == 3);
    // int13: 2 encoding/payload 字节 + 1 backlen 字节 = 3 字节 per entry
    CHECK(lp.byte_size() == EMPTY_SIZE + 3 * 3);
}

TEST_CASE("ListPack append int64 — int16", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(std::numeric_limits<int16_t>::min());
    lp.push_back(std::numeric_limits<int16_t>::max());

    CHECK(lp.size() == 2);
    // int16: 1 type + 2 payload + 1 backlen = 4 字节 per entry
    CHECK(lp.byte_size() == EMPTY_SIZE + 2 * 4);
}

TEST_CASE("ListPack append int64 — int24", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(-8388608);
    lp.push_back(8388607);

    CHECK(lp.size() == 2);
    // int24: 1 type + 3 payload + 1 backlen = 5 字节 per entry
    CHECK(lp.byte_size() == EMPTY_SIZE + 2 * 5);
}

TEST_CASE("ListPack append int64 — int32", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(std::numeric_limits<int32_t>::min());
    lp.push_back(std::numeric_limits<int32_t>::max());

    CHECK(lp.size() == 2);
    // int32: 1 type + 4 payload + 1 backlen = 6 字节 per entry
    CHECK(lp.byte_size() == EMPTY_SIZE + 2 * 6);
}

TEST_CASE("ListPack append int64 — int64", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(std::numeric_limits<int64_t>::min());
    lp.push_back(std::numeric_limits<int64_t>::max());

    CHECK(lp.size() == 2);
    // int64: 1 type + 8 payload + 1 backlen = 10 字节 per entry
    CHECK(lp.byte_size() == EMPTY_SIZE + 2 * 10);
}

TEST_CASE("ListPack append string — str6 (< 64 字节)", "[list_pack]")
{
    ListPack lp{ 128 };
    std::string s(10, 'x');
    lp.push_back(s);

    CHECK(lp.size() == 1);
    // str6: 1 encoding + 10 payload + 1 backlen = 12 字节
    CHECK(lp.byte_size() == EMPTY_SIZE + 12);
}

TEST_CASE("ListPack append string — str12 (64~4095 字节)", "[list_pack]")
{
    ListPack lp{ 256 };
    std::string s(100, 'a');
    lp.push_back(s);

    CHECK(lp.size() == 1);
    // str12: 2 encoding/len + 100 payload + 1 backlen = 103 字节
    CHECK(lp.byte_size() == EMPTY_SIZE + 103);
}

TEST_CASE("ListPack append string — str32 (>= 4096 字节)", "[list_pack]")
{
    ListPack lp{ 8192 };
    std::string s(5000, 'z');
    lp.push_back(s);

    CHECK(lp.size() == 1);
    // str32: 1 type + 4 len + 5000 payload = 5005，backlen(5005) 需要 2 字节 → 共 5007 字节
    CHECK(lp.byte_size() == EMPTY_SIZE + 5007);
}

TEST_CASE("ListPack 数字字符串走整数路径", "[list_pack]")
{
    ListPack lp1{ 64 };
    ListPack lp2{ 64 };

    lp1.push_back("42");         // string 接口，应该走 int7 路径
    lp2.push_back(42);

    CHECK(lp1.size() == lp2.size());
    CHECK(lp1.byte_size() == lp2.byte_size());
    CHECK(lp1.view() == lp2.view());
}

TEST_CASE("ListPack 多次 append 累加 size", "[list_pack]")
{
    ListPack lp{ 128 };
    for (int i = 0; i < 10; ++i)
        lp.push_back(i);

    CHECK(lp.size() == 10);
}

// ── insert 路径 ─────────────────────────────────────────────────────────────

TEST_CASE("ListPack insert int64 — 中间插入保持顺序", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(1);
    lp.push_back(3);

    auto pos = lp.begin();
    ++pos; // 指向 3，在其前面插入
    lp.insert(pos, 2);

    REQUIRE(lp.size() == 3);
    check_int_sequence(lp, { 1, 2, 3 });
}

TEST_CASE("ListPack insert string — 头尾插入", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(std::string_view{ "mid" });

    lp.insert(lp.begin(), std::string_view{ "head" });
    lp.insert(lp.end(), std::string_view{ "tail" });

    REQUIRE(lp.size() == 3);
    check_string_sequence(lp, { "head", "mid", "tail" });
}

TEST_CASE("ListPack insert 数字字符串走整数路径", "[list_pack]")
{
    ListPack lp1{ 64 };
    ListPack lp2{ 64 };

    lp1.push_back(1);
    lp1.push_back(3);

    lp2.push_back(1);
    lp2.push_back(3);

    auto p1 = lp1.begin();
    auto p2 = lp2.begin();
    ++p1;
    ++p2;

    lp1.insert(p1, std::string_view{ "2" });
    lp2.insert(p2, 2);

    CHECK(lp1.size() == lp2.size());
    CHECK(lp1.byte_size() == lp2.byte_size());
    CHECK(lp1.view() == lp2.view());
}

TEST_CASE("ListPack insert '0' 开头字符串不走整数路径", "[list_pack]")
{
    ListPack lp1{ 64 };
    ListPack lp2{ 64 };

    lp1.push_back(1);
    lp2.push_back(1);

    lp1.insert(lp1.end(), std::string_view{ "0" });
    lp2.insert(lp2.end(), 0);

    // "0" 走字符串编码，通常比 int7(0) 编码更大
    CHECK(lp1.byte_size() > lp2.byte_size());
}

// ── erase 路径 ──────────────────────────────────────────────────────────────

TEST_CASE("ListPack erase 单个元素 — 删除中间元素", "[list_pack]")
{
    ListPack lp{ 128 };
    lp.push_back(1);
    lp.push_back(2);
    lp.push_back(3);
    lp.push_back(4);

    auto it = lp.begin();
    ++it; // 指向 2
    lp.erase(it);

    REQUIRE(lp.size() == 3);
    check_int_sequence(lp, { 1, 3, 4 });
}

TEST_CASE("ListPack erase 区间 — 删除中间连续元素", "[list_pack]")
{
    ListPack lp{ 128 };
    for (int i = 1; i <= 6; ++i)
        lp.push_back(i);

    auto first = lp.begin();
    ++first; // 2
    auto last = first;
    ++last;
    ++last;
    ++last; // 指向 5（半开区间，不删 5）

    lp.erase(first, last); // 删除 2,3,4

    REQUIRE(lp.size() == 3);
    check_int_sequence(lp, { 1, 5, 6 });
}

TEST_CASE("ListPack erase 区间 — first==last 时无变化", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(10);
    lp.push_back(20);

    const auto before_view = lp.view();
    const auto before_size = lp.size();

    auto p = lp.begin();
    ++p;
    lp.erase(p, p);

    CHECK(lp.size() == before_size);
    CHECK(lp.view() == before_view);
    check_int_sequence(lp, { 10, 20 });
}

TEST_CASE("ListPack erase 区间 — 删除全部元素", "[list_pack]")
{
    ListPack lp{ 128 };
    lp.push_back(7);
    lp.push_back(8);
    lp.push_back(9);

    lp.erase(lp.begin(), lp.end());

    CHECK(lp.empty());
    CHECK(lp.size() == 0);
    CHECK(lp.byte_size() == EMPTY_SIZE);
}

TEST_CASE("ListPack erase 混合类型 — 删除字符串元素", "[list_pack]")
{
    ListPack lp{ 128 };
    lp.push_back(1);
    lp.push_back(std::string_view{ "x" });
    lp.push_back(2);

    auto it = lp.begin();
    ++it; // 指向 "x"
    lp.erase(it);

    REQUIRE(lp.size() == 2);

    auto e = lp.begin();
    auto v0 = *e++;
    auto v1 = *e++;

    REQUIRE(std::holds_alternative<int64_t>(v0));
    REQUIRE(std::holds_alternative<int64_t>(v1));
    CHECK(std::get<int64_t>(v0) == 1);
    CHECK(std::get<int64_t>(v1) == 2);
    CHECK(e == lp.end());
}

// ── 解码往返 ──────────────────────────────────────────────────────────────

TEST_CASE("ListPack 正向迭代 — 整数解码往返", "[list_pack]")
{
    ListPack lp{ 128 };
    lp.push_back(0);
    lp.push_back(127);
    lp.push_back(128);
    lp.push_back(-1);
    lp.push_back(4095);
    lp.push_back(-4096);
    lp.push_back(std::numeric_limits<int16_t>::max());
    lp.push_back(std::numeric_limits<int16_t>::min());
    lp.push_back(8388607);
    lp.push_back(-8388608);
    lp.push_back(std::numeric_limits<int32_t>::max());
    lp.push_back(std::numeric_limits<int32_t>::min());
    lp.push_back(std::numeric_limits<int64_t>::max());
    lp.push_back(std::numeric_limits<int64_t>::min());

    REQUIRE(lp.size() == 14);

    const std::vector<int64_t> expected = {
        0, 127, 128, -1, 4095, -4096,
        std::numeric_limits<int16_t>::max(),
        std::numeric_limits<int16_t>::min(),
        8388607, -8388608,
        std::numeric_limits<int32_t>::max(),
        std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::min(),
    };

    check_int_sequence(lp, expected);
}

// ── 边界分类 ──────────────────────────────────────────────────────────────
TEST_CASE("ListPack int13 边界 — -4096 和 4095", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(-4096);
    lp.push_back(4095);

    REQUIRE(lp.size() == 2);
    // int13: 2 字节 + 1 backlen = 3 字节 × 2
    CHECK(lp.byte_size() == EMPTY_SIZE + 3 * 2);
    check_int_sequence(lp, { -4096, 4095 });
}

TEST_CASE("ListPack int7/int13 边界 — 127 vs 128", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(127);
    lp.push_back(128);

    REQUIRE(lp.size() == 2);
    // int7(2 字节) + int13(3 字节)
    CHECK(lp.byte_size() == EMPTY_SIZE + 2 + 3);
}

// ── 字符串路径 ────────────────────────────────────────────────────────────

TEST_CASE("ListPack 空字符串走 str6", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(std::string_view{});

    CHECK(lp.size() == 1);
    // str6: 1 encoding + 0 payload + 1 backlen = 2 字节
    CHECK(lp.byte_size() == EMPTY_SIZE + 2);
}

TEST_CASE("ListPack '0' 开头的字符串不走整数路径", "[list_pack]")
{
    ListPack lp1{ 64 };
    ListPack lp2{ 64 };

    lp1.push_back("0");    // 以 '0' 开头，必须走字符串路径
    lp2.push_back("42");   // 纯数字，走整数路径

    // "0" 走 str 编码，比整数大（1 encoding + 1 payload('0') + 1 backlen = 3）
    // "42" 走 int7 编码（2 字节）
    CHECK(lp1.byte_size() > lp2.byte_size());
}

TEST_CASE("ListPack 负数字符串走整数路径", "[list_pack]")
{
    ListPack lp1{ 64 };
    ListPack lp2{ 64 };

    lp1.push_back("-1");
    lp2.push_back(-1);

    CHECK(lp1.size() == 1);
    CHECK(lp1.byte_size() == lp2.byte_size());
    CHECK(lp1.view() == lp2.view());
}

TEST_CASE("ListPack 非数字字符串走 str 路径", "[list_pack]")
{
    ListPack lp1{ 64 };
    ListPack lp2{ 64 };

    lp1.push_back("hello");   // 非数字
    lp2.push_back(0);

    // str6: 1 + 5 + 1 = 7 字节；int7: 2 字节
    CHECK(lp1.byte_size() > lp2.byte_size());
}

// ── pmr 资源隔离 ──────────────────────────────────────────────────────────

TEST_CASE("ListPack 使用自定义 pmr 资源", "[list_pack]")
{
    std::pmr::monotonic_buffer_resource pool{ 4096 };
    ListPack lp{ 256, &pool };

    lp.push_back(42);
    lp.push_back("hello");

    CHECK(lp.size() == 2);
    CHECK(!lp.empty());
}

// ── 字符串解码往返 ────────────────────────────────────────────────────────────

TEST_CASE("ListPack str6 解码往返", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(std::string_view{ "hello" });

    REQUIRE(lp.size() == 1);
    // 非数字，走 str6: 1 encoding + 5 payload + 1 backlen = 7 字节
    CHECK(lp.byte_size() == EMPTY_SIZE + 7);
    check_string_sequence(lp, { "hello" });
}

TEST_CASE("ListPack str6 空字符串解码往返", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(std::string_view{});

    REQUIRE(lp.size() == 1);
    check_string_sequence(lp, { "" });
}

TEST_CASE("ListPack str12 解码往返", "[list_pack]")
{
    ListPack lp{ 256 };
    std::string s(100, 'a');
    lp.push_back(s);

    REQUIRE(lp.size() == 1);
    check_string_sequence(lp, { s });
}

TEST_CASE("ListPack str32 解码往返", "[list_pack]")
{
    ListPack lp{ 8192 };
    std::string s(5000, 'z');
    lp.push_back(s);

    REQUIRE(lp.size() == 1);
    check_string_sequence(lp, { s });
}

// ── 混合迭代 ──────────────────────────────────────────────────────────────────

TEST_CASE("ListPack 混合整数与字符串正向迭代", "[list_pack]")
{
    ListPack lp{ 256 };
    lp.push_back(42);                        // int7, 2 字节
    lp.push_back(std::string_view{ "hi" }); // str6, 4 字节
    lp.push_back(-1);                        // int13, 3 字节
    std::string hundred(100, 'x');
    lp.push_back(hundred);                   // str12, 103 字节

    REQUIRE(lp.size() == 4);
    CHECK(lp.byte_size() == EMPTY_SIZE + 2 + 4 + 3 + 103);

    auto it = lp.begin();

    auto e0 = *it++;
    REQUIRE(std::holds_alternative<int64_t>(e0));
    CHECK(std::get<int64_t>(e0) == 42);

    auto e1 = *it++;
    REQUIRE(std::holds_alternative<std::string_view>(e1));
    CHECK(std::get<std::string_view>(e1) == "hi");

    auto e2 = *it++;
    REQUIRE(std::holds_alternative<int64_t>(e2));
    CHECK(std::get<int64_t>(e2) == -1);

    auto e3 = *it++;
    REQUIRE(std::holds_alternative<std::string_view>(e3));
    CHECK(std::get<std::string_view>(e3) == hundred);

    CHECK(it == lp.end());
}

TEST_CASE("ListPack str6 多个元素正向迭代", "[list_pack]")
{
    ListPack lp{ 128 };
    const std::vector<std::string_view> words = { "foo", "bar", "baz", "qux" };
    for (auto w : words)
        lp.push_back(w);

    REQUIRE(lp.size() == 4);
    check_string_sequence(lp, words);
}

// ── operator* 幂等性 ───────────────────────────────────────────────────────────

TEST_CASE("ListPack operator* 幂等 — 整数", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(42);

    auto it = lp.begin();
    auto first = *it;
    auto second = *it;  // 不调用 ++，再次解引用

    REQUIRE(std::holds_alternative<int64_t>(first));
    REQUIRE(std::holds_alternative<int64_t>(second));
    CHECK(std::get<int64_t>(first) == std::get<int64_t>(second));
    CHECK(std::get<int64_t>(first) == 42);
}

TEST_CASE("ListPack operator* 幂等 — 字符串", "[list_pack]")
{
    ListPack lp{ 64 };
    lp.push_back(std::string_view{ "hi" });

    auto it = lp.begin();
    auto first = *it;
    auto second = *it;  // 不调用 ++，再次解引用

    REQUIRE(std::holds_alternative<std::string_view>(first));
    REQUIRE(std::holds_alternative<std::string_view>(second));
    CHECK(std::get<std::string_view>(first) == std::get<std::string_view>(second));
}

// ── operator-- 反向迭代 ───────────────────────────────────────────────────────

TEST_CASE("ListPack operator-- — 整数序列反向迭代", "[list_pack]")
{
    ListPack lp{ 128 };
    const std::vector<int64_t> values = { 0, 127, 128, -1, 4095, -4096 };
    for (auto v : values)
        lp.push_back(v);

    REQUIRE(lp.size() == values.size());

    // 从 end() 往前，依次对比期望值（逆序）
    auto it = lp.end();
    for (int i = static_cast<int>(values.size()) - 1; i >= 0; --i) {
        --it;
        auto elem = *it;
        REQUIRE(std::holds_alternative<int64_t>(elem));
        CHECK(std::get<int64_t>(elem) == values[static_cast<size_t>(i)]);
    }
    CHECK(it == lp.begin());
}

TEST_CASE("ListPack operator-- — 混合序列反向迭代", "[list_pack]")
{
    ListPack lp{ 256 };
    lp.push_back(10);
    lp.push_back(std::string_view{ "abc" });
    lp.push_back(-1);
    lp.push_back(std::string_view{ "xyz" });

    REQUIRE(lp.size() == 4);

    auto it = lp.end();

    --it; CHECK(std::get<std::string_view>(*it) == "xyz");
    --it; CHECK(std::get<int64_t>(*it) == -1);
    --it; CHECK(std::get<std::string_view>(*it) == "abc");
    --it; CHECK(std::get<int64_t>(*it) == 10);
    CHECK(it == lp.begin());
}

TEST_CASE("ListPack operator-- — 前进再后退回到原位", "[list_pack]")
{
    ListPack lp{ 128 };
    lp.push_back(1);
    lp.push_back(2);
    lp.push_back(3);

    auto it = lp.begin();
    ++it;
    ++it; // 指向第 3 个元素
    auto val_forward = std::get<int64_t>(*it);

    --it;
    --it; // 退回到第 1 个元素
    ++it;
    ++it; // 再次前进到第 3 个元素
    auto val_back = std::get<int64_t>(*it);

    CHECK(val_forward == val_back);
    CHECK(val_forward == 3);
}

// ── range-based for ────────────────────────────────────────────────────────────

TEST_CASE("ListPack range-based for — 整数序列", "[list_pack]")
{
    ListPack lp{ 128 };
    const std::vector<int64_t> values = { 0, 42, -1, 127, 128 };
    for (auto v : values)
        lp.push_back(v);

    std::vector<int64_t> result;
    for (auto elem : lp)
        result.push_back(std::get<int64_t>(elem));

    CHECK(result == values);
}

// ── ReverseIterator ───────────────────────────────────────────────────────────

TEST_CASE("ListPack ReverseIterator — 整数逆序遍历", "[list_pack]")
{
    ListPack lp{ 128 };
    const std::vector<int64_t> values = { 1, 2, 3, 127, 128, -1 };
    for (auto v : values)
        lp.push_back(v);

    std::vector<int64_t> reversed;
    for (auto it = lp.rbegin(); it != lp.rend(); ++it) {
        auto elem = *it;
        REQUIRE(std::holds_alternative<int64_t>(elem));
        reversed.push_back(std::get<int64_t>(elem));
    }

    const std::vector<int64_t> expected(values.rbegin(), values.rend());
    CHECK(reversed == expected);
}

TEST_CASE("ListPack ReverseIterator — 混合类型逆序遍历", "[list_pack]")
{
    ListPack lp{ 256 };
    lp.push_back(10);
    lp.push_back(std::string_view{ "abc" });
    lp.push_back(-1);
    lp.push_back(std::string_view{ "xyz" });

    auto it = lp.rbegin();

    REQUIRE(std::holds_alternative<std::string_view>(*it));
    CHECK(std::get<std::string_view>(*it) == "xyz");

    ++it;
    REQUIRE(std::holds_alternative<int64_t>(*it));
    CHECK(std::get<int64_t>(*it) == -1);

    ++it;
    REQUIRE(std::holds_alternative<std::string_view>(*it));
    CHECK(std::get<std::string_view>(*it) == "abc");

    ++it;
    REQUIRE(std::holds_alternative<int64_t>(*it));
    CHECK(std::get<int64_t>(*it) == 10);

    ++it;
    CHECK(it == lp.rend());
}

TEST_CASE("ListPack ReverseIterator -- 可回退到更新元素", "[list_pack]")
{
    ListPack lp{ 128 };
    lp.push_back(1);
    lp.push_back(2);
    lp.push_back(3);

    auto it = lp.rbegin(); // 3
    REQUIRE(std::get<int64_t>(*it) == 3);

    ++it; // 2
    REQUIRE(std::get<int64_t>(*it) == 2);

    --it; // 回到 3
    CHECK(std::get<int64_t>(*it) == 3);
}