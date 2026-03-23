#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::image_component_viewer::implementation
{
    App::App()
    {
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& eventArgs)
        {
            if (IsDebuggerPresent())
            {
                auto message = eventArgs.Message();
                (void)message;
                __debugbreak();
            }
        });
#endif
    }

    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& launchArgs)
    {
        (void)launchArgs;
        m_window = make<MainWindow>();
        m_window.Activate();
    }
}