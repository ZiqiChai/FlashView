# FlashView

[English](README.md) | [简体中文](README.zh-CN.md) · [更新日志](CHANGELOG.md)

FlashView 是一款基于 Qt 6 的 Linux 图片与视频浏览器，适合快速浏览文件夹、查看图片
和视频。它轻量、快速、美观、兼容；使用 **Quick Look（快速预览）** 时，在 GNOME
文件中选中图片或视频后按 `空格`，即可立即查看，无需先打开完整应用窗口。

## 产品特点

| 特点 | 使用感受 |
| --- | --- |
| **轻量** | 启动快，专注于查看；视频播放和高质量处理按需进行，不额外占用资源。 |
| **快速** | 通过自然排序、快速缩略图、图像缓存和相邻图片预加载，让文件夹浏览更连贯。 |
| **美观** | 工具栏简洁、图标比例协调，提供深浅主题、平滑切换和沉浸式全屏查看。 |
| **兼容** | 使用系统中已有的图片处理器和视频 codec，不把查看器限制在少量内置格式内。 |

## 界面截图

以下截图均为**简体中文界面**。仓库保留深色、浅色主题以及对应的“视图”菜单截图。

### 深色主题

![FlashView 简体中文界面——深色主题](screenshots/flashview-zh-dark.png)

### 浅色主题

![FlashView 简体中文界面——浅色主题](screenshots/flashview-zh-light.png)

<details>
<summary>查看两种主题的视图菜单</summary>

![中文视图菜单——深色主题](screenshots/flashview-zh-dark-menu.png)

![中文视图菜单——浅色主题](screenshots/flashview-zh-light-menu.png)

</details>

## 快速开始

在 Debian/Ubuntu 上，从项目根目录执行两条命令：

```bash
./install.sh --deps --prefix ~/.local
~/.local/bin/flashview /path/to/image-or-folder
```

如果依赖已经安装，可以去掉 `--deps`。也可以把文件或文件夹拖入窗口；`Ctrl+O` 打开
文件，`Ctrl+Shift+O` 打开文件夹。

## 功能特性

### Quick Look：打开前先看一眼

安装可选预览服务后，在 GNOME 文件中选中图片或视频并按 `空格`。FlashView 会显示
无边框预览浮层，沿用文件管理器自身的选中顺序，并支持：

| Quick Look 操作 | 快捷键 |
| --- | --- |
| 关闭 | `空格` 或 `Esc` |
| 请求文件管理器选择上一个/下一个项目 | `←` `→` `↑` `↓` |
| 用完整 FlashView 窗口打开 | `回车` |
| 图片适应窗口 / 显示实际大小 | `F` / `1` |

预览服务在需要时启动，空闲十分钟后退出；只预览本地文件。预览服务不可用时，不影响
FlashView 主查看器。Wayland 下浮层无法附着文件管理器窗口，会改为居中显示。无法解码、
已消失或远程文件会显示解释，并可按 `回车` 交给其他应用打开。

可使用 `./install.sh --no-previewer` 或 `-DFLASHVIEW_ENABLE_PREVIEWER=OFF` 关闭
该可选集成。

### 快速浏览文件夹

- 支持文件来自已安装的 Qt 图片处理器和系统 MIME 数据库；对于没有扩展名或扩展名
  不可靠的文件，还会进行内容探测。
- 默认使用**文件名自然排序**，因此 `image2` 排在 `image10` 前。设置和“视图”菜单
  可固定按名称、修改时间、创建时间、文件大小或文件类型排序，并选择升序或降序。
- 实时监视当前文件夹和当前文件的新增、删除、替换；文件仍存在时尽量保持当前项目。
- 缩略图在后台渐进式解码。缩略图栏横向滚动、平滑居中选中项；显示/隐藏缩略图栏
  使用 `T` 快捷键切换。
- `Delete` 尽可能把当前文件移入桌面回收站；删除确认可选，默认关闭。

### 图片查看与自适应缩放

- 大图适应窗口时只会缩小，小图**不会自动放大**。`1` 回到 100%，手动缩放范围为
  2% 至 10000%。
- `Ctrl` + 滚轮以光标位置为中心缩放。在设置中也可以把普通滚轮改为专门缩放；否则
  大图滚轮用于平移，图片完整显示时滚轮用于翻页。
- 平滑缩放和短暂淡入过渡让快速浏览保持连贯。暂停一小段时间后，程序会把相邻图片
  预加载到 128 MB 的 Qt 像素图缓存；缓存键包含绝对路径、文件大小和修改时间。
- 提供**自动**、**平滑（照片）**和**最近邻（像素画）**三种缩放质量。静态图片在
  缩放停止后可进行延迟高质量重采样；动图和超大图片使用实时路径，以限制内存与延迟。
  这是插值，不是 AI 超分辨率。
- 当已安装的 Qt 处理器报告支持动画时，GIF 等动图使用 Qt 动画处理器播放。

### 视频播放

Qt Multimedia 提供播放/暂停、进度跳转、音量和静音控制。音量与静音状态保存在应用
设置中。切换离开视频时会停止播放器，播放错误会显示在状态栏。

### 外观与自定义

内置深浅主题、中英文界面、协调的工具栏/菜单图标、全屏查看、空闲隐藏光标、旋转和
可重新绑定的快捷键。设置通过 `QSettings` 持久化保存。

## 媒体格式与运行时支持

文件列表有意基于运行时能力，而不是固定扩展名表；打开文件对话框的过滤器也由同一组
运行时列表生成。

| 媒体 | 运行时路径 | 实际边界 |
| --- | --- | --- |
| 图片 | `QImageReader::supportedImageFormats()` 与 `QMovie::supportedFormats()` | 常见安装通常包含 JPEG、PNG、BMP、GIF、WebP、TIFF、SVG、ICO；准确列表取决于 Qt 插件。 |
| 现代/扩展图片 | 相同的 Qt 插件发现和内容探测 | APNG、AVIF、HEIF/HEIC、JPEG XL 以及相机 RAW（如 DNG/CR2/NEF/ARW）只有在安装兼容 Qt 图片处理器后才会出现。 |
| 视频容器 | Qt Multimedia + Linux GStreamer 后端 | MP4、MKV、MOV、AVI、WebM 是常见示例；容器内部编码器缺失时仍可能无法播放。 |
| 编解码诊断 | 启动时检查部分图片处理器和 GStreamer 解码元素 | 状态栏会提示缺失的现代图片格式，或 H.264/H.265/VP9/AV1 等常见解码器。 |

依赖脚本安装 GStreamer Base、Good、Bad、Ugly 和 libav 插件。单独安装底层 codec
库不会自动生成 Qt 图片处理器，文件扩展名本身也不能证明文件一定可以解码或播放。

## 键盘与鼠标参考

浏览和显示快捷键可在**设置 → 快捷键**中修改。以下是默认值：

| 操作 | 默认快捷键 |
| --- | --- |
| 上一个 / 下一个文件 | `←` / `→`；图片完整显示时滚轮；鼠标后退/前进键 |
| 第一个 / 最后一个文件 | `Home` / `End` |
| 向后 / 向前翻页 | `PageUp` / `PageDown` |
| 放大 / 缩小 | `Ctrl++` / `Ctrl+-`，或 `Ctrl` + 滚轮 |
| 适应窗口 / 实际大小 | `F` / `1` |
| 向左 / 向右旋转 | `Ctrl+L` / `Ctrl+R` |
| 播放 / 暂停视频 | `Space` |
| 调整视频音量 | `Ctrl` + 滚轮 |
| 全屏 | `F11` |
| 显示/隐藏缩略图栏 | `T` |
| 将当前文件移入回收站 | `Delete` |
| 打开文件 / 文件夹 | `Ctrl+O` / `Ctrl+Shift+O` |
| 设置 | `Ctrl+,` |

## 构建、测试与部署

### 依赖

- CMake ≥ 3.16 和支持 C++17 的编译器
- Qt 6：Core、Gui、Widgets、Concurrent、Multimedia、MultimediaWidgets、
  LinguistTools、Test
- 可选 Quick Look 服务所需的 Qt DBus
- Qt SVG 与图片格式运行时插件
- GStreamer Base、Good、Bad、Ugly、libav 运行时插件

`scripts/install-deps.sh` 面向 Debian/Ubuntu，可重复执行：

```bash
./scripts/install-deps.sh       # 交互式
./scripts/install-deps.sh -y    # 非交互式 / CI
```

### 构建与回归测试

```bash
./scripts/build.sh               # 在 ./build 中进行 Release 构建
./scripts/build.sh --debug       # Debug 构建
./scripts/build.sh --clean       # 只删除选定的构建目录

ctest --test-dir build --output-on-failure
```

Qt Test 覆盖运行时媒体识别、自然排序、解码失败、GIF 动画、适应窗口不放大、插值响应、
滚轮缩放、缩略图、文件夹更新、删除、工具栏语义，以及在有会话总线时的 Quick Look
D-Bus 服务。

GitHub Actions 会在 Ubuntu 22.04、24.04 和 26.04 上重复安装依赖、Release 构建、
回归测试、离屏启动 smoke test 和 staged install 布局检查。工作流位于
[.github/workflows/build.yml](.github/workflows/build.yml)。

### 安装与卸载

安装脚本会构建 Release 目标，安装二进制、桌面入口和图标；启用 Quick Look 时还会
安装 D-Bus 服务。

```bash
./install.sh --prefix ~/.local       # 当前用户安装
./install.sh                         # 默认目录：/usr/local
./install.sh --deps --prefix ~/.local
./install.sh --uninstall --prefix ~/.local
```

卸载时使用相同的 `--prefix`。`--no-previewer` 可将部署保持为纯图片/视频查看器。桌面
入口声明常见图片/视频 MIME 类型，但文件夹浏览仍以运行时发现结果为准。

### 可复现的文档截图

截图测试使用应用嵌入的翻译资源和自动生成的示例图片，不依赖可选媒体文件。请先安装
中文字体（例如 Ubuntu 的 `fonts-noto-cjk`），构建测试目标后执行：

```bash
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 QT_FONT_DPI=96 \
FLASHVIEW_SCREENSHOT_DIR="$PWD/screenshots" \
./build/flashview_tests generateDocumentationScreenshots
```

该命令重新生成 8 张 1200 × 800 PNG：
`flashview-{en,zh}-{dark,light}.png` 和
`flashview-{en,zh}-{dark,light}-menu.png`。每份 README 只引用对应语言的界面截图，
并保留深色、浅色两种主题。

## 仓库结构

```text
src/            Qt 查看器、图片/视频播放、缩略图、设置和 Quick Look 服务
i18n/           Qt Linguist 中英文翻译目录
resources/      图标、Qt 资源集合和 D-Bus 服务模板
screenshots/    按语言和主题保存的文档截图
scripts/        Debian/Ubuntu 依赖安装脚本和 CMake 构建封装
tests/          Qt Test 回归测试与截图生成器
.github/        GitHub Actions 构建/测试/安装布局工作流
CMakeLists.txt  构建图、Quick Look 开关和安装规则
install.sh      Release 构建、部署和卸载入口
CHANGELOG.md    面向用户的更新记录
```

## 许可证

项目暂未包含许可证文件。在添加许可证前，作者保留全部权利。
