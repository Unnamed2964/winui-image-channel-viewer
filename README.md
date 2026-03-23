本项目是一个基于 Modern C++ (>= C++20) + C++/WinRT + WinUI 3 的图片通道查看器，界面风格偏向现代 Windows 11 桌面应用。

目前已实现：

- 选择并加载常见图片格式：PNG、JPEG、BMP、GIF、TIFF、WebP
- 颜色模式切换：原图、RGB、HSL、HSV、CMYK、LAB
- 通道切换：
	- RGB: R、G、B
	- HSL: H、S、L
	- HSV: H、S、V
	- CMYK: C、M、Y、K
	- LAB: L、a、b
- 主体区域实时显示所选通道对应的预览结果
- 对 RGB 和 CMYK 通道提供“黑白显示”切换
	- 关闭时显示对应纯色亮度变化
	- 开启时显示灰度强度变化

这类似于 PhotoShop 等软件中单独查看 R、G、B 通道的体验，但这里扩展到了更多颜色空间。

## 项目结构

- image_channel_viewer.sln: Visual Studio 解决方案
- image_channel_viewer.vcxproj: WinUI 3 C++/WinRT 项目文件
- MainWindow.xaml: 主界面布局
- MainWindow.xaml.cpp: 图片加载、颜色空间转换与通道渲染逻辑
- App.xaml / App.xaml.cpp: 应用入口

## 构建环境

建议环境：

- Windows 11
- Visual Studio 2026 / 2022 Preview 或兼容的 MSBuild 18
- 已安装 WinUI / Windows App SDK 对应桌面开发工作负载
- Windows SDK 10.0.26100 或更高版本

项目使用 NuGet 自动还原以下依赖：

- Microsoft.WindowsAppSDK
- Microsoft.Windows.CppWinRT
- Microsoft.Windows.SDK.BuildTools
- Microsoft.Windows.ImplementationLibrary

## 构建方式

1. 使用 Visual Studio 打开 image_channel_viewer.sln
2. 选择 Debug | x64 或 Release | x64
3. 直接生成并运行

也可以通过 MSBuild 构建：

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe" ".\image_channel_viewer.sln" /restore /t:Build /p:Configuration=Debug /p:Platform=x64
```

## 当前实现说明

- RGB 和 CMYK 通道支持彩色/灰度两种观察方式
- HSL / HSV 中的 H 通道使用高饱和色相预览
- LAB 中的 L、a、b 当前采用单通道可视化映射
- 预览图像在内存中转换为 BGRA8 后再进行逐像素渲染

## 已验证

已在当前机器上通过以下命令成功构建：

```powershell
MSBuild.exe .\image_channel_viewer.sln /t:Build /p:Configuration=Debug /p:Platform=x64
```========================

nent_viewer.sln /t:Build /p:Configuration=Debug /p:Platform=x64
```