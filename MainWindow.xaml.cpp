#include "pch.h"
#include "ContinuousPixelBuffer.h"
#include "ImageProcessing.h"
#include "LocalizationService.h"
#include "MainWindow.xaml.h"
#include "AppVersion.g.h"

#include <filesystem>
#include <algorithm>
#include <ranges>

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace
{
    using BitmapEncoder = winrt::Windows::Graphics::Imaging::BitmapEncoder;
    using ColorMode = winrt::image_channel_viewer::implementation::ColorMode;
    using IBufferByteAccess = ::image_channel_viewer::image_processing::IBufferByteAccess;
    using IMemoryBufferByteAccess = ::image_channel_viewer::image_processing::IMemoryBufferByteAccess;
    using ::image_channel_viewer::image_processing::CreateSoftwareBitmapFromPixels;
    using ::image_channel_viewer::image_processing::RenderPixels;
    using ::image_channel_viewer::localization::LocalizedString;
    using ::image_channel_viewer::localization::StoreLanguagePreference;
    using ::image_channel_viewer::localization::StoredLanguagePreference;

    constexpr std::array<std::wstring_view, 8> kSupportedImageExtensions{
        L".png",
        L".jpg",
        L".jpeg",
        L".bmp",
        L".gif",
        L".tif",
        L".tiff",
        L".webp",
    };

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

    bool IsSupportedImageExtension(std::wstring const& extension)
    {
        auto const normalized = ToLowerCopy(extension);
        return std::ranges::contains(kSupportedImageExtensions, std::wstring_view{ normalized });
    }

    bool IsSupportedImageFile(winrt::Windows::Storage::StorageFile const& file)
    {
        return file && IsSupportedImageExtension(std::wstring{ file.FileType().c_str() });
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
        return ::image_channel_viewer::localization::LocalizedString(std::wstring_view{ resourceId.c_str() });
    }

    winrt::hstring MainWindow::LocalizedString(winrt::hstring const& resourceId, std::initializer_list<winrt::hstring> arguments)
    {
        return ::image_channel_viewer::localization::LocalizedString(
            std::wstring_view{ resourceId.c_str() },
            arguments);
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

    void MainWindow::OnWindowDragOver(
        [[maybe_unused]] IInspectable const& sender,
        Microsoft::UI::Xaml::DragEventArgs const& args)
    {
        if (args.DataView().Contains(Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
        {
            args.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
            return;
        }

        args.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::None);
    }

    void MainWindow::OnWindowDrop(
        [[maybe_unused]] IInspectable const& sender,
        Microsoft::UI::Xaml::DragEventArgs const& args)
    {
        HandleDropAsync(args);
    }

    Windows::Foundation::IAsyncAction MainWindow::LoadImageAsync()
    {
        Windows::Storage::Pickers::FileOpenPicker picker;
        picker.ViewMode(Windows::Storage::Pickers::PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);
        for (auto extension : kSupportedImageExtensions)
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

        co_await LoadImageFileAsync(file);
    }

    Windows::Foundation::IAsyncAction MainWindow::LoadImageFileAsync(Windows::Storage::StorageFile const& file)
    {
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
        ActionResultInfoBar().IsOpen(false);
        EmptyStatePanel().Visibility(Visibility::Collapsed);
        UpdateCommandStates();
        RefreshPreview();
    }

    winrt::fire_and_forget MainWindow::HandleDropAsync(Microsoft::UI::Xaml::DragEventArgs args)
    {
        auto lifetime = get_strong();
        auto deferral = args.GetDeferral();

        try
        {
            auto const dataView = args.DataView();
            if (!dataView.Contains(Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
            {
                args.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::None);
            }
            else
            {
                auto const items = co_await dataView.GetStorageItemsAsync();

                Windows::Storage::StorageFile fileToLoad{ nullptr };
                // use last item that is a supported image file, in case multiple items are dropped
                for (auto const& item : items)
                {
                    auto const file = item.try_as<Windows::Storage::StorageFile>();
                    if (IsSupportedImageFile(file))
                    {
                        fileToLoad = file;
                        break;
                    }
                }

                if (fileToLoad)
                {
                    args.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
                    ActionResultInfoBar().IsOpen(false);
                    co_await LoadImageFileAsync(fileToLoad);
                }
                else
                {
                    args.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::None);
                    ShowActionResultInfoBar(
                        Controls::InfoBarSeverity::Error,
                        LocalizedString(L"OpenResult.FailureTitle"),
                        LocalizedString(L"OpenResult.UnsupportedFileMessage"));
                }
            }
        }
        catch (winrt::hresult_error const& error)
        {
            hstring message = error.message();
            if (message.empty())
            {
                message = LocalizedString(L"OpenResult.FailureMessage");
            }

            ShowActionResultInfoBar(
                Controls::InfoBarSeverity::Error,
                LocalizedString(L"OpenResult.FailureTitle"),
                message);
        }
        catch (...)
        {
            ShowActionResultInfoBar(
                Controls::InfoBarSeverity::Error,
                LocalizedString(L"OpenResult.FailureTitle"),
                LocalizedString(L"OpenResult.FailureMessage"));
        }

        deferral.Complete();
    }

    Windows::Foundation::IAsyncAction MainWindow::SaveCurrentViewAsync()
    {
        auto lifetime = get_strong();
        auto uiThread = winrt::apartment_context();
        auto dispatcherQueue = DispatcherQueue();

        auto renderState = CaptureRenderStateSnapshot();

        if (m_isSaving || !renderState.has_value())
        {
            co_return;
        }

        auto const snapshot = std::move(renderState.value());

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
        ActionResultInfoBar().IsOpen(false);
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
                snapshot.renderRequest,
                reportProgress);

            auto softwareBitmap = CreateSoftwareBitmapFromPixels(
                renderedPixels,
                snapshot.renderRequest.pixelWidth,
                snapshot.renderRequest.pixelHeight);

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
            resultMessage = LocalizedString(L"SaveResult.SuccessMessageFormat", { outputFile.Path() });
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
        ShowActionResultInfoBar(resultSeverity, resultTitle, resultMessage);
    }

    Windows::Foundation::IAsyncAction MainWindow::ShowAboutDialogAsync()
    {
        Controls::ContentDialog dialog;
        dialog.Title(box_value(LocalizedString(L"About.DialogTitleFormat", { LocalizedString(L"Window.Title.AppName") })));
        dialog.PrimaryButtonText(LocalizedString(L"Common.Close"));
        dialog.DefaultButton(Controls::ContentDialogButton::Primary);
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Microsoft::UI::ColorHelper::FromArgb(0xFF, 0x91, 0xD4, 0xE4)));

        Microsoft::UI::Xaml::Controls::StackPanel contentPanel;
        contentPanel.Spacing(12);

        Microsoft::UI::Xaml::Controls::TextBlock versionText;
        versionText.Text(LocalizedString(L"About.VersionFormat", { hstring{ AppVersion } }));
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
        restartDialog.Title(box_value(::image_channel_viewer::localization::EffectiveLocalizedString(L"Settings.RestartDialog.Title")));
        restartDialog.CloseButtonText(::image_channel_viewer::localization::EffectiveLocalizedString(L"Common.Close"));
        restartDialog.DefaultButton(Controls::ContentDialogButton::Close);
        restartDialog.XamlRoot(Content().XamlRoot());

        Controls::TextBlock restartMessage;
        restartMessage.Text(::image_channel_viewer::localization::EffectiveLocalizedString(L"Settings.RestartDialog.Message"));
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

            auto renderState = CaptureRenderStateSnapshot();
            if (!renderState.has_value())
            {
                PreviewProgressHost().Visibility(Visibility::Collapsed);
                co_return;
            }

            auto const snapshot = std::move(renderState.value());

            // resume_background schedules the coroutine continuation 
            // on a thread-pool thread. Unlike JavaScript await, this 
            // await point changes the execution context, so the code 
            // below no longer runs on the UI thread.
            co_await winrt::resume_background();

            auto weakThis = get_weak();
            auto previewPixels = RenderPixels(
                snapshot.renderRequest,
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
                static_cast<int32_t>(snapshot.renderRequest.pixelWidth),
                static_cast<int32_t>(snapshot.renderRequest.pixelHeight));

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

            const hstring statusText = snapshot.renderRequest.selectedMode == ColorMode::Original
                ? snapshot.modeLabel
                : LocalizedString(L"Window.Status.ModeChannelFormat", { snapshot.modeLabel, snapshot.channelLabel });

            hstring windowTitle = snapshot.loadedFileName.empty()
                ? LocalizedString(L"Window.Title.AppName")
                : snapshot.loadedFileName;

            Title(LocalizedString(L"Window.Title.WithStatus", { windowTitle, statusText }));
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

    std::optional<MainWindow::RenderStateSnapshot> MainWindow::CaptureRenderStateSnapshot() const
    {
        if (!m_sourcePixels.has_value() || m_sourcePixels->empty() || m_pixelWidth == 0 || m_pixelHeight == 0)
        {
            return std::nullopt;
        }

        if (m_selectedModeIndex >= m_modes.size())
        {
            return std::nullopt;
        }

        auto const& definition = m_modes.at(m_selectedModeIndex);
        if (m_selectedChannelIndex >= definition.channels.size())
        {
            return std::nullopt;
        }

        return RenderStateSnapshot{
            {
                *m_sourcePixels,
                m_pixelWidth,
                m_pixelHeight,
                definition.mode,
                m_selectedChannelIndex,
                m_showGrayscale,
            },
            definition.label,
            definition.channels.at(m_selectedChannelIndex),
            m_loadedFileName,
        };
    }

    void MainWindow::UpdateCommandStates()
    {
        const bool hasImage = m_sourcePixels.has_value() && !m_sourcePixels->empty();
        SaveAsButton().IsEnabled(hasImage && !m_isSaving);
    }

    void MainWindow::ShowActionResultInfoBar(
        Controls::InfoBarSeverity severity,
        hstring const& title,
        hstring const& message)
    {
        ActionResultInfoBar().IsOpen(false);
        ActionResultInfoBar().Severity(severity);
        ActionResultInfoBar().Title(title);
        ActionResultInfoBar().Message(message);
        ActionResultInfoBar().IsOpen(true);
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
            baseName = ::image_channel_viewer::localization::LocalizedString(L"SaveDialog.DefaultFileBaseName").c_str();
        }
        if (!SaveFormatIndexForExtension(extension).has_value())
        {
            extension = L".png";
        }

        std::wstring modeLabel = ::image_channel_viewer::localization::LocalizedString(L"Mode.Original.Label").c_str();
        std::wstring channelLabel = ::image_channel_viewer::localization::LocalizedString(L"Channel.Original.Label").c_str();
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