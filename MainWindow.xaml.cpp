#include "pch.h"
#include "MainWindow.xaml.h"

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

    struct PixelF
    {
        double r;
        double g;
        double b;
    };

    struct Hsl
    {
        double h;
        double s;
        double l;
    };

    struct Hsv
    {
        double h;
        double s;
        double v;
    };

    struct Cmyk
    {
        double c;
        double m;
        double y;
        double k;
    };

    struct Lab
    {
        double l;
        double a;
        double b;
    };

    double Clamp01(double value)
    {
        return std::clamp(value, 0.0, 1.0);
    }

    uint8_t ToByte(double value)
    {
        return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0, 255.0)));
    }

    double SrgbToLinear(double value)
    {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    }

    double HueToRgb(double p, double q, double t)
    {
        if (t < 0.0)
        {
            t += 1.0;
        }
        if (t > 1.0)
        {
            t -= 1.0;
        }
        if (t < 1.0 / 6.0)
        {
            return p + (q - p) * 6.0 * t;
        }
        if (t < 1.0 / 2.0)
        {
            return q;
        }
        if (t < 2.0 / 3.0)
        {
            return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
        }
        return p;
    }

    PixelF HslToRgb(double hue, double saturation, double lightness)
    {
        hue = std::fmod(hue, 360.0);
        if (hue < 0.0)
        {
            hue += 360.0;
        }

        const double normalizedHue = hue / 360.0;
        if (saturation <= 0.0)
        {
            return { lightness, lightness, lightness };
        }

        const double q = lightness < 0.5 ? lightness * (1.0 + saturation) : lightness + saturation - lightness * saturation;
        const double p = 2.0 * lightness - q;
        return {
            HueToRgb(p, q, normalizedHue + 1.0 / 3.0),
            HueToRgb(p, q, normalizedHue),
            HueToRgb(p, q, normalizedHue - 1.0 / 3.0),
        };
    }

    PixelF HsvToRgb(double hue, double saturation, double value)
    {
        hue = std::fmod(hue, 360.0);
        if (hue < 0.0)
        {
            hue += 360.0;
        }

        if (saturation <= 0.0)
        {
            return { value, value, value };
        }

        const double chroma = value * saturation;
        const double huePrime = hue / 60.0;
        const double x = chroma * (1.0 - std::fabs(std::fmod(huePrime, 2.0) - 1.0));
        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;

        if (huePrime < 1.0)
        {
            red = chroma;
            green = x;
        }
        else if (huePrime < 2.0)
        {
            red = x;
            green = chroma;
        }
        else if (huePrime < 3.0)
        {
            green = chroma;
            blue = x;
        }
        else if (huePrime < 4.0)
        {
            green = x;
            blue = chroma;
        }
        else if (huePrime < 5.0)
        {
            red = x;
            blue = chroma;
        }
        else
        {
            red = chroma;
            blue = x;
        }

        const double match = value - chroma;
        return { red + match, green + match, blue + match };
    }

    Hsl RgbToHsl(PixelF pixel)
    {
        const double maxValue = std::max({ pixel.r, pixel.g, pixel.b });
        const double minValue = std::min({ pixel.r, pixel.g, pixel.b });
        const double delta = maxValue - minValue;
        const double lightness = (maxValue + minValue) * 0.5;

        Hsl result{};
        result.l = lightness;

        if (delta <= std::numeric_limits<double>::epsilon())
        {
            return result;
        }

        result.s = lightness > 0.5 ? delta / (2.0 - maxValue - minValue) : delta / (maxValue + minValue);

        if (maxValue == pixel.r)
        {
            result.h = 60.0 * std::fmod(((pixel.g - pixel.b) / delta), 6.0);
        }
        else if (maxValue == pixel.g)
        {
            result.h = 60.0 * (((pixel.b - pixel.r) / delta) + 2.0);
        }
        else
        {
            result.h = 60.0 * (((pixel.r - pixel.g) / delta) + 4.0);
        }

        if (result.h < 0.0)
        {
            result.h += 360.0;
        }
        return result;
    }

    Hsv RgbToHsv(PixelF pixel)
    {
        const double maxValue = std::max({ pixel.r, pixel.g, pixel.b });
        const double minValue = std::min({ pixel.r, pixel.g, pixel.b });
        const double delta = maxValue - minValue;

        Hsv result{};
        result.v = maxValue;

        if (maxValue <= std::numeric_limits<double>::epsilon())
        {
            return result;
        }

        result.s = delta / maxValue;
        if (delta <= std::numeric_limits<double>::epsilon())
        {
            return result;
        }

        if (maxValue == pixel.r)
        {
            result.h = 60.0 * std::fmod(((pixel.g - pixel.b) / delta), 6.0);
        }
        else if (maxValue == pixel.g)
        {
            result.h = 60.0 * (((pixel.b - pixel.r) / delta) + 2.0);
        }
        else
        {
            result.h = 60.0 * (((pixel.r - pixel.g) / delta) + 4.0);
        }

        if (result.h < 0.0)
        {
            result.h += 360.0;
        }
        return result;
    }

    Cmyk RgbToCmyk(PixelF pixel)
    {
        const double black = 1.0 - std::max({ pixel.r, pixel.g, pixel.b });
        if (black >= 1.0 - std::numeric_limits<double>::epsilon())
        {
            return { 0.0, 0.0, 0.0, 1.0 };
        }

        const double denominator = 1.0 - black;
        return {
            (1.0 - pixel.r - black) / denominator,
            (1.0 - pixel.g - black) / denominator,
            (1.0 - pixel.b - black) / denominator,
            black,
        };
    }

    double LabPivot(double value)
    {
        return value > 0.008856 ? std::cbrt(value) : (7.787 * value) + (16.0 / 116.0);
    }

    Lab RgbToLab(PixelF pixel)
    {
        const double linearRed = SrgbToLinear(pixel.r);
        const double linearGreen = SrgbToLinear(pixel.g);
        const double linearBlue = SrgbToLinear(pixel.b);

        const double x = linearRed * 0.4124564 + linearGreen * 0.3575761 + linearBlue * 0.1804375;
        const double y = linearRed * 0.2126729 + linearGreen * 0.7151522 + linearBlue * 0.0721750;
        const double z = linearRed * 0.0193339 + linearGreen * 0.1191920 + linearBlue * 0.9503041;

        const double fx = LabPivot(x / 0.95047);
        const double fy = LabPivot(y / 1.0);
        const double fz = LabPivot(z / 1.08883);

        return {
            (116.0 * fy) - 16.0,
            500.0 * (fx - fy),
            200.0 * (fy - fz),
        };
    }

    std::array<uint8_t, 4> ComposePixel(double red, double green, double blue, uint8_t alpha)
    {
        return {
            ToByte(Clamp01(blue) * 255.0),
            ToByte(Clamp01(green) * 255.0),
            ToByte(Clamp01(red) * 255.0),
            alpha,
        };
    }
}

namespace winrt::image_channel_viewer::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        InitializeModes();
        Populatechannels();
    }

    void MainWindow::OnOpenImageClick(IInspectable const& sender, RoutedEventArgs const& args)
    {
        (void)sender;
        (void)args;
        LoadImageAsync();
    }

    void MainWindow::OnColorModeChanged(IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args)
    {
        (void)sender;
        (void)args;
        if (m_isUpdatingUi)
        {
            return;
        }

        Populatechannels();
        RefreshPreview();
    }

    void MainWindow::OnchannelChanged(IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args)
    {
        (void)sender;
        (void)args;
        if (!m_isUpdatingUi)
        {
            RefreshPreview();
        }
    }

    void MainWindow::OnGrayscaleToggled(IInspectable const& sender, RoutedEventArgs const& args)
    {
        (void)sender;
        (void)args;
        if (!m_isUpdatingUi)
        {
            RefreshPreview();
        }
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
        (void)capacity;

        const auto plane = buffer.GetPlaneDescription(0);
        m_stride = static_cast<uint32_t>(plane.Stride);
        m_sourcePixels.resize(m_stride * m_pixelHeight);

        for (uint32_t row = 0; row < m_pixelHeight; ++row)
        {
            const auto* rowStart = sourceData + plane.StartIndex + (row * m_stride);
            std::copy_n(rowStart, m_stride, m_sourcePixels.data() + (row * m_stride));
        }

        EmptyStatePanel().Visibility(Visibility::Collapsed);
        StatusTextBlock().Text(L"图片已载入，可以切换颜色模式和通道。");
        MetaTextBlock().Text(m_loadedFileName + L"  ·  " + to_hstring(m_pixelWidth) + L" × " + to_hstring(m_pixelHeight));
        RefreshPreview();
    }

    void MainWindow::InitializeModes()
    {
        m_modes = {
            { ColorMode::Original, L"原图", { L"原图" }, false },
            { ColorMode::RGB, L"RGB", { L"R", L"G", L"B" }, true },
            { ColorMode::HSL, L"HSL", { L"H", L"S", L"L" }, false },
            { ColorMode::HSV, L"HSV", { L"H", L"S", L"V" }, false },
            { ColorMode::CMYK, L"CMYK", { L"C", L"M", L"Y", L"K" }, true },
            { ColorMode::LAB, L"LAB", { L"L", L"a", L"b" }, false },
        };

        auto items = ColorModeComboBox().Items();
        items.Clear();
        for (auto const& mode : m_modes)
        {
            items.Append(box_value(mode.label));
        }
        ColorModeComboBox().SelectedIndex(0);
    }

    void MainWindow::Populatechannels()
    {
        const auto selectedMode = SelectedMode();
        if (!selectedMode.has_value())
        {
            return;
        }

        const auto modeIndex = static_cast<uint32_t>(ColorModeComboBox().SelectedIndex());
        auto const& definition = m_modes.at(modeIndex);

        m_isUpdatingUi = true;

        auto items = channelComboBox().Items();
        items.Clear();
        for (auto const& channel : definition.channels)
        {
            items.Append(box_value(channel));
        }

        channelComboBox().SelectedIndex(0);
        channelComboBox().IsEnabled(definition.mode != ColorMode::Original);
        GrayscaleToggle().IsEnabled(definition.supportsGrayscaleToggle);
        if (!definition.supportsGrayscaleToggle)
        {
            GrayscaleToggle().IsOn(false);
        }

        m_isUpdatingUi = false;
    }

    void MainWindow::RefreshPreview()
    {
        if (m_sourcePixels.empty() || m_pixelWidth == 0 || m_pixelHeight == 0)
        {
            PreviewImage().Source(nullptr);
            EmptyStatePanel().Visibility(Visibility::Visible);
            return;
        }

        const auto selectedMode = SelectedMode().value_or(ColorMode::Original);
        const auto channelIndex = SelectedchannelIndex().value_or(0);
        const bool showGrayscale = GrayscaleToggle().IsOn();

        std::vector<uint8_t> previewPixels(m_stride * m_pixelHeight);
        for (uint32_t row = 0; row < m_pixelHeight; ++row)
        {
            for (uint32_t column = 0; column < m_pixelWidth; ++column)
            {
                const uint32_t offset = row * m_stride + column * 4;
                const uint8_t blue = m_sourcePixels[offset + 0];
                const uint8_t green = m_sourcePixels[offset + 1];
                const uint8_t red = m_sourcePixels[offset + 2];
                const uint8_t alpha = m_sourcePixels[offset + 3];

                const PixelF pixel{
                    red / 255.0,
                    green / 255.0,
                    blue / 255.0,
                };

                std::array<uint8_t, 4> mappedPixel{};
                switch (selectedMode)
                {
                case ColorMode::Original:
                    mappedPixel = { blue, green, red, alpha };
                    break;

                case ColorMode::RGB:
                {
                    const double channelValue = channelIndex == 0 ? pixel.r : (channelIndex == 1 ? pixel.g : pixel.b);
                    if (showGrayscale)
                    {
                        mappedPixel = ComposePixel(channelValue, channelValue, channelValue, alpha);
                    }
                    else
                    {
                        mappedPixel = ComposePixel(
                            channelIndex == 0 ? channelValue : 0.0,
                            channelIndex == 1 ? channelValue : 0.0,
                            channelIndex == 2 ? channelValue : 0.0,
                            alpha);
                    }
                    break;
                }

                case ColorMode::HSL:
                {
                    const auto hsl = RgbToHsl(pixel);
                    if (channelIndex == 0)
                    {
                        const auto huePixel = HslToRgb(hsl.h, 1.0, 0.5);
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
                        const auto huePixel = HsvToRgb(hsv.h, 1.0, 1.0);
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
                    const double channelValue = channelIndex == 0 ? cmyk.c : (channelIndex == 1 ? cmyk.m : (channelIndex == 2 ? cmyk.y : cmyk.k));
                    if (showGrayscale || channelIndex == 3)
                    {
                        mappedPixel = ComposePixel(channelValue, channelValue, channelValue, alpha);
                    }
                    else if (channelIndex == 0)
                    {
                        mappedPixel = ComposePixel(0.0, channelValue, channelValue, alpha);
                    }
                    else if (channelIndex == 1)
                    {
                        mappedPixel = ComposePixel(channelValue, 0.0, channelValue, alpha);
                    }
                    else
                    {
                        mappedPixel = ComposePixel(channelValue, channelValue, 0.0, alpha);
                    }
                    break;
                }

                case ColorMode::LAB:
                {
                    const auto lab = RgbToLab(pixel);
                    if (channelIndex == 0)
                    {
                        const double lightness = Clamp01(lab.l / 100.0);
                        mappedPixel = ComposePixel(lightness, lightness, lightness, alpha);
                    }
                    else if (channelIndex == 1)
                    {
                        const double value = Clamp01((lab.a + 128.0) / 255.0);
                        mappedPixel = ComposePixel(value, value, value, alpha);
                    }
                    else
                    {
                        const double value = Clamp01((lab.b + 128.0) / 255.0);
                        mappedPixel = ComposePixel(value, value, value, alpha);
                    }
                    break;
                }
                }

                previewPixels[offset + 0] = mappedPixel[0];
                previewPixels[offset + 1] = mappedPixel[1];
                previewPixels[offset + 2] = mappedPixel[2];
                previewPixels[offset + 3] = mappedPixel[3];
            }
        }

        Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap writeableBitmap(
            static_cast<int32_t>(m_pixelWidth),
            static_cast<int32_t>(m_pixelHeight));

        auto pixelBuffer = writeableBitmap.PixelBuffer();
        auto bufferByteAccess = pixelBuffer.as<IBufferByteAccess>();

        uint8_t* destination = nullptr;
        check_hresult(bufferByteAccess->Buffer(&destination));
        std::copy(previewPixels.begin(), previewPixels.end(), destination);
        writeableBitmap.Invalidate();

        PreviewImage().Source(writeableBitmap);
        EmptyStatePanel().Visibility(Visibility::Collapsed);

        auto const& definition = m_modes.at(static_cast<uint32_t>(ColorModeComboBox().SelectedIndex()));
        const auto channelLabel = unbox_value<hstring>(channelComboBox().SelectedItem());
        const hstring statusText = definition.mode == ColorMode::Original
            ? hstring{ L"当前显示原图。" }
            : hstring{ L"当前显示 " } + hstring{ definition.label } + hstring{ L" · " } + channelLabel;
        StatusTextBlock().Text(statusText);
    }

    std::optional<MainWindow::ColorMode> MainWindow::SelectedMode()
    {
        const int32_t selectedIndex = ColorModeComboBox().SelectedIndex();
        if (selectedIndex < 0 || static_cast<uint32_t>(selectedIndex) >= m_modes.size())
        {
            return std::nullopt;
        }

        return m_modes.at(static_cast<uint32_t>(selectedIndex)).mode;
    }

    std::optional<uint32_t> MainWindow::SelectedchannelIndex()
    {
        const int32_t selectedIndex = channelComboBox().SelectedIndex();
        if (selectedIndex < 0)
        {
            return std::nullopt;
        }

        return static_cast<uint32_t>(selectedIndex);
    }

    HWND MainWindow::WindowHandle() const
    {
        auto nativeWindow = this->try_as<IWindowNative>();
        HWND windowHandle = nullptr;
        check_hresult(nativeWindow->get_WindowHandle(&windowHandle));
        return windowHandle;
    }
}