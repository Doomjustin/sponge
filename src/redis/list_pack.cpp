#include <sponge/redis/list_pack.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ranges>
#include <utility>

#include <sponge/utility.h>

namespace spg::redis {

namespace {

template<typename Iterator>
[[nodiscard]]
auto backlen(Iterator iter, uint64_t length) noexcept -> Iterator
{
    if (length <= 127) {
        *iter++ = static_cast<std::byte>(length); // 直接写入一个字节
        return iter;
    }

    std::array<std::byte, 5> buffer{};
    int idx = 0;

    while (length > 0) {
        buffer[idx++] = static_cast<std::byte>((length & 127) | 128);
        length >>= 7;
    }

    buffer[idx - 1] &= static_cast<std::byte>(127); // 最高块清掉 high bit（它被反转到最前，是终止标志）
    for (int i = idx - 1; i >= 0; --i)
        *iter++ = buffer[i];

    return iter;
}


template<uint32_t Pattern, uint32_t Mask, size_t EntrySize>
struct IntegralEncoding {
    static constexpr auto pattern = static_cast<std::byte>(Pattern);
    static constexpr auto mask = static_cast<std::byte>(Mask);
    static constexpr auto entry_size = EntrySize;

    // 确保容器中有足够的空间来存储这个整数
    template<typename Iterator>
        requires std::output_iterator<Iterator, std::byte>
    [[nodiscard]] auto encode(Iterator iter, int64_t value) const noexcept -> Iterator
    {
        *iter++ = pattern; // 写入编码字节
        for (size_t i = 0; i < entry_size - 1; ++i)
            *iter++ = static_cast<std::byte>((value >> (8 * i)) & 0xff); // 写入整数的字节

        return backlen(iter, entry_size);
    }

    template<typename Iterator>
        requires std::input_iterator<Iterator>
    [[nodiscard]] auto decode(Iterator iter) const noexcept -> int64_t
    {
        ++iter; // 跳过 pattern 字节
        int64_t value = 0;
        for (size_t i = 0; i < entry_size - 1; ++i) {
            auto byte = *iter++;
            value |= std::to_integer<int64_t>(byte) << (8 * i);
        }

        // int64 恰好占满 64 位，符号位自然正确；更窄的类型需要符号扩展
        if constexpr (entry_size < 9) {
            constexpr auto bits = (entry_size - 1) * 8;
            if (value & (static_cast<int64_t>(1) << (bits - 1)))
                value -= static_cast<int64_t>(1) << bits;
        }

        return value;
    }
};

// int7 编码是特殊的
template<>
struct IntegralEncoding<0b00000000, 0b10000000, 1> {
    static constexpr auto pattern = static_cast<std::byte>(0b00000000);
    static constexpr auto mask = static_cast<std::byte>(0b10000000);
    static constexpr auto entry_size = 1;

    // 确保容器中有足够的空间来存储这个整数
    template<typename Iterator>
        requires std::output_iterator<Iterator, std::byte>
    [[nodiscard]] auto encode(Iterator iter, int64_t value) const noexcept -> Iterator
    {
        *iter++ = static_cast<std::byte>(value);

        return backlen(iter, entry_size);
    }

    template<typename Iterator>
        requires std::input_iterator<Iterator>
    [[nodiscard]] auto decode(Iterator iter) const noexcept -> int64_t
    {
        return std::to_integer<int64_t>(*iter);
    }
};


// int13 编码是特殊的
template<>
struct IntegralEncoding<0b11000000, 0b11100000, 2> {
    static constexpr auto pattern = static_cast<std::byte>(0b11000000);
    static constexpr auto mask = static_cast<std::byte>(0b11100000);
    static constexpr auto entry_size = 2;

    // 确保容器中有足够的空间来存储这个整数
    template<typename Iterator>
        requires std::output_iterator<Iterator, std::byte>
    [[nodiscard]] auto encode(Iterator iter, int64_t value) const noexcept -> Iterator
    {
        uint16_t val = (value < 0) ? (1 << 13) + value : value;
        *iter++ = static_cast<std::byte>(val >> 8) | pattern;
        *iter++ = static_cast<std::byte>(val & 0xFF);

        return backlen(iter, entry_size);
    }

    template<typename Iterator>
        requires std::input_iterator<Iterator>
    [[nodiscard]] auto decode(Iterator iter) const noexcept -> int64_t
    {
        uint16_t value = 0;
        auto byte = *iter++;
        value |= (std::to_integer<uint16_t>(byte & static_cast<std::byte>(0b00011111)) << 8);

        byte = *iter;
        value |= std::to_integer<uint16_t>(byte);

        if (value & (1 << 12)) // 如果最高位（第13位）是1，说明这是一个负数
            return static_cast<int64_t>(value) - (INT64_C(1) << 13);
            
        return static_cast<int64_t>(value);
    }
};


// int24 编码是特殊的
template<>
struct IntegralEncoding<0b11110010, 0b11111111, 4> {
    static constexpr auto pattern = static_cast<std::byte>(0b11110010);
    static constexpr auto mask = static_cast<std::byte>(0b11111111);
    static constexpr auto entry_size = 4;

    // 确保容器中有足够的空间来存储这个整数
    template<typename Iterator>
        requires std::output_iterator<Iterator, std::byte>
    [[nodiscard]] auto encode(Iterator iter, int64_t value) const noexcept -> Iterator
    {
        *iter++ = pattern; // 写入编码字节

        uint32_t val = (value < 0) ? (1 << 24) + value : value;
        *iter++ = static_cast<std::byte>(val & 0xFF);
        *iter++ = static_cast<std::byte>((val >> 8) & 0xFF);
        *iter++ = static_cast<std::byte>((val >> 16) & 0xFF);

        return backlen(iter, entry_size);
    }

    template<typename Iterator>
        requires std::input_iterator<Iterator>
    [[nodiscard]] auto decode(Iterator iter) const noexcept -> int64_t
    {
        ++iter; // 跳过 pattern 字节
        uint32_t value = 0;
        for (size_t i = 0; i < entry_size - 1; ++i) {
            auto byte = *iter++;
            value |= std::to_integer<uint32_t>(byte) << (8 * i);
        }

        if (value & (1u << 23)) // 如果最高位（第24位）是1，说明这是一个负数
            return static_cast<int64_t>(value) - (INT64_C(1) << 24);

        return static_cast<int64_t>(value);
    }
};


template<uint32_t Pattern, uint32_t Mask>
struct StringEncoding {
    static constexpr auto pattern = static_cast<std::byte>(Pattern);
    static constexpr auto mask = static_cast<std::byte>(Mask);

    template<typename Iterator>
        requires std::output_iterator<Iterator, std::byte>
    [[nodiscard]] auto encode(Iterator iter, std::string_view value) const noexcept -> Iterator
    {
        *iter++ = pattern; // 写入编码字节
        *iter++ = static_cast<std::byte>(value.size() & 0xFF); 
        *iter++ = static_cast<std::byte>((value.size() >> 8) & 0xFF); 
        *iter++ = static_cast<std::byte>((value.size() >> 16) & 0xFF); 
        *iter++ = static_cast<std::byte>((value.size() >> 24) & 0xFF);

        auto views = value | std::views::transform([](char c) { return static_cast<std::byte>(c); });
        iter = std::ranges::copy(views, iter).out; // 写入字符串内容

        return backlen(iter, 1 + 4 + value.size()); // 编码字节 + 长度字节 + 字符串内容
    }

    template<typename Iterator>
        requires std::input_iterator<Iterator>
    [[nodiscard]] auto decode(Iterator iter) const noexcept -> std::string_view
    {
        ++iter; // 跳过 pattern 字节
        uint32_t length = 0;
        for (size_t i = 0; i < 4; ++i) {
            auto byte = *iter++;
            length |= std::to_integer<uint32_t>(byte) << (8 * i);
        }

        std::string_view value{ reinterpret_cast<const char*>(std::addressof(*iter)), length };
        
        return value;
    }
};


// str6 编码是特殊的
template<>
struct StringEncoding<0b10000000, 0b11000000> {
    static constexpr auto pattern = static_cast<std::byte>(0b10000000);
    static constexpr auto mask = static_cast<std::byte>(0b11000000);

    template<typename Iterator>
        requires std::output_iterator<Iterator, std::byte>
    [[nodiscard]] auto encode(Iterator iter, std::string_view value) const noexcept -> Iterator
    {
        *iter++ = pattern | static_cast<std::byte>(value.size() & 0x3F); // 写入编码字节和长度（6位）

        auto views = value | std::views::transform([](char c) { return static_cast<std::byte>(c); });
        iter = std::ranges::copy(views, iter).out; // 写入字符串内容
        return backlen(iter, 1 + value.size()); // 编码字节 + 字符串内容
    }

    template<typename Iterator>
        requires std::input_iterator<Iterator>
    [[nodiscard]] auto decode(Iterator iter) const noexcept -> std::string_view
    {
        auto length = std::to_integer<uint8_t>(*iter++ & std::byte{0x3F}); // 读取长度（6位）
        std::string_view value{ reinterpret_cast<const char*>(std::addressof(*iter)), static_cast<std::size_t>(length) };
        return value; 
    }
};


// str12 编码是特殊的
template<>
struct StringEncoding<0b11100000, 0b11110000> {
    static constexpr auto pattern = static_cast<std::byte>(0b11100000);
    static constexpr auto mask = static_cast<std::byte>(0b11110000);

    template<typename Iterator>
        requires std::output_iterator<Iterator, std::byte>
    [[nodiscard]] auto encode(Iterator iter, std::string_view value) const noexcept -> Iterator
    {
        *iter++ = pattern | static_cast<std::byte>((value.size() >> 8) & 0x0F); // 写入编码字节和长度的高4位
        *iter++ = static_cast<std::byte>(value.size() & 0xFF); // 写入长度的低8位

        auto views = value | std::views::transform([](char c) { return static_cast<std::byte>(c); });
        iter = std::ranges::copy(views, iter).out; // 写入字符串内容
        return backlen(iter, 1 + 1 + value.size()); // 编码字节 + 长度字节 + 字符串内容
    }

    template<typename Iterator>
        requires std::input_iterator<Iterator>
    [[nodiscard]] auto decode(Iterator iter) const noexcept -> std::string_view
    {
        auto length = (std::to_integer<uint8_t>(*iter++) & 0x0F) << 8; // 读取长度的高4位
        length |= std::to_integer<uint8_t>(*iter++); // 读取长度的低8位

        std::string_view value{ reinterpret_cast<const char*>(std::addressof(*iter)), static_cast<std::size_t>(length) };
        iter += length; // 移动迭代器到字符串内容之后
        return value;
    }
};


static constexpr auto int7 = IntegralEncoding<0b00000000, 0b10000000, 1>{};
static constexpr auto str6 = StringEncoding<0b10000000, 0b11000000>{};
static constexpr auto int13 = IntegralEncoding<0b11000000, 0b11100000, 2>{};
static constexpr auto str12 = StringEncoding<0b11100000, 0b11110000>{};
static constexpr auto str32 = StringEncoding<0b11110000, 0b11111111>{};
static constexpr auto int16 = IntegralEncoding<0b11110001, 0b11111111, 3>{};
static constexpr auto int24 = IntegralEncoding<0b11110010, 0b11111111, 4>{};
static constexpr auto int32 = IntegralEncoding<0b11110011, 0b11111111, 5>{};
static constexpr auto int64 = IntegralEncoding<0b11110100, 0b11111111, 9>{};

struct FromBacklenT {};
struct FromEntryT {};

static constexpr auto from_backlen = FromBacklenT{};
static constexpr auto from_entry = FromEntryT{};

template<typename Iterator>
    requires std::input_iterator<Iterator>
auto entry_size_of(Iterator iter, FromEntryT from_entry) -> size_t
{
    auto pattern = *iter;
    if (matches(pattern, int7))
        return int7.entry_size;

    if (matches(pattern, int13))
        return int13.entry_size;

    if (matches(pattern, int16))
        return int16.entry_size;

    if (matches(pattern, int24))
        return int24.entry_size;

    if (matches(pattern, int32))
        return int32.entry_size;

    if (matches(pattern, int64))
        return int64.entry_size;

    if (matches(pattern, str6)) {
        auto length = std::to_integer<uint8_t>(pattern & std::byte{0x3F});
        return 1 + length;
    }

    if (matches(pattern, str12)) {
        uint16_t length = 0;
        length |= (std::to_integer<uint8_t>(pattern & std::byte{0x0F}) << 8);
        length |= std::to_integer<uint8_t>(*(iter + 1));

        size_t entry = 2 + length; // 编码字节 + 长度字节 + 字符串内容
        return entry;
    }

    if (matches(pattern, str32)) {
        uint32_t length = 0;
        for (size_t i = 0; i < 4; ++i) {
            auto byte = *(iter + 1 + i);
            length |= std::to_integer<uint32_t>(byte) << (8 * i);
        }

        size_t entry = 5 + length; // 编码字节 + 4字节长度 + 字符串内容
        return entry;
    }

    std::unreachable();
}

template<typename Iterator>
    requires std::bidirectional_iterator<Iterator>
auto entry_size_of(Iterator iter, FromBacklenT from_backlen) -> size_t
{
    // iter是现在的位置（entry 起始位置），需要退到 backlen 的起始位置
    --iter; // 指向最后一个 backlen 字节（最低 chunk，可能有 continuation bit）

    int shift = 0;
    size_t entry_size = 0;
    while (true) {
        auto b = std::to_integer<uint8_t>(*iter);
        entry_size |= static_cast<size_t>(b & 0x7F) << shift;
        shift += 7;
        if (!(b & 0x80))
            break; // 终止字节（最高 chunk，high bit = 0）

        --iter;   // 继续往前读更高的 chunk
    }

    return entry_size;
}

[[nodiscard]]
constexpr auto backlen_size_for(size_t entry_bytes) noexcept -> size_t
{
    size_t n = 1;
    while (entry_bytes > 127) { ++n; entry_bytes >>= 7; }
    return n;
}

template<typename Encoding>
constexpr auto matches(std::byte pattern, Encoding encoding) noexcept -> bool
{
    return (pattern & Encoding::mask) == Encoding::pattern;
}

template<typename Iterator>
auto decode_of(Iterator iter) -> std::variant<int64_t, std::string_view>
{
    auto pattern = *iter;
    if (matches(pattern, int7))
        return int7.decode(iter);

    if (matches(pattern, int13))
        return int13.decode(iter);

    if (matches(pattern, int16))
        return int16.decode(iter);

    if (matches(pattern, int24))
        return int24.decode(iter);

    if (matches(pattern, int32))
        return int32.decode(iter);

    if (matches(pattern, int64))
        return int64.decode(iter);

    if (matches(pattern, str6))
        return str6.decode(iter);

    if (matches(pattern, str12))
        return str12.decode(iter);

    if (matches(pattern, str32))
        return str32.decode(iter);

    std::unreachable();
}

template<typename Iterator>
auto encode(Iterator iter, std::string_view value) -> Iterator
{
    if (value.size() < 64)
        return str6.encode(iter, value);
    else if (value.size() < 4096)
        return str12.encode(iter, value);
    else
        return str32.encode(iter, value);
}

template<typename Iterator>
auto encode(Iterator iter, int64_t value) -> Iterator
{
    if (value >= 0 && value <= 127)
        return int7.encode(iter, value);
    else if (-4096 <= value && value <= 4095)
        return int13.encode(iter, value);
    else if (std::numeric_limits<int16_t>::min() <= value && value <= std::numeric_limits<int16_t>::max())
        return int16.encode(iter, value);
    else if (-8388608 <= value && value <= 8388607)
        return int24.encode(iter, value);
    else if (std::numeric_limits<int32_t>::min() <= value && value <= std::numeric_limits<int32_t>::max())
        return int32.encode(iter, value);
    else
        return int64.encode(iter, value);
}

} // namespace

// ------------------------- ListPack 实现 --------------------------

ListPack::ListPack(size_t capacity) 
{
    buffer_.reserve(std::max(capacity, HEADER_SIZE + 1));
    buffer_.resize(HEADER_SIZE + 1); // header 6 字节 + EOF 1 字节
    num_elements(0);
    buffer_.back() = END_OF_BUFFER;
    update_total_bytes();
}

void ListPack::push_back(std::string_view view)
{
    // Redis 的 listpack 允许空字符串，但不允许以 '0' 开头的字符串（因为 '0' 是 int7 的编码起始字节）
    if (!view.empty() && view[0] != '0') {
        auto res = numeric_cast<int64_t>(view);
        if (res)
            return push_back(*res);
    }   

    buffer_.pop_back();
    auto iter = encode(std::back_inserter(buffer_), view);
    *iter = END_OF_BUFFER;

    increase_num_elements(1);
    update_total_bytes();
}

void ListPack::push_back_string(std::string_view view)
{
    buffer_.pop_back();
    auto iter = encode(std::back_inserter(buffer_), view);
    *iter = END_OF_BUFFER;

    increase_num_elements(1);
    update_total_bytes();
}

void ListPack::push_back(int64_t value)
{
    buffer_.pop_back(); // 移除 EOF，准备添加新元素

    auto iter = encode(std::back_inserter(buffer_), value);
    *iter = END_OF_BUFFER;
    
    increase_num_elements(1);
    update_total_bytes();
}

auto ListPack::insert(Iterator pos, std::string_view view) -> Iterator
{
    // 如果不是以 '0' 开头的字符串，尝试解析为整数
    if (!view.empty() && view[0] != '0') {
        auto res = numeric_cast<int64_t>(view);
        if (res)
            return insert(pos, *res);
    }

    auto index = static_cast<size_t>(std::distance(buffer_.begin(), pos.iter_));

    // 中间插入时无需重写 EOF，末尾 EOF 已由现有 buffer 维护。
    encode(std::inserter(buffer_, pos.iter_), view);

    increase_num_elements(1);
    update_total_bytes();
    return Iterator{ buffer_.begin() + static_cast<std::ptrdiff_t>(index) };
}

auto ListPack::insert_string(Iterator pos, std::string_view view) -> Iterator
{
    auto index = static_cast<size_t>(std::distance(buffer_.begin(), pos.iter_));

    // 中间插入时无需重写 EOF，末尾 EOF 已由现有 buffer 维护。
    encode(std::inserter(buffer_, pos.iter_), view);

    increase_num_elements(1);
    update_total_bytes();
    return Iterator{ buffer_.begin() + static_cast<std::ptrdiff_t>(index) };
}

auto ListPack::insert(Iterator pos, int64_t value) -> Iterator
{
    auto index = static_cast<size_t>(std::distance(buffer_.begin(), pos.iter_));

    // 中间插入时无需重写 EOF，末尾 EOF 已由现有 buffer 维护。
    encode(std::inserter(buffer_, pos.iter_), value);

    increase_num_elements(1);
    update_total_bytes();
    return Iterator{ buffer_.begin() + static_cast<std::ptrdiff_t>(index) };
}

void ListPack::erase(Iterator pos)
{
    auto entry_size = entry_size_of(pos.iter_, from_entry);
    auto backlen_size = backlen_size_for(entry_size);
    auto total_size = entry_size + backlen_size;

    buffer_.erase(pos.iter_, pos.iter_ + total_size);

    decrease_num_elements(1);
    update_total_bytes();
}

void ListPack::erase(Iterator first, Iterator last)
{
    if (first == last)
        return;

    auto entry_size = entry_size_of(first.iter_, from_entry);
    auto backlen_size = backlen_size_for(entry_size);
    auto total_size = entry_size + backlen_size;

    size_t count = 0;
    for (auto it = first.iter_; it != last.iter_; it += total_size) {
        ++count;
        entry_size = entry_size_of(it, from_entry);
        backlen_size = backlen_size_for(entry_size);
        total_size = entry_size + backlen_size;
    }

    buffer_.erase(first.iter_, last.iter_);

    decrease_num_elements(count);
    update_total_bytes();
}

void ListPack::clear() noexcept
{
    buffer_.clear();
    buffer_.resize(HEADER_SIZE + 1); // header 6 字节 + EOF 1 字节
    num_elements(0);
    buffer_.back() = END_OF_BUFFER;
    update_total_bytes();
}

[[nodiscard]]
auto ListPack::begin() noexcept -> Iterator
{
    return Iterator{ buffer_.begin() + HEADER_SIZE };
}

[[nodiscard]]
auto ListPack::begin() const noexcept -> Iterator
{
    auto& mutable_buffer = const_cast<std::pmr::vector<std::byte>&>(buffer_);
    return Iterator{ mutable_buffer.begin() + HEADER_SIZE };
}

[[nodiscard]]
auto ListPack::end() noexcept -> Iterator
{
    return Iterator{ buffer_.end() - 1 }; // 指向 EOF
}

[[nodiscard]]
auto ListPack::end() const noexcept -> Iterator
{
    auto& mutable_buffer = const_cast<std::pmr::vector<std::byte>&>(buffer_);
    return Iterator{ mutable_buffer.end() - 1 }; // 指向 EOF
}

[[nodiscard]]
auto ListPack::rbegin() noexcept -> ReverseIterator
{
    if (buffer_.size() == HEADER_SIZE + 1)
        return rend();

    auto eof = buffer_.end() - 1;
    auto entry_size = entry_size_of(eof, from_backlen);
    auto backlen_size = backlen_size_for(entry_size);
    return ReverseIterator{ eof - (entry_size + backlen_size) };
}

[[nodiscard]]
auto ListPack::rbegin() const noexcept -> ReverseIterator
{
    auto& mutable_buffer = const_cast<std::pmr::vector<std::byte>&>(buffer_);
    if (mutable_buffer.size() == HEADER_SIZE + 1)
        return rend();

    auto eof = mutable_buffer.end() - 1;
    auto entry_size = entry_size_of(eof, from_backlen);
    auto backlen_size = backlen_size_for(entry_size);
    return ReverseIterator{ eof - (entry_size + backlen_size) };
}

[[nodiscard]]
auto ListPack::rend() noexcept -> ReverseIterator
{
    auto first = buffer_.begin() + HEADER_SIZE;

    // 与 ReverseIterator::operator++ 在首元素位置的前移规则保持一致。
    auto entry_size = entry_size_of(first, from_backlen);
    auto backlen_size = backlen_size_for(entry_size);
    return ReverseIterator{ first - (entry_size + backlen_size) };
}

[[nodiscard]]
auto ListPack::rend() const noexcept -> ReverseIterator
{
    auto& mutable_buffer = const_cast<std::pmr::vector<std::byte>&>(buffer_);
    auto first = mutable_buffer.begin() + HEADER_SIZE;

    auto entry_size = entry_size_of(first, from_backlen);
    auto backlen_size = backlen_size_for(entry_size);
    return ReverseIterator{ first - (entry_size + backlen_size) };
}

void ListPack::update_total_bytes() noexcept
{
    auto total = buffer_.size();
    buffer_[0] = static_cast<std::byte>(total & 0b11111111);
    buffer_[1] = static_cast<std::byte>((total >> 8) & 0b11111111);
    buffer_[2] = static_cast<std::byte>((total >> 16) & 0b11111111);
    buffer_[3] = static_cast<std::byte>((total >> 24) & 0b11111111);
}

void ListPack::increase_num_elements(NumElements delta) noexcept
{
    auto current = num_elements();
    auto max = std::numeric_limits<NumElements>::max();
    if (current + delta < max - 2) // 留点余量，避免接近上限时频繁 saturate
        num_elements(current + delta);
    else
        num_elements(max);
}

void ListPack::decrease_num_elements(NumElements delta) noexcept
{
    auto current = num_elements();
    if (current > delta)
        num_elements(current - delta);
    else
        num_elements(0);
}

void ListPack::num_elements(NumElements value) noexcept
{
    buffer_[4] = static_cast<std::byte>(value & 0b11111111);
    buffer_[5] = static_cast<std::byte>((value >> 8) & 0b11111111);
}

auto ListPack::num_elements() const noexcept -> NumElements
{
    NumElements value = 0;
    value |= static_cast<NumElements>(buffer_[4]) << 0;
    value |= static_cast<NumElements>(buffer_[5]) << 8;
    return value;
}

void ListPack::increase_capacity(size_t increment)
{
    auto new_capacity = buffer_.capacity() + increment;
    // 相信标准库的扩容策略，暂时不自己计算新容量了
    buffer_.reserve(new_capacity);
}

// --------------------- Iterator 实现 ------------------------

auto ListPack::Iterator::operator*() const -> Element
{
    return decode_of(iter_);
}

auto ListPack::Iterator::operator++() -> Iterator&
{
    auto pattern = *iter_;
    auto entry_bytes = entry_size_of(iter_, from_entry);
    auto backlen_size = backlen_size_for(entry_bytes);
    iter_ += (entry_bytes + backlen_size); // 跳过当前元素的编码字节、数据字节和 backlen 字节
    return *this;
}

auto ListPack::Iterator::operator++(int) -> Iterator
{
    auto temp = *this;
    ++(*this);
    return temp;
}

auto ListPack::Iterator::operator--() -> Iterator&
{
    auto entry_size = entry_size_of(iter_, from_backlen);
    auto backlen_size = backlen_size_for(entry_size);
    iter_ -= (entry_size + backlen_size); // 跳过当前元素的编码字节、数据字节和 backlen 字节，回到上一个元素的起始位置
    return *this;
}

auto ListPack::Iterator::operator--(int) -> Iterator
{
    auto temp = *this;
    --(*this);
    return temp;
}

// --------------------- ReverseIterator 实现 ------------------------

auto ListPack::ReverseIterator::operator*() const -> Element
{
    return decode_of(iter_);
}

auto ListPack::ReverseIterator::operator++() -> ReverseIterator&
{
    auto entry_size = entry_size_of(iter_, from_backlen);
    auto backlen_size = backlen_size_for(entry_size);
    iter_ -= (entry_size + backlen_size); // 跳过当前元素的编码字节、数据字节和 backlen 字节，回到上一个元素的起始位置
    return *this;
}

auto ListPack::ReverseIterator::operator++(int) -> ReverseIterator
{
    auto temp = *this;
    ++(*this);
    return temp;
}

auto ListPack::ReverseIterator::operator--() -> ReverseIterator&
{
    auto pattern = *iter_;
    auto entry_bytes = entry_size_of(iter_, from_entry);
    auto backlen_size = backlen_size_for(entry_bytes);
    iter_ += (entry_bytes + backlen_size); // 跳过当前元素的编码字节、数据字节和 backlen 字节
    return *this;
}

auto ListPack::ReverseIterator::operator--(int) -> ReverseIterator
{
    auto temp = *this;
    --(*this);
    return temp;
}

} // namespace spg::redis
