# 图片通道查看器

## 简介

本项目是一个基于 Modern C++ (>= C++20) + C++/WinRT + WinUI 3 的图片通道查看器，现代 Windows 11 风格，采用 Mica 效果的桌面打包应用（MSIX）。

**下载：** [Releases · Unnamed2964/winui-image-channel-viewer](https://github.com/Unnamed2964/winui-image-channel-viewer/releases)

## 运行截图

<img width="1069" height="660" alt="screenshot-resized" src="https://github.com/user-attachments/assets/0cfae48a-8047-4ab8-9dd6-3a4471cf3a96" />

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
- image_channel_viewer.vcxproj: WinUI 3 C++/WinRT 单项目 MSIX 工程
- Package.appxmanifest: MSIX 包清单
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

如果要生成可分发的 MSIX 包，可以使用 Visual Studio 的 Package and Publish 菜单，或者使用下面的 MSBuild 命令：

```powershell
& MSBuild.exe ".\image_channel_viewer.sln" /restore /t:Build /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:AppxPackageSigningEnabled=false
```

上面的命令会生成未签名的 sideload MSIX，仅适合本地测试或手动分发。

如果要生成可直接安装的已签名 MSIX，你需要自行提供代码签名证书 `.pfx` 文件，仓库不会附带该文件。可以在本机自建自签名证书，或者使用外部 CA 签发的正式证书。带签名的本地构建命令示例如下：

```powershell
& MSBuild.exe ".\image_channel_viewer.sln" /restore /t:Build /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:PackageCertificateKeyFile="D:\certs\yourname.pfx" /p:PackageCertificatePassword="你的证书密码"
```

请确保 `Package.appxmanifest` 中的 `Identity Publisher` 与证书主题一致，例如证书主题是 `CN=Unnamed2964`，那么包清单中的 Publisher 也必须是 `CN=Unnamed2964`，否则签名或安装会失败。

构建前会自动执行版本生成脚本：

- 从 Git tag 或提交信息推导显示版本号
- 自动更新 app.manifest 和 Package.appxmanifest 中的版本号
- 自动生成供应用读取的版本头文件

请参见 [Git 版本号、自动写入与自动发布方案记录](./docs/git-versioning-and-release-notes.md) 中的 GPT 问答记录

也可以通过 MSBuild 执行普通构建（请确保 MSBuild.exe 所在目录在 PATH 中）：

```powershell
&MSBuild.exe ".\image_channel_viewer.sln" /restore /t:Build /p:Configuration=Release /p:Platform=x64
```

如果要保存向量化报告等编译器输出信息，请追加
```powershell
| Select-String "向量化" | Out-File verctorize-log.txt
```

## 版本号与发布

项目使用 Git tag 作为发布版本来源，建议采用语义化版本号：

- v1.0.0
- v1.0.1
- v1.1.0
- v2.0.0

发布一个正式版本的基本流程：

```powershell
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

版本生成脚本会把：

- Git 版本 v1.2.3 映射成清单版本 1.2.3.0
- About 对话框显示版本写成 v1.2.3

如果当前提交不在正式 tag 上，脚本会生成 dev 版本，例如：

- v1.2.3-dev+4.gabc1234
- 0.0.0-dev+abc1234

仓库包含一个 GitHub Actions 发布工作流：

- 当推送 vX.Y.Z tag 时自动构建各平台 Release 包
- 自动生成包含 MSIX、依赖包和安装脚本的压缩包
- 自动创建 GitHub Release 并上传 zip 资产

如果要让 GitHub Actions 生成已签名的 MSIX，不要把 `.pfx` 文件直接提交到仓库，而是应当在仓库 Secrets 中提供：

- `MSIX_CERT_BASE64`: 证书 `.pfx` 文件的 Base64 内容
- `MSIX_CERT_PASSWORD`: 证书密码

工作流会在运行时把证书还原到临时目录后再参与构建。若未配置这两个 Secrets，则当前工作流会退回到生成未签名的 sideload MSIX。

## 附注

本项目许多代码由 Vibe Coding 生成，你可以在提交记录中查阅主要的 prompt。但是本人依然对代码的最终质量负责。但是上一句话并不是在意味着或暗示取消 MIT 协议中 “THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, ” 所开头的一段，或者恢复或重新包含那一段所排除的一部分保证。
