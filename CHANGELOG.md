# Changelog

## 0.1.0 - 2026-10-30

### 新增

- 传送门面板：DBlur 双栏、layer-shell 定位、拖拽自动弹出、位置/高度可调并持久化
- 暂存区：文件/文字/图片、去重、上限、文件消失清理、固定/消耗语义、全选/反选/删除选中
- 内容工厂：按类型给出动作，动作按钮可直接接收 drop
  - http(s) 链接按扩展名分类，支持下载/另存为/复制/Markdown/待办/问小埃
  - thunder:// 解析真实地址，magnet 解析名称，非标准 scheme 尝试打开
  - 文字：ICU 分词、提取链接、朗读
  - 文件/图片：打开、复制路径、保存图片、占位 OCR/描述
- 待办窗口：layer-shell、不抢焦点、复选框/删除/新建、持久化
- 小埃同学对话：DeepSeek/自定义 API 流式输出、多会话、设置、三块拖放区、
  编辑/重试/分支、上下文条数与自动压缩、关于
- 合成器 portal（flake-wm / gxde-wlcom）：org.gxde.Portal、缩略图坐标映射、
  原生 drop 到目标窗口
- gxde-k9 开机自启与保活开关

 -- shenmo <shenmo@spark-app.store>
