#pragma once

#include "ContinuousPixelBuffer.h"
#include "MainWindow.g.h"

namespace winrt::image_channel_viewer::implementation
{
    enum class ColorMode
    {
        Original,
        RGB,
        HSL,
        HSV,
        CMYK,
        LAB,
    };

    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnOpenImageClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnAboutClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnColorModeItemClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnchannelItemClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnGrayscaleItemClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnPreviewViewChanged(IInspectable const& sender, Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& args);

    private:
        struct ModeDefinition
        {
            ColorMode mode;
            winrt::hstring label;
            std::vector<winrt::hstring> channels;
            bool supportsGrayscaleToggle;
        };

        winrt::Windows::Foundation::IAsyncAction LoadImageAsync();
        winrt::Windows::Foundation::IAsyncAction ShowAboutDialogAsync();
        void InitializeModes();
        void Populatechannels();
        void UpdateGrayscaleControls(bool supportsGrayscaleToggle);
        winrt::fire_and_forget RefreshPreview();
        std::optional<ColorMode> SelectedMode();
        std::optional<uint32_t> SelectedchannelIndex();
        float ComputeFitZoomFactor();
        void RestorePreviewView();
        HWND WindowHandle() const;

        std::vector<ModeDefinition> m_modes;
        winrt::Windows::Graphics::Imaging::SoftwareBitmap m_sourceBitmap{ nullptr };
        std::optional<::image_channel_viewer::ContinuousPixelBuffer> m_sourcePixels;
        winrt::hstring m_loadedFileName;
        uint32_t m_pixelWidth{ 0 };
        uint32_t m_pixelHeight{ 0 };
        uint32_t m_stride{ 0 };
        uint32_t m_selectedModeIndex{ 0 };
        uint32_t m_selectedChannelIndex{ 0 };
        float m_savedHorizontalOffset{ 0.0f };
        float m_savedVerticalOffset{ 0.0f };
        float m_savedZoomFactor{ 1.0f };
        uint64_t m_previewRequestId{ 0 };
        bool m_fitPreviewOnNextRefresh{ false };
        bool m_isUpdatingUi{ false };
        bool m_showGrayscale{ false };
    };
}

namespace winrt::image_channel_viewer::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}