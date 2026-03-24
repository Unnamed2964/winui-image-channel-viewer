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
    using ContinuousPixel = ::image_channel_viewer::ContinuousPixelBuffer::Pixel;

    struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) IBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
    };

    struct __declspec(uuid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")) IMemoryBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
    };
    
    struct PixelF
    {
        float r;
        float g;
        float b;
    };

    struct Hsl
    {
        float h;
        float s;
        float l;
    };

    struct Hsv
    {
        float h;
        float s;
        float v;
    };

    struct Cmyk
    {
        float c;
        float m;
        float y;
        float k;
    };

    struct Lab
    {
        float l;
        float a;
        float b;
    };

    float Clamp01(float value)
    {
        return ::image_channel_viewer::ContinuousPixelBuffer::Clamp01(value);
    }

    float SrgbToLinear(float value)
    {
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
    }

    float HueToRgb(float p, float q, float t)
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

    PixelF HslToRgb(float hue, float saturation, float lightness)
    {
        hue = std::fmod(hue, 360.0f);
        if (hue < 0.0f)
        {
            hue += 360.0f;
        }

        const float normalizedHue = hue / 360.0f;
        if (saturation <= 0.0f)
        {
            return { lightness, lightness, lightness };
        }

        const float q = lightness < 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
        const float p = 2.0f * lightness - q;
        return {
            HueToRgb(p, q, normalizedHue + 1.0f / 3.0f),
            HueToRgb(p, q, normalizedHue),
            HueToRgb(p, q, normalizedHue - 1.0f / 3.0f),
        };
    }

    PixelF HsvToRgb(float hue, float saturation, float value)
    {
        hue = std::fmod(hue, 360.0f);
        if (hue < 0.0f)
        {
            hue += 360.0f;
        }

        if (saturation <= 0.0f)
        {
            return { value, value, value };
        }

        const float chroma = value * saturation;
        const float huePrime = hue / 60.0f;
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;

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
        return { red + match, green + match, blue + match };
    }

    Hsl RgbToHsl(PixelF pixel)
    {
        const float maxValue = std::max({ pixel.r, pixel.g, pixel.b });
        const float minValue = std::min({ pixel.r, pixel.g, pixel.b });
        const float delta = maxValue - minValue;
        const float lightness = (maxValue + minValue) * 0.5f;

        Hsl result{};
        result.l = lightness;

        if (delta <= std::numeric_limits<float>::epsilon())
        {
            return result;
        }

        result.s = lightness > 0.5f ? delta / (2.0f - maxValue - minValue) : delta / (maxValue + minValue);

        if (maxValue == pixel.r)
        {
            result.h = 60.0f * ((pixel.g - pixel.b) / delta);
        }
        else if (maxValue == pixel.g)
        {
            result.h = 60.0f * (((pixel.b - pixel.r) / delta) + 2.0f);
        }
        else
        {
            result.h = 60.0f * (((pixel.r - pixel.g) / delta) + 4.0f);
        }

        if (result.h < 0.0f)
        {
            result.h += 360.0f;
        }
        return result;
    }

    Hsv RgbToHsv(PixelF pixel)
    {
        const float maxValue = std::max({ pixel.r, pixel.g, pixel.b });
        const float minValue = std::min({ pixel.r, pixel.g, pixel.b });
        const float delta = maxValue - minValue;

        Hsv result{};
        result.v = maxValue;

        if (maxValue <= std::numeric_limits<float>::epsilon())
        {
            return result;
        }

        result.s = delta / maxValue;
        if (delta <= std::numeric_limits<float>::epsilon())
        {
            return result;
        }

        if (maxValue == pixel.r)
        {
            result.h = 60.0f * ((pixel.g - pixel.b) / delta);
        }
        else if (maxValue == pixel.g)
        {
            result.h = 60.0f * (((pixel.b - pixel.r) / delta) + 2.0f);
        }
        else
        {
            result.h = 60.0f * (((pixel.r - pixel.g) / delta) + 4.0f);
        }

        if (result.h < 0.0f)
        {
            result.h += 360.0f;
        }
        return result;
    }

    Cmyk RgbToCmyk(PixelF pixel)
    {
        const float black = 1.0f - std::max({ pixel.r, pixel.g, pixel.b });
        if (black >= 1.0f - std::numeric_limits<float>::epsilon())
        {
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }

        const float denominator = 1.0f - black;
        return {
            (1.0f - pixel.r - black) / denominator,
            (1.0f - pixel.g - black) / denominator,
            (1.0f - pixel.b - black) / denominator,
            black,
        };
    }

    float LabPivot(float value)
    {
        return value > 0.008856f ? std::cbrt(value) : (7.787f * value) + (16.0f / 116.0f);
    }

    Lab RgbToLab(PixelF pixel)
    {
        const float linearRed = SrgbToLinear(pixel.r);
        const float linearGreen = SrgbToLinear(pixel.g);
        const float linearBlue = SrgbToLinear(pixel.b);

        const float x = linearRed * 0.4124564f + linearGreen * 0.3575761f + linearBlue * 0.1804375f;
        const float y = linearRed * 0.2126729f + linearGreen * 0.7151522f + linearBlue * 0.0721750f;
        const float z = linearRed * 0.0193339f + linearGreen * 0.1191920f + linearBlue * 0.9503041f;

        const float fx = LabPivot(x / 0.95047f);
        const float fy = LabPivot(y / 1.0f);
        const float fz = LabPivot(z / 1.08883f);

        return {
            (116.0f * fy) - 16.0f,
            500.0f * (fx - fy),
            200.0f * (fy - fz),
        };
    }

    ContinuousPixel ComposePixel(float red, float green, float blue, float alpha)
    {
        return {
            Clamp01(red),
            Clamp01(green),
            Clamp01(blue),
            Clamp01(alpha),
        };
    }
}

namespace winrt::image_channel_viewer::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
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
        ColorModeSelectionText().Text(m_modes.at(m_selectedModeIndex).label);
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
        ChannelSelectionText().Text(menuItem.Text());
        RefreshPreview();
    }

    void MainWindow::OnGrayscaleToggled(
        [[maybe_unused]] IInspectable const& sender, 
        [[maybe_unused]] RoutedEventArgs const& args)
    {
        if (!m_isUpdatingUi)
        {
            RefreshPreview();
        }
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
        ColorModeSelectionText().Text(m_modes.front().label);
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
        ChannelSelectionText().Text(definition.channels.front());
        ChannelDropDownButton().IsEnabled(definition.mode != ColorMode::Original);
        GrayscaleToggle().IsEnabled(definition.supportsGrayscaleToggle);
        if (!definition.supportsGrayscaleToggle)
        {
            GrayscaleToggle().IsOn(false);
        }

        m_isUpdatingUi = false;
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
            const bool showGrayscale = GrayscaleToggle().IsOn();
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
            auto const* sourcePixelData = sourcePixels.data();
            auto* previewPixelData = previewPixels.data();
            const size_t pixelCount = sourcePixels.pixel_count();
            uint32_t lastReportedProgress = 0;
            for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
            {
                auto const& sourcePixel = sourcePixelData[pixelIndex];
                const float alpha = sourcePixel[3];

                const PixelF pixel{
                    sourcePixel[0],
                    sourcePixel[1],
                    sourcePixel[2],
                };

                ContinuousPixel mappedPixel{};
                switch (selectedMode)
                {
                case ColorMode::Original:
                    mappedPixel = sourcePixel;
                    break;

                case ColorMode::RGB:
                {
                    const float channelValue = channelIndex == 0 ? pixel.r : (channelIndex == 1 ? pixel.g : pixel.b);
                    if (showGrayscale)
                    {
                        mappedPixel = ComposePixel(channelValue, channelValue, channelValue, alpha);
                    }
                    else
                    {
                        mappedPixel = ComposePixel(
                            channelIndex == 0 ? channelValue : 0.0f,
                            channelIndex == 1 ? channelValue : 0.0f,
                            channelIndex == 2 ? channelValue : 0.0f,
                            alpha);
                    }
                    break;
                }

                case ColorMode::HSL:
                {
                    const auto hsl = RgbToHsl(pixel);
                    if (channelIndex == 0)
                    {
                        const auto huePixel = HslToRgb(hsl.h, 1.0f, 0.5f);
                        mappedPixel = ComposePixel(huePixel.r, huePixel.g, huePixel.b, alpha);
                    }
                    else if (channelIndex == 1)
                    {
                        mappedPixel = ComposePixel(hsl.s, hsl.s, hsl.s, alpha);
                    }
                    else
                    {
                        mappedPixel = ComposePixel(hsl.l, hsl.l, hsl.l, alpha);
                    }
                    break;
                }

                case ColorMode::HSV:
                {
                    const auto hsv = RgbToHsv(pixel);
                    if (channelIndex == 0)
                    {
                        const auto huePixel = HsvToRgb(hsv.h, 1.0f, 1.0f);
                        mappedPixel = ComposePixel(huePixel.r, huePixel.g, huePixel.b, alpha);
                    }
                    else if (channelIndex == 1)
                    {
                        mappedPixel = ComposePixel(hsv.s, hsv.s, hsv.s, alpha);
                    }
                    else
                    {
                        mappedPixel = ComposePixel(hsv.v, hsv.v, hsv.v, alpha);
                    }
                    break;
                }

                case ColorMode::CMYK:
                {
                    const auto cmyk = RgbToCmyk(pixel);
                    const float channelValue = channelIndex == 0 ? cmyk.c : (channelIndex == 1 ? cmyk.m : (channelIndex == 2 ? cmyk.y : cmyk.k));
                    if (showGrayscale || channelIndex == 3)
                    {
                        mappedPixel = ComposePixel(channelValue, channelValue, channelValue, alpha);
                    }
                    else if (channelIndex == 0)
                    {
                        mappedPixel = ComposePixel(0.0f, channelValue, channelValue, alpha);
                    }
                    else if (channelIndex == 1)
                    {
                        mappedPixel = ComposePixel(channelValue, 0.0f, channelValue, alpha);
                    }
                    else
                    {
                        mappedPixel = ComposePixel(channelValue, channelValue, 0.0f, alpha);
                    }
                    break;
                }

                case ColorMode::LAB:
                {
                    const auto lab = RgbToLab(pixel);
                    if (channelIndex == 0)
                    {
                        const float lightness = Clamp01(lab.l / 100.0f);
                        mappedPixel = ComposePixel(lightness, lightness, lightness, alpha);
                    }
                    else if (channelIndex == 1)
                    {
                        const float value = Clamp01((lab.a + 128.0f) / 255.0f);
                        mappedPixel = ComposePixel(value, value, value, alpha);
                    }
                    else
                    {
                        const float value = Clamp01((lab.b + 128.0f) / 255.0f);
                        mappedPixel = ComposePixel(value, value, value, alpha);
                    }
                    break;
                }
                }

                previewPixelData[pixelIndex] = mappedPixel;

                const uint32_t progress = static_cast<uint32_t>(((pixelIndex + 1) * 100) / pixelCount);
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

    std::optional<MainWindow::ColorMode> MainWindow::SelectedMode()
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