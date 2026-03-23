#pragma once

#include "MainWindow.g.h"

namespace winrt::image_channel_viewer::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnOpenImageClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnColorModeItemClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnchannelItemClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnGrayscaleToggled(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnPreviewViewChanged(IInspectable const& sender, Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& args);

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
        void RestorePreviewView();
        HWND WindowHandle() const;

        std::vector<ModeDefinition> m_modes;
        winrt::Windows::Graphics::Imaging::SoftwareBitmap m_sourceBitmap{ nullptr };
        std::vector<std::uint8_t> m_sourcePixels;
        winrt::hstring m_loadedFileName;
        uint32_t m_pixelWidth{ 0 };
        uint32_t m_pixelHeight{ 0 };
        uint32_t m_stride{ 0 };
        uint32_t m_selectedModeIndex{ 0 };
        uint32_t m_selectedChannelIndex{ 0 };
        double m_savedHorizontalOffset{ 0.0 };
        double m_savedVerticalOffset{ 0.0 };
        float m_savedZoomFactor{ 1.0f };
        bool m_isUpdatingUi{ false };
    };
}

namespace winrt::image_channel_viewer::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}