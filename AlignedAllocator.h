#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

namespace image_channel_viewer::memory
{
    template<typename T, std::size_t Alignment>
    class aligned_allocator
    {
    public:
        static_assert(Alignment >= alignof(T), "Alignment must satisfy alignof(T)");
        static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of two");

        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        aligned_allocator() noexcept = default;

        template<typename U>
        aligned_allocator(aligned_allocator<U, Alignment> const&) noexcept
        {
        }

        [[nodiscard]] T* allocate(size_type count)
        {
            if (count > max_size())
            {
                throw std::bad_array_new_length();
            }

            return static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{ Alignment }));
        }

        void deallocate(T* pointer, size_type) noexcept
        {
            ::operator delete(pointer, std::align_val_t{ Alignment });
        }

        [[nodiscard]] constexpr size_type max_size() const noexcept
        {
            return std::numeric_limits<size_type>::max() / sizeof(T);
        }

        template<typename U>
        struct rebind
        {
            using other = aligned_allocator<U, Alignment>;
        };

        using is_always_equal = std::true_type;
    };

    template<typename T, typename U, std::size_t Alignment>
    constexpr bool operator==(aligned_allocator<T, Alignment> const&, aligned_allocator<U, Alignment> const&) noexcept
    {
        return true;
    }

    template<typename T, typename U, std::size_t Alignment>
    constexpr bool operator!=(aligned_allocator<T, Alignment> const&, aligned_allocator<U, Alignment> const&) noexcept
    {
        return false;
    }
}