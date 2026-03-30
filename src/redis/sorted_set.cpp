#include <sponge/redis/sorted_set.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <variant>

#include <fmt/format.h>

#include <sponge/utility.h>

namespace spg::redis {

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

            if (extract(member_iter, as_member) == member)
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

            if (extract(member_iter, as_member) == member)
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

            if (extract(member_iter, as_member) == member) {
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

    std::vector<std::pair<String, Score>> entries;
    entries.reserve((listpack.size() / 2) + 1);

    bool inserted = true;
    bool found = false;
    for (auto it = listpack.begin(); it != listpack.end();) {
        auto existing_member = extract(it++, as_member);
        auto existing_score = extract(it++, as_score);

        if (existing_member == member) {
            entries.emplace_back(String{ member }, score);
            inserted = false;
            found = true;
            continue;
        }

        entries.emplace_back(String{ existing_member }, existing_score);
    }

    if (!found)
        entries.emplace_back(String{ member }, score);

    if (!found && (member.size() > MAX_LISTPACK_BYTES || entries.size() > MAX_LISTPACK_ENTRIES)) {
        upgrade();
        return add_to_skiplist(score, member);
    }

    std::ranges::sort(entries, [] (const auto& lhs, const auto& rhs) {
        if (lhs.second != rhs.second)
            return lhs.second < rhs.second;
        return lhs.first < rhs.first;
    });

    listpack.clear();
    for (const auto& [entry_member, entry_score] : entries) {
        listpack.push_back_string(entry_member);
        listpack.push_back(fmt::format("{}", entry_score));
    }

    return inserted;
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
