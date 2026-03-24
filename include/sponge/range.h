#ifndef SPONGE_RANGE_H
#define SPONGE_RANGE_H

#include <cstdint>

namespace spg {

enum class Boundary: uint8_t {
    Inclusive,
    Exclusive,
};

template<typename T, Boundary Left, Boundary Right>
class Range {
public:
    constexpr Range(T lower, T upper)
      : lower_{ lower }, upper_{ upper } 
    {}

    auto contains(T value) const noexcept -> bool
    {
        bool min_ok = (Left == Boundary::Inclusive) ? (value >= lower_) : (value > lower_);
        bool max_ok = (Right == Boundary::Inclusive) ? (value <= upper_) : (value < upper_);
        return min_ok && max_ok;
    }
    
    template<Boundary OtherLeft, Boundary OtherRight>
    auto overlaps(const Range<T, OtherLeft, OtherRight>& other) const noexcept -> bool
    {
        if (upper_ < other.lower_ || other.upper_ < lower_)
            return false;

        // Touching at a single boundary point.
        if (upper_ == other.lower_)
            return Right == Boundary::Inclusive && OtherLeft == Boundary::Inclusive;

        if (other.upper_ == lower_)
            return OtherRight == Boundary::Inclusive && Left == Boundary::Inclusive;

        return true;
    }

private:
    template<typename U, Boundary L, Boundary R>
    friend class Range;

    T lower_;
    T upper_;
};

} // namespace spg

#endif // SPONGE_RANGE_H
