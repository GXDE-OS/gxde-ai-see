# 更新日志

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
