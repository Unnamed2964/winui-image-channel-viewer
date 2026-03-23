#pragma once

#include "MainWindow.g.h"

namespace winrt::image_channel_viewer::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnOpenImageClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnColorModeChanged(IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnchannelChanged(IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnGrayscaleToggled(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        enum class ColorMode
        {
            Original,
            RGB,
            HSL,
            HSV,
            CMYK,
            LAB,
        };

        struct ModeDefinition
        {
            ColorMode mode;
            wchar_t const* label;
            std::vector<winrt::hstring> channels;
            bool supportsGrayscaleToggle;
        };

        winrt::Windows::Foundation::IAsyncAction LoadImageAsync();
        void InitializeModes();
        void Populatechannels();
        void RefreshPreview();
        std::optional<ColorMode> SelectedMode();
        std::optional<uint32_t> SelectedchannelIndex();
        HWND WindowHandle() const;

        std::vector<ModeDefinition> m_modes;
        winrt::Windows::Graphics::Imaging::SoftwareBitmap m_sourceBitmap{ nullptr };
        std::vector<std::uint8_t> m_sourcePixels;
        winrt::hstring m_loadedFileName;
        uint32_t m_pixelWidth{ 0 };
        uint32_t m_pixelHeight{ 0 };
        uint32_t m_stride{ 0 };
        bool m_isUpdatingUi{ false };
    };
}

namespace winrt::image_channel_viewer::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}