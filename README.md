# 洛水别绪 · 洛神赋 OpenGL 小组作业

C++17 / OpenGL 实时场景：曹植停留河岸，洛神在水面回望，沿 Bézier 曲线渐渐远去。当前已接入最终曹植和洛神模型，包含贴图、Phong 光照、水面、倒影、阴影及键鼠交互。

![当前实际运行截图](docs/scene-main.png)

## 从这里开始

1. 安装 Visual Studio 的“使用 C++ 的桌面开发”与 Windows SDK。
2. 克隆仓库后，在 PowerShell 执行 `./scripts/build.ps1`。脚本查找 MSBuild，选择 v143 或 v145 工具集。
3. 执行 `./scripts/check.ps1` 检查模型、贴图和路径数学。
4. 双击根目录“打开洛水别绪.cmd”，或运行“场景工程_美术重制/bin/LuoshenScene.exe”。

也可打开“场景工程_美术重制/LuoshenScene.vcxproj”，选择 Release / x64；VS 2022将工具集设置为v143。需要 Windows x64 和支持 OpenGL 2.1、深度纹理及Framebuffer Object的显卡驱动。采用GLSL 1.20兼容上下文，不是OpenGL Core Profile工程；教师另有要求时须迁移。

## 目录

| 路径 | 用途 |
|---|---|
| 场景工程_美术重制/ | 唯一维护的C++场景工程，assets为实际运行素材 |
| assets/caozhi-current/caozhi.blend | 最终曹植可编辑工程 |
| assets/luoshen-current/luoshen.blend | 最终洛神可编辑工程 |
| scripts/ | 构建与检查入口 |
| docs/协作约定.md | 四人分工、分支及合并流程 |
| 场景工程_美术重制/素材来源与许可.md | 素材及依赖来源 |
| references/papercraft/target.png | 美术参考，不参与运行 |

人物工程已包含，无需旧压缩包或Blender即可编译运行。编辑人物时自行安装Blender；本机便携软件与编译缓存不提交Git。

## 修改入口

| 内容 | 文件（位于场景工程_美术重制） |
|---|---|
| 布局、角色朝向与比例、相机 | main.cpp |
| 主镜头和人物/舟/岩岸的集中布局参数 | scene_layout.h |
| 光照、雾、水纹、贴图 | graphics_support.h |
| 阴影 | shadow_map.h |
| 洛神路径与时长 | scene_motion.h |
| OBJ/MTL读取 | mesh_data.h |
| 题款与提示 | scene_overlay.h |

矩阵使用Y轴向上，位置为归一化包围盒后的落脚点。洛神约45秒后停于路径终点。

## 操作

左键旋转，右键平移，滚轮缩放；WASD移动观察中心；1/2/3/4切换主视/全景/洛神/曹植；空格暂停，R重播，P路径，H帮助，F9截图，Esc退出。

`./scripts/check.ps1 -Capture`在真实图形环境中生成六张静态截图，不录屏、不输出MP4；视频由小组自行录制。

## 当前状态

曹植3309三角面，洛神2861三角面（课程读取器统计）；洛神只修颈部和耳坠，保留原脸型和头发。下一步打磨构图、光照、动画和性能。Windows CI验证编译与素材读取，不代表真实显卡画面或帧率通过。

当前分支包含参考图构图小样：洛神右近、曹植左远、左上花枝、折面岩岸和中景舟篷。布局集中在scene_layout.h，主镜头随离别缓慢拉远；见[构图说明](docs/场景构图说明.md)。人物模型及原路径不变，光照和水面质感仍待打磨。

第三方素材、人物资产和源码不共用同一许可；请阅读来源说明。本仓库未额外授予项目源码或人物资产的开源许可。
