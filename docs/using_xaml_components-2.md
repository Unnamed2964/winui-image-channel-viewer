> Ported from WinUI 3 Gallery ([microsoft/WinUI-Gallery: This app demonstrates the controls available in WinUI and the Fluent Design System.](https://github.com/microsoft/WinUI-Gallery)):

## ItemTemplate

<Grid>
    <Grid.Resources>
        <DataTemplate x:Key="CustomComboBoxItemTemplate">
            <StackPanel Orientation="Horizontal" Spacing="8">
                <Ellipse
                    Width="8"
                    Height="8"
                    Fill="{ThemeResource AccentFillColorDefaultBrush}" />
                <TextBlock Text="{Binding}" />
            </StackPanel>
        </DataTemplate>
    </Grid.Resources>
    <ComboBox Header="Options" ItemTemplate="{StaticResource CustomComboBoxItemTemplate}" SelectedIndex="0">
        <ComboBox.Items>
            <x:String>Option 1</x:String>
            <x:String>Option 2</x:String>
            <x:String>Option 3</x:String>
        </ComboBox.Items>
    </ComboBox>
</Grid>

## InfoBar

<InfoBar
    IsOpen="True"
    Severity="Success"
    Title="Title"
    Message="Essential app message for your users to be informed of, acknowledge, or take action on." />