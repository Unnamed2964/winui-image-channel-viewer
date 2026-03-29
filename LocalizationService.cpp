#include "pch.h"
#include "LocalizationService.h"

#include "ConfigStore.h"

#include <algorithm>

#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>

using namespace winrt;

namespace
{
    using ResourceLoader = winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceLoader;
    using ResourceManager = winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceManager;
    using ResourceMap = winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceMap;
    using KnownResourceQualifierName = winrt::Microsoft::Windows::ApplicationModel::Resources::KnownResourceQualifierName;

    ResourceManager& AppResourceManager()
    {
        static ResourceManager manager{ ResourceLoader::GetDefaultResourceFilePath() };
        return manager;
    }

    ResourceMap& AppResourceMap()
    {
        static ResourceMap resourceMap = AppResourceManager().MainResourceMap().GetSubtree(L"Resources");
        return resourceMap;
    }

    bool IsSupportedLanguageTag(std::wstring_view languageTag)
    {
        return languageTag == L"zh-CN" || languageTag == L"en-US";
    }

    hstring EffectiveLanguageOverrideTag()
    {
        auto const storedLanguage = ::image_channel_viewer::config::LoadConfiguredLanguagePreference();
        if (!storedLanguage.empty())
        {
            return IsSupportedLanguageTag(storedLanguage.c_str()) ? storedLanguage : hstring{};
        }

        return {};
    }

    winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext CreateResourceContext()
    {
        auto context = AppResourceManager().CreateResourceContext();
        auto const languageTag = EffectiveLanguageOverrideTag();
        if (!languageTag.empty())
        {
            context.QualifierValues().Insert(KnownResourceQualifierName::Language(), languageTag);
        }

        return context;
    }
}

namespace image_channel_viewer::localization
{
    winrt::hstring LocalizedString(std::wstring_view resourceId)
    {
        try
        {
            std::wstring normalizedResourceId{ resourceId };
            std::replace(normalizedResourceId.begin(), normalizedResourceId.end(), L'.', L'/');

            auto const context = CreateResourceContext();
            auto const candidate = AppResourceMap().TryGetValue(hstring{ normalizedResourceId }, context);
            if (candidate)
            {
                hstring const value = candidate.ValueAsString();
                if (!value.empty())
                {
                    return value;
                }
            }

            return hstring{ resourceId };
        }
        catch (...)
        {
            return hstring{ resourceId };
        }
    }

    winrt::hstring FormatLocalizedString(std::wstring_view resourceId, std::initializer_list<winrt::hstring> arguments)
    {
        std::wstring formatted = LocalizedString(resourceId).c_str();
        size_t argumentIndex = 0;

        for (auto const& argument : arguments)
        {
            std::wstring const token = L"{" + std::to_wstring(argumentIndex) + L"}";
            size_t searchIndex = 0;

            while ((searchIndex = formatted.find(token, searchIndex)) != std::wstring::npos)
            {
                formatted.replace(searchIndex, token.size(), argument.c_str());
                searchIndex += argument.size();
            }

            ++argumentIndex;
        }

        return hstring{ formatted };
    }

    winrt::hstring StoredLanguagePreference()
    {
        return ::image_channel_viewer::config::LoadConfiguredLanguagePreference();
    }

    void StoreLanguagePreference(winrt::hstring const& languageTag)
    {
        ::image_channel_viewer::config::SaveConfiguredLanguagePreference(languageTag);
    }
}