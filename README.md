# VisualOptimization（视觉优化）

> [!NOTE]
> 🤖 **本项目全程使用 AI 辅助完成**

> 作者：Entity_303-E3 ｜ 版本：1.0.0
> 合并自 BallanceBug 的 ViewDistanceEditor（视距调整），并新增 Preload 预加载与光照优化两大功能。

一个整合型 Ballance Mod，同时提供：
- **视距调整**：自由设置视距 / 分数球距离 / 生命球距离，飞行不再被迷雾和远裁剪限制；
- **Preload 预加载**：进关时于黑屏期强制渲染全部场景网格，彻底消除飞行卡顿；
- **光照优化**：在主光的背光方向补上微弱的多方向补光，让"背光面一片黑"的贴图也能看清，同时保持明暗层次、不改变原版观感。

---

## 📦 文件说明

| 文件 | 平台 | 用途 |
|---|---|---|
| `VisualOptimization.bmodp` | **BML+**（BallancePlayer / BML+ 加载器） | 写死版：视距 1e9，Preload 与光照优化全部开启，安装即用 |
| `VisualOptimizationConfigurable.bmodp` | **BML+** | 配置版：视距/分数球/生命球三个距离输入栏（默认 1200），Preload 与光照优化带开关 |
| `VisualOptimization.bmod` | **老 BML**（v0.3.40+） | 写死版 |
| `VisualOptimizationConfigurable.bmod` | **老 BML** | 配置版 |

> ⚠️ 两个版本的 Mod ID 相同（`VisualOptimization`），**同一时间只能安装一个**，请按需选择。

---

## 🚀 安装

### BML+（.bmodp）
1. 把选定的 `.bmodp` 文件复制到 `Ballance 目录\ModLoader\Mods\`；
2. 用 BallancePlayer 启动游戏即可。

### 老 BML（.bmod）
1. 把选定的 `.bmod` 文件复制到 `Ballance 目录\ModLoader\Mods\`；
2. 启动游戏即可（需 BML v0.3.40 及以上）。

---

## ⚙️ 功能说明

### 写死版（VisualOptimization）
- 视距 / 分数球距离 / 生命球距离：**1e9**（无限远）
- Preload：开启
- 光照优化：开启

进关后游戏内提示：
```
View distance set to 1e+09
Preloaded N static meshes
Lighting Optimization Enabled
```

### 配置版（VisualOptimizationConfigurable）
在 Mod 配置界面（ModLoader 配置菜单）的 **Main** 分类下可调：

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `ViewDistance` | 输入栏 | 1200 | 视距（游戏原版默认 1200） |
| `ExtraPointDistance` | 输入栏 | 1200 | 分数球拾取/视距 |
| `ExtraLifeDistance` | 输入栏 | 1200 | 生命球拾取/视距 |
| `PreloadMeshes` | 开关 | Enabled | 进关预加载全部静态网格（消除卡顿） |
| `LightingOptimization` | 开关 | Enabled | 背光补光（提亮背光面） |

- 修改后**实时生效**（无需重启游戏或重进关卡）；
- 光照优化关闭时立即移除补光并恢复主光（提示 `Lighting Optimization Disabled`），重新开启立即恢复（提示 `Lighting Optimization Enabled`）。

---

## 🧠 工作原理（简要）

- **视距**：设置 `InGameCam` 的远裁剪面，并修改分数球/生命球脚本中拾取检测距离参数；进关/切小节/死亡重置/切关等所有时机自动兜底，防止被重置。
- **Preload**：进关第一帧遍历场景全部 3D 实体，对每个网格执行一次强制渲染，把"首次进入视野时的渲染准备开销"集中到黑屏期，之后飞行/切小节全程无卡顿。
- **光照优化**：Ballance 使用 CPU 软件光照。Mod 读取主光（Light_Ingame）方向，在其背光方向半球上均匀布置 5 个微弱平行光（深背光区），并按关卡主光实际强度自适应（如第 12 关脚本调暗主光时补光自动跟随），同时主光亮度保持原关卡设定 ×0.985。背光面从"全黑"变为"微亮且均匀"，向光面几乎不变。

---

## 🖥️ 兼容性

- **BML+**：声明 BML 0.3.0，兼容 BML+ v0.3.0 ~ v0.3.12 全系列
- **老 BML**：声明 BML 0.3.40，兼容 v0.3.40 ~ v0.3.43

---

## ❓ 常见问题

**Q：进关没有"View distance"提示？**
A：相机视距已是目标值时会静默跳过（如退出关卡再进、BML+ 重开关卡）。重开关卡时老 BML 会提示一次——这是老 BML 重置了相机属性、Mod 在自动修复视距，属正常现象。

**Q：为什么第一次进 12 关感觉更亮？**
A：12 关原版主光较弱（脚本会把它调暗到约 0.588），光照优化会按主光实际强度自适应补光，并保持主光 ×0.985，相对亮度与其他关一致。如果仍然觉得偏亮/偏暗，可改用配置版调整。

**Q：两个 .bmodp 可以一起装吗？**
A：不可以，二者 Mod ID 相同，一次只装一个。

**Q：能和其他 Mod 一起用吗？**
A：可以。Mod 创建的补光对象命名唯一（`VisualOpt_Fill0~4`），不会与其它 Mod 冲突（请勿与旧版 LightingOptimization 类 Mod 同时使用，补光会叠加）。

---

## 📄 致谢

- 视距调整功能源自 [Xenapte 的 ViewDistanceEditor](https://github.com/Xenapte/MyBMLMods/tree/main/ViewDistanceEditor)（BallanceBug），本项目基于其已修复卡顿问题的版本整合。
- 感谢 BML / BML+ 加载器社区提供的 Mod 框架与测试环境。
