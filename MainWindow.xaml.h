#pragma once

#include "ContinuousPixelBuffer.h"
#include "ImageProcessing.h"
#include "MainWindow.g.h"

namespace winrt::image_channel_viewer::implementation
{
    using ColorMode = ::image_channel_viewer::image_processing::ColorMode;

    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        winrt::hstring LocalizedString(winrt::hstring const& resourceId);

        void OnOpenImageClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnSettingsClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnSaveAsClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
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

        struct RenderStateSnapshot
        {
            ::image_channel_viewer::image_processing::RenderRequestSnapshot renderRequest;
            winrt::hstring modeLabel;
            winrt::hstring channelLabel;
            winrt::hstring loadedFileName;
        };

        winrt::Windows::Foundation::IAsyncAction LoadImageAsync();
        winrt::Windows::Foundation::IAsyncAction ShowSettingsDialogAsync();
        winrt::Windows::Foundation::IAsyncAction SaveCurrentViewAsync();
        winrt::Windows::Foundation::IAsyncAction ShowAboutDialogAsync();
        void InitializeModes();
        void Populatechannels();
        void UpdateGrayscaleControls(bool supportsGrayscaleToggle);
        winrt::fire_and_forget RefreshPreview();
        std::optional<RenderStateSnapshot> CaptureRenderStateSnapshot() const;
        std::optional<ColorMode> SelectedMode();
        std::optional<uint32_t> SelectedchannelIndex();
        float ComputeFitZoomFactor();
        void RestorePreviewView();
        void UpdateCommandStates();
        void ShowSaveResultInfoBar(Microsoft::UI::Xaml::Controls::InfoBarSeverity severity, winrt::hstring const& title, winrt::hstring const& message);
        winrt::hstring CurrentStatusText() const;
        winrt::hstring BuildSuggestedSaveFileName() const;
        HWND WindowHandle() const;

        std::vector<ModeDefinition> m_modes;
        winrt::Windows::Graphics::Imaging::SoftwareBitmap m_sourceBitmap{ nullptr };
        std::optional<::image_channel_viewer::imaging::ContinuousPixelBuffer> m_sourcePixels;
        winrt::Windows::Storage::StorageFile m_loadedFile{ nullptr };
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
        uint64_t m_saveRequestId{ 0 };
        bool m_fitPreviewOnNextRefresh{ false };
        bool m_isUpdatingUi{ false };
        bool m_showGrayscale{ false };
        bool m_isSaving{ false };
    };
}

namespace winrt::image_channel_viewer::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}