# 1.0.0~beta2 之后待办

已完成：会话列表拖拽排序/多选/批量删除（可复用控件已加入 dtk2widget-qt6）、
token 压缩上限、待办取消置顶、全屏隐藏联动、英文回退翻译、新图标、DTK 关于窗口。

## 仍需单独排期

- [x] 对话浅色/深色主题跟随 DTK（已完成：`ChatTheme` 双套调色板，监听
      `DGuiApplicationHelper::themeTypeChanged` 实时切换，含浅色对比度自检）
- [ ] 消息版本树：重试/编辑保留历史并可左右切换
- [ ] OCR / TTS / 翻译对接 GXDE Daemon（当前仓库未找到对应 D-Bus 接口，需先定义）
- [ ] X11 / 非 GXDE 合成器窗口图标网格降级方案
- [ ] DTK Wayland 文本拖拽预览（合成器侧渲染，工作量大）
- [ ] 更多窗口改用 DTK 控件（聊天设置、待办窗口等）
- [ ] dtk2widget-qt6 的 DReorderListWidget 需要随库发布后，应用改为链接该控件
