# 图片通道查看器

[![GitHub Release](https://img.shields.io/github/v/release/Unnamed2964/winui-image-channel-viewer?display_name=tag)](https://github.com/Unnamed2964/winui-image-channel-viewer/releases) [![License: MIT](https://img.shields.io/github/license/Unnamed2964/winui-image-channel-viewer)](https://github.com/Unnamed2964/winui-image-channel-viewer/blob/master/LICENSE) ![Platform](https://img.shields.io/badge/platform-Windows%2011%2B-0078D6) ![C++](https://img.shields.io/badge/C%2B%2B-23-00599C) ![UI](https://img.shields.io/badge/UI-WinUI%203-0a7cff)

一个基于 Modern C++、C++/WinRT 和 WinUI 3 的 Windows 桌面应用，用来查看图片在不同颜色空间下的单独通道。

它提供接近图像编辑软件中单通道预览的体验，但把范围扩展到了 RGB、HSL、HSV、CMYK 和 LAB 等颜色空间，并采用贴近 Windows 11 的界面风格与 Mica 背景效果。

下载地址：[Releases · Unnamed2964/winui-image-channel-viewer](https://github.com/Unnamed2964/winui-image-channel-viewer/releases)

## 运行截图

![App in Light Mode](https://github.com/user-attachments/assets/8adf4e4e-e35e-4c25-b694-b40bd05201ed)
![viewing CMYK C channel](https://github.com/user-attachments/assets/797d4e85-cb41-4cd4-a5be-a9daff328a97)
![switch language](https://github.com/user-attachments/assets/7b99e3df-1263-464c-982f-eca6bb71c48f)
![App in Dark Mode](https://github.com/user-attachments/assets/f3faedce-025f-42b8-9bbf-6a137a8fe468)

## 功能

- 加载常见图片格式：PNG、JPEG、BMP、GIF、TIFF、WebP
- 切换颜色模式：原图、RGB、HSL、HSV、CMYK、LAB
- 查看各颜色模式对应通道：
  - RGB：R、G、B
  - HSL：H、S、L
  - HSV：H、S、V
  - CMYK：C、M、Y、K
  - LAB：L、a、b
- 实时刷新预览区域，显示当前模式与通道对应的结果
- 对 RGB 与 CMYK 通道提供彩色显示 / 灰度显示切换
- 支持缩放、滚动和拖拽查看较大的预览图
- 支持将当前预览结果另存为图片
- 内置中英文界面资源

## 项目结构

- image_channel_viewer.sln：Visual Studio 解决方案
- image_channel_viewer.vcxproj：WinUI 3 C++/WinRT 项目文件
- MainWindow.xaml：主窗口布局
- MainWindow.xaml.cpp：主界面交互、文件选择、预览刷新、保存导出等逻辑
- ImageProcessing.h / ImageProcessing.cpp：颜色空间转换与通道处理逻辑
- ConfigStore.h / ConfigStore.cpp：配置读写与持久化
- LocalizationService.h / LocalizationService.cpp：本地化资源读取与语言偏好处理
- App.xaml / App.xaml.cpp：应用入口
- scripts/update-version.ps1：构建前版本生成脚本

## 构建环境

推荐环境：

- Windows 11
- Visual Studio 2022 或更新版本
- 已安装适用于 C++ 的桌面开发工作负载，以及 WinUI / Windows App SDK 相关组件
- Windows SDK 10.0.26100 或更高版本

项目当前使用：

- C++23
- C++/WinRT
- WinUI 3
- Windows App SDK

NuGet 依赖会在构建时自动还原，主要包括：

- Microsoft.WindowsAppSDK
- Microsoft.Windows.CppWinRT
- Microsoft.Windows.SDK.BuildTools
- Microsoft.Windows.ImplementationLibrary

## 构建方式

### 使用 Visual Studio

1. 打开 image_channel_viewer.sln
2. 选择目标配置，例如 Debug | x64 或 Release | x64
3. 直接生成并运行

项目文件同时包含 x86、x64 和 ARM64 配置；日常开发通常使用 x64。

### 使用 MSBuild

请确保 MSBuild.exe 已在 PATH 中，然后执行：

```powershell
& MSBuild.exe ".\image_channel_viewer.sln" /restore /t:Build /p:Configuration=Release /p:Platform=x64
```

### 构建前自动步骤

每次构建前会自动执行 scripts/update-version.ps1，用于：

- 根据 Git tag 或提交状态推导显示版本号
- 更新 app.manifest 中的清单版本
- 生成供应用读取的版本头文件

相关设计记录见 [docs/git-versioning-and-release-notes.md](./docs/git-versioning-and-release-notes.md)。

## 版本号与发布

项目使用 Git tag 作为发布版本来源，采用语义化版本号：

- v1.0.0
- v1.0.1
- v1.1.0
- v2.0.0

发布一个正式版本的基本流程：

```powershell
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

版本生成脚本会将：

- Git 版本 v1.2.3 映射为清单版本 1.2.3.0
- About 对话框显示版本写为 v1.2.3

如果当前提交不在正式 tag 上，则会生成 dev 版本，例如：

- v1.2.3-dev+4.gabc1234
- 0.0.0-dev+abc1234

仓库包含 GitHub Actions 发布工作流：

- 当推送 vX.Y.Z tag 时自动构建 Release 版本
- 自动生成 x86、x64、ARM64 三种架构的 zip 包
- 自动创建 GitHub Release 并上传构建产物

## 许可证

本项目采用 MIT License，详见 [LICENSE](./LICENSE)。

## 附注

项目中有一部分代码和文档整理受 Vibe Coding 工作流辅助生成，但仓库维护者对最终提交内容负责。相关讨论和部分 prompt 可以在提交记录与 docs 目录中找到。
