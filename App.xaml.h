#pragma once

#include "App.xaml.g.h"

namespace winrt::image_component_viewer::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const& launchArgs);

    private:
        winrt::Microsoft::UI::Xaml::Window m_window{ nullptr };
    };
}
