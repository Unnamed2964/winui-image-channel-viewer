#pragma once

#include <unknwn.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>

#include "ContinuousPixelBuffer.h"

namespace image_channel_viewer::image_processing
{
    struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) IBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
    };

    struct __declspec(uuid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")) IMemoryBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
    };

    enum class ColorMode
    {
        Original,
        RGB,
        HSL,
        HSV,
        CMYK,
        LAB,
    };

    winrt::Windows::Graphics::Imaging::SoftwareBitmap CreateSoftwareBitmapFromPixels(
        ::image_channel_viewer::imaging::ContinuousPixelBuffer& pixels,
        uint32_t pixelWidth,
        uint32_t pixelHeight);

    namespace detail
    {
        // These helpers intentionally stay in the header so every RenderPixels/MapPixel template
        // instantiation can see the function bodies and honor __forceinline during optimization.
        __forceinline float Clamp01(float value)
        {
            return ::image_channel_viewer::imaging::ContinuousPixelBuffer::Clamp01(value);
        }

        __forceinline float SrgbToLinear(float value)
        {
            return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        __forceinline float HueToRgb(float p, float q, float t)
        {
            if (t < 0.0f)
            {
                t += 1.0f;
            }
            if (t > 1.0f)
            {
                t -= 1.0f;
            }
            if (t < 1.0f / 6.0f)
            {
                return p + (q - p) * 6.0f * t;
            }
            if (t < 1.0f / 2.0f)
            {
                return q;
            }
            if (t < 2.0f / 3.0f)
            {
                return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
            }
            return p;
        }

        __forceinline void HslToRgb(float hue, float saturation, float lightness, float& red, float& green, float& blue)
        {
            hue = std::fmod(hue, 360.0f);
            if (hue < 0.0f)
            {
                hue += 360.0f;
            }

            const float normalizedHue = hue / 360.0f;
            if (saturation <= 0.0f)
            {
                red = lightness;
                green = lightness;
                blue = lightness;
                return;
            }

            const float q = lightness < 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
            const float p = 2.0f * lightness - q;
            red = HueToRgb(p, q, normalizedHue + 1.0f / 3.0f);
            green = HueToRgb(p, q, normalizedHue);
            blue = HueToRgb(p, q, normalizedHue - 1.0f / 3.0f);
        }

        __forceinline void HsvToRgb(float hue, float saturation, float value, float& red, float& green, float& blue)
        {
            hue = std::fmod(hue, 360.0f);
            if (hue < 0.0f)
            {
                hue += 360.0f;
            }

            if (saturation <= 0.0f)
            {
                red = value;
                green = value;
                blue = value;
                return;
            }

            const float chroma = value * saturation;
            const float huePrime = hue / 60.0f;
            red = 0.0f;
            green = 0.0f;
            blue = 0.0f;

            if (huePrime < 1.0f)
            {
                red = chroma;
                green = chroma * huePrime;
            }
            else if (huePrime < 2.0f)
            {
                red = chroma * (2.0f - huePrime);
                green = chroma;
            }
            else if (huePrime < 3.0f)
            {
                green = chroma;
                blue = chroma * (huePrime - 2.0f);
            }
            else if (huePrime < 4.0f)
            {
                green = chroma * (4.0f - huePrime);
                blue = chroma;
            }
            else if (huePrime < 5.0f)
            {
                red = chroma * (huePrime - 4.0f);
                blue = chroma;
            }
            else
            {
                red = chroma;
                blue = chroma * (6.0f - huePrime);
            }

            const float match = value - chroma;
            red += match;
            green += match;
            blue += match;
        }

        __forceinline void RgbToHsl(float red, float green, float blue, float& hue, float& saturation, float& lightness)
        {
            const float maxValue = std::max({ red, green, blue });
            const float minValue = std::min({ red, green, blue });
            const float delta = maxValue - minValue;
            hue = 0.0f;
            saturation = 0.0f;
            lightness = (maxValue + minValue) * 0.5f;

            if (delta <= std::numeric_limits<float>::epsilon())
            {
                return;
            }

            saturation = lightness > 0.5f ? delta / (2.0f - maxValue - minValue) : delta / (maxValue + minValue);

            if (maxValue == red)
            {
                hue = 60.0f * ((green - blue) / delta);
            }
            else if (maxValue == green)
            {
                hue = 60.0f * (((blue - red) / delta) + 2.0f);
            }
            else
            {
                hue = 60.0f * (((red - green) / delta) + 4.0f);
            }

            if (hue < 0.0f)
            {
                hue += 360.0f;
            }
        }

        __forceinline void RgbToHsv(float red, float green, float blue, float& hue, float& saturation, float& value)
        {
            const float maxValue = std::max({ red, green, blue });
            const float minValue = std::min({ red, green, blue });
            const float delta = maxValue - minValue;

            hue = 0.0f;
            saturation = 0.0f;
            value = maxValue;

            if (maxValue <= std::numeric_limits<float>::epsilon())
            {
                return;
            }

            saturation = delta / maxValue;
            if (delta <= std::numeric_limits<float>::epsilon())
            {
                return;
            }

            if (maxValue == red)
            {
                hue = 60.0f * ((green - blue) / delta);
            }
            else if (maxValue == green)
            {
                hue = 60.0f * (((blue - red) / delta) + 2.0f);
            }
            else
            {
                hue = 60.0f * (((red - green) / delta) + 4.0f);
            }

            if (hue < 0.0f)
            {
                hue += 360.0f;
            }
        }

        __forceinline void RgbToCmyk(float red, float green, float blue, float& cyan, float& magenta, float& yellow, float& black)
        {
            black = 1.0f - std::max({ red, green, blue });
            if (black >= 1.0f - std::numeric_limits<float>::epsilon())
            {
                cyan = 0.0f;
                magenta = 0.0f;
                yellow = 0.0f;
                black = 1.0f;
                return;
            }

            const float denominator = 1.0f - black;
            cyan = (1.0f - red - black) / denominator;
            magenta = (1.0f - green - black) / denominator;
            yellow = (1.0f - blue - black) / denominator;
        }

        __forceinline float LabPivot(float value)
        {
            return value > 0.008856f ? std::cbrt(value) : (7.787f * value) + (16.0f / 116.0f);
        }

        __forceinline void RgbToLab(float red, float green, float blue, float& lightness, float& a, float& b)
        {
            const float linearRed = SrgbToLinear(red);
            const float linearGreen = SrgbToLinear(green);
            const float linearBlue = SrgbToLinear(blue);

            const float x = linearRed * 0.4124564f + linearGreen * 0.3575761f + linearBlue * 0.1804375f;
            const float y = linearRed * 0.2126729f + linearGreen * 0.7151522f + linearBlue * 0.0721750f;
            const float z = linearRed * 0.0193339f + linearGreen * 0.1191920f + linearBlue * 0.9503041f;

            const float fx = LabPivot(x / 0.95047f);
            const float fy = LabPivot(y / 1.0f);
            const float fz = LabPivot(z / 1.08883f);

            lightness = (116.0f * fy) - 16.0f;
            a = 500.0f * (fx - fy);
            b = 200.0f * (fy - fz);
        }

        __forceinline void ComposePixel(
            float red,
            float green,
            float blue,
            float alpha,
            float& mappedRed,
            float& mappedGreen,
            float& mappedBlue,
            float& mappedAlpha)
        {
            mappedRed = Clamp01(red);
            mappedGreen = Clamp01(green);
            mappedBlue = Clamp01(blue);
            mappedAlpha = Clamp01(alpha);
        }
    }

    template<ColorMode colorMode, uint32_t channelIndex, bool showGrayscale>
    __forceinline void MapPixel(
        float sourceRed,
        float sourceGreen,
        float sourceBlue,
        float sourceAlpha,
        float& mappedRed,
        float& mappedGreen,
        float& mappedBlue,
        float& mappedAlpha)
    {
        const float alpha = sourceAlpha;

        if constexpr (colorMode == ColorMode::Original)
        {
            static_assert(channelIndex == 0);
            mappedRed = sourceRed;
            mappedGreen = sourceGreen;
            mappedBlue = sourceBlue;
            mappedAlpha = sourceAlpha;
        }
        else if constexpr (colorMode == ColorMode::RGB)
        {
            static_assert(channelIndex < 3);

            float channelValue;

            if constexpr (channelIndex == 0)
            {
                channelValue = sourceRed;
            }
            else if constexpr (channelIndex == 1)
            {
                channelValue = sourceGreen;
            }
            else
            {
                channelValue = sourceBlue;
            }

            if constexpr (showGrayscale)
            {
                detail::ComposePixel(channelValue, channelValue, channelValue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                detail::ComposePixel(
                    channelIndex == 0 ? channelValue : 0.0f,
                    channelIndex == 1 ? channelValue : 0.0f,
                    channelIndex == 2 ? channelValue : 0.0f,
                    alpha,
                    mappedRed,
                    mappedGreen,
                    mappedBlue,
                    mappedAlpha);
            }
        }
        else if constexpr (colorMode == ColorMode::HSL)
        {
            static_assert(channelIndex < 3);

            float hue = 0.0f;
            float saturation = 0.0f;
            float lightness = 0.0f;
            detail::RgbToHsl(sourceRed, sourceGreen, sourceBlue, hue, saturation, lightness);
            if constexpr (channelIndex == 0)
            {
                float red = 0.0f;
                float green = 0.0f;
                float blue = 0.0f;
                detail::HslToRgb(hue, 1.0f, 0.5f, red, green, blue);
                detail::ComposePixel(red, green, blue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 1)
            {
                detail::ComposePixel(saturation, saturation, saturation, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                detail::ComposePixel(lightness, lightness, lightness, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
        }
        else if constexpr (colorMode == ColorMode::HSV)
        {
            static_assert(channelIndex < 3);

            float hue = 0.0f;
            float saturation = 0.0f;
            float value = 0.0f;
            detail::RgbToHsv(sourceRed, sourceGreen, sourceBlue, hue, saturation, value);
            if constexpr (channelIndex == 0)
            {
                float red = 0.0f;
                float green = 0.0f;
                float blue = 0.0f;
                detail::HsvToRgb(hue, 1.0f, 1.0f, red, green, blue);
                detail::ComposePixel(red, green, blue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 1)
            {
                detail::ComposePixel(saturation, saturation, saturation, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                detail::ComposePixel(value, value, value, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
        }
        else if constexpr (colorMode == ColorMode::CMYK)
        {
            static_assert(channelIndex < 4);

            float cyan = 0.0f;
            float magenta = 0.0f;
            float yellow = 0.0f;
            float black = 0.0f;
            detail::RgbToCmyk(sourceRed, sourceGreen, sourceBlue, cyan, magenta, yellow, black);
            float channelValue;
            if constexpr (channelIndex == 0)
            {
                channelValue = cyan;
            }
            else if constexpr (channelIndex == 1)
            {
                channelValue = magenta;
            }
            else if constexpr (channelIndex == 2)
            {
                channelValue = yellow;
            }
            else
            {
                channelValue = black;
            }

            if constexpr (showGrayscale || channelIndex == 3)
            {
                detail::ComposePixel(channelValue, channelValue, channelValue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 0)
            {
                detail::ComposePixel(0.0f, channelValue, channelValue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 1)
            {
                detail::ComposePixel(channelValue, 0.0f, channelValue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                detail::ComposePixel(channelValue, channelValue, 0.0f, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
        }
        else
        {
            static_assert(colorMode == ColorMode::LAB);
            static_assert(channelIndex < 3);

            float lightness = 0.0f;
            float a = 0.0f;
            float b = 0.0f;
            detail::RgbToLab(sourceRed, sourceGreen, sourceBlue, lightness, a, b);
            if constexpr (channelIndex == 0)
            {
                const float mappedLightness = detail::Clamp01(lightness / 100.0f);
                detail::ComposePixel(mappedLightness, mappedLightness, mappedLightness, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 1)
            {
                const float value = detail::Clamp01((a + 128.0f) / 255.0f);
                detail::ComposePixel(value, value, value, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                const float value = detail::Clamp01((b + 128.0f) / 255.0f);
                detail::ComposePixel(value, value, value, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
        }
    }

    template<ColorMode colorMode, uint32_t mappedChannelIndex, bool showGrayscale>
    __forceinline void RenderPreviewWork(
        float const* __restrict sourceRed,
        float const* __restrict sourceGreen,
        float const* __restrict sourceBlue,
        float const* __restrict sourceAlpha,
        float* __restrict previewRed,
        float* __restrict previewGreen,
        float* __restrict previewBlue,
        float* __restrict previewAlpha,
        size_t beginPixelIndex,
        size_t endPixelIndex)
    {
        for (size_t pixelIndex = beginPixelIndex; pixelIndex < endPixelIndex; ++pixelIndex)
        {
            const float red = sourceRed[pixelIndex];
            const float green = sourceGreen[pixelIndex];
            const float blue = sourceBlue[pixelIndex];
            const float alpha = sourceAlpha[pixelIndex];

            float mappedRed = 0.0f;
            float mappedGreen = 0.0f;
            float mappedBlue = 0.0f;
            float mappedAlpha = 0.0f;
            MapPixel<colorMode, mappedChannelIndex, showGrayscale>(
                red,
                green,
                blue,
                alpha,
                mappedRed,
                mappedGreen,
                mappedBlue,
                mappedAlpha);

            previewRed[pixelIndex] = mappedRed;
            previewGreen[pixelIndex] = mappedGreen;
            previewBlue[pixelIndex] = mappedBlue;
            previewAlpha[pixelIndex] = mappedAlpha;
        }
    }

    template<typename ProgressCallback>
    ::image_channel_viewer::imaging::ContinuousPixelBuffer RenderPixels(
        ::image_channel_viewer::imaging::ContinuousPixelBuffer const& sourcePixels,
        uint32_t pixelWidth,
        uint32_t pixelHeight,
        ColorMode selectedMode,
        uint32_t channelIndex,
        bool showGrayscale,
        ProgressCallback&& reportProgress)
    {
        ::image_channel_viewer::imaging::ContinuousPixelBuffer previewPixels(pixelWidth * 4, pixelWidth, pixelHeight);
        auto const* sourceRed = sourcePixels.red_data();
        auto const* sourceGreen = sourcePixels.green_data();
        auto const* sourceBlue = sourcePixels.blue_data();
        auto const* sourceAlpha = sourcePixels.alpha_data();
        auto* previewRed = previewPixels.red_data();
        auto* previewGreen = previewPixels.green_data();
        auto* previewBlue = previewPixels.blue_data();
        auto* previewAlpha = previewPixels.alpha_data();
        const size_t pixelCount = sourcePixels.pixel_count();

        constexpr size_t progressChunkSize = 65536;
        uint32_t lastReportedProgress = 0;
        auto renderChunk = [&](auto renderer)
            {
                for (size_t beginPixelIndex = 0; beginPixelIndex < pixelCount; beginPixelIndex += progressChunkSize)
                {
                    const size_t endPixelIndex = std::min(beginPixelIndex + progressChunkSize, pixelCount);
                    renderer(beginPixelIndex, endPixelIndex);

                    const uint32_t progress = static_cast<uint32_t>((endPixelIndex * 100) / pixelCount);
                    if (progress != lastReportedProgress)
                    {
                        lastReportedProgress = progress;
                        reportProgress(progress);
                    }
                }
            };

#define RENDER_TEMPLATE_INSTANCE(modeValue, channelValue, grayscaleValue) \
        renderChunk([&](size_t beginPixelIndex, size_t endPixelIndex) \
            { \
                RenderPreviewWork<modeValue, channelValue, grayscaleValue>( \
                    sourceRed, \
                    sourceGreen, \
                    sourceBlue, \
                    sourceAlpha, \
                    previewRed, \
                    previewGreen, \
                    previewBlue, \
                    previewAlpha, \
                    beginPixelIndex, \
                    endPixelIndex); \
            })

#define RENDER_GRAYSCALE_VARIANT(modeValue, channelValue) \
        if (showGrayscale) \
        { \
            RENDER_TEMPLATE_INSTANCE(modeValue, channelValue, true); \
        } \
        else \
        { \
            RENDER_TEMPLATE_INSTANCE(modeValue, channelValue, false); \
        }

        switch (selectedMode)
        {
        case ColorMode::Original:
            RENDER_GRAYSCALE_VARIANT(ColorMode::Original, 0);
            break;

        case ColorMode::RGB:
            switch (channelIndex)
            {
            case 0:
                RENDER_GRAYSCALE_VARIANT(ColorMode::RGB, 0);
                break;
            case 1:
                RENDER_GRAYSCALE_VARIANT(ColorMode::RGB, 1);
                break;
            default:
                RENDER_GRAYSCALE_VARIANT(ColorMode::RGB, 2);
                break;
            }
            break;

        case ColorMode::HSL:
            switch (channelIndex)
            {
            case 0:
                RENDER_GRAYSCALE_VARIANT(ColorMode::HSL, 0);
                break;
            case 1:
                RENDER_GRAYSCALE_VARIANT(ColorMode::HSL, 1);
                break;
            default:
                RENDER_GRAYSCALE_VARIANT(ColorMode::HSL, 2);
                break;
            }
            break;

        case ColorMode::HSV:
            switch (channelIndex)
            {
            case 0:
                RENDER_GRAYSCALE_VARIANT(ColorMode::HSV, 0);
                break;
            case 1:
                RENDER_GRAYSCALE_VARIANT(ColorMode::HSV, 1);
                break;
            default:
                RENDER_GRAYSCALE_VARIANT(ColorMode::HSV, 2);
                break;
            }
            break;

        case ColorMode::CMYK:
            switch (channelIndex)
            {
            case 0:
                RENDER_GRAYSCALE_VARIANT(ColorMode::CMYK, 0);
                break;
            case 1:
                RENDER_GRAYSCALE_VARIANT(ColorMode::CMYK, 1);
                break;
            case 2:
                RENDER_GRAYSCALE_VARIANT(ColorMode::CMYK, 2);
                break;
            default:
                RENDER_GRAYSCALE_VARIANT(ColorMode::CMYK, 3);
                break;
            }
            break;

        case ColorMode::LAB:
            switch (channelIndex)
            {
            case 0:
                RENDER_GRAYSCALE_VARIANT(ColorMode::LAB, 0);
                break;
            case 1:
                RENDER_GRAYSCALE_VARIANT(ColorMode::LAB, 1);
                break;
            default:
                RENDER_GRAYSCALE_VARIANT(ColorMode::LAB, 2);
                break;
            }
            break;
        }

#undef RENDER_GRAYSCALE_VARIANT
#undef RENDER_TEMPLATE_INSTANCE

        return previewPixels;
    }
}