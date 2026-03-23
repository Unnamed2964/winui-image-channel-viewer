# Git 版本号、自动写入与自动发布方案记录

## 用户问题

请介绍如何实现：
1. git上使用语义化版本号；
2. 构建脚本自动写入相关文件的位置；
2. 自动编译发布release版本。

## 回答全文

可以，针对你这个 WinUI 3 / C++ 项目，比较稳妥的实现方式是：

1. 用 Git tag 作为发布版本的唯一来源
2. 构建前由脚本把版本写入应用真正使用的位置
3. 由 CI 在打 tag 时自动编译 Release 并发布 GitHub Release

你现在的版本号实际上有两处人工维护点：
[MainWindow.xaml.cpp](MainWindow.xaml.cpp#L13)
[app.manifest](app.manifest#L3)

这正是后面容易不同步的根源。解决思路就是把这两处都变成“构建产物”，不要手改。

**一、Git 上使用语义化版本号**
建议直接使用 SemVer 约定：

1. 稳定版本：
   v1.0.0
   v1.0.1
   v1.1.0
   v2.0.0

2. 预发布版本：
   v1.1.0-beta.1
   v1.1.0-rc.1

3. 规则：
   破坏兼容改动时升 major
   向后兼容功能新增时升 minor
   纯修复时升 patch

对这个项目，最实用的发布流程是：

    git tag -a v1.2.0 -m "Release v1.2.0"
    git push origin v1.2.0

然后 CI 监听这种 tag，自动构建并发布。

有一个 Windows 侧需要注意的点：
SemVer 是三段，app.manifest 里的 version 是四段数字。

也就是说你需要一个映射规则，比如：

1. Git tag：
   v1.2.3

2. Manifest 版本：
   1.2.3.0

这是最简单、最稳的映射。

如果以后要支持 beta 或 rc：
1. Git tag 仍然可以是 v1.2.3-beta.1
2. manifest 仍写 1.2.3.0
3. “About” 对话框显示的字符串版本可以保留完整的 v1.2.3-beta.1

这样既满足 Windows 数字版本要求，也不丢失语义化版本信息。

**二、构建脚本自动写入哪些文件**
你当前至少有两处应该由脚本管理：

1. [app.manifest](app.manifest#L3)
作用：Windows 清单版本

2. [MainWindow.xaml.cpp](MainWindow.xaml.cpp#L13)
作用：About 对话框显示版本

但我不建议继续直接改 [MainWindow.xaml.cpp](MainWindow.xaml.cpp#L13) 这一行。
更好的做法是：

1. 保留 [app.manifest](app.manifest#L3) 作为最终清单落点
2. 新生成一个头文件，比如 AppVersion.g.h
3. 在 [MainWindow.xaml.cpp](MainWindow.xaml.cpp#L13) 的位置改成 include 这个生成文件，而不是手写 constexpr

推荐结构：

1. 新增一个脚本
   scripts/update-version.ps1

2. 新增一个模板或生成目标
   Generated Files/AppVersion.g.h
   或者项目根目录下的 AppVersion.g.h

3. 构建前执行脚本：
   读取 Git tag
   解析 SemVer
   生成 display version
   生成 manifest version
   更新 app.manifest
   生成 AppVersion.g.h

脚本的职责建议拆成这几步：

1. 获取版本源
   优先取当前 tag
   没有 tag 时回退到 git describe
   再不行回退到 0.0.0-dev

2. 解析版本
   例如把 v1.2.3 解析成：
   semver = 1.2.3
   manifestVersion = 1.2.3.0
   displayVersion = v1.2.3

3. 写入 [app.manifest](app.manifest#L3)
   把 assemblyIdentity 的 version 更新为 1.2.3.0

4. 生成头文件
   例如写出：
   inline constexpr wchar_t AppVersion[] = L"v1.2.3";

这样 About 对话框始终显示脚本生成的版本，而不是手工维护。

**三、如何把脚本接入 MSBuild**
你这个项目本来就是用 MSBuild 构建的，见 [README.md](README.md#L61) 和 [image_channel_viewer.vcxproj](image_channel_viewer.vcxproj)。

最自然的方式是在 [image_channel_viewer.vcxproj](image_channel_viewer.vcxproj) 里加一个构建前目标：

1. BeforeBuild
2. 或者 BeforeClCompile

逻辑上类似：

    Target Name="GenerateVersionInfo" BeforeTargets="BeforeBuild"
      Exec Command="pwsh -ExecutionPolicy Bypass -File scripts/update-version.ps1"
    EndTarget

脚本执行后：
1. 更新 manifest
2. 生成 AppVersion.g.h
3. 然后正常进入编译

这样无论你在 Visual Studio 里点生成，还是命令行跑 MSBuild，版本写入都会自动发生。

**四、建议的版本来源策略**
我建议分成两类构建：

1. 本地开发构建
用于日常调试，不一定有正式 tag

建议显示：
v1.2.3-dev+abcdef0
或者
0.0.0-dev+abcdef0

2. 正式发布构建
只在 tag 上触发

建议显示：
v1.2.3

这样你能一眼区分：
1. 本地临时构建
2. 正式发布版本

**五、自动编译并发布 Release 版本**
最适合你的方案是 GitHub Actions。

建议新增一个工作流：
.github/workflows/release.yml

触发条件：
1. push tag
2. tag 格式匹配 v*.*.*

流程建议：

1. checkout 仓库
2. 安装 MSBuild / Visual Studio build tools
3. restore NuGet 包
4. 运行版本生成脚本
5. 构建 Release x64
6. 打包输出目录
7. 创建 GitHub Release
8. 上传 zip 作为 release asset

你的 Release 构建命令可以直接沿用 [README.md](README.md#L61) 的模式：

    MSBuild.exe image_channel_viewer.sln /restore /t:Build /p:Configuration=Release /p:Platform=x64

如果只发 x64，已经足够简单。
如果以后要发多架构，可以扩成：

1. Release x64
2. Release ARM64

然后分别上传：
1. image_channel_viewer-v1.2.3-win-x64.zip
2. image_channel_viewer-v1.2.3-win-arm64.zip

**六、GitHub Release 里放什么**
建议自动发布时附上：

1. 主程序目录压缩包
2. 版本号
3. 变更摘要
4. 提交 SHA

如果你不想先做自动生成 changelog，第一版可以只做最小可用版本：

1. tag 触发构建
2. 上传 zip
3. Release 标题使用 tag 名

后面再补 changelog 自动生成。

**七、你这个仓库最推荐的落地方案**
按你当前结构，我建议这样实施，最少改动、收益最高：

1. 保留 [app.manifest](app.manifest#L3) 作为最终写入位置
2. 去掉 [MainWindow.xaml.cpp](MainWindow.xaml.cpp#L13) 的手写版本号常量
3. 增加一个由脚本生成的 AppVersion.g.h
4. 在 [image_channel_viewer.vcxproj](image_channel_viewer.vcxproj) 里加 BeforeBuild 调用 PowerShell
5. 用 Git tag 作为正式版本源
6. 用 GitHub Actions 在 tag push 时自动构建 Release 和发 Release

**八、一个实用的版本规则样例**
你可以直接采用下面这套约定：

1. 开发分支日常构建
显示：
v1.3.0-dev+abc1234

2. 准备发布
打 tag：
v1.3.0

3. 构建脚本自动生成：
manifest version:
1.3.0.0

about dialog version:
v1.3.0

4. GitHub Actions 自动发布：
Release 标题：
v1.3.0

资产名：
image_channel_viewer-v1.3.0-win-x64.zip

**九、为什么这套方案适合你现在的项目**
因为你现在已经具备这些条件：

1. 项目是标准 MSBuild 工程
见 [image_channel_viewer.vcxproj](image_channel_viewer.vcxproj)

2. 已经有清单版本入口
见 [app.manifest](app.manifest#L3)

3. 已经有 About 对话框显示版本的入口
见 [MainWindow.xaml.cpp](MainWindow.xaml.cpp#L455)
见 [MainWindow.xaml.cpp](MainWindow.xaml.cpp#L467)

4. 已经有稳定的 Release 构建命令
见 [README.md](README.md#L61)

所以你并不需要引入复杂的第三方版本系统。
一个 PowerShell 脚本加一个 GitHub Actions 工作流就够了。

**十、实施顺序**
如果按工程效率排优先级，我建议你这样做：

1. 先统一版本来源
把 [MainWindow.xaml.cpp](MainWindow.xaml.cpp#L13) 改成读取生成头文件

2. 再加本地构建脚本
让每次 Build 前都自动写 manifest 和版本头文件

3. 最后接 GitHub Actions
只在 tag push 时构建 Release 并发布

这样每一步都能单独验证，不会一下子把发布链路搞得太复杂。

如果你要，我下一步可以直接替你把这套方案落到仓库里，最小实现我会做这三件事：

1. 新增 PowerShell 版本生成脚本
2. 修改 [image_channel_viewer.vcxproj](image_channel_viewer.vcxproj) 在构建前自动执行脚本
3. 新增 GitHub Actions 工作流，在推送 vX.Y.Z tag 时自动构建并发布