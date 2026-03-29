#pragma once

#include <filesystem>
#include <string_view>

#include <winrt/base.h>

namespace image_channel_viewer
{
    winrt::hstring LoadConfiguredLanguagePreference();
    void SaveConfiguredLanguagePreference(winrt::hstring const& languageTag);
}
