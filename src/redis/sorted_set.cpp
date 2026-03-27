#include <sponge/redis/sorted_set.h>

#include <cstdint>
#include <utility>
#include <variant>

#include <sponge/utility.h>

namespace spg::redis {

auto SortedSet::add(Score score, Member member) -> bool {
    if (std::holds_alternative<ListPack>(backend_))
        return add_to_listpack(score, member);
    
    return add_to_skiplist(score, member);
}

auto SortedSet::add_to_listpack(Score score, Member member) -> bool 
{
    auto& listpack = std::get<ListPack>(backend_);

    if (member.size() > MAX_LISTPACK_BYTES || listpack.size() >= MAX_LISTPACK_ENTRIES) {
        upgrade();
        return add_to_skiplist(score, member);
    }

    return true;
}

auto SortedSet::add_to_skiplist(Score score, Member member) -> bool 
{
    // Implementation for adding to SkipList
    return true;
}

void SortedSet::upgrade() 
{
    Node new_node{ resource_ };
    auto& listpack = std::get<ListPack>(backend_);

    auto it = listpack.begin();
    while (it != listpack.end()) {
        auto member = extract(it++, as_member);
        auto score = extract(it++, as_score);
        auto [iter, _] = new_node.dict.emplace(String{ member, resource_ }, score);
        new_node.skip_list.insert(score, iter->first);
    }

    backend_ = std::move(new_node);
}

auto SortedSet::extract(ListPack::Iterator iter, AsMemberT as_member) -> std::string_view
{
    auto element = *iter;
    if (std::holds_alternative<Member>(element))
        return std::get<Member>(element);

    // 在我们的路径里，member一定会是一个字符串而不是整数
    std::unreachable();
}

auto SortedSet::extract(ListPack::Iterator iter, AsScoreT as_score) -> Score
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

} // namespace spg::redis
