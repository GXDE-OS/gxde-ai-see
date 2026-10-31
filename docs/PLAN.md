# GXDE 小埃同学 · 设计方案

> 项目：GXDE AI See（小埃同学）
> 版本：v0.2（方案评审稿）
> 日期：2026-10-30
> 仓库：<https://git.spark-app.store/shenmo7192/gxde-ai-see>
> 状态：**方案评审中，确认后开始编码**

---

## 1. 一句话

**拖一下，世界就到你手边。**

小埃同学是 GXDE 桌面的常驻 AI 助手。它把「拖拽」变成一个贯穿桌面的动作：
拖到**传送门**，选一个窗口，内容就落进那个窗口的输入框；
拖到**内容工厂**，按文件类型分词、提链接、翻译、OCR、加待办、问小埃；
拖进**暂存区**，东西就先放着，随时再拖出去。

---

## 2. 产品组成

一个常驻进程（`gxde-ai-see`），四个模块 + 一个聊天客户端：

| 模块 | 说明 | 入口 |
| --- | --- | --- |
| **传送门 Portal** | 拖拽暂存面板 + 原生 drop 到目标窗口 | 拖拽开始时自动出现 |
| **内容工厂 Factory** | 按内容类型提供动作 | 拖动时出现在面板下方 |
| **待办 Todo** | 可选置顶、可拖动、不进任务栏的小窗口 | 面板菜单 / 内容工厂 |
| **小埃同学 Chat** | DTK2 AI 聊天（DeepSeek / 自定义 API / MCP） | 独立 desktop 入口、传送门里的「问问小埃」 |

三者共享一套存储与 UI 组件；聊天是独立窗口，传送门是常驻面板。

```
                    ┌─────────────────────────────┐
   拖拽开始  ──────▶ │  传送门面板（右下角，常驻）  │
                    │  ┌─────────┐  ┌──────────┐  │
                    │  │ 传送门   │  │ 暂存区    │  │
                    │  │         │  │ 列表      │  │
                    │  └─────────┘  └──────────┘  │
                    │  ┌─────────────────────────┐│
                    │  │ 🔧 内容工厂（按类型出现） ││
                    │  └─────────────────────────┘│
                    └──────────────┬──────────────┘
                                   │
                    ┌──────────────┴──────────────┐
                    │ 合成器 portal（爆炸平铺）     │
                    │ 原生 drop 到目标窗口输入框    │
                    └─────────────────────────────┘
```

---

## 3. 设计原则

1. **原生优先**：能用 Wayland/X11 原生协议完成的，不自己模拟。
   传送门的落点用**原生 DnD**，不是剪贴板 + Ctrl+V。
2. **不抢焦点、不打扰**：面板不进入 dock，不抢键盘焦点，不打断用户正在做的事。
3. **不监听剪贴板**：暂存区不是剪贴板管理器，只收用户主动拖入的内容。
4. **可降级**：没有 GXDE portal 合成器时功能降级但程序可用，不崩溃、不反复重启。
5. **数据可恢复**：暂存区、待办、对话都持久化；文件消失自动清理。

---

## 4. 传送门（核心）

### 4.1 用户流程

```
用户拖起文件/文字/图片
  └─▶ 面板在右下角淡入（暂存区为空时才需要弹；非空时本来就常显）
       ├─▶ 松手在「暂存区」：加入暂存区列表
       ├─▶ 松手在「内容工厂」：展开动作，松手在某个动作上执行
       └─▶ 悬停「传送门」约 150ms：进入爆炸平铺（portal 模式）
              └─▶ 鼠标移到某个窗口缩略图上
                     └─▶ 松手：内容**原生 drop** 进该窗口的输入框 / 拖放区
                            └─▶ 关闭平铺、恢复窗口状态、激活目标窗口
```

没有 `org.gxde.Portal`（X11、treeland 等）时改走窗口网格，不需要合成器配合：

```
悬停「传送门」约 80ms
  └─▶ 记下当前展开的窗口与焦点窗口，全部最小化
       └─▶ 传送门左侧出现窗口网格（图标 + 标题）
              ├─▶ 拖到某个图标上停留 350ms：恢复其他窗口，激活该窗口，网格收起
              │      └─▶ 继续拖进该窗口松手：原生 drop
              └─▶ 离开传送门与网格 180ms 以上 / 在此处松手：按原顺序恢复全部窗口
```

窗口列表与最小化/激活：X11 用 EWMH（`_NET_CLIENT_LIST_STACKING`、`WM_CHANGE_STATE`、
`_NET_ACTIVE_WINDOW`），Wayland 用 `zwlr_foreign_toplevel_manager_v1`（treeland 由 waylib 的
`WForeignToplevel` 提供）。代码见 `src/portal/taskwindows.*` 与 `src/portal/portalsession.*`。

### 4.2 为什么不用剪贴板 + Ctrl+V

剪贴板只能粘贴到**当前焦点**控件，无法命中「鼠标下的那个输入框」；
而且会污染用户剪贴板、需要还原、需要处理图片/文件多 MIME。
原生 drop 由目标 App 自己决定在哪个控件、以什么形式接收——这正是用户
说的「把文件 drop 到输入框本身就是粘贴的结果」。

### 4.3 原生 drop 的实现（关键设计）

Wayland 的拖拽目标由合成器内部的 `wlr_drag->focus` 决定。目标 App 只有在
收到 `wl_data_device.enter` 后才会 `accept`，松手时才收到 `drop`。

我们**不修改 wlroots**，只在合成器里做三件事：

1. **portal 模式下的 present_windows**：缩略图照常飞入，但
   - 不安装 wlroots 的 pointer/keyboard/touch grab（否则会顶掉拖拽自己的 grab）；
   - 不锁 cursor，保留拖拽图标；
   - 每帧按 cursor 更新 hover 高亮（复用现有的 105% 放大 + 亮度插值）。

2. **一个 wlcom 层的 `seat_pointer_grab`**：`cursor_feed_motion()` 会先询问它，
   返回 true 就吞掉事件。在这个 grab 里：
   - 根据 cursor 位置找到 hover 的缩略图；
   - 把光标坐标从缩略图矩形线性映射回窗口本地坐标：
     `sx = (px - thumb.x) * view.w / thumb.w`，`sy` 同理；
   - 目标变了就调 `wlr_seat_pointer_notify_enter(seat, view->surface, sx, sy)`，
     否则调 `wlr_seat_pointer_notify_motion(seat, time, sx, sy)`；
   - 这样目标 App 会像普通 DnD 一样收到 `enter/motion`，并在光标位置判断
     「这里是不是一个可接收的输入框」，回 `accept`。
   - 松手时调 `wlr_seat_pointer_notify_button(..., RELEASED)` 后返回 true：
     `wlr_drag` 的 `drag_handle_pointer_button` 看到 `focus_client && accepted`，
     就向真实窗口发 `drop` —— **原生 drop 完成**。
   - 同时调用 `selection_handle_cursor_move()` 让拖拽图标继续跟随。

3. **状态收尾**：
   - 关闭 present_windows（反向动画）；
   - 恢复窗口状态（portal 不移动真实窗口，只隐藏缩略图，所以恢复很轻）；
   - 激活目标窗口，让用户看到粘贴结果；
   - `cursor_rebase()` 恢复指针焦点。

关键前提（已核对代码）：
- present_windows 用 `ky_scene_node_set_enabled(false)` 隐藏真实 view 的 scene node，
  **surface 仍然是 mapped 的**，client 并不知道自己不可见，收 drop 完全正常。
- `drag_set_focus()` 会为目标 client 创建 offer 并发送 `enter`；
  client 的 `accept` 在 hover 期间就完成，松手时不存在时序竞争。
- `drag_handle_pointer_button` 在 `focus_client && accepted` 时才会真正 drop，
  否则 drag 自然取消——目标不接受时不会误操作。

### 4.4 合成器改动（flake-wm + gxde-wlcom）

两个仓库同源（flake-wm 用 CMake，gxde-wlcom 用 meson），代码相同、构建文件不同。

| 文件 | 改动 |
| --- | --- |
| `src/vendor/dkwin/presentwindows/wlcom_presentwindows.c` | 增加 portal 模式：不装 grab、不锁 cursor、每帧高亮；导出 `present_windows_begin_portal()` / `end_portal()` / `portal_item_rect()` / `portal_hovered_uuid()` |
| `src/vendor/gxde/portal/portal.c` | 新模块：D-Bus、状态机、grab、坐标映射、收尾 |
| `src/vendor/gxde/portal/portal_dbus.c` | `org.gxde.Portal` 接口 |
| `src/input/selection.c` | 暴露当前 drag 指针（或 portal 模块直接监听 seat 的 start_drag/destroy） |
| `src/view/view_p.h` | 导出 portal 需要的 present_windows 函数 |
| `CMakeLists.txt` / `meson.build` | 加 `src/vendor/gxde/portal` 目录 |

D-Bus 接口草案（会话总线，合成器持有）：

```
interface org.gxde.Portal
  BeginPortal() -> b           # 打开 portal；没有可平铺窗口返回 false
  CancelPortal()
  ListWindows() -> a(ssiiii)   # uuid, title, app_id, x, y, w, h（调试 / 降级用）
  signal DragStarted(ay mimes, s source_app_id)
  signal DragEnded()
  signal PortalDropped(s uuid, b accepted)
```

### 4.5 边界与异常

| 情况 | 行为 |
| --- | --- |
| 没有已打开窗口 | `BeginPortal()` 返回 false，面板提示「当前无已打开的窗口」，拖拽不中断 |
| 目标窗口不接受 drop | `accept` 为 false，drag 自然取消，面板提示「该窗口不接受拖放」 |
| 拖到缩略图之间的空隙 | 无目标，drag 取消 |
| 拖回面板/暂存区 | 按正常 drop 处理（暂存区接住） |
| 按 Esc | drag 取消，portal 关闭 |
| 目标窗口是拖拽源自己 | 允许（等同拖回原窗口） |
| 最小化/全屏窗口 | v1 只平铺当前工作区可见窗口；全屏窗口先退出全屏再平铺，结束后恢复 |
| 多显示器 | v1 在面板所在输出上平铺；后续支持跨屏 |
| 触摸拖拽 | v1 只做鼠标，触摸后续补 |

### 4.6 降级：X11 / 非 GXDE 合成器

面板在传送门旁边扩展一块**窗口图标网格**（按空间自动选左/右/上/下）：

- 图标来源：Wayland 走 `org.gxde.Portal.ListWindows`，X11 走 EWMH `_NET_CLIENT_LIST`。
- hover 图标 → 最小化其他窗口，激活/提升该窗口；用户把内容拖到已显示的窗口上松开
  → X11 的 XDND 是真实 drop，直接生效。
- 拖出传送门区域 → 恢复原窗口状态。
- 完全不支持 portal 的 Wayland 合成器：退化为「复制到剪贴板 + 提示」。
- Windows 后续复用同一套图标网格 + `SetForegroundWindow`。

### 4.7 数据流（谁拿到 drop 数据）

传输全程由源 App 和目标 App 通过 Wayland DnD 完成，**小埃同学不碰数据**：
- 从别的 App 拖入：源 App → 目标 App 直连；暂存区只保存 URI/文本/图片副本。
- 从暂存区拖出：小埃同学作为 source 提供 `text/uri-list` / `text/plain` / `image/png`；
  目标 App 收到 drop。

因此传送门不需要读取别人的 data source，也不需要剪贴板，隐私面最小。

---

## 5. 暂存区

### 5.1 条目

| 类型 | 存储 | 说明 |
| --- | --- | --- |
| 文件 | URI | 保留原路径，可拖出到文件管理器/编辑器 |
| 文字 | 原文 | 展示做截断，完整保留 |
| 图片 | 缓存 PNG | 落盘到 `~/.cache/gxde-ai-see/images/`，不依赖源文件 |

### 5.2 行为

- 只有**主动松手在暂存区**才添加；路过不算。不监听剪贴板。
- 去重：同类型同内容（路径 / 文本 hash / 图片 hash）只留一条，重复拖入置顶并高亮。
- 上限：默认 200 条，超出丢最旧，可配置。
- 文件消失：`QFileSystemWatcher` + 2s 轮询双保险，消失则播放删除动画并移除。
- 持久化：`~/.local/share/gxde-ai-see/staging.json`，原子写（QSaveFile）。
- 暂存区非空 → 面板常显；为空 → 只在拖拽时出现。
- 从暂存区跃迁后条目保留（除非用户删除或文件消失）。

### 5.3 列表交互

- 每行：复选框、缩略图/类型图标、名称、关闭按钮。
- 选中任意复选框 → 列表上方出现「全选 / 反选 / 删除选中」；取消全不选后收起。
- 拖动行排序（带动画），拖出到别的 App（`QDrag`），拖到传送门（原生 drop）。
- 每行有「固定」按钮：默认拖出后**消耗**（drop 成功即从暂存区移除）；固定后拖出保留。
  拖回暂存区或按 Esc 取消拖拽不消耗。

---

## 6. 面板 UI 与动画

### 6.1 窗口形态

- Wayland：`layer-shell` Overlay 层，锚定右下，`keyboard_interactivity = none`，
  不抢焦点、不进 dock、可精确定位。
- X11：无边框工具窗口 + `_NET_WM_STATE_SKIP_TASKBAR` + 右下定位。
- 尺寸：宽 500；高度随内容，列表上限 `屏高 / 3`，超出滚动；顶部手柄可拖高；
  可「固定大小」。
- 位置：默认右下（输入法面板附近），可拖边框移动，可恢复默认，可固定。

### 6.2 弹窗时机与「打扰」控制

拖拽在很多场景都会发生，无脑弹窗会很烦。控制策略：

- **MIME 过滤**：只对 `text/uri-list`、`text/plain`、`image/*` 响应；
  拖窗口、拖滚动条、拖 Dock 图标这类没有这些 MIME 的不会触发。
- **120ms 延迟**：拖拽开始后延迟再显示，快速点击式拖拽不闪面板。
- **忽略自身拖拽**：从暂存区拖出时不重复弹面板。
- **不抢焦点**：layer-shell + `keyboard none`，不会打断正在编辑的窗口。
- **不遮挡**：面板固定在右下角，尺寸克制；可配置「仅暂存区非空时常显」。
- **可关闭自动弹出**：设置里可关闭「拖拽时自动出现」，只保留常驻面板。
- **悬停延迟**：传送门需要悬停约 150ms 才展开，路过不会炸开全屏。
- **多显示器**：只在光标所在输出弹出，不在所有屏幕同时出现。

### 6.3 动画

| 场景 | 参考 | 效果 |
| --- | --- | --- |
| 面板出现/消失 | qimpanel | 上移 12px + 淡入，160ms OutCubic |
| 行 hover | desktop-panel hover box | 背景淡入 + 图标 1.04 + 关闭按钮淡入，120ms |
| 删除 | control-center `NotifyModel::showRemoveAnim` | 右移淡出 160ms，再折叠 + 后续行上移 180ms |
| 拖动排序 | desktop-panel dodge | 其余行 150ms 平移到新槽位，InOutCubic |
| 内容工厂展开 | — | 高度展开 + 图标错峰淡入 180ms |
| 传送门悬停 | — | 呼吸光晕 + 边框高亮 |
| 爆炸平铺 | present_windows | OutQuint 300ms，缩略图从真实位置飞入 |
| 复选框 | — | 勾选描边 120ms |

### 6.4 右键菜单

固定位置 / 恢复默认位置 / 固定大小 / 恢复默认大小 / 置顶 /
全屏时隐藏 / 拖拽时自动出现 / 开机自启与保活 / 清空暂存区 / 设置 / 关于。

> 「置顶」的现实取舍见 §13 待确认 1。

---

## 7. 内容工厂

拖动时若类型受支持，面板下方出现「🔧 内容工厂」框；把内容拖到框上，框朝**有空间的
方向**（左/右/上/下）展开动作面板；每个动作 hover 有缩放/光晕动画；松手在哪项执行哪项。

### 7.1 动作矩阵

| 类型 | 动作 |
| --- | --- |
| 文本 / 文本类 Office | 打开、**分词**、**提取链接**、翻译成…、朗读、添加到待办、问问小埃、复制 |
| 图片 | 打开、去除背景（占位）、OCR、添加到待办、问问小埃、**提取主色**、**生成描述** |
| 音频 | 打开、人声提取（占位）、添加到待办、问问小埃、**转文字**、**时长/码率信息** |
| 其他 | 打开、添加到待办、问问小埃、复制路径 |

### 7.2 能力来源

- **分词**：ICU `BreakIterator`（zh）先做；效果不够再换内置词典。
- **提取链接**：正则抽 `http(s)://`、`www.`、邮箱；弹框列出、单条/全部复制。
- **打开**：`QDesktopServices::openUrl`。
- **链接分类**（`factory/linktools.*`）：
  - http(s) 按扩展名判定 图片 / 音频 / 视频 / 文档 / 压缩包 / 代码 / 网页；
  - 动作：浏览器打开、下载、另存为、复制、复制为 Markdown、复制文件名、待办、问小埃；
    图片额外「预览」，音视频额外「在线播放」；
  - 非标准 scheme（thunder / spk / magnet / ed2k / 其它）：
    尝试打开、复制、待办、问小埃；thunder 额外解析 base64 得到真实地址（查看/复制），
    magnet 解析 `dn` 作为显示名。
- **翻译 / 生成描述**：走小埃同学的模型（设置里选模型）。
- **朗读**：优先 GXDE Daemon TTS，回退 `spd-say`。
- **OCR**：优先 GXDE Daemon，未安装则禁用并提示。
- **待办**：§8。
- **问问小埃**：不直接粘贴，而是**激活小埃同学窗口**，让用户把内容拖过去：
  左侧列表空白 = 新建对话；左侧某个对话 = 该对话；右侧对话区 = 当前对话。

---

## 8. 待办

- 独立小窗口：可选置顶、可拖动、不进任务栏、位置大小持久化。
- 列表项：复选框、文本、删除；支持拖动排序。
- 数据存 `~/.local/share/gxde-ai-see/todo.json`。
- 入口：面板右键菜单、内容工厂「添加到待办」、桌面文件。

---

## 9. 小埃同学 Chat

### 9.1 初版范围

- **API**：DeepSeek 官方预设 + 自定义 `base_url / model / key`；
  首次启动引导开通 DeepSeek 并填 Key（可跳过，稍后在设置里填）。
- **对话**：多对话管理（新建/重命名/删除/置顶）、流式输出、Markdown 渲染、
  代码块高亮/复制。
- **编辑**：修改用户消息、重试上一条、从任意消息 fork 新对话。
- **上下文**：token 估算；手动压缩；超阈值自动压缩（让模型总结旧消息）；
  可设上下文长度。
- **MCP**：stdio server 配置、`tools/list`、`tools/call`，以 function calling 暴露给模型。
- **设置**：模型参数、系统提示、代理、MCP、数据目录、关于。
- **独立入口**：`gxde-ai-see.desktop`，图标由 `Pictures/同步区/erotica/image_7.png`
  处理成多尺寸 hicolor 图标。
- **关于**：版本、许可、仓库、致谢。

### 9.2 技术

- Qt6 Widgets + DTK2；HTTP 用 `QNetworkAccessManager`（SSE 流式）；SQLite 存对话。
- MCP 用 `QProcess` + JSON-RPC 2.0（stdio），协议与 DSH 对齐。
- UI 参考 DSH Web：左侧会话栏、消息流、底部输入框、设置。
- 后续：skill、DSH 定制发行、让聊天修改自身配置（安装 skill 等）。

---

## 10. 后台与 gxde-k9

- 安装 `~/.local/share/GXDE/gxde-k9/slimy/gxde-ai-see.slimy`，
  由 k9 每 5s 做 PID 检测并拉起 `gxde-ai-see --daemon`。
- 设置项「开机自启动与后台保活」= 创建/删除该 `.slimy` 文件。
- **环境不支持**（没有 `org.gxde.Portal`、X11 且无降级后端等）：
  - 首次弹一次 toast（dconf/配置记 `warned-unsupported`）；
  - 进程**不退出**，进入低资源空闲（不轮询、不建窗口），避免 k9 反复拉起。

---

## 11. 工程结构

```
gxde-ai-see/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app.{h,cpp}            # 单实例、生命周期、后台模式
│   ├── config.{h,cpp}         # 配置
│   ├── staging/               # 暂存区模型/存储/缩略图
│   ├── ui/                    # 面板、列表、委托、内容工厂、待办、聊天
│   ├── portal/                # org.gxde.Portal 客户端 + 降级后端
│   ├── chat/                  # DeepSeek 客户端、MCP、会话存储
│   └── actions/               # 分词、链接提取、TTS、OCR 适配
├── data/                      # desktop、图标、gschema
├── debian/
├── docs/PLAN.md
└── README.md
```

合成器侧：`flake-wm` 和 `gxde-wlcom` 各加 `src/vendor/gxde/portal/`。

---

## 12. 里程碑

| 阶段 | 内容 | 可验证 |
| --- | --- | --- |
| **M0** | 技术验证：portal grab + 重定向 drop + present_windows portal 模式 | 嵌套合成器里拖一个文件进编辑器输入框 |
| **M1** | App 骨架：构建/单实例/配置/面板/暂存区/持久化/动画 | 当前会话可用的暂存区 |
| **M2** | 合成器 portal 模块（两个仓库）+ 原生 drop 全流程 | 传送门端到端 |
| **M3** | 内容工厂（本地动作 + AI 动作）+ 待办 | 分词/提链接/翻译/待办 |
| **M4** | 小埃同学 Chat（API/流式/多对话/编辑/重试/fork/上下文） | 可日常对话 |
| **M5** | MCP、自动压缩、skill 预留 | 接 MCP server |
| **M6** | 打包、k9、i18n、关于、图标、文档 | deb 可安装 |

---

## 13. 待确认

1. ✅ **已确认（2026-10-30）**：面板采用 **layer-shell**；「取消置顶」实现为
   **被窗口遮挡时自动隐藏**（需要合成器提供窗口几何，放在 M2 之后）。
2. **传送门原生 drop 方案**（§4.3）是否认可？这是整个方案的地基。
3. **内容工厂**的动作集合与「朝有空间方向展开」的交互是否 OK？
4. **Chat 初版范围**（§9.1）是否合适？MCP / 压缩是否要放进初版？
5. **分词**先用 ICU 是否可接受？
6. **合成器落地顺序**：先 flake-wm 再移植 gxde-wlcom，还是两个仓库同步改？

---

## 14. 风险

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| client 对不可见 surface 忽略 DnD | portal 失效 | M0 先验证；不行则降级为「激活窗口 + 粘贴」 |
| 坐标映射误差（缩放/分数缩放） | drop 落点偏 | 用 item 实际 rect 与 view geometry 计算，加 clamp |
| 合成器 grab 与拖拽 grab 冲突 | 拖拽中断 | portal 模式不装 wlroots grab，只用 wlcom 层 grab |
| 目标 App 不支持 DnD | 没反应 | 提示「该窗口不接受拖放」 |
| 大图/大文本 | 内存 | 图片落盘、文本截断展示、上限 200 |
| k9 反复拉起 | 资源浪费 | 不支持环境不退出，低资源空闲 |
| X11 行为差异 | 降级 | 图标网格 + EWMH |

---

## 15. 参考

- `flake-wm` / `gxde-wlcom`：`src/vendor/dkwin/presentwindows/`、
  `src/input/selection.c`、`src/input/cursor.c`、`src/view/view_p.h`
- `gxde-file-manager/gxde-desktop-panel`：hover / dodge 动画
- `gxde-control-center/src/frame/notify`：删除动画
- `dtk2widget-qt6`：DBlurEffectWidget / DApplication
- `gxde-k9`：保活
- DSH（`deepseek-harness`）：对话界面与 MCP 参考
