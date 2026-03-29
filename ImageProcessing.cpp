#include "pch.h"
#include "ImageProcessing.h"

namespace image_channel_viewer::image_processing
{
    winrt::Windows::Graphics::Imaging::SoftwareBitmap CreateSoftwareBitmapFromPixels(
        ::image_channel_viewer::ContinuousPixelBuffer& pixels,
        uint32_t pixelWidth,
        uint32_t pixelHeight)
    {
        winrt::Windows::Graphics::Imaging::SoftwareBitmap softwareBitmap(
            winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
            static_cast<int32_t>(pixelWidth),
            static_cast<int32_t>(pixelHeight),
            winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied);

        auto buffer = softwareBitmap.LockBuffer(winrt::Windows::Graphics::Imaging::BitmapBufferAccessMode::Write);
        auto reference = buffer.CreateReference();
        auto byteAccess = reference.as<IMemoryBufferByteAccess>();

        uint8_t* destination = nullptr;
        uint32_t capacity = 0;
        winrt::check_hresult(byteAccess->GetBuffer(&destination, &capacity));

        const auto plane = buffer.GetPlaneDescription(0);
        std::copy(pixels.winrt_begin(), pixels.winrt_end(), destination + plane.StartIndex);
        return softwareBitmap;
    }

    namespace detail
    {
        __forceinline float Clamp01(float value)
        {
            return ::image_channel_viewer::ContinuousPixelBuffer::Clamp01(value);
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

        __forceinline void HslToRgb(
            float hue,
            float saturation,
            float lightness,
            float& red,
            float& green,
            float& blue)
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

        __forceinline void HsvToRgb(
            float hue,
            float saturation,
            float value,
            float& red,
            float& green,
            float& blue)
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

        __forceinline void RgbToHsl(
            float red,
            float green,
            float blue,
            float& hue,
            float& saturation,
            float& lightness)
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

        __forceinline void RgbToHsv(
            float red,
            float green,
            float blue,
            float& hue,
            float& saturation,
            float& value)
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

        __forceinline void RgbToCmyk(
            float red,
            float green,
            float blue,
            float& cyan,
            float& magenta,
            float& yellow,
            float& black)
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

        __forceinline void RgbToLab(
            float red,
            float green,
            float blue,
            float& lightness,
            float& a,
            float& b)
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
}