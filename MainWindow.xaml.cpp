#include "pch.h"
#include "ContinuousPixelBuffer.h"
#include "MainWindow.xaml.h"
#include "AppVersion.g.h"

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) IBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
    };

    struct __declspec(uuid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")) IMemoryBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
    };

    using ColorMode = winrt::image_channel_viewer::implementation::ColorMode;

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
}

namespace winrt::image_channel_viewer::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        AppWindow().TitleBar().PreferredTheme(Microsoft::UI::Windowing::TitleBarTheme::UseDefaultAppMode);
        Title(L"Image Channel Viewer");
        InitializeModes();
        Populatechannels();
    }

    void MainWindow::OnOpenImageClick(
        [[maybe_unused]] IInspectable const& sender, 
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        LoadImageAsync();
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
        picker.FileTypeFilter().Append(L".png");
        picker.FileTypeFilter().Append(L".jpg");
        picker.FileTypeFilter().Append(L".jpeg");
        picker.FileTypeFilter().Append(L".bmp");
        picker.FileTypeFilter().Append(L".gif");
        picker.FileTypeFilter().Append(L".tif");
        picker.FileTypeFilter().Append(L".tiff");
        picker.FileTypeFilter().Append(L".webp");

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
        EmptyStatePanel().Visibility(Visibility::Collapsed);
        RefreshPreview();
    }

    Windows::Foundation::IAsyncAction MainWindow::ShowAboutDialogAsync()
    {
        Controls::ContentDialog dialog;
        dialog.Title(box_value(L"关于 Image Channel Viewer"));
        dialog.PrimaryButtonText(L"关闭");
        dialog.DefaultButton(Controls::ContentDialogButton::Primary);
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Microsoft::UI::ColorHelper::FromArgb(0xFF, 0x91, 0xD4, 0xE4)));

        Microsoft::UI::Xaml::Controls::StackPanel contentPanel;
        contentPanel.Spacing(12);

        Microsoft::UI::Xaml::Controls::TextBlock versionText;
        versionText.Text(hstring{ L"版本：" } + hstring{ AppVersion });
        versionText.TextWrapping(TextWrapping::WrapWholeWords);
        contentPanel.Children().Append(versionText);

        Microsoft::UI::Xaml::Controls::RichTextBlock authorText;
        authorText.TextWrapping(TextWrapping::WrapWholeWords);

        Microsoft::UI::Xaml::Documents::Paragraph authorParagraph;

        Microsoft::UI::Xaml::Documents::Run authorPrefix;
        authorPrefix.Text(L"由 Umaichi/Unnamed2964 制作. 由 ");
        authorParagraph.Inlines().Append(authorPrefix);

        Microsoft::UI::Xaml::Documents::Hyperlink mitLicenseLink;
        mitLicenseLink.NavigateUri(Windows::Foundation::Uri(L"https://github.com/Unnamed2964/winui-image-channel-viewer/blob/master/LICENSE"));

        Microsoft::UI::Xaml::Documents::Run mitLicenseRun;
        mitLicenseRun.Text(L"MIT 许可证");
        mitLicenseLink.Inlines().Append(mitLicenseRun);
        authorParagraph.Inlines().Append(mitLicenseLink);

        Microsoft::UI::Xaml::Documents::Run authorSuffix;
        authorSuffix.Text(L" 许可给你.");
        authorParagraph.Inlines().Append(authorSuffix);

        authorText.Blocks().Append(authorParagraph);
        contentPanel.Children().Append(authorText);

        Microsoft::UI::Xaml::Controls::StackPanel linksPanel;

        Microsoft::UI::Xaml::Controls::HyperlinkButton githubLink;
        githubLink.Content(box_value(L"GitHub 主页"));
        githubLink.NavigateUri(Windows::Foundation::Uri(L"https://github.com/Unnamed2964"));
        githubLink.HorizontalAlignment(HorizontalAlignment::Left);
        linksPanel.Children().Append(githubLink);

        Microsoft::UI::Xaml::Controls::HyperlinkButton websiteLink;
        websiteLink.Content(box_value(L"个人网站"));
        websiteLink.NavigateUri(Windows::Foundation::Uri(L"https://umamichi.moe"));
        websiteLink.HorizontalAlignment(HorizontalAlignment::Left);
        linksPanel.Children().Append(websiteLink);

        contentPanel.Children().Append(linksPanel);

        dialog.Content(contentPanel);

        co_await dialog.ShowAsync();
    }

    void MainWindow::InitializeModes()
    {
        m_modes = {
            { ColorMode::Original, L"原图", { L"原图" }, true },
            { ColorMode::RGB, L"RGB", { L"R", L"G", L"B" }, true },
            { ColorMode::HSL, L"HSL", { L"H", L"S", L"L" }, false },
            { ColorMode::HSV, L"HSV", { L"H", L"S", L"V" }, false },
            { ColorMode::CMYK, L"CMYK", { L"C", L"M", L"Y", L"K" }, true },
            { ColorMode::LAB, L"LAB", { L"L", L"a", L"b" }, false },
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
    }

    void MainWindow::UpdateGrayscaleControls(bool supportsGrayscaleToggle)
    {
        if (!supportsGrayscaleToggle)
        {
            m_showGrayscale = false;
        }

        GrayscaleAppBarButton().IsEnabled(supportsGrayscaleToggle);
        GrayscaleAppBarButton().Label(m_showGrayscale ? L"黑白显示" : L"彩色显示");
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
            // update progress for every 2^16 pixels processed, which is about 1/30 of a 1080p image
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
                            auto weakThis = get_weak();
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
                ? hstring{ L"原图" }
                : hstring{ definition.label } + hstring{ L" · " } + channelLabel;

            hstring windowTitle = loadedFileName.empty()
                ? hstring{ L"Image Channel Viewer" }
                : loadedFileName;

            Title(windowTitle + hstring{ L" - " } + statusText);
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