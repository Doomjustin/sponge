#ifndef SPONGE_REDIS_SORTED_SET_H
#define SPONGE_REDIS_SORTED_SET_H

#include <functional>
#include <memory_resource>
#include <string>
#include <string_view>

#include <boost/unordered/unordered_node_map.hpp>

#include <sponge/hash.h>
#include <sponge/redis/list_pack.h>
#include <sponge/redis/skip_list.h>

namespace spg::redis {

class SortedSet {
public:
    using Score = double;
    using Member = std::string_view;

    explicit SortedSet(std::pmr::memory_resource* resource)
      : resource_{ resource }, backend_{ std::in_place_type<ListPack>, MAX_LISTPACK_ENTRIES, resource }
    {}

    auto add(Score score, Member member) -> bool;

private:
    using String = std::pmr::string;
    using Allocator = std::pmr::polymorphic_allocator<std::pair<String, Score>>;
    using Dict = boost::unordered_node_map<String, Score, PmrStringHash, std::equal_to<>, Allocator>;


    struct Node {
        Dict dict;
        SkipList skip_list;

        explicit Node(std::pmr::memory_resource* resource)
          : dict{ Allocator(resource) }, 
            skip_list{ resource } 
        {}
    };

    struct AsMemberT {};
    struct AsScoreT {};
    static constexpr AsMemberT as_member{};
    static constexpr AsScoreT as_score{};

    static constexpr size_t MAX_LISTPACK_ENTRIES = 128;
    static constexpr size_t MAX_LISTPACK_BYTES = 64;

    std::pmr::memory_resource* resource_;
    std::variant<ListPack, Node> backend_;

    auto add_to_listpack(Score score, Member member) -> bool;

    auto add_to_skiplist(Score score, Member member) -> bool;

    void upgrade();

    auto extract(ListPack::Iterator iter, AsMemberT as_member) -> std::string_view;

    auto extract(ListPack::Iterator iter, AsScoreT as_score) -> Score;
}; 



} // namespace spg::redis

#endif // SPONGE_REDIS_SORTED_SET_H
