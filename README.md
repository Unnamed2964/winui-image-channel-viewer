# 图片通道查看器

## 简介

本项目是一个基于 Modern C++ (>= C++20) + C++/WinRT + WinUI 3 的图片通道查看器，现代 Windows 11 风格的桌面应用。

## 运行截图

<img width="898.5" height="549.5" alt="image" src="https://github.com/user-attachments/assets/0621721d-d21d-4abe-aed9-48e3da5c4e58" />

## 功能

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
& "C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe" ".\image_channel_viewer.sln" /restore /t:Build /p:Configuration=Release /p:Platform=x64
```

## 附注

本项目许多代码由 Vibe Coding 生成，你可以在提交记录中查阅主要的 prompt。但是本人依然对代码的最终质量负责。