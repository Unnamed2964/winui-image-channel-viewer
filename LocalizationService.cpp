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

    hstring const& RuntimeLanguageOverrideTag()
    {
        static hstring runtimeLanguage = EffectiveLanguageOverrideTag();
        return runtimeLanguage;
    }

    winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext CreateResourceContext(hstring const& languageTag)
    {
        auto context = AppResourceManager().CreateResourceContext();
        if (!languageTag.empty())
        {
            context.QualifierValues().Insert(KnownResourceQualifierName::Language(), languageTag);
        }

        return context;
    }

    winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext CreateRuntimeResourceContext()
    {
        return CreateResourceContext(RuntimeLanguageOverrideTag());
    }

    winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext CreateEffectiveResourceContext()
    {
        return CreateResourceContext(EffectiveLanguageOverrideTag());
    }

    winrt::hstring LocalizedStringCore(
        std::wstring_view resourceId,
        winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext const& context)
    {
        std::wstring normalizedResourceId{ resourceId };
        std::replace(normalizedResourceId.begin(), normalizedResourceId.end(), L'.', L'/');

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

    winrt::hstring FormatLocalizedStringCore(
        winrt::hstring const& formatString,
        std::initializer_list<winrt::hstring> arguments)
    {
        // std::v_format is intentionally not used because it may introduce unexpected
        // behaviors and potential attack surfaces with an externally constructed format 
        // strings and an ill-written custom global std::formatter.
        
        std::wstring formatted = formatString.c_str();
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
}

namespace image_channel_viewer::localization
{
    void InitializeRuntimeLanguage()
    {
        (void)RuntimeLanguageOverrideTag();
    }

    winrt::hstring LocalizedString(std::wstring_view resourceId)
    {
        try
        {
            return LocalizedStringCore(resourceId, CreateRuntimeResourceContext());
        }
        catch (...)
        {
            return hstring{ resourceId };
        }
    }

    winrt::hstring LocalizedString(std::wstring_view resourceId, std::initializer_list<winrt::hstring> arguments)
    {
        return FormatLocalizedStringCore(LocalizedString(resourceId), arguments);
    }

    winrt::hstring EffectiveLocalizedString(std::wstring_view resourceId)
    {
        try
        {
            return LocalizedStringCore(resourceId, CreateEffectiveResourceContext());
        }
        catch (...)
        {
            return hstring{ resourceId };
        }
    }

    winrt::hstring EffectiveLocalizedString(std::wstring_view resourceId, std::initializer_list<winrt::hstring> arguments)
    {
        return FormatLocalizedStringCore(EffectiveLocalizedString(resourceId), arguments);
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