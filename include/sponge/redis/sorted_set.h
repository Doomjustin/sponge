#ifndef SPONGE_REDIS_SORTED_SET_H
#define SPONGE_REDIS_SORTED_SET_H

#include <functional>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <boost/unordered/unordered_node_map.hpp>

#include <sponge/redis/list_pack.h>
#include <sponge/redis/skip_list.h>
#include <sponge/utility.h>

namespace spg::redis {

class SortedSet {
public:
    using Score = double;
    using Member = std::string_view;
    using String = std::pmr::string;
    using ValueType = std::pair<String, Score>;

    class Iterator {
    public:
        auto operator*() const -> ValueType;

        auto operator++() -> Iterator&;

        auto operator++(int) -> Iterator;

        auto operator==(const Iterator& other) const -> bool;

        auto operator!=(const Iterator& other) const -> bool;

    private:
        friend class SortedSet;

        using BackendIterator = std::variant<ListPack::Iterator, SkipList::Iterator>;

        explicit Iterator(const SortedSet* owner, BackendIterator iter)
          : owner_{ owner }, iter_{ iter }
        {}

        const SortedSet* owner_ = nullptr;
        BackendIterator iter_;
    };

    auto add(Score score, Member member) -> bool;

    auto score(Member member) -> std::optional<Score>;

    auto contains(Member member) -> bool;

    auto erase(Member member) -> bool;

    auto zcount(Score min, Score max) const -> size_t;

    auto zrank(Member member) const -> std::optional<int64_t>;

    [[nodiscard]]
    auto size() const -> size_t;

    auto begin() -> Iterator;

    auto end() -> Iterator;

    auto begin() const -> Iterator;

    auto end() const -> Iterator;

private:
    using Allocator = std::pmr::polymorphic_allocator<std::pair<String, Score>>;
    using Dict = boost::unordered_node_map<String, Score, PmrStringHash, std::equal_to<>, Allocator>;

    struct Node {
        Dict dict;
        SkipList skip_list;
    };

    struct AsMemberT {};
    struct AsScoreT {};
    static constexpr AsMemberT as_member{};
    static constexpr AsScoreT as_score{};

    static constexpr size_t MAX_LISTPACK_ENTRIES = 128;
    static constexpr size_t MAX_LISTPACK_BYTES = 64;

    std::variant<ListPack, Node> backend_{ std::in_place_type<ListPack>, MAX_LISTPACK_ENTRIES };

    auto add_to_listpack(Score score, Member member) -> bool;

    auto add_to_skiplist(Score score, Member member) -> bool;

    void upgrade();

    auto extract(ListPack::Iterator iter, AsMemberT as_member) const -> String;

    auto extract(ListPack::Iterator iter, AsScoreT as_score) const -> Score;
}; 

} // namespace spg::redis

#endif // SPONGE_REDIS_SORTED_SET_H
