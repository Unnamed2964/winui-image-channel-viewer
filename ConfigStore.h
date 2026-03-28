#pragma once

#include <windows.h>
#include <shlobj_core.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>

#include <winrt/base.h>
#include <winrt/Windows.Data.Json.h>
#include <wil/resource.h>

namespace image_channel_viewer
{
    struct AppConfig
    {
        uint32_t version{ 1 };
        std::string languageTag;
    };

    constexpr uint32_t CurrentConfigVersion = 1;

    [[noreturn]] inline void ThrowConfigError(std::filesystem::path const& configPath, std::wstring_view reason)
    {
        std::wstring message = L"Configuration file error: ";
        message += reason;
        if (!configPath.empty())
        {
            message += L" (";
            message += configPath.wstring();
            message += L")";
        }

        throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), message);
    }

    template<typename T>
    T config_cast(winrt::Windows::Data::Json::IJsonValue const& value);

    template<>
    inline uint32_t config_cast<uint32_t>(winrt::Windows::Data::Json::IJsonValue const& value)
    {
        if (value.ValueType() != winrt::Windows::Data::Json::JsonValueType::Number)
        {
            throw std::bad_cast{};
        }

        auto const number = value.GetNumber();
        if (number <= 0.0 ||
            number > static_cast<double>(std::numeric_limits<uint32_t>::max()) ||
            std::floor(number) != number)
        {
            throw std::out_of_range{ "value is not representable as uint32_t" };
        }

        return static_cast<uint32_t>(number);
    }

    template<>
    inline std::string config_cast<std::string>(winrt::Windows::Data::Json::IJsonValue const& value)
    {
        if (value.ValueType() != winrt::Windows::Data::Json::JsonValueType::String)
        {
            throw std::bad_cast{};
        }

        return winrt::to_string(value.GetString());
    }

    inline std::filesystem::path ConfigRootPath()
    {
        wil::unique_cotaskmem_string localAppDataPath;
        const HRESULT result = ::SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            KF_FLAG_DEFAULT,
            nullptr,
            wil::out_param(localAppDataPath));
        if (FAILED(result) || !localAppDataPath)
        {
            return {};
        }

        std::filesystem::path configRoot{ localAppDataPath.get() };
        configRoot /= L"image_channel_viewer";
        return configRoot;
    }

    inline std::filesystem::path ConfigFilePath()
    {
        auto configRoot = ConfigRootPath();
        if (configRoot.empty())
        {
            return {};
        }

        return configRoot / L"settings.json";
    }

    inline bool EnsureConfigDirectory(std::filesystem::path const& configPath)
    {
        std::error_code errorCode;
        std::filesystem::create_directories(configPath.parent_path(), errorCode);
        return !errorCode;
    }

    inline bool WriteConfigFile(std::filesystem::path const& configPath, AppConfig const& config)
    {
        if (configPath.empty() || !EnsureConfigDirectory(configPath))
        {
            return false;
        }

        std::ofstream output(configPath, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            return false;
        }

        winrt::Windows::Data::Json::JsonObject jsonObject;
        jsonObject.SetNamedValue(L"version", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(config.version));
        jsonObject.SetNamedValue(L"language", winrt::Windows::Data::Json::JsonValue::CreateStringValue(winrt::to_hstring(config.languageTag)));

        output << winrt::to_string(jsonObject.Stringify());
        return output.good();
    }

    inline winrt::Windows::Data::Json::JsonObject MigrateConfigJsonToCurrent(
        winrt::Windows::Data::Json::JsonObject jsonObject,
        std::filesystem::path const& configPath)
    {
        // nothing to do now

        return jsonObject;
    }

    inline uint32_t ReadConfigVersion(
        winrt::Windows::Data::Json::JsonObject const& jsonObject,
        std::filesystem::path const& configPath)
    {
        if (!jsonObject.HasKey(L"version"))
        {
            ThrowConfigError(configPath, L"missing required 'version' field");
        }
        try
        {
            return config_cast<uint32_t>(jsonObject.GetNamedValue(L"version"));
        }
        catch (std::bad_cast const&)
        {
            ThrowConfigError(configPath, L"'version' must be a number");
        }
        catch (std::out_of_range const&)
        {
            ThrowConfigError(configPath, L"'version' must be a positive 32-bit integer");
        }
    }

    inline AppConfig ReadCurrentConfig(
        winrt::Windows::Data::Json::JsonObject const& jsonObject,
        std::filesystem::path const& configPath)
    {
        auto const configVersion = ReadConfigVersion(jsonObject, configPath);
        if (configVersion != CurrentConfigVersion)
        {
            if (configVersion > CurrentConfigVersion)
            {
                ThrowConfigError(configPath, L"configuration version is newer than this application supports");
            }

            ThrowConfigError(configPath, L"configuration was not migrated to the current version");
        }

        AppConfig config{};
        config.version = configVersion;

        if (jsonObject.HasKey(L"language"))
        {
            try
            {
                config.languageTag = config_cast<std::string>(jsonObject.GetNamedValue(L"language"));
            }
            catch (std::bad_cast const&)
            {
                ThrowConfigError(configPath, L"'language' must be a string");
            }
        }

        return config;
    }

    inline AppConfig LoadOrCreateConfig()
    {
        auto const configPath = ConfigFilePath();
        if (configPath.empty())
        {
            throw winrt::hresult_error(E_FAIL, L"Unable to resolve the configuration file path.");
        }

        auto defaultConfig = AppConfig{ CurrentConfigVersion, {} };
        if (!std::filesystem::exists(configPath))
        {
            WriteConfigFile(configPath, defaultConfig);

            return defaultConfig;
        }

        std::ifstream input(configPath, std::ios::binary);
        if (!input.is_open())
        {
            throw winrt::hresult_error(E_FAIL, L"Unable to open the configuration file.");
        }

        std::string jsonText{
            std::istreambuf_iterator<char>{ input },
            std::istreambuf_iterator<char>{}
        };
        if (jsonText.empty())
        {
            ThrowConfigError(configPath, L"configuration file is empty");
        }

        bool shouldRewrite = false;

        auto jsonObject = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(jsonText));
        auto const originalVersion = ReadConfigVersion(jsonObject, configPath);
        if (originalVersion < CurrentConfigVersion)
        {
            jsonObject = MigrateConfigJsonToCurrent(jsonObject, configPath);
            shouldRewrite = true;
        }

        auto const config = ReadCurrentConfig(jsonObject, configPath);

        if (shouldRewrite)
        {
            WriteConfigFile(configPath, config);
        }

        return config;
    }

    inline winrt::hstring LoadConfiguredLanguagePreference()
    {
        return winrt::to_hstring(LoadOrCreateConfig().languageTag);
    }

    inline void SaveConfiguredLanguagePreference(winrt::hstring const& languageTag)
    {
        auto const configPath = ConfigFilePath();
        if (configPath.empty())
        {
            throw winrt::hresult_error(E_FAIL, L"Unable to resolve the configuration file path.");
        }

        auto config = LoadOrCreateConfig();
        config.version = CurrentConfigVersion;
        config.languageTag = winrt::to_string(languageTag);
        WriteConfigFile(configPath, config);
    }
}
