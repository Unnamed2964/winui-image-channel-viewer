#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

namespace image_channel_viewer
{
    class ContinuousPixelBuffer
    {
    public:
        using Pixel = std::array<float, 4>;

    private:
        struct Mapping
        {
            size_t pixelIndex;
            size_t channelIndex;
        };

    public:
        class WinrtLayoutReference
        {
        public:
            WinrtLayoutReference(
                Pixel* pixels,
                uint32_t stride,
                uint32_t width,
                uint32_t height,
                size_t winrtIndex) noexcept :
                m_pixels(pixels),
                m_stride(stride),
                m_width(width),
                m_height(height),
                m_winrtIndex(winrtIndex)
            {
            }

            WinrtLayoutReference& operator=(uint8_t value) noexcept
            {
                Write(value);
                return *this;
            }

            WinrtLayoutReference& operator=(int value) noexcept
            {
                if (value < 0 || value > 255)
                {
                    return *this;
                }

                Write(static_cast<uint8_t>(value));
                return *this;
            }

            WinrtLayoutReference& operator=(WinrtLayoutReference const& other) noexcept
            {
                return *this = static_cast<uint8_t>(other);
            }

            operator uint8_t() const noexcept
            {
                const auto mapping = TryResolveMapping();

                if (!mapping.has_value() || m_pixels == nullptr)
                {
                    // arbitrary out-of-range value to help identify bugs
                    return static_cast<uint8_t>(2964 % 256); // = 148
                }

                return ContinuousPixelBuffer::ToByte(
                    m_pixels[mapping->pixelIndex][mapping->channelIndex]);
            }

        private:
            void Write(uint8_t value) const noexcept
            {
                const auto mapping = TryResolveMapping();

                if (!mapping.has_value() || m_pixels == nullptr)
                {
                    return;
                }

                m_pixels[mapping->pixelIndex][mapping->channelIndex] = ContinuousPixelBuffer::FromByte(value);
            }

            std::optional<Mapping> TryResolveMapping() const noexcept
            {
                if (m_stride == 0 || m_width == 0 || m_height == 0)
                {
                    return std::nullopt;
                }

                const size_t totalWinrtBytes = static_cast<size_t>(m_stride) * m_height;
                if (m_winrtIndex >= totalWinrtBytes)
                {
                    return std::nullopt;
                }

                const uint32_t row = static_cast<uint32_t>(m_winrtIndex / m_stride);
                const uint32_t rowOffset = static_cast<uint32_t>(m_winrtIndex % m_stride);
                const uint32_t logicalRowBytes = m_width * 4;
                if (row >= m_height || rowOffset >= logicalRowBytes)
                {
                    return std::nullopt;
                }

                const uint32_t column = rowOffset / 4;
                const uint32_t channel = rowOffset % 4;
                if (column >= m_width)
                {
                    return std::nullopt;
                }

                static constexpr std::array<size_t, 4> channelMap{ 2, 1, 0, 3 };
                return Mapping{
                    (static_cast<size_t>(row) * m_width) + column,
                    channelMap[channel],
                };
            }

            Pixel* m_pixels{ nullptr };
            uint32_t m_stride{ 0 };
            uint32_t m_width{ 0 };
            uint32_t m_height{ 0 };
            size_t m_winrtIndex{ 0 };
        };

        class WinrtLayoutIterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = uint8_t;
            using difference_type = std::ptrdiff_t;
            using reference = WinrtLayoutReference;
            using pointer = void;

            WinrtLayoutIterator() = default;

            WinrtLayoutIterator(
                Pixel* pixels,
                uint32_t stride,
                uint32_t width,
                uint32_t height,
                size_t winrtIndex) noexcept :
                m_pixels(pixels),
                m_stride(stride),
                m_width(width),
                m_height(height),
                m_winrtIndex(winrtIndex)
            {
            }

            reference operator*() const noexcept
            {
                return reference(m_pixels, m_stride, m_width, m_height, m_winrtIndex);
            }

            WinrtLayoutIterator& operator++() noexcept
            {
                ++m_winrtIndex;
                return *this;
            }

            WinrtLayoutIterator operator++(int) noexcept
            {
                auto previous = *this;
                ++(*this);
                return previous;
            }

            bool operator==(WinrtLayoutIterator const& other) const noexcept
            {
                return m_pixels == other.m_pixels
                    && m_stride == other.m_stride
                    && m_width == other.m_width
                    && m_height == other.m_height
                    && m_winrtIndex == other.m_winrtIndex;
            }

            bool operator!=(WinrtLayoutIterator const& other) const noexcept
            {
                return !(*this == other);
            }

        private:
            Pixel* m_pixels{ nullptr };
            uint32_t m_stride{ 0 };
            uint32_t m_width{ 0 };
            uint32_t m_height{ 0 };
            size_t m_winrtIndex{ 0 };
        };

        ContinuousPixelBuffer(uint32_t stride, uint32_t width, uint32_t height) :
            m_stride(stride),
            m_width(width),
            m_height(height),
            m_pixels(static_cast<size_t>(width) * height)
        {
        }

        Pixel* data() noexcept
        {
            return m_pixels.data();
        }

        Pixel const* data() const noexcept
        {
            return m_pixels.data();
        }

        bool empty() const noexcept
        {
            return m_pixels.empty();
        }

        size_t size() const noexcept
        {
            return m_pixels.size();
        }

        size_t pixel_count() const noexcept
        {
            return m_pixels.size();
        }

        uint32_t stride() const noexcept
        {
            return m_stride;
        }

        uint32_t width() const noexcept
        {
            return m_width;
        }

        uint32_t height() const noexcept
        {
            return m_height;
        }

        size_t winrt_size() const noexcept
        {
            return static_cast<size_t>(m_stride) * m_height;
        }

        WinrtLayoutIterator winrt_begin() noexcept
        {
            return WinrtLayoutIterator(m_pixels.data(), m_stride, m_width, m_height, 0);
        }

        WinrtLayoutIterator winrt_end() noexcept
        {
            return WinrtLayoutIterator(m_pixels.data(), m_stride, m_width, m_height, winrt_size());
        }

        static float Clamp01(float value) noexcept
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        static float FromByte(uint8_t value) noexcept
        {
            return static_cast<float>(value) / 255.0f;
        }

        static uint8_t ToByte(float value) noexcept
        {
            return static_cast<uint8_t>(std::lround(Clamp01(value) * 255.0f));
        }

    private:
        const uint32_t m_stride;
        const uint32_t m_width;
        const uint32_t m_height;
        std::vector<Pixel> m_pixels;
    };
}