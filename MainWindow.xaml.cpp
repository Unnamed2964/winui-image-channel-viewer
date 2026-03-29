#include "pch.h"
#include "ContinuousPixelBuffer.h"
#include "MainWindow.xaml.h"
#include "ConfigStore.h"
#include "AppVersion.g.h"
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <shlwapi.h>


#include <filesystem>
#include <algorithm>
#include <ranges>

#pragma comment(lib, "shlwapi.lib")

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace
{
    using BitmapEncoder = winrt::Windows::Graphics::Imaging::BitmapEncoder;
    using ResourceLoader = winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceLoader;
    using ResourceManager = winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceManager;
    using ResourceMap = winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceMap;
    using KnownResourceQualifierName = winrt::Microsoft::Windows::ApplicationModel::Resources::KnownResourceQualifierName;

    struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) IBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
    };

    struct __declspec(uuid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")) IMemoryBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
    };

    using ColorMode = winrt::image_channel_viewer::implementation::ColorMode;

    ResourceManager& AppResourceManager()
    {
        static ResourceManager manager{ ResourceLoader::GetDefaultResourceFilePath() };
        return manager;
    }

    ResourceMap& AppResourceMap()
    {
        static ResourceMap resourceMap = AppResourceManager().MainResourceMap().GetSubtree(L"Resources");
        return resourceMap;
    }

    bool IsSupportedLanguageTag(std::wstring_view languageTag)
    {
        return languageTag == L"zh-CN" || languageTag == L"en-US";
    }

    hstring EffectiveLanguageOverrideTag()
    {
        auto const storedLanguage = ::image_channel_viewer::LoadConfiguredLanguagePreference();
        if (!storedLanguage.empty())
        {
            return IsSupportedLanguageTag(storedLanguage.c_str()) ? storedLanguage : hstring{};
        }

        return {};
    }

    winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext CreateResourceContext()
    {
        auto context = AppResourceManager().CreateResourceContext();
        auto const languageTag = EffectiveLanguageOverrideTag();
        if (!languageTag.empty())
        {
            context.QualifierValues().Insert(KnownResourceQualifierName::Language(), languageTag);
        }

        return context;
    }

    hstring LocalizedString(std::wstring_view resourceId)
    {
        try
        {
            std::wstring normalizedResourceId{ resourceId };
            std::replace(normalizedResourceId.begin(), normalizedResourceId.end(), L'.', L'/');

            auto const context = CreateResourceContext();
            auto const candidate = AppResourceMap().TryGetValue(hstring{ normalizedResourceId }, context);
            if (candidate)
            {
                hstring const value = candidate.ValueAsString();
                if (!value.empty())
                {
                    return value;
                }
            }

            return hstring{ resourceId };
        }
        catch (...)
        {
            return hstring{ resourceId };
        }
    }

    hstring FormatLocalizedString(std::wstring_view resourceId, std::initializer_list<hstring> arguments)
    {
        // std::v_format is intentionally not used because it may introduce unexpected
        // behaviors and potential attack surfaces with an externally constructed format 
        // strings and an ill-written custom global std::formatter.
        std::wstring formatted = LocalizedString(resourceId).c_str();
        size_t argumentIndex = 0;

        for (auto const& argument : arguments)
        {
            std::wstring const token = L"{" + std::to_wstring(argumentIndex) + L"}";
            size_t searchIndex = 0;

            while ((searchIndex = formatted.find(token, searchIndex)) != std::wstring::npos)
            {
                formatted.replace(searchIndex, token.size(), argument.c_str());
                searchIndex += argument.size();
            }

            ++argumentIndex;
        }

        return hstring{ formatted };
    }

    hstring StoredLanguagePreference()
    {
        return ::image_channel_viewer::LoadConfiguredLanguagePreference();
    }

    void StoreLanguagePreference(hstring const& languageTag)
    {
        ::image_channel_viewer::SaveConfiguredLanguagePreference(languageTag);
    }

    Controls::ComboBoxItem CreateLanguageOption(hstring const& label, hstring const& tag)
    {
        Controls::ComboBoxItem item;
        item.Content(box_value(label));
        item.Tag(box_value(tag));
        return item;
    }
    
    struct SaveFormatDefinition {
        wchar_t const* labelResourceId;
        winrt::guid (*encoderIdFactory)();
        std::vector<wchar_t const*> extensions; // extensions[0] is the primary
    };

    const SaveFormatDefinition kSaveFormats[] = {
        { L"SaveDialog.FileType.Png", &BitmapEncoder::PngEncoderId, { L".png" } },
        { L"SaveDialog.FileType.Jpeg", &BitmapEncoder::JpegEncoderId, { L".jpg", L".jpeg" } },
        { L"SaveDialog.FileType.Bmp", &BitmapEncoder::BmpEncoderId, { L".bmp" } },
        { L"SaveDialog.FileType.Tiff", &BitmapEncoder::TiffEncoderId, { L".tif", L".tiff" } },
        { L"SaveDialog.FileType.Gif", &BitmapEncoder::GifEncoderId, { L".gif" } },
    };

    std::wstring ToLowerCopy(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), ::towlower);
        return value;
    }

    std::optional<size_t> SaveFormatIndexForExtension(std::wstring const& extension)
    {
        auto const normalized = ToLowerCopy(extension);
        for (size_t index = 0; index < std::size(kSaveFormats); ++index)
        {
            if (std::ranges::contains(kSaveFormats[index].extensions, normalized))
            {
                return index;
            }
        }
        return std::nullopt;
    }

    winrt::guid EncoderIdForExtension(std::wstring const& extension)
    {
        auto const saveFormatIndex = SaveFormatIndexForExtension(extension);
        return kSaveFormats[saveFormatIndex.value_or(0)].encoderIdFactory();
    }

    winrt::Windows::Foundation::Collections::IVector<hstring> SavePickerExtensions(const std::vector<wchar_t const*>& exts)
    {
        std::vector<hstring> result;
        for (auto ext : exts) {
            result.emplace_back(ext);
        }
        return winrt::single_threaded_vector(std::move(result));
    }

    std::wstring SanitizeFileComponent(std::wstring value)
    {
        static constexpr std::array invalidCharacters{ L'\\', L'/', L':', L'*', L'?', L'"', L'<', L'>', L'|' };

        std::replace_if(value.begin(), value.end(), [](wchar_t character)
            {
                return std::ranges::find(invalidCharacters, character) != invalidCharacters.end();
            }, L'_');

        while (!value.empty() && (value.back() == L' ' || value.back() == L'.'))
        {
            value.pop_back();
        }

        if (value.empty())
        {
            value = L"image";
        }

        return value;
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> PickSaveFileAsync(
        HWND windowHandle,
        std::wstring const& suggestedFileName,
        uint32_t defaultTypeIndex)
    {
        winrt::Windows::Storage::Pickers::FileSavePicker picker;
        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);

        for (size_t index = 0; index < std::size(kSaveFormats); ++index)
        {
            picker.FileTypeChoices().Insert(
                LocalizedString(kSaveFormats[index].labelResourceId),
                SavePickerExtensions(kSaveFormats[index].extensions));
        }

        picker.SettingsIdentifier(L"SaveCurrentView");
        picker.SuggestedFileName(suggestedFileName);
        // defaultTypeIndex is 1-based
        picker.DefaultFileExtension(hstring{ kSaveFormats[defaultTypeIndex - 1].extensions[0] });

        auto initializeWithWindow = picker.as<IInitializeWithWindow>();
        check_hresult(initializeWithWindow->Initialize(windowHandle));

        co_return co_await picker.PickSaveFileAsync();
    }

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
        check_hresult(byteAccess->GetBuffer(&destination, &capacity));

        const auto plane = buffer.GetPlaneDescription(0);
        std::copy(pixels.winrt_begin(), pixels.winrt_end(), destination + plane.StartIndex);
        return softwareBitmap;
    }

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
                ComposePixel(channelValue, channelValue, channelValue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
                return;
            }

            ComposePixel(
                channelIndex == 0 ? channelValue : 0.0f,
                channelIndex == 1 ? channelValue : 0.0f,
                channelIndex == 2 ? channelValue : 0.0f,
                alpha,
                mappedRed,
                mappedGreen,
                mappedBlue,
                mappedAlpha);
        }
        else if constexpr (colorMode == ColorMode::HSL)
        {
            static_assert(channelIndex < 3);

            float hue = 0.0f;
            float saturation = 0.0f;
            float lightness = 0.0f;
            RgbToHsl(sourceRed, sourceGreen, sourceBlue, hue, saturation, lightness);
            if constexpr (channelIndex == 0)
            {
                float red = 0.0f;
                float green = 0.0f;
                float blue = 0.0f;
                HslToRgb(hue, 1.0f, 0.5f, red, green, blue);
                ComposePixel(red, green, blue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 1)
            {
                ComposePixel(saturation, saturation, saturation, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                ComposePixel(lightness, lightness, lightness, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
        }
        else if constexpr (colorMode == ColorMode::HSV)
        {
            static_assert(channelIndex < 3);

            float hue = 0.0f;
            float saturation = 0.0f;
            float value = 0.0f;
            RgbToHsv(sourceRed, sourceGreen, sourceBlue, hue, saturation, value);
            if constexpr (channelIndex == 0)
            {
                float red = 0.0f;
                float green = 0.0f;
                float blue = 0.0f;
                HsvToRgb(hue, 1.0f, 1.0f, red, green, blue);
                ComposePixel(red, green, blue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 1)
            {
                ComposePixel(saturation, saturation, saturation, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                ComposePixel(value, value, value, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
        }
        else if constexpr (colorMode == ColorMode::CMYK)
        {
            static_assert(channelIndex < 4);

            float cyan = 0.0f;
            float magenta = 0.0f;
            float yellow = 0.0f;
            float black = 0.0f;
            RgbToCmyk(sourceRed, sourceGreen, sourceBlue, cyan, magenta, yellow, black);
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
                ComposePixel(channelValue, channelValue, channelValue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
                return;
            }

            if constexpr (channelIndex == 0)
            {
                ComposePixel(0.0f, channelValue, channelValue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 1)
            {
                ComposePixel(channelValue, 0.0f, channelValue, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                ComposePixel(channelValue, channelValue, 0.0f, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
        }
        else
        {
            static_assert(colorMode == ColorMode::LAB);
            static_assert(channelIndex < 3);

            float lightness = 0.0f;
            float a = 0.0f;
            float b = 0.0f;
            RgbToLab(sourceRed, sourceGreen, sourceBlue, lightness, a, b);
            if constexpr (channelIndex == 0)
            {
                const float mappedLightness = Clamp01(lightness / 100.0f);
                ComposePixel(mappedLightness, mappedLightness, mappedLightness, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else if constexpr (channelIndex == 1)
            {
                const float value = Clamp01((a + 128.0f) / 255.0f);
                ComposePixel(value, value, value, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
            }
            else
            {
                const float value = Clamp01((b + 128.0f) / 255.0f);
                ComposePixel(value, value, value, alpha, mappedRed, mappedGreen, mappedBlue, mappedAlpha);
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
    ::image_channel_viewer::ContinuousPixelBuffer RenderPixels(
        ::image_channel_viewer::ContinuousPixelBuffer const& sourcePixels,
        uint32_t pixelWidth,
        uint32_t pixelHeight,
        ColorMode selectedMode,
        uint32_t channelIndex,
        bool showGrayscale,
        ProgressCallback&& reportProgress)
    {
        ::image_channel_viewer::ContinuousPixelBuffer previewPixels(pixelWidth * 4, pixelWidth, pixelHeight);
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

namespace winrt::image_channel_viewer::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        AppWindow().TitleBar().PreferredTheme(Microsoft::UI::Windowing::TitleBarTheme::UseDefaultAppMode);
        InitializeModes();
        Populatechannels();
        Title(LocalizedString(L"Window.Title.AppName"));
    }

    winrt::hstring MainWindow::LocalizedString(winrt::hstring const& resourceId)
    {
        return ::LocalizedString(std::wstring_view{ resourceId.c_str() });
    }

    void MainWindow::OnOpenImageClick(
        [[maybe_unused]] IInspectable const& sender, 
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        LoadImageAsync();
    }

    void MainWindow::OnSettingsClick(
        [[maybe_unused]] IInspectable const& sender,
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        ShowSettingsDialogAsync();
    }

    void MainWindow::OnSaveAsClick(
        [[maybe_unused]] IInspectable const& sender,
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        if (!m_isSaving)
        {
            SaveCurrentViewAsync();
        }
    }

    void MainWindow::OnAboutClick(
        [[maybe_unused]] IInspectable const& sender,
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        ShowAboutDialogAsync();
    }

    void MainWindow::OnColorModeItemClick(
        [[maybe_unused]] IInspectable const& sender, 
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        if (m_isUpdatingUi)
        {
            return;
        }

        auto menuItem = sender.try_as<Controls::MenuFlyoutItem>();
        if (!menuItem)
        {
            return;
        }

        m_selectedModeIndex = unbox_value<uint32_t>(menuItem.Tag());
        ColorModeAppBarButton().Label(m_modes.at(m_selectedModeIndex).label);
        Populatechannels();
        RefreshPreview();
    }

    void MainWindow::OnchannelItemClick(
        [[maybe_unused]] IInspectable const& sender, 
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        if (m_isUpdatingUi)
        {
            return;
        }

        auto menuItem = sender.try_as<Controls::MenuFlyoutItem>();
        if (!menuItem)
        {
            return;
        }

        m_selectedChannelIndex = unbox_value<uint32_t>(menuItem.Tag());
        ChannelAppBarButton().Label(menuItem.Text());
        RefreshPreview();
    }

    void MainWindow::OnGrayscaleItemClick(
        [[maybe_unused]] IInspectable const& sender, 
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        if (m_isUpdatingUi)
        {
            return;
        }

        auto menuItem = sender.try_as<Controls::RadioMenuFlyoutItem>();
        if (!menuItem)
        {
            return;
        }

        m_showGrayscale = menuItem == GrayscaleDisplayMenuItem();
        UpdateGrayscaleControls(GrayscaleAppBarButton().IsEnabled());
        RefreshPreview();
    }

    void MainWindow::OnPreviewViewChanged(
        [[maybe_unused]] IInspectable const& sender, 
        [[maybe_unused]] Controls::ScrollViewerViewChangedEventArgs const& args)
    {
        m_savedHorizontalOffset = static_cast<float>(PreviewScrollViewer().HorizontalOffset());
        m_savedVerticalOffset = static_cast<float>(PreviewScrollViewer().VerticalOffset());
        m_savedZoomFactor = PreviewScrollViewer().ZoomFactor();
    }

    Windows::Foundation::IAsyncAction MainWindow::LoadImageAsync()
    {
        Windows::Storage::Pickers::FileOpenPicker picker;
        picker.ViewMode(Windows::Storage::Pickers::PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);
        for(auto extension : { L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".tif", L".tiff", L".webp" })
        {
            picker.FileTypeFilter().Append(extension);
        }

        auto initializeWithWindow = picker.as<IInitializeWithWindow>();
        check_hresult(initializeWithWindow->Initialize(WindowHandle()));

        auto file = co_await picker.PickSingleFileAsync();
        if (!file)
        {
            co_return;
        }

        auto stream = co_await file.OpenAsync(Windows::Storage::FileAccessMode::Read);
        auto decoder = co_await Windows::Graphics::Imaging::BitmapDecoder::CreateAsync(stream);
        auto decodedBitmap = co_await decoder.GetSoftwareBitmapAsync();

        m_sourceBitmap = Windows::Graphics::Imaging::SoftwareBitmap::Convert(
            decodedBitmap,
            Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
            Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied);

        m_pixelWidth = static_cast<uint32_t>(m_sourceBitmap.PixelWidth());
        m_pixelHeight = static_cast<uint32_t>(m_sourceBitmap.PixelHeight());
        m_loadedFile = file;
        m_loadedFileName = file.Name();

        auto buffer = m_sourceBitmap.LockBuffer(Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);
        auto reference = buffer.CreateReference();
        auto byteAccess = reference.as<IMemoryBufferByteAccess>();

        uint8_t* sourceData = nullptr;
        uint32_t capacity = 0;
        check_hresult(byteAccess->GetBuffer(&sourceData, &capacity));

        const auto plane = buffer.GetPlaneDescription(0);
        m_stride = static_cast<uint32_t>(plane.Stride);
        m_sourcePixels.emplace(m_stride, m_pixelWidth, m_pixelHeight);
        std::copy_n(sourceData + plane.StartIndex, m_sourcePixels->winrt_size(), m_sourcePixels->winrt_begin());

        m_fitPreviewOnNextRefresh = true;
        m_savedHorizontalOffset = 0.0f;
        m_savedVerticalOffset = 0.0f;
        SaveResultInfoBar().IsOpen(false);
        EmptyStatePanel().Visibility(Visibility::Collapsed);
        UpdateCommandStates();
        RefreshPreview();
    }

    Windows::Foundation::IAsyncAction MainWindow::SaveCurrentViewAsync()
    {
        auto lifetime = get_strong();
        auto uiThread = winrt::apartment_context();
        auto dispatcherQueue = DispatcherQueue();

        if (m_isSaving || !m_sourcePixels.has_value() || m_sourcePixels->empty() || m_pixelWidth == 0 || m_pixelHeight == 0)
        {
            co_return;
        }

        const auto selectedMode = SelectedMode().value_or(ColorMode::Original);
        const auto channelIndex = SelectedchannelIndex().value_or(0);
        const bool showGrayscale = m_showGrayscale;
        const uint32_t pixelWidth = m_pixelWidth;
        const uint32_t pixelHeight = m_pixelHeight;
        auto sourcePixels = *m_sourcePixels;

        std::wstring sourceExtension;
        if (m_loadedFile)
        {
            const std::filesystem::path sourcePath{ m_loadedFile.Path().c_str() };
            sourceExtension = sourcePath.extension().wstring();
        }
        else if (!m_loadedFileName.empty())
        {
            sourceExtension = std::filesystem::path{ m_loadedFileName.c_str() }.extension().wstring();
        }

        auto saveFormatIndex = SaveFormatIndexForExtension(sourceExtension);
        if (!saveFormatIndex.has_value())
        {
            sourceExtension = L".png";
            saveFormatIndex = 0;
        }

        auto const outputFile = co_await PickSaveFileAsync(
            WindowHandle(),
            BuildSuggestedSaveFileName().c_str(),
            static_cast<uint32_t>(saveFormatIndex.value() + 1));

        if (!outputFile)
        {
            co_return;
        }

        const uint64_t requestId = ++m_saveRequestId;
        m_isSaving = true;
        UpdateCommandStates();
        SaveResultInfoBar().IsOpen(false);
        PreviewProgressBar().IsIndeterminate(false);
        PreviewProgressBar().Value(0.0);
        PreviewProgressHost().Visibility(Visibility::Visible);

        Controls::InfoBarSeverity resultSeverity = Controls::InfoBarSeverity::Success;
        hstring resultTitle = LocalizedString(L"SaveResult.SuccessTitle");
        hstring resultMessage;
        bool saveSucceeded = false;

        try
        {
            const std::filesystem::path outputPath{ outputFile.Path().c_str() };
            const winrt::guid encoderId = EncoderIdForExtension(outputPath.extension().wstring());

            co_await winrt::resume_background();

            auto weakThis = get_weak();
            auto reportProgress = [&](uint32_t progress)
                {
                    const double scaledProgress = static_cast<double>(progress) * 0.85;
                    dispatcherQueue.TryEnqueue([weakThis, requestId, scaledProgress]()
                        {
                            if (auto self = weakThis.get())
                            {
                                if (requestId == self->m_saveRequestId)
                                {
                                    self->PreviewProgressBar().Value(scaledProgress);
                                }
                            }
                        });
                };

            auto renderedPixels = RenderPixels(
                sourcePixels,
                pixelWidth,
                pixelHeight,
                selectedMode,
                channelIndex,
                showGrayscale,
                reportProgress);

            auto softwareBitmap = CreateSoftwareBitmapFromPixels(renderedPixels, pixelWidth, pixelHeight);

            dispatcherQueue.TryEnqueue([weakThis, requestId]()
                {
                    if (auto self = weakThis.get())
                    {
                        if (requestId == self->m_saveRequestId)
                        {
                            self->PreviewProgressBar().Value(90.0);
                        }
                    }
                });

            auto stream = co_await outputFile.OpenAsync(Windows::Storage::FileAccessMode::ReadWrite);
            auto encoder = co_await Windows::Graphics::Imaging::BitmapEncoder::CreateAsync(encoderId, stream);
            encoder.SetSoftwareBitmap(softwareBitmap);

            dispatcherQueue.TryEnqueue([weakThis, requestId]()
                {
                    if (auto self = weakThis.get())
                    {
                        if (requestId == self->m_saveRequestId)
                        {
                            self->PreviewProgressBar().Value(95.0);
                        }
                    }
                });

            co_await encoder.FlushAsync();
            co_await stream.FlushAsync();

            saveSucceeded = true;
            resultMessage = FormatLocalizedString(L"SaveResult.SuccessMessageFormat", { outputFile.Path() });
        }
        catch (winrt::hresult_error const& error)
        {
            resultSeverity = Controls::InfoBarSeverity::Error;
            resultTitle = LocalizedString(L"SaveResult.FailureTitle");
            hstring message = error.message();
            if (message.empty())
            {
                message = LocalizedString(L"SaveResult.FailureMessage");
            }
            resultMessage = message;
        }
        catch (...)
        {
            resultSeverity = Controls::InfoBarSeverity::Error;
            resultTitle = LocalizedString(L"SaveResult.FailureTitle");
            resultMessage = LocalizedString(L"SaveResult.FailureMessage");
        }

        co_await uiThread;

        if (requestId != m_saveRequestId)
        {
            co_return;
        }

        if (saveSucceeded)
        {
            PreviewProgressBar().Value(100.0);
        }

        PreviewProgressHost().Visibility(Visibility::Collapsed);
        m_isSaving = false;
        UpdateCommandStates();
        ShowSaveResultInfoBar(resultSeverity, resultTitle, resultMessage);
    }

    Windows::Foundation::IAsyncAction MainWindow::ShowAboutDialogAsync()
    {
        Controls::ContentDialog dialog;
        dialog.Title(box_value(FormatLocalizedString(L"About.DialogTitleFormat", { LocalizedString(L"Window.Title.AppName") })));
        dialog.PrimaryButtonText(LocalizedString(L"Common.Close"));
        dialog.DefaultButton(Controls::ContentDialogButton::Primary);
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Microsoft::UI::ColorHelper::FromArgb(0xFF, 0x91, 0xD4, 0xE4)));

        Microsoft::UI::Xaml::Controls::StackPanel contentPanel;
        contentPanel.Spacing(12);

        Microsoft::UI::Xaml::Controls::TextBlock versionText;
        versionText.Text(FormatLocalizedString(L"About.VersionFormat", { hstring{ AppVersion } }));
        versionText.TextWrapping(TextWrapping::WrapWholeWords);
        contentPanel.Children().Append(versionText);

        Microsoft::UI::Xaml::Controls::RichTextBlock authorText;
        authorText.TextWrapping(TextWrapping::WrapWholeWords);

        Microsoft::UI::Xaml::Documents::Paragraph authorParagraph;

        Microsoft::UI::Xaml::Documents::Run authorPrefix;
        authorPrefix.Text(LocalizedString(L"About.AuthorPrefix"));
        authorParagraph.Inlines().Append(authorPrefix);

        Microsoft::UI::Xaml::Documents::Hyperlink mitLicenseLink;
        mitLicenseLink.NavigateUri(Windows::Foundation::Uri(L"https://github.com/Unnamed2964/winui-image-channel-viewer/blob/master/LICENSE"));

        Microsoft::UI::Xaml::Documents::Run mitLicenseRun;
        mitLicenseRun.Text(LocalizedString(L"About.MitLicense"));
        mitLicenseLink.Inlines().Append(mitLicenseRun);
        authorParagraph.Inlines().Append(mitLicenseLink);

        Microsoft::UI::Xaml::Documents::Run authorSuffix;
        authorSuffix.Text(LocalizedString(L"About.AuthorSuffix"));
        authorParagraph.Inlines().Append(authorSuffix);

        authorText.Blocks().Append(authorParagraph);
        contentPanel.Children().Append(authorText);

        Microsoft::UI::Xaml::Controls::StackPanel linksPanel;

        Microsoft::UI::Xaml::Controls::HyperlinkButton githubLink;
        githubLink.Content(box_value(LocalizedString(L"About.GitHub")));
        githubLink.NavigateUri(Windows::Foundation::Uri(L"https://github.com/Unnamed2964"));
        githubLink.HorizontalAlignment(HorizontalAlignment::Left);
        linksPanel.Children().Append(githubLink);

        Microsoft::UI::Xaml::Controls::HyperlinkButton websiteLink;
        websiteLink.Content(box_value(LocalizedString(L"About.Website")));
        websiteLink.NavigateUri(Windows::Foundation::Uri(L"https://umamichi.moe"));
        websiteLink.HorizontalAlignment(HorizontalAlignment::Left);
        linksPanel.Children().Append(websiteLink);

        contentPanel.Children().Append(linksPanel);

        dialog.Content(contentPanel);

        co_await dialog.ShowAsync();
    }

    Windows::Foundation::IAsyncAction MainWindow::ShowSettingsDialogAsync()
    {
        Controls::ContentDialog dialog;
        dialog.Title(box_value(LocalizedString(L"Settings.DialogTitle")));
        dialog.PrimaryButtonText(LocalizedString(L"Common.Save"));
        dialog.CloseButtonText(LocalizedString(L"Common.Cancel"));
        dialog.DefaultButton(Controls::ContentDialogButton::Primary);
        dialog.XamlRoot(Content().XamlRoot());

        Controls::StackPanel contentPanel;
        contentPanel.Spacing(12);

        Controls::TextBlock descriptionText;
        descriptionText.Text(LocalizedString(L"Settings.DialogDescription"));
        descriptionText.TextWrapping(TextWrapping::WrapWholeWords);
        contentPanel.Children().Append(descriptionText);

        Controls::TextBlock languageLabel;
        languageLabel.Text(LocalizedString(L"Settings.Language.Label"));
        contentPanel.Children().Append(languageLabel);

        Controls::ComboBox languageComboBox;
        languageComboBox.MinWidth(240.0);
        languageComboBox.Items().Append(CreateLanguageOption(LocalizedString(L"Settings.Language.System"), hstring{}));
        languageComboBox.Items().Append(CreateLanguageOption(LocalizedString(L"Settings.Language.ZhCN"), hstring{ L"zh-CN" }));
        languageComboBox.Items().Append(CreateLanguageOption(LocalizedString(L"Settings.Language.EnUS"), hstring{ L"en-US" }));

        auto const storedLanguage = StoredLanguagePreference();
        uint32_t selectedIndex = 0;
        if (storedLanguage == L"zh-CN")
        {
            selectedIndex = 1;
        }
        else if (storedLanguage == L"en-US")
        {
            selectedIndex = 2;
        }
        languageComboBox.SelectedIndex(selectedIndex);
        contentPanel.Children().Append(languageComboBox);

        Controls::TextBlock restartHint;
        restartHint.Text(LocalizedString(L"Settings.Language.RestartHint"));
        restartHint.TextWrapping(TextWrapping::WrapWholeWords);
        restartHint.Opacity(0.74);
        contentPanel.Children().Append(restartHint);

        dialog.Content(contentPanel);

        auto const result = co_await dialog.ShowAsync();
        if (result != Controls::ContentDialogResult::Primary)
        {
            co_return;
        }

        auto const selectedItem = languageComboBox.SelectedItem().try_as<Controls::ComboBoxItem>();
        if (!selectedItem)
        {
            co_return;
        }

        auto const newLanguage = unbox_value_or<hstring>(selectedItem.Tag(), hstring{});
        auto const previousLanguage = StoredLanguagePreference();
        StoreLanguagePreference(newLanguage);

        if (newLanguage == previousLanguage)
        {
            co_return;
        }

        Controls::ContentDialog restartDialog;
        restartDialog.Title(box_value(LocalizedString(L"Settings.RestartDialog.Title")));
        restartDialog.CloseButtonText(LocalizedString(L"Common.Close"));
        restartDialog.DefaultButton(Controls::ContentDialogButton::Close);
        restartDialog.XamlRoot(Content().XamlRoot());

        Controls::TextBlock restartMessage;
        restartMessage.Text(LocalizedString(L"Settings.RestartDialog.Message"));
        restartMessage.TextWrapping(TextWrapping::WrapWholeWords);
        restartDialog.Content(restartMessage);

        co_await restartDialog.ShowAsync();
    }

    void MainWindow::InitializeModes()
    {
        m_modes = {
            { ColorMode::Original, LocalizedString(L"Mode.Original.Label"), { LocalizedString(L"Channel.Original.Label") }, true },
            { ColorMode::RGB, LocalizedString(L"Mode.RGB.Label"), { LocalizedString(L"Channel.RGB.R"), LocalizedString(L"Channel.RGB.G"), LocalizedString(L"Channel.RGB.B") }, true },
            { ColorMode::HSL, LocalizedString(L"Mode.HSL.Label"), { LocalizedString(L"Channel.HSL.H"), LocalizedString(L"Channel.HSL.S"), LocalizedString(L"Channel.HSL.L") }, false },
            { ColorMode::HSV, LocalizedString(L"Mode.HSV.Label"), { LocalizedString(L"Channel.HSV.H"), LocalizedString(L"Channel.HSV.S"), LocalizedString(L"Channel.HSV.V") }, false },
            { ColorMode::CMYK, LocalizedString(L"Mode.CMYK.Label"), { LocalizedString(L"Channel.CMYK.C"), LocalizedString(L"Channel.CMYK.M"), LocalizedString(L"Channel.CMYK.Y"), LocalizedString(L"Channel.CMYK.K") }, true },
            { ColorMode::LAB, LocalizedString(L"Mode.LAB.Label"), { LocalizedString(L"Channel.LAB.L"), LocalizedString(L"Channel.LAB.A"), LocalizedString(L"Channel.LAB.B") }, false },
        };

        auto items = ColorModeFlyout().Items();
        items.Clear();
        for (uint32_t index = 0; index < m_modes.size(); ++index)
        {
            Controls::MenuFlyoutItem item;
            item.Text(m_modes.at(index).label);
            item.Tag(box_value(index));
            item.Click({ this, &MainWindow::OnColorModeItemClick });
            items.Append(item);
        }

        m_selectedModeIndex = 0;
        ColorModeAppBarButton().Label(m_modes.front().label);
    }

    void MainWindow::Populatechannels()
    {
        const auto selectedMode = SelectedMode();
        if (!selectedMode.has_value())
        {
            return;
        }

        auto const& definition = m_modes.at(m_selectedModeIndex);

        m_isUpdatingUi = true;

        auto items = ChannelFlyout().Items();
        items.Clear();
        for (uint32_t index = 0; index < definition.channels.size(); ++index)
        {
            Controls::MenuFlyoutItem item;
            item.Text(definition.channels.at(index));
            item.Tag(box_value(index));
            item.Click({ this, &MainWindow::OnchannelItemClick });
            items.Append(item);
        }

        m_selectedChannelIndex = 0;
        ChannelAppBarButton().Label(definition.channels.front());
        ChannelAppBarButton().IsEnabled(definition.mode != ColorMode::Original);
        UpdateGrayscaleControls(definition.supportsGrayscaleToggle);

        m_isUpdatingUi = false;
        UpdateCommandStates();
    }

    void MainWindow::UpdateGrayscaleControls(bool supportsGrayscaleToggle)
    {
        if (!supportsGrayscaleToggle)
        {
            m_showGrayscale = false;
        }

        GrayscaleAppBarButton().IsEnabled(supportsGrayscaleToggle);
        GrayscaleAppBarButton().Label(m_showGrayscale ? LocalizedString(L"DisplayMode.Grayscale") : LocalizedString(L"DisplayMode.Color"));
        ColorDisplayMenuItem().IsChecked(!m_showGrayscale);
        GrayscaleDisplayMenuItem().IsChecked(m_showGrayscale);
    }

    winrt::fire_and_forget MainWindow::RefreshPreview()
    {
        auto lifetime = get_strong();
        auto uiThread = winrt::apartment_context();
        auto dispatcherQueue = DispatcherQueue();
        uint64_t requestId = 0;

        try
        {
            // Not in background thread yet, so we can safely access member variables without locks.
            requestId = ++m_previewRequestId;
            PreviewProgressBar().Value(0.0);
            PreviewProgressHost().Visibility(Visibility::Visible);

            if (!m_sourcePixels.has_value() || m_sourcePixels->empty() || m_pixelWidth == 0 || m_pixelHeight == 0)
            {
                PreviewProgressHost().Visibility(Visibility::Collapsed);
                PreviewImage().Source(nullptr);
                EmptyStatePanel().Visibility(Visibility::Visible);
                co_return;
            }

            const auto selectedMode = SelectedMode().value_or(ColorMode::Original);
            const auto channelIndex = SelectedchannelIndex().value_or(0);
            const bool showGrayscale = m_showGrayscale;
            const uint32_t pixelWidth = m_pixelWidth;
            const uint32_t pixelHeight = m_pixelHeight;
            const auto selectedModeIndex = m_selectedModeIndex;
            const auto selectedChannelIndex = m_selectedChannelIndex;
            const auto definition = m_modes.at(selectedModeIndex);
            const auto channelLabel = definition.channels.at(std::min<uint32_t>(selectedChannelIndex, static_cast<uint32_t>(definition.channels.size() - 1)));
            const auto loadedFileName = m_loadedFileName;
            auto sourcePixels = *m_sourcePixels;

            // resume_background schedules the coroutine continuation 
            // on a thread-pool thread. Unlike JavaScript await, this 
            // await point changes the execution context, so the code 
            // below no longer runs on the UI thread.
            co_await winrt::resume_background();

            auto weakThis = get_weak();
            auto previewPixels = RenderPixels(
                sourcePixels,
                pixelWidth,
                pixelHeight,
                selectedMode,
                channelIndex,
                showGrayscale,
                [&](uint32_t progress)
                {
                    dispatcherQueue.TryEnqueue([weakThis, requestId, progress]()
                        {
                            if (auto self = weakThis.get())
                            {
                                if (requestId == self->m_previewRequestId)
                                {
                                    self->PreviewProgressBar().Value(progress);
                                }
                            }
                        });
                });

            // switch back to UI thread
            // also see comment on "co_await winrt::resume_background();"
            co_await uiThread;

            if (requestId != m_previewRequestId)
            {
                co_return;
            }

            Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap writeableBitmap(
                static_cast<int32_t>(pixelWidth),
                static_cast<int32_t>(pixelHeight));

            auto pixelBuffer = writeableBitmap.PixelBuffer();
            auto bufferByteAccess = pixelBuffer.as<IBufferByteAccess>();

            uint8_t* destination = nullptr;
            check_hresult(bufferByteAccess->Buffer(&destination));
            std::copy(previewPixels.winrt_begin(), previewPixels.winrt_end(), destination);
            writeableBitmap.Invalidate();

            PreviewProgressBar().Value(100.0);
            PreviewProgressHost().Visibility(Visibility::Collapsed);
            PreviewImage().Source(writeableBitmap);
            EmptyStatePanel().Visibility(Visibility::Collapsed);
            RestorePreviewView();

            const hstring statusText = definition.mode == ColorMode::Original
                ? definition.label
                : FormatLocalizedString(L"Window.Status.ModeChannelFormat", { definition.label, channelLabel });

            hstring windowTitle = loadedFileName.empty()
                ? LocalizedString(L"Window.Title.AppName")
                : loadedFileName;

            Title(FormatLocalizedString(L"Window.Title.WithStatus", { windowTitle, statusText }));
        }
        catch (...)
        {
            if (requestId != 0)
            {
                auto weakThis = get_weak();
                dispatcherQueue.TryEnqueue([weakThis, requestId]()
                    {
                        if (auto self = weakThis.get())
                        {
                            if (requestId == self->m_previewRequestId)
                            {
                                self->PreviewProgressHost().Visibility(Visibility::Collapsed);
                            }
                        }
                    });
            }
        }
    }

    void MainWindow::UpdateCommandStates()
    {
        const bool hasImage = m_sourcePixels.has_value() && !m_sourcePixels->empty();
        SaveAsButton().IsEnabled(hasImage && !m_isSaving);
    }

    void MainWindow::ShowSaveResultInfoBar(
        Controls::InfoBarSeverity severity,
        hstring const& title,
        hstring const& message)
    {
        SaveResultInfoBar().IsOpen(false);
        SaveResultInfoBar().Severity(severity);
        SaveResultInfoBar().Title(title);
        SaveResultInfoBar().Message(message);
        SaveResultInfoBar().IsOpen(true);
    }

    hstring MainWindow::CurrentStatusText() const
    {
        if (m_selectedModeIndex >= m_modes.size())
        {
            return L"原图";
        }

        auto const& definition = m_modes.at(m_selectedModeIndex);
        if (definition.mode == ColorMode::Original || m_selectedChannelIndex >= definition.channels.size())
        {
            return hstring{ definition.label };
        }

        return hstring{ definition.label } + hstring{ L" · " } + definition.channels.at(m_selectedChannelIndex);
    }

    hstring MainWindow::BuildSuggestedSaveFileName() const
    {
        std::filesystem::path sourcePath;
        if (m_loadedFile)
        {
            sourcePath = std::filesystem::path{ m_loadedFile.Path().c_str() };
        }
        else if (!m_loadedFileName.empty())
        {
            sourcePath = std::filesystem::path{ m_loadedFileName.c_str() };
        }

        std::wstring baseName = sourcePath.stem().wstring();
        std::wstring extension = sourcePath.extension().wstring();
        if (baseName.empty())
        {
            baseName = ::LocalizedString(L"SaveDialog.DefaultFileBaseName").c_str();
        }
        if (!SaveFormatIndexForExtension(extension).has_value())
        {
            extension = L".png";
        }

        std::wstring modeLabel = ::LocalizedString(L"Mode.Original.Label").c_str();
        std::wstring channelLabel = ::LocalizedString(L"Channel.Original.Label").c_str();
        if (m_selectedModeIndex < m_modes.size())
        {
            auto const& definition = m_modes.at(m_selectedModeIndex);
            modeLabel = definition.label;
            if (m_selectedChannelIndex < definition.channels.size())
            {
                channelLabel = definition.channels.at(m_selectedChannelIndex).c_str();
            }
        }

        std::wstring suggestedName = SanitizeFileComponent(baseName)
            + L"-"
            + SanitizeFileComponent(modeLabel)
            + L"-"
            + SanitizeFileComponent(channelLabel)
            + extension;

        return hstring{ suggestedName };
    }

    std::optional<ColorMode> MainWindow::SelectedMode()
    {
        if (m_selectedModeIndex >= m_modes.size())
        {
            return std::nullopt;
        }

        return m_modes.at(m_selectedModeIndex).mode;
    }

    std::optional<uint32_t> MainWindow::SelectedchannelIndex()
    {
        const auto selectedMode = SelectedMode();
        if (!selectedMode.has_value())
        {
            return std::nullopt;
        }

        auto const& definition = m_modes.at(m_selectedModeIndex);
        if (m_selectedChannelIndex >= definition.channels.size())
        {
            return std::nullopt;
        }

        return m_selectedChannelIndex;
    }

    float MainWindow::ComputeFitZoomFactor()
    {
        const float viewportWidth = static_cast<float>(PreviewScrollViewer().ActualWidth());
        const float viewportHeight = static_cast<float>(PreviewScrollViewer().ActualHeight());
        if (viewportWidth <= 0.0f || viewportHeight <= 0.0f || m_pixelWidth == 0 || m_pixelHeight == 0)
        {
            return 1.0f;
        }

        const float widthRatio = viewportWidth / static_cast<float>(m_pixelWidth);
        const float heightRatio = viewportHeight / static_cast<float>(m_pixelHeight);
        return std::min(widthRatio, heightRatio);
    }

    void MainWindow::RestorePreviewView()
    {
        PreviewScrollViewer().UpdateLayout();
        if (m_fitPreviewOnNextRefresh)
        {
            m_savedZoomFactor = ComputeFitZoomFactor();
            m_savedHorizontalOffset = 0.0f;
            m_savedVerticalOffset = 0.0f;
            m_fitPreviewOnNextRefresh = false;
        }

        PreviewScrollViewer().ChangeView(
            m_savedHorizontalOffset,
            m_savedVerticalOffset,
            m_savedZoomFactor,
            true);
    }

    HWND MainWindow::WindowHandle() const
    {
        auto nativeWindow = this->try_as<IWindowNative>();
        HWND windowHandle = nullptr;
        check_hresult(nativeWindow->get_WindowHandle(&windowHandle));
        return windowHandle;
    }
}