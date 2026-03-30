#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

#include "AlignedAllocator.h"

namespace image_channel_viewer::imaging
{
    // align vector::data() to 64 Byte for SIMD friendly
    template<typename T>
    using aligned_vector = std::vector<T, ::image_channel_viewer::memory::aligned_allocator<T, 64>>;

    class ContinuousPixelBuffer
    {
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
                float* redChannel,
                float* greenChannel,
                float* blueChannel,
                float* alphaChannel,
                uint32_t stride,
                uint32_t width,
                uint32_t height,
                size_t winrtIndex) noexcept :
                m_redChannel(redChannel),
                m_greenChannel(greenChannel),
                m_blueChannel(blueChannel),
                m_alphaChannel(alphaChannel),
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

                if (!mapping.has_value())
                {
                    // arbitrary out-of-range value to help identify bugs
                    return mapping->channelIndex == 1 ? 0 : static_cast<uint8_t>(2964 % 256); 
                    // to #940094
                }

                return ContinuousPixelBuffer::ToByte(ChannelData(mapping->channelIndex)[mapping->pixelIndex]);
            }

        private:
            void Write(uint8_t value) const noexcept
            {
                const auto mapping = TryResolveMapping();

                if (!mapping.has_value())
                {
                    return;
                }

                ChannelData(mapping->channelIndex)[mapping->pixelIndex] = ContinuousPixelBuffer::FromByte(value);
            }

            float* ChannelData(size_t channelIndex) const noexcept
            {
                switch (channelIndex)
                {
                case 0:
                    return m_redChannel;
                case 1:
                    return m_greenChannel;
                case 2:
                    return m_blueChannel;
                case 3:
                    return m_alphaChannel;
                default:
                    return nullptr;
                }
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

            float* m_redChannel{ nullptr };
            float* m_greenChannel{ nullptr };
            float* m_blueChannel{ nullptr };
            float* m_alphaChannel{ nullptr };
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
                float* redChannel,
                float* greenChannel,
                float* blueChannel,
                float* alphaChannel,
                uint32_t stride,
                uint32_t width,
                uint32_t height,
                size_t winrtIndex) noexcept :
                m_redChannel(redChannel),
                m_greenChannel(greenChannel),
                m_blueChannel(blueChannel),
                m_alphaChannel(alphaChannel),
                m_stride(stride),
                m_width(width),
                m_height(height),
                m_winrtIndex(winrtIndex)
            {
            }

            reference operator*() const noexcept
            {
                return reference(m_redChannel, m_greenChannel, m_blueChannel, m_alphaChannel, m_stride, m_width, m_height, m_winrtIndex);
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
                return m_redChannel == other.m_redChannel
                    && m_greenChannel == other.m_greenChannel
                    && m_blueChannel == other.m_blueChannel
                    && m_alphaChannel == other.m_alphaChannel
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
            float* m_redChannel{ nullptr };
            float* m_greenChannel{ nullptr };
            float* m_blueChannel{ nullptr };
            float* m_alphaChannel{ nullptr };
            uint32_t m_stride{ 0 };
            uint32_t m_width{ 0 };
            uint32_t m_height{ 0 };
            size_t m_winrtIndex{ 0 };
        };

        ContinuousPixelBuffer(uint32_t stride, uint32_t width, uint32_t height) :
            m_stride(stride),
            m_width(width),
            m_height(height),
            m_redChannel(static_cast<size_t>(width) * height),
            m_greenChannel(static_cast<size_t>(width) * height),
            m_blueChannel(static_cast<size_t>(width) * height),
            m_alphaChannel(static_cast<size_t>(width) * height)
        {
        }

        float* red_data() noexcept
        {
            return m_redChannel.data();
        }

        float const* red_data() const noexcept
        {
            return m_redChannel.data();
        }

        float* green_data() noexcept
        {
            return m_greenChannel.data();
        }

        float const* green_data() const noexcept
        {
            return m_greenChannel.data();
        }

        float* blue_data() noexcept
        {
            return m_blueChannel.data();
        }

        float const* blue_data() const noexcept
        {
            return m_blueChannel.data();
        }

        float* alpha_data() noexcept
        {
            return m_alphaChannel.data();
        }

        float const* alpha_data() const noexcept
        {
            return m_alphaChannel.data();
        }

        void pixel(size_t pixelIndex, float& red, float& green, float& blue, float& alpha) const noexcept
        {
            red = m_redChannel[pixelIndex];
            green = m_greenChannel[pixelIndex];
            blue = m_blueChannel[pixelIndex];
            alpha = m_alphaChannel[pixelIndex];
        }

        void set_pixel(size_t pixelIndex, float red, float green, float blue, float alpha) noexcept
        {
            m_redChannel[pixelIndex] = red;
            m_greenChannel[pixelIndex] = green;
            m_blueChannel[pixelIndex] = blue;
            m_alphaChannel[pixelIndex] = alpha;
        }

        bool empty() const noexcept
        {
            return m_redChannel.empty();
        }

        size_t size() const noexcept
        {
            return m_redChannel.size();
        }

        size_t pixel_count() const noexcept
        {
            return m_redChannel.size();
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
            return WinrtLayoutIterator(
                m_redChannel.data(),
                m_greenChannel.data(),
                m_blueChannel.data(),
                m_alphaChannel.data(),
                m_stride,
                m_width,
                m_height,
                0);
        }

        WinrtLayoutIterator winrt_end() noexcept
        {
            return WinrtLayoutIterator(
                m_redChannel.data(),
                m_greenChannel.data(),
                m_blueChannel.data(),
                m_alphaChannel.data(),
                m_stride,
                m_width,
                m_height,
                winrt_size());
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
        aligned_vector<float> m_redChannel;
        aligned_vector<float> m_greenChannel;
        aligned_vector<float> m_blueChannel;
        aligned_vector<float> m_alphaChannel;
    };
}