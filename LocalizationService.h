#pragma once

#include <initializer_list>
#include <string_view>

#include <winrt/Windows.Foundation.h>

namespace image_channel_viewer::localization
{
    winrt::hstring LocalizedString(std::wstring_view resourceId);
    winrt::hstring FormatLocalizedString(std::wstring_view resourceId, std::initializer_list<winrt::hstring> arguments);
    winrt::hstring StoredLanguagePreference();
    void StoreLanguagePreference(winrt::hstring const& languageTag);
}