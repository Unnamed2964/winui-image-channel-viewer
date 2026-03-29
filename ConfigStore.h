#pragma once

#include <filesystem>
#include <string_view>

#include <winrt/base.h>

namespace image_channel_viewer::config
{
    winrt::hstring LoadConfiguredLanguagePreference();
    void SaveConfiguredLanguagePreference(winrt::hstring const& languageTag);
}
