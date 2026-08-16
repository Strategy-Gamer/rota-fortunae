#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace rota::core {

class Hasher {
public:
    void feed_bytes(const void* data, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        for (std::size_t i = 0; i < size; ++i) {
            hash_ ^= bytes[i];
            hash_ *= PRIME;
        }
    }

    template <typename T>
    void feed(T value) {
        static_assert(std::is_integral_v<T>, "hash integral types only");
        feed_bytes(&value, sizeof(value));
    }

    std::uint64_t value() const { return hash_; }
private:
    static constexpr std::uint64_t OFFSET = 14695981039346656037ULL;
    static constexpr std::uint64_t PRIME  = 1099511628211ULL;
    std::uint64_t hash_ = OFFSET;
};

}