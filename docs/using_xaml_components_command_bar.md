> Ported from WinUI 3 Gallery ([microsoft/WinUI-Gallery: This app demonstrates the controls available in WinUI and the Fluent Design System.](https://github.com/microsoft/WinUI-Gallery)):

## CommandBar

```xaml
<CommandBar Background="Transparent" IsOpen="False" DefaultLabelPosition="Right">
    <AppBarButton Icon="Add" Label="Add"/>
    <AppBarButton Icon="Edit" Label="Edit"/>
    <AppBarButton Icon="Share" Label="Share"/>
    <CommandBar.SecondaryCommands>
        <AppBarButton Icon="Setting" Label="Settings">
            <AppBarButton.KeyboardAccelerators>
                    <KeyboardAccelerator Modifiers="Control" Key="I" />
            </AppBarButton.KeyboardAccelerators>
        </AppBarButton>
    </CommandBar.SecondaryCommands>
</CommandBar>
```

## An AppBarButton with a MenuFlyout
```xaml
<AppBarButton Icon="Sort" IsCompact="True" ToolTipService.ToolTip="Sort" AutomationProperties.Name="Sort">
    <AppBarButton.Flyout>
        <MenuFlyout>
            <MenuFlyoutItem Text="By rating" Click="MenuFlyoutItem_Click" Tag="rating"/>
            <MenuFlyoutItem Text="By match" Click="MenuFlyoutItem_Click" Tag="match"/>
            <MenuFlyoutItem Text="By distance" Click="MenuFlyoutItem_Click" Tag="distance"/>
        </MenuFlyout>
    </AppBarButton.Flyout>
</AppBarButton>
```