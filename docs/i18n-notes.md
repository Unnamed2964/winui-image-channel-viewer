> 问：请问本项目如何实现i18n最符合winui+winrt开发最佳实践？

> 答（GPT5.4）：

正在优化工具选择...我先看一下这个仓库当前有没有本地化相关实现，以及项目里已有的 WinUI/WinRT 约束，再给出针对这个项目的做法，而不是泛泛而谈。

搜索正则表达式 `x:Uid|ResourceLoader|resw|Resources\\|ApplicationLanguages|PrimaryLanguageOverride`，200 个结果

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/MainWindow.xaml)

读取内存

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/App.xaml)

读取内存 main-window-notes.md

读取内存 encoding-build-note.md

已经看到当前项目基本还没有正式 i18n 层，界面文本主要直接写在 XAML 和 C++ 里。我再确认一下资源文件和打包配置现状，然后给出一套适合这个仓库的落地方案。

搜索匹配 `**/*.resw` 的文件，无匹配项

搜索匹配 `**/Package.appxmanifest` 的文件，无匹配项

搜索正则表达式 `L"[^"]*[\x{4e00}-\x{9fff}]|L"[^"]*About|MessageDialog|ContentDialog|Label=\"|Text=\"|Title=\"` (`**/MainWindow.xaml.cpp|MainWindow.xaml|App.xaml.cpp|App.xaml.h|*.idl`)，无结果

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/image_channel_viewer.vcxproj)

项目文件里已经有 DefaultLanguage=zh-CN 和 /utf-8，但还没有任何 .resw 资源文件，这说明现在属于“源码内硬编码文案”阶段。接下来我看一下 C++ 侧有哪些用户可见字符串，以便给你一套和现状匹配的迁移建议。

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/MainWindow.xaml.cpp#1-1)，行 1 到 420

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/App.xaml.cpp)

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/README.md)

我已经确认仓库是典型的未本地化 WinUI 3 桌面项目：XAML 文案直写，C++ 里也有用户可见字符串。下面我补一刀定位这些字符串分布，然后直接给你按“这个项目怎么改”来讲。

搜索正则表达式 `选择图片|原图|颜色模式|分量|彩色显示|黑白显示|尚未载入图片|About|关于|版本|失败|错误|不支持|RGB|HSL|HSV|CMYK|LAB` (`**/MainWindow.xaml|MainWindow.xaml.cpp|README.md`)，无结果

搜索正则表达式 `wstring|hstring|L"` (`**/MainWindow.xaml.cpp`)，61 个结果

我补一下平台层面的准确性，确认 WinUI 3 桌面项目现在推荐用哪套资源 API，避免把 UWP 时代的做法直接照搬过来。



已提取 3 资源

平台层面有一个关键点已经明确了：你这个仓库是非打包桌面应用，所以 i18n 方案不能只说“加 resw 就行”，还要考虑 PRI 资源编译和运行时加载。再确认一下当前产物里有没有 PRI。

搜索匹配 `**/*.pri` 的文件，无匹配项

搜索正则表达式 `MakePri|resources.pri|PRIResource|ResourceManager\(|ResourceLoader\(` (`**/image_channel_viewer.vcxproj|scripts/**|MainWindow.xaml.cpp|App.xaml.cpp|README.md`)，无结果

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/x64/Debug)

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/x64/Debug/image_channel_viewer)

我已经确认输出目录里会生成 image_channel_viewer.pri，所以这个仓库做 MRT Core 是可行的。为了给你更具体的迁移建议，我再把 XAML 里当前最该替换成资源键的点位标出来。

搜索正则表达式 `选择图片|颜色模式|分量|彩色显示|黑白显示|关于|尚未载入图片|选择图片后` (`**/MainWindow.xaml`)，24 个结果

搜索正则表达式 `DefaultLanguage|AppxPackage|WindowsPackageType|AdditionalOptions` (`**/image_channel_viewer.vcxproj`)，15 个结果

搜索正则表达式 `Title\(L"Image Channel Viewer"\)|dialog\.Title|PrimaryButtonText|版本：|MIT 许可证|GitHub 主页|个人网站|ColorMode::Original|黑白显示|彩色显示|Title\(windowTitle` (`**/MainWindow.xaml.cpp`)，17 个结果

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/MainWindow.xaml.cpp#740-740)，行 740 到 830

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/MainWindow.xaml.cpp#1036-1036)，行 1036 到 1052

我已经把结论收敛了：这个项目最合适的不是自造一套翻译层，而是沿用 WinUI/Windows App SDK 原生资源体系，XAML 走 x:Uid，C++ 走 MRT Core 资源访问，并针对非打包桌面应用处理好 PRI。顺手把这个仓库的 PRI 现状记一下，后面如果你要我直接改，会少走弯路。

已创建内存文件 i18n-note.md

最符合 WinUI 3 + C++/WinRT 最佳实践的方案是：

把所有用户可见文本统一迁移到 MRT Core 资源体系里，XAML 用 x:Uid，C++ 代码里用 Microsoft.Windows.ApplicationModel.Resources 的资源加载 API，不要继续把中文或英文直接写在 XAML 和 .cpp 里。

**为什么这个项目应该这样做**
- 你现在的 UI 文案大多还是硬编码，XAML 里很明显，例如 MainWindow.xaml、MainWindow.xaml、MainWindow.xaml、MainWindow.xaml。
- C++ 里也有大量用户可见字符串，例如 MainWindow.xaml.cpp、MainWindow.xaml.cpp、MainWindow.xaml.cpp、MainWindow.xaml.cpp、MainWindow.xaml.cpp。
- 项目已经明确把默认语言设成了中文，image_channel_viewer.vcxproj 是 zh-CN，这和当前源码语言一致。
- 项目是非打包桌面应用，image_channel_viewer.vcxproj 和 image_channel_viewer.vcxproj 能看出来，但当前输出目录已经有 image_channel_viewer.pri，说明这条路在你仓库里是能走通的。

**推荐落地方式**
1. 保留 zh-CN 作为源语言
2. 新建按语言分目录的 .resw 资源文件
3. XAML 全面切到 x:Uid
4. C++ 代码统一通过一个小型本地化服务取字符串
5. 所有可变句子改成格式化资源，不做字符串拼接
6. 让枚举和状态只存“语义”，不存“已翻译文本”

**具体怎么做**

1. 资源文件组织
- 先从一个主资源文件开始就够了，不要一开始拆太细。
- 目录结构建议是：
  Strings 下放 zh-CN 和 en-US 两个语言目录，每个目录一个 Resources.resw。
- 如果后面字符串增多，再拆成多个资源文件，比如 Resources.resw、Dialogs.resw、Modes.resw，但第一步没必要。

2. XAML 用 x:Uid，而不是写死 Label/Text
- MainWindow.xaml 的 选择图片
- MainWindow.xaml 和 MainWindow.xaml 的 颜色模式 tooltip 和无障碍名称
- MainWindow.xaml 和 MainWindow.xaml 的 分量 tooltip 和无障碍名称
- MainWindow.xaml、MainWindow.xaml、MainWindow.xaml 的 显示模式相关文本
- MainWindow.xaml 的 关于
- MainWindow.xaml 和 MainWindow.xaml 的空状态文案

这部分最标准的做法就是给控件加 x:Uid，然后在 .resw 里写属性资源键。

例如你这个项目里会经常用到这种键名风格：
- OpenImageButton.Label
- ColorModeAppBarButton.Label
- ColorModeAppBarButton.[using:Microsoft.UI.Xaml.Controls]ToolTipService.ToolTip
- ColorModeAppBarButton.[using:Microsoft.UI.Xaml.Automation]AutomationProperties.Name
- EmptyStateTitle.Text
- EmptyStateDescription.Text

重点是：资源键要表达语义，不要用中文当键，也不要用随手编号。

3. C++ 代码不要再出现用户可见字面量
这些位置都应该改成从资源读取：
- 初始窗口标题：MainWindow.xaml.cpp
- About 对话框标题和按钮：MainWindow.xaml.cpp
- 版本文字：MainWindow.xaml.cpp
- MIT / GitHub / 个人网站等链接文案：MainWindow.xaml.cpp、MainWindow.xaml.cpp、MainWindow.xaml.cpp
- 显示模式按钮文本：MainWindow.xaml.cpp

对这个仓库，我不建议在各处直接 new ResourceLoader 然后零散调用。最佳实践是包一层很薄的本地化辅助类，只暴露两类接口：
- 取普通字符串
- 取格式化字符串

这样能避免后面项目变大后，到处散落资源访问逻辑。

4. 模式和通道定义不要直接存翻译后的字符串
现在 MainWindow.xaml.cpp 到 MainWindow.xaml.cpp 直接把 原图、RGB、HSL、CMYK 这些展示文本塞进了数据结构。

更好的做法是：
- 数据结构里保留枚举值和资源键
- UI 渲染时再查资源

例如思路上变成：
- Mode.Original.Label
- Mode.RGB.Label
- Channel.RGB.R
- Channel.RGB.G
- Channel.CMYK.K

这样以后你新增语言、改文案、做术语统一，都不需要碰业务逻辑。

5. 禁止字符串拼接，改成格式化资源
这在 i18n 里很关键。你现在至少有两处不适合继续保留拼接：

- MainWindow.xaml.cpp 的 版本： + 版本号
- MainWindow.xaml.cpp 到 MainWindow.xaml.cpp 的 窗口标题 + 状态文本

最佳实践不是：
- “版本：” + AppVersion
- appName + “ - ” + status

而是把整句做成资源：
- About.VersionFormat = 版本：{0}
- Window.Title.WithStatus = {0} - {1}

原因很简单：不同语言的语序不一样，连接符也不一定一样。把整句交给翻译，才是真正可本地化。

6. 关于对话框里的长句要尽量减少“程序拼句”
MainWindow.xaml.cpp 到 MainWindow.xaml.cpp 这一段把作者文本、许可证链接、后缀拆成多段。

这种做法有时是为了插入超链接，能接受，但要注意：
- 文本切片越碎，翻译越难
- 语序在其他语言里可能对不上

最佳实践是：
- 能整句就整句
- 必须带链接时，也尽量把普通文本片段控制到最少
- 给资源项写 Comment，告诉翻译人员这一句的上下文和链接含义

7. 非打包桌面应用的注意点
你这个项目不是 MSIX 打包应用，所以有一个 WinUI 桌面项目常见坑：

- 不要假设“加了 resw 就自动万事大吉”
- 要确认构建产物始终包含 PRI，并且运行目录能找到它

当前调试输出目录已经有 image_channel_viewer.pri，这是好现象。
但如果你后面手工改项目文件、拆资源文件、改构建脚本，记得验证这一点不要被破坏。

另外，代码侧如果你想在非打包场景下做更稳妥的资源访问，我建议优先围绕 Microsoft.Windows.ApplicationModel.Resources.ResourceManager 来封装，而不是把旧 UWP 时代的资源访问习惯直接搬过来。

**这类项目里我建议你不要做的事**
- 不要继续把文本写成 constexpr、宏或 L"..." 常量，哪怕先集中到一个头文件里也不够好
- 不要自己造 JSON/YAML 翻译文件系统，WinUI 原生资源体系已经足够
- 不要一开始就做应用内热切语言切换
- 不要把资源键命名成中文句子或 UI 位置编号

**这个仓库最合理的阶段性方案**
1. 保持 zh-CN 为默认语言
2. 先补一套 zh-CN 和 en-US 的 Resources.resw
3. 先迁移 XAML 静态文案
4. 再迁移 About、窗口标题、模式名、通道名这些 C++ 文案
5. 最后把所有拼接句改成格式化资源

**如果按优先级排，我会先改这些点**
- MainWindow.xaml:38
- MainWindow.xaml:48
- MainWindow.xaml:61
- MainWindow.xaml:104
- MainWindow.xaml:171
- MainWindow.xaml.cpp:690
- MainWindow.xaml.cpp:753
- MainWindow.xaml.cpp:815
- MainWindow.xaml.cpp:1040

**一句话结论**
对这个项目，最符合 WinUI + WinRT 最佳实践的 i18n 方案就是：MRT Core 的 .resw 资源 + XAML 的 x:Uid + C++ 的统一资源访问封装 + 全面消除硬编码和字符串拼接。

1. 如果你要，我可以直接帮你把这个仓库改出第一版 i18n 骨架，包含 resw、x:Uid 和 C++ 资源助手。
2. 如果你要更稳一点，我也可以先只做一轮“字符串盘点 + 资源键设计”，把后续迁移成本降下来。

> 问：请问为什么“你这个仓库是非打包桌面应用，所以 i18n 方案不能只说“加 resw 就行”，还要考虑 PRI 资源编译和运行时加载。”，但是可用MRT Core？

> 答（GPT5.4）：

因为这两句话描述的是两个不同层次的事实，并不矛盾。

结论先说：

MRT Core 本身可以用于非打包桌面应用。
但在非打包场景里，资源解析没有“包身份 + 包资源索引”这层默认兜底，所以不能停留在“加个 .resw”这个表面动作，必须同时确认：
- 资源有没有被编译成 PRI
- PRI 有没有随 exe 一起部署
- 运行时是怎么找到并加载这份 PRI 的

**核心区别**
.resw 不是运行时直接读取的最终格式。

对 Windows 资源系统来说：
- .resw 是作者编辑的源文件
- .pri 才是运行时真正参与查找的资源索引

所以“加 resw 就行”只完成了源数据准备，没覆盖运行时链路。

**为什么打包应用看起来更省心**
在打包应用里，系统会提供这些默认前提：
- 应用有包身份
- 构建和打包流程天然围绕包资源工作
- 资源索引和部署路径更标准
- 运行时更容易拿到默认资源视图

所以在打包应用里，很多文章可以简化成：
- 建 Strings/xx-XX/Resources.resw
- XAML 写 x:Uid
- 代码里用资源加载 API

这通常就能跑起来。

**为什么非打包应用不能只讲 resw**
非打包桌面应用没有这层天然保证，主要差别是：

1. 没有标准包身份
打包应用的资源系统会依赖包上下文来定位资源。
非打包应用没有这层环境，资源定位需要更显式。

2. 资源文件不会因为“存在 resw”就自动可用
你把 .resw 放进项目，只代表有翻译源。
如果没有生成对应的 .pri，运行时根本没有统一的资源索引可查。

3. 就算生成了 PRI，也还要保证部署位置正确
如果 exe 旁边没有对应的 resources.pri 或应用 PRI，运行时还是可能找不到资源。

4. 代码侧资源访问在非打包场景往往要更显式
Windows App SDK 文档对非打包场景的重点就是：
- 可以用 MRT Core
- 但代码加载资源时，往往要显式指定 PRI 文件或使用更明确的 ResourceManager 初始化方式

也就是说，非打包不是“不能用”，而是“不能假设默认上下文一定存在”。

**那为什么又说它可用 MRT Core**
因为从 Windows 10 1903 起，非打包应用也可以使用 Resource Management System。
Windows App SDK 也专门支持这一点。

所以正确表述是：

- MRT Core 不是打包应用专属
- 非打包应用一样能用
- 只是你要自己把资源编译、部署、定位这条链路交代完整

**把这件事套到你这个仓库上**
你这个仓库虽然是非打包桌面应用，但它当前输出目录里已经有应用 PRI，这就是为什么我说它“可用 MRT Core”。

关键不在于它有没有 MSIX，而在于它实际上已经满足了 MRT Core 运行所需的一部分条件：
- 有 WinUI 3 / Windows App SDK 资源体系
- 有生成出来的应用 PRI
- XAML 和代码都有机会走同一套资源系统

所以这里真正的判断逻辑不是：
- 非打包，所以不能用 MRT Core

而是：
- 非打包，所以不能偷懒地只说“加 resw”
- 但只要 PRI 生成和加载链路成立，MRT Core 完全可以用

**最容易混淆的一点**
很多人把下面两件事混成一件事了：

- 资源“作者层”
  指你写了 .resw

- 资源“运行时层”
  指系统能不能通过 PRI 把正确语言的字符串解析出来

我前一句话强调的是运行时层不能省略。
后一句话强调的是这条运行时链路在非打包桌面应用里仍然成立。
所以两者并不冲突。

**用一句更精确的话重写就是**
非打包桌面应用可以使用 MRT Core 做 i18n，但因为它不像打包应用那样天然具备完整的资源上下文，所以实现方案不能只停留在添加 .resw，还必须验证 PRI 的生成、部署和运行时加载方式。

**如果你愿意，我下一步可以继续两件事里的任意一个**
1. 结合你这个仓库，具体解释它现在的 PRI 是怎么生成出来的，以及为什么调试输出里会出现它
2. 直接给你一份“这个项目做 i18n 的最小正确改造清单”，包含 XAML、C++ 和构建层分别要改什么

> 问：请给出“这个项目做 i18n 的最小正确改造清单”

> 答（GPT-5.4）：

我先补一个实现层面的关键点：最小改造清单不能只列“改文案”，还要覆盖项目项类型和构建验证，否则做完容易发现运行时根本没读到资源。



搜索正则表达式 `<PRIResource|<Resource Include=|Resources.resw|Strings\\` (`**/image_channel_viewer.vcxproj|**/*.vcxproj.filters`)，无结果

读取 [](file:///c%3A/Users/Umamichi/Desktop/image_channel_viewer/MainWindow.xaml.cpp#680-680)，行 680 到 760

已提取 https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/mrtcore/localize-strings

**最小清单**

1. 保持默认语言为 zh-CN，作为源语言继续使用。
说明：这一点项目已经满足，见 image_channel_viewer.vcxproj。

2. 新增一套最小资源目录。
建议至少先建：
Strings/zh-CN/Resources.resw
Strings/en-US/Resources.resw
说明：最小正确改造至少要有默认语言资源；如果你准备真正做 i18n，最好同时放一份 en-US，这样能立刻验证切换和回退逻辑。

3. 确保新增的 resw 被当作资源参与 PRI 生成，而不是普通内容文件。
最稳妥做法：用 VS 的 Resources File 项模板添加，不要手搓一个普通 XML 文件后随手塞进目录。
验收标准：构建后输出目录里仍然有应用 PRI，当前可参考 image_channel_viewer.pri。

4. 不改当前项目的默认语言配置，先不碰打包模型。
说明：这个仓库现在是非打包桌面应用，见 image_channel_viewer.vcxproj:21 和 image_channel_viewer.vcxproj:68。
最小正确改造不需要把它改成打包应用，也不需要先上 MSIX。

5. 先把 MainWindow.xaml 里的静态可见文案全部改成 x:Uid。
第一批至少覆盖这些位置：
MainWindow.xaml:38
MainWindow.xaml:48
MainWindow.xaml:61
MainWindow.xaml:73
MainWindow.xaml:84
MainWindow.xaml:90
MainWindow.xaml:104
MainWindow.xaml:171
MainWindow.xaml:176

6. 对 attached property 也走资源键，不要留硬编码。
这一项在你这个页面里尤其重要，因为已经有这些属性：
MainWindow.xaml:48
MainWindow.xaml:49
MainWindow.xaml:61
MainWindow.xaml:62
也就是 Tooltip 和 AutomationProperties.Name 都要进入 resw，而不是只改 Label 或 Text。

7. 在 C++ 侧引入一层最小资源访问封装。
最小要求不是“到处直接 new ResourceLoader”，而是统一一个小 helper，负责：
按键取字符串
按格式取字符串
必要时持有 ResourceManager 或 ResourceLoader
这样后面不会把资源访问逻辑散落到窗口代码各处。

8. 把 MainWindow.xaml.cpp 里的所有用户可见字符串迁移到资源。
第一批至少覆盖这些位置：
MainWindow.xaml.cpp:690
MainWindow.xaml.cpp:691
MainWindow.xaml.cpp:701
MainWindow.xaml.cpp:711
MainWindow.xaml.cpp:718
MainWindow.xaml.cpp:732
MainWindow.xaml.cpp:738
MainWindow.xaml.cpp:815
MainWindow.xaml.cpp:1040
MainWindow.xaml.cpp:1045
MainWindow.xaml.cpp:1048

9. 不要继续在模式定义里直接存展示文案。
当前这里把“原图、RGB、HSL...”直接写死在数据里：
MainWindow.xaml.cpp
最小正确改造应改成：
数据层保存枚举值和资源键
UI 层渲染时再查资源
否则后面每加一种语言都要继续改 C++ 业务代码。

10. 所有会受语序影响的句子，改成整句资源，不要字符串拼接。
当前至少这两处必须改：
MainWindow.xaml.cpp
MainWindow.xaml.cpp
正确方向是：
版本文案做成带占位符的资源
窗口标题做成带占位符的资源
原因很简单，不同语言的语序和分隔符未必一致。

11. About 对话框里的富文本也要本地化，但最小阶段可以分段处理。
重点位置在：
MainWindow.xaml.cpp
MainWindow.xaml.cpp
最小正确要求是把文本片段全进资源。
更高一层的优化才是减少碎片化句子，便于翻译。

12. 不要动这些非文本常量。
这些目前不是 i18n 优先项：
文件扩展名过滤器
URL
内部枚举名
颜色空间算法常量
最小改造只改用户可见文本，不扩大战线。

**建议的最小资源键范围**
第一版只需要覆盖四类：
Window
Commands
Dialogs
Modes

可包含这些语义：
应用标题
打开图片
颜色模式
分量
彩色显示
黑白显示
关于
空状态标题
空状态描述
About 标题
关闭
版本格式
GitHub 主页
个人网站
原图
RGB
HSL
HSV
CMYK
LAB
各通道名
窗口标题格式

**构建和运行时验收清单**
1. Debug x64 构建通过。
2. 输出目录存在应用 PRI，见 image_channel_viewer.pri。
3. 启动后主界面所有原先硬编码文案都能正常显示，不出现空白或原始键名。
4. About 对话框文案正常。
5. 模式菜单、通道菜单、灰度模式按钮文案正常。
6. 切换系统显示语言后，应用至少能正确回退到默认语言；如果 en-US 已补齐，则应能显示英文。
7. 不出现因为 x:Uid 属性名不匹配导致的运行时错误。

**这次改造里可以明确不做的事**
1. 不做应用内即时切换语言设置页。
2. 不做多 resw 文件拆分。
3. 不做 RTL 适配。
4. 不做本地化图片资源。
5. 不做 manifest 文案本地化。
这五项都属于后续增强，不是“最小正确改造”的必要条件。

**按这个仓库的实际情况，我建议的实施顺序**
1. 先加 zh-CN 和 en-US 的 Resources.resw。
2. 先改 MainWindow.xaml 的静态文案。
3. 再加一个 C++ 资源 helper。
4. 再改 MainWindow.xaml.cpp 到 MainWindow.xaml.cpp 的 About 文案。
5. 再改 MainWindow.xaml.cpp 这一组模式和通道标签。
6. 最后改 MainWindow.xaml.cpp 这类格式化标题。

**一句话版本**
这个项目做 i18n 的最小正确改造，不是“加两个 resw 文件”，而是“加 resw + 让 XAML 和 C++ 都真正改为资源驱动 + 验证 PRI 生成和运行时解析成立”。

1. 如果你要，我下一步可以直接把这份清单落成可执行改造方案，细化到“新增哪些资源键、改哪些控件、改哪些函数”。
2. 如果你更想直接开工，我也可以直接帮你做第一版最小 i18n 改造。