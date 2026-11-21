# 更新日志

## 1.0.2~beta7 — 2026-11-21

- 输入框上方附件预览（DSH 风格）：图片显示缩略图、可移除；纯附件也可发送
  · shenmo <shenmo@spark-app.store>
- 账号设置支持「从上游获取」模型列表（AI Core >= 1.0.0-beta4）
  · shenmo <shenmo@spark-app.store>
- 移除说明文案；图片不再被 AI Core 上下文管理降级（配合 AI Core beta4）
  · shenmo <shenmo@spark-app.store>
- 应用图标换成透明底、由 384px 源图重新生成；聊天头像/favicon 同步更新
  · shenmo <shenmo@spark-app.store>

## 1.0.2~beta6 — 2026-11-21

- 修复第一次从传送门添加待办时卡顿、待办窗口不出现：窗口创建/显示推迟到
  拖拽释放后，不再在 drop 事件里建窗口 · shenmo <shenmo@spark-app.store>
- 附件消息去掉重复的 file:// URI / 路径文本行；图片消息附带视觉能力提示，
  当前模型看图不支持时会直接建议换视觉模型 · shenmo <shenmo@spark-app.store>
- 设置里说明模型列表来源（预设/手动），AI Core 暂无上游拉取接口
  · shenmo <shenmo@spark-app.store>

## 1.0.2~beta5 — 2026-11-21

- 内容工厂：去掉重复的「问问小埃」动作，改成右侧贯穿整块的大按钮；
  暂未本地实现的动作（翻译 / OCR / 生成描述 / 转写）改成「附件 + 提示词」
  交给小埃处理
  · shenmo <shenmo@spark-app.store>
- 附件真正对接 AI Core：图片内嵌 base64、文本文件内容内嵌，不再只发路径；
  输入框 +、拖拽、问问小埃、内容工厂统一走这套
  · shenmo <shenmo@spark-app.store>
- 暂存区向外拖出不再误触传送门：先拖出面板、再拖回来才触发
  · shenmo <shenmo@spark-app.store>
- 待办默认非置顶、dock 不显示图标（ukui-shell skip-taskbar + 定位），
  关闭收进托盘；新增托盘图标（显示/新建/隐藏/退出）
  · shenmo <shenmo@spark-app.store>

## 1.0.2~beta4 — 2026-11-17

- 内容工厂「添加到待办」后按需打开待办窗口：未打开或已隐藏时才弹出，
  已经打开时只把条目加进列表，不再每次前置打扰
  · shenmo <shenmo@spark-app.store>

## 1.0.2~beta3 — 2026-11-16

- 对话输入框新增「+」按钮：手动选择文件 / 图片加入消息，按拖拽时的
  `[文件] 路径` / `[图片] 路径` 格式填入输入框，由 AI Core 读取
  · shenmo <shenmo@spark-app.store>
- 对话设置新增「传送门」分区与「传送门快捷入口」开关：关闭后拖拽不再
  自动唤起传送门面板；应用菜单 / `--show` 显式打开与暂存区常显不受影响
  · shenmo <shenmo@spark-app.store>
- 补齐英文翻译（附件选择对话框、设置说明）
  · shenmo <shenmo@spark-app.store>

## 1.0.2~beta2 — 2026-11-16

- 传送门面板收紧体积，减少遮挡：拖拽面板 420×184（原 500×373），
  暂存区有货时常显 320 宽的列表面板；空暂存区只留一行提示，
  内容工厂空闲态收成一行、拖入内容时才展开动作（4 列网格）
  · shenmo <shenmo@spark-app.store>
- 面板尺寸随暂存区条数 / 内容工厂动作数自适应，启动即落在紧凑形态
  · shenmo <shenmo@spark-app.store>
- 安装后随包提供系统级 gxde-k9 保活服务
  （`/usr/share/gxde-k9/slimy/gxde-ai-see.slimy`）：登录后拉起
  `gxde-ai-see --daemon`，崩溃/被杀后 5 秒内自动恢复；只有用户选择
  「退出（停止后台保活）」或取消勾选时才停，状态记在
  `~/.config/GXDE/gxde-ai-see/k9-disabled`；旧版用户目录 `.slimy` 自动迁移
  · shenmo <shenmo@spark-app.store>

## 1.0.0~beta3 — 2026-10-31

- 对话窗口支持浅色 / 深色主题跟随 DTK：新增 `ChatTheme` 双套调色板，
  侧栏、气泡、输入区、会话列表、系统提示随 `DGuiApplicationHelper::themeTypeChanged`
  实时切换；补齐全浅色对比度自检 · shenmo <shenmo@spark-app.store>
- 修复 Markdown 正文不随主题换色（暗色下暗字暗底）：块级选择器着色，
  浅色对比度调优、暗色观感对齐旧版；主题切换保留滚动位置
  · shenmo <shenmo@spark-app.store>

## 1.0.0~beta2 — 2026-10-30

- 会话列表：拖拽排序、多选、批量删除
- 上下文压缩支持 128k / 1024k token 上限
- 待办取消置顶（普通窗口）
- 全屏时隐藏联动合成器信号
- 非中文环境回退英文；英文翻译补齐
- 新应用图标、DTK 关于窗口

## 1.0.0~beta — 2026-10-30

首个 beta 版本。

- 传送门：合成器 portal 原生 drop，拖到窗口即落在光标位置
- 面板：layer-shell 双栏、暂存区、拖拽/删除/排序动画、位置与大小持久化
- 内容工厂：常驻面板底部，按拖拽内容分类展示动作，动作可直接 drop
- 待办：独立浮窗与图标，添加待办后自动打开
- 小埃同学：DTK 主窗口、气泡消息、多会话、流式输出、MCP、上下文压缩
- 分词胶囊选择、朗读 Dialog、DTK 原生关于窗口
- 传送门桌面入口：再次点击回到后台模式
- 英文翻译与本地化加载

# Changelog

## 0.1.0 - 2026-10-30

### 新增

- 传送门面板：DBlur 双栏、layer-shell 定位、拖拽自动弹出、位置/高度可调并持久化
- 暂存区：文件/文字/图片、去重、上限、文件消失清理、固定/消耗语义、全选/反选/删除选中
- 暂存区动画：hover 高亮、删除右移淡出、拖动排序
- 内容工厂：按类型给出动作，动作按钮可直接接收 drop
  - http(s) 链接按扩展名分类，支持下载/另存为/复制/Markdown/待办/问小埃
  - thunder:// 解析真实地址，magnet 解析名称，非标准 scheme 尝试打开
  - 文字：ICU 分词、提取链接、朗读
  - 文件/图片：打开、复制路径、保存图片、占位 OCR/描述
- 待办窗口：layer-shell、不抢焦点、复选框/删除/新建、持久化
- 小埃同学对话：气泡消息/头像/会话列表（标题+预览+时间）/输入区重做；
  DeepSeek/自定义 API 流式输出、多会话、设置、三块拖放区、
  编辑/重试/分支、上下文条数与自动压缩、Markdown、关于
- MCP：stdio JSON-RPC，自动 initialize / tools-list，工具以 function calling 暴露给模型并在对话中执行
- 合成器 portal（flake-wm / gxde-wlcom）：org.gxde.Portal、缩略图坐标映射、
  原生 drop 到目标窗口
- 对话窗口改用 DMainWindow/DTitlebar（DTK），消息气泡、会话列表重做
- DeepSeek 模型更新为 deepseek-flash / deepseek-v4-pro，设置里可自动获取 /models
- 内容工厂右侧常驻「问问小埃同学」大按钮；面板为工厂预留空间，不再上挤
- 英文翻译（95 条）与翻译加载；gxde-k9 开机自启与保活开关；不支持环境只提示一次并保持待命

 -- shenmo <shenmo@spark-app.store>
