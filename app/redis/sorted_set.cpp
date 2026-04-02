#include "sorted_set.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <utility>
#include <variant>

#include <fmt/format.h>

#include <sponge/utility.h>

namespace spg::redis {

namespace {

[[nodiscard]]
auto serialize_score(SortedSet::Score score) -> std::string
{
    std::array<char, 64> buffer{};
    auto [ptr, ec] = std::to_chars(
      buffer.data(),
      buffer.data() + static_cast<std::ptrdiff_t>(buffer.size()),
      score,
      std::chars_format::general,
      17
    );
    if (ec == std::errc{})
        return std::string{ buffer.data(), static_cast<size_t>(ptr - buffer.data()) };

    return fmt::format("{}", score);
}

[[nodiscard]]
auto member_view(ListPack::Iterator it) -> std::string_view
{
    auto element = *it;
    if (std::holds_alternative<std::string_view>(element))
        return std::get<std::string_view>(element);

    std::unreachable();
}

} // namespace

auto SortedSet::add(Score score, Member member) -> bool {
    if (std::holds_alternative<ListPack>(backend_))
        return add_to_listpack(score, member);
    
    return add_to_skiplist(score, member);
}

auto SortedSet::score(Member member) -> std::optional<Score>
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        for (auto it = listpack.begin(); it != listpack.end();) {
            auto member_iter = it;
            auto score_iter = it;
            ++score_iter;
            auto next = score_iter;
            ++next;

            if (member_view(member_iter) == member)
                return extract(score_iter, as_score);

            it = next;
        }
        
        return {};
    }

    auto& node = std::get<Node>(backend_);
    auto it = node.dict.find(member);
    if (it != node.dict.end()) {
        return it->second;
    }
    return {};
}

auto SortedSet::contains(Member member) -> bool
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        for (auto it = listpack.begin(); it != listpack.end();) {
            auto member_iter = it;
            auto score_iter = it;
            ++score_iter;
            auto next = score_iter;
            ++next;

            if (member_view(member_iter) == member)
                return true;

            it = next;
        }
        
        return false;
    }

    auto& node = std::get<Node>(backend_);
    return node.dict.contains(member);
}

auto SortedSet::erase(Member member) -> bool
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        for (auto it = listpack.begin(); it != listpack.end();) {
            auto member_iter = it;
            auto score_iter = it;
            ++score_iter;
            auto next = score_iter;
            ++next;

            if (member_view(member_iter) == member) {
                listpack.erase(member_iter, next);
                return true;
            }

            it = next;
        }

        return false;
    }

    auto& node = std::get<Node>(backend_);
    auto it = node.dict.find(member);
    if (it == node.dict.end())
        return false;

    auto score = it->second;
    node.skip_list.erase(score, it->first);
    node.dict.erase(it);
    return true;
}

auto SortedSet::zcount(Score min, Score max) const -> size_t
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        size_t count = 0;

        for (auto it = listpack.begin(); it != listpack.end();) {
            auto member_iter = it;
            auto score_iter = it;
            ++score_iter;
            auto next = score_iter;
            ++next;

            auto score = extract(score_iter, as_score);
            if (score > max)
                break;

            if (score >= min)
                ++count;

            it = next;
        }

        return count;
    }

    auto& node = std::get<Node>(backend_);
    return node.skip_list.count_by_score(min, max);
}

auto SortedSet::zrank(Member member) const -> std::optional<int64_t>
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        int64_t rank = 0;
        for (auto it = listpack.begin(); it != listpack.end();) {
            auto member_iter = it;
            auto score_iter = it;
            ++score_iter;
            auto next = score_iter;
            ++next;

            auto element = *member_iter;
            if (std::holds_alternative<Member>(element) && std::get<Member>(element) == member)
                return rank;

            ++rank;
            it = next;
        }

        return std::nullopt;
    }

    auto& node = std::get<Node>(backend_);
    auto it = node.dict.find(member);
    if (it == node.dict.end())
        return std::nullopt;

    auto rank = node.skip_list.rank(it->second, it->first);
    if (rank == 0)
        return std::nullopt;

    return static_cast<int64_t>(rank - 1);
}

auto SortedSet::size() const -> size_t
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        return listpack.size() / 2;
    }

    auto& node = std::get<Node>(backend_);
    return node.dict.size();
}

auto SortedSet::begin() -> Iterator
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        return Iterator{ this, listpack.begin() };
    }

    auto& node = std::get<Node>(backend_);
    return Iterator{ this, node.skip_list.begin() };
}

auto SortedSet::end() -> Iterator
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        return Iterator{ this, listpack.end() };
    }

    auto& node = std::get<Node>(backend_);
    return Iterator{ this, node.skip_list.end() };
}

auto SortedSet::begin() const -> Iterator
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        return Iterator{ this, listpack.begin() };
    }

    auto& node = std::get<Node>(backend_);
    return Iterator{ this, node.skip_list.begin() };
}

auto SortedSet::end() const -> Iterator
{
    if (std::holds_alternative<ListPack>(backend_)) {
        auto& listpack = std::get<ListPack>(backend_);
        return Iterator{ this, listpack.end() };
    }

    auto& node = std::get<Node>(backend_);
    return Iterator{ this, node.skip_list.end() };
}

auto SortedSet::add_to_listpack(Score score, Member member) -> bool 
{
    auto& listpack = std::get<ListPack>(backend_);

    auto upsert_pair = [&](ListPack::Iterator pos) {
        auto inserted_member = listpack.insert_string(pos, member);
        auto score_pos = inserted_member;
        ++score_pos;
        auto score_text = serialize_score(score);
        listpack.insert_string(score_pos, score_text);
    };

    auto less_than = [&](Score existing_score, std::string_view existing_member) {
        if (existing_score != score)
            return existing_score < score;

        return existing_member < member;
    };

    std::optional<ListPack::Iterator> old_member_it;
    std::optional<ListPack::Iterator> old_next_it;
    for (auto it = listpack.begin(); it != listpack.end();) {
        auto member_it = it;
        auto score_it = it;
        ++score_it;
        auto next = score_it;
        ++next;

        auto existing_member = member_view(member_it);
        if (existing_member == member) {
            auto existing_score = extract(score_it, as_score);
            if (existing_score == score)
                return false;

            old_member_it = member_it;
            old_next_it = next;
            break;
        }

        it = next;
    }

    if (!old_member_it && (member.size() > MAX_LISTPACK_BYTES || (listpack.size() / 2 + 1) > MAX_LISTPACK_ENTRIES)) {
        upgrade();
        return add_to_skiplist(score, member);
    }

    if (old_member_it)
        listpack.erase(*old_member_it, *old_next_it);

    auto insert_pos = listpack.end();
    for (auto it = listpack.begin(); it != listpack.end();) {
        auto member_it = it;
        auto score_it = it;
        ++score_it;
        auto next = score_it;
        ++next;

        auto existing_score = extract(score_it, as_score);
        auto existing_member = member_view(member_it);
        if (!less_than(existing_score, existing_member)) {
            insert_pos = member_it;
            break;
        }

        it = next;
    }

    upsert_pair(insert_pos);

    return !old_member_it.has_value();
}

auto SortedSet::add_to_skiplist(Score score, Member member) -> bool 
{
    auto& node = std::get<Node>(backend_);
    if (auto it = node.dict.find(member); it != node.dict.end()) {
        auto old_score = it->second;
        if (old_score == score)
            return false;

        node.skip_list.update(old_score, score, it->first);
        it->second = score;
        return false;
    }

    auto [it, inserted] = node.dict.emplace(member, score);
    node.skip_list.insert(score, it->first);
    return inserted;
}

void SortedSet::upgrade() 
{
    Node new_node{};
    auto& listpack = std::get<ListPack>(backend_);

    auto it = listpack.begin();
    while (it != listpack.end()) {
        auto member = extract(it++, as_member);
        auto score = extract(it++, as_score);
        auto [iter, _] = new_node.dict.emplace(member, score);
        new_node.skip_list.insert(score, iter->first);
    }

    backend_ = std::move(new_node);
}

auto SortedSet::extract(ListPack::Iterator iter, AsMemberT as_member) const -> String
{
    auto element = *iter;
    if (std::holds_alternative<Member>(element))
        return String{ std::get<Member>(element) };

    std::unreachable();
}

auto SortedSet::extract(ListPack::Iterator iter, AsScoreT as_score) const -> Score
{
    auto element = *iter;
    if (std::holds_alternative<int64_t>(element))
        return std::get<int64_t>(element);

    auto score = std::get<std::string_view>(element);
    auto res = numeric_cast<Score>(score);
    if (!res)
        return 0.0;

    return *res;
}

auto SortedSet::Iterator::operator*() const -> ValueType
{
    if (std::holds_alternative<ListPack::Iterator>(iter_)) {
        auto member_iter = std::get<ListPack::Iterator>(iter_);
        auto score_iter = member_iter;
        ++score_iter;
        return { owner_->extract(member_iter, as_member), owner_->extract(score_iter, as_score) };
    }

    auto [score, member] = *std::get<SkipList::Iterator>(iter_);
    return ValueType{ String{ member }, score };
}

auto SortedSet::Iterator::operator++() -> Iterator&
{
    if (std::holds_alternative<ListPack::Iterator>(iter_)) {
        auto& it = std::get<ListPack::Iterator>(iter_);
        ++it;
        if (it != std::get<ListPack::Iterator>(owner_->end().iter_))
            ++it;
        return *this;
    }

    ++std::get<SkipList::Iterator>(iter_);
    return *this;
}

auto SortedSet::Iterator::operator++(int) -> Iterator
{
    auto old = *this;
    ++(*this);
    return old;
}

auto SortedSet::Iterator::operator==(const Iterator& other) const -> bool
{
    if (owner_ != other.owner_)
        return false;

    return iter_ == other.iter_;
}

auto SortedSet::Iterator::operator!=(const Iterator& other) const -> bool
{
    return !(*this == other);
}

} // namespace spg::redis
