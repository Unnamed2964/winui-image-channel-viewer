#pragma once

#include "MainWindow.g.h"

namespace winrt::image_component_viewer::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow()
        {
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::image_component_viewer::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
