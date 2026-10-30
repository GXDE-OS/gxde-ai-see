# GXDE 小埃同学 · GXDE AI See

> **拖一下，世界就到你手边。**

**GXDE 小埃同学** 是 GXDE 桌面的常驻 AI 助手与拖拽中枢。
它把「拖拽」变成贯穿整个桌面的动作：拖到 **传送门**，选一个窗口，内容就落进那个窗口的输入框；
拖到 **内容工厂**，按类型分词、提链接、翻译、OCR、加待办、问小埃；拖进 **暂存区**，东西先放着，随时再拖出去。

> 当前状态：**0.1.0 初版**，可安装试用。设计文档见 [docs/PLAN.md](docs/PLAN.md)。

---

## ✨ 它能做什么

### 🌀 传送门 Portal

- 拖起文件 / 文字 / 图片时，右下角淡入双栏面板（DBlur 毛玻璃、置顶、不进 dock、不抢焦点）。
- 把内容拖到「传送门」上，桌面**爆炸平铺**（复用 GXDE 的 Win+A 全窗口视图）。
- 把内容拖到任意窗口的缩略图上松手 —— **原生 drop**，直接落进那个窗口的输入框 / 拖放区。
- 不是剪贴板 + Ctrl+V：目标窗口收到的是一次真正的拖放，落点就是鼠标位置。

### 🗂 暂存区

- 文件、文字、图片都能暂存；复选框、缩略图、名称、关闭按钮。
- 拖动排序、拖出到任意 App、拖进传送门；从暂存区跃迁后条目仍然保留。
- 高度随内容增长，最高 1/3 屏，超出滚动；可手调、可固定。
- 文件消失自动清理；重启后恢复（JSON 持久化）。

### 🔧 内容工厂

拖动时按内容类型出现在面板下方，朝有空间的方向展开：

| 类型 | 动作 |
| --- | --- |
| 文本 / Office | 打开、分词、提取链接、翻译、朗读、添加到待办、问问小埃 |
| 图片 | 打开、OCR、提取文字、提取主色、生成描述、添加到待办、问问小埃 |
| 音频 | 打开、转文字、时长/码率信息、添加到待办、问问小埃 |
| 其他 | 打开、添加到待办、问问小埃 |

### ✅ 待办

可选置顶、可拖动、不进任务栏的小窗口；和暂存区共用一套列表交互。

### 💬 小埃同学 Chat

- DeepSeek 官方 API 预设 + 自定义 API；首次使用引导开通并填 Key。
- 多对话管理、流式输出、Markdown、代码块。
- 编辑消息、重试、从任意消息 fork。
- 上下文长度设置 + 手动/自动压缩。
- MCP（stdio）工具接入：设置里填服务器 JSON，工具自动以 function calling 暴露给模型。
- 独立桌面入口；后续作为 GXDE 各项能力的 AI 入口与 skill 平台。

---

## 🧠 它是怎么工作的

```
用户拖拽
   │
   ├─▶ 合成器检测到 drag 开始 ──▶ 面板淡入
   │
   ├─▶ 拖到「传送门」
   │      └─▶ 合成器进入 portal 模式：窗口缩略图爆炸平铺
   │             └─▶ 光标悬停时把 drag 焦点按缩略图坐标重定向到真实窗口
   │                    └─▶ 松手 = 原生 drop 进目标窗口的输入框
   │
   ├─▶ 拖到「暂存区」 ──▶ 加入列队（文件 / 文字 / 图片）
   │
   └─▶ 拖到「内容工厂」──▶ 按类型展开动作
```

核心是两个决定：

1. **原生 drop，不是粘贴模拟**。合成器在 hover 时把拖拽焦点按缩略图坐标映射到真实窗口，
   目标 App 自己判断落点在哪个输入框，松手是一次真正的 `wl_data_device.drop`。
2. **合成器只负责「画」和「转交」**。小埃同学不读取别人的拖拽数据，也不需要剪贴板。

需要 GXDE 的 Wayland 合成器（flake-wm / gxde-wlcom）配合；其他环境会自动降级为
「窗口图标网格 + 激活目标窗口」的模式，X11 后续支持。

---

## 🧩 组成

| 组件 | 说明 |
| --- | --- |
| `gxde-ai-see` | Qt6 + DTK2 的常驻应用（面板 / 暂存区 / 内容工厂 / 待办 / 聊天） |
| `flake-wm` / `gxde-wlcom` 的 `gxde-portal` 模块 | 拖拽监听、爆炸平铺、原生 drop |
| `gxde-k9` | 开机自启与后台保活（可开关） |

---

## 🚧 路线图

| 阶段 | 内容 |
| --- | --- |
| M0 | 技术验证：portal grab + 原生 drop |
| M1 | 应用骨架 + 面板 + 暂存区 |
| M2 | 合成器 portal 模块 + 端到端 |
| M3 | 内容工厂 + 待办 |
| M4 | 小埃同学 Chat |
| M5 | MCP / 压缩 / skill |
| M6 | 打包、k9、i18n、关于 |

---

## 🔨 构建

依赖：Qt 6.8、dtk2widget 6、LayerShellQt 6、ICU、Wayland。

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build          # 安装程序、desktop 与图标
```

打 deb：

```bash
dpkg-buildpackage -b -us -uc
```

## 🚀 使用

```bash
gxde-ai-see --show     # 打开传送门面板
gxde-ai-see --chat     # 打开小埃同学对话
gxde-ai-see --todo     # 打开待办
gxde-ai-see --daemon   # 后台常驻（由 gxde-k9 拉起）
```

- 首次对话：`--chat` → 左下「设置」→ 填 DeepSeek API Key。
- 传送门需要 GXDE Wayland 合成器（flake-wm / gxde-wlcom）配合，见
  [方案 §4](docs/PLAN.md)；其他环境自动降级。
- 面板右键菜单：固定位置/大小、恢复默认位置、开机自启与保活、清空、关于。

---

## 📄 文档

- [设计方案（v0.2）](docs/PLAN.md)
- 翻译：`translations/gxde-ai-see_en.ts`（`lupdate src -ts translations/gxde-ai-see_en.ts` 更新，`lrelease` 生成 .qm）

---

## 📜 许可证

GPL-3.0-or-later。见 [LICENSE](LICENSE)。

---

## 🔗 相关

- GXDE OS：<https://gitee.com/GXDE-OS>
- flake-wm：<https://gitee.com/GXDE-OS/gxde-wlcom>
- gxde-k9：<https://gitee.com/GXDE-OS/gxde-k9>
