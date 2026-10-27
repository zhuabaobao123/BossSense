# Boss Sense

动态 Boss 识别（Dynamic Boss Recognition）—— 给 TrueHUD 的 Boss 血条提供运行时判定。

原版模组（Nexus 上的 *TrueHUD Boss Scanner - SSEEdit Script*）在 xEdit 里静态扫描全部插件，
把一个一千多条的 NPC 名单写进 `TrueHUD_missing_bosses.ini`。名单越长越接近"人人都是 Boss"，
而且之后新装/更新的模组不会自动进名单。

这个 DLL 把同一件事搬到游戏里做：

* **运行时判定**：谁进战斗、谁被打，就当场算分，不再预先生成名单。
* **动态跟随**：后装、后更新的模组，只要进战斗就会被评估。
* **宁缺毋滥**：默认只认结构性证据（地点 Boss 标记、种族白名单、EditorID/模板含 Boss），
  等级/血量/AIDT 只做加权，阈值以下不给血条。
* **可调**：SKSE Menu Framework 菜单里实时改阈值、各信号权重、等级差、血量线。
* **不写 ESP**：纯 SKSE 插件，无插件槽位占用。

## 依赖

| 需求 | 说明 |
| --- | --- |
| SKSE64 | 1.5.97 或 1.6.x（同一 DLL，CommonLibSSE-NG 多运行时） |
| TrueHUD | 1.1.10+（使用 IVTrueHUD4 / AddBoss） |
| SKSE Menu Framework | 可选。没装也能用，只是没有游戏内调参菜单 |

## 安装

1. 把 `SKSE/Plugins/BossSense.dll`、`BossSense.pdb`、`BossSense.ini` 放进 `Data/SKSE/Plugins/`。
2. **停用旧的静态名单**：删掉/禁用 `TrueHUD_missing_bosses.ini`（就是原 SSEEdit 脚本生成的那份）。
   TrueHUD 会读取 `SKSE/Plugins/TrueHUD/` 下所有 ini，两份同时存在会互相打架。
3. 进游戏。日志在 `Documents/My Games/Skyrim Special Edition/SKSE/BossSense.log`。

## 判定逻辑

每个敌对目标在进入战斗 / 被击中时算一次分，达到 `Threshold`（默认 70）就成为 Boss。

| 信号 | 默认分 | 判定依据 |
| --- | --- | --- |
| Race | 100 | 种族在 `[Lists] BossRace` 里（龙、龙祭司、吸血鬼大君、Miraak 等） |
| LocRef | 100 | 目标就是当前地点 `LocRefType Boss` / `DLC2Boss1` 标记的引用 |
| Edid | 90 | NPC_ 或其 leveled 原始模板的 EditorID 含 `boss`（DLL 自己解析插件文件，不依赖运行时 EDID） |
| CustomName | 25 | 手工命名 NPC：EditorID 不以 `Lvl`/`Enc` 开头 |
| NotLeveled | 20 | 不是从 `Lvl...` 随机刷怪列表生成（手工放置/任务指定） |
| Unique | 20 | NPC_ 带 Unique 标记 |
| Level | 20 | 等级 ≥ 玩家等级 + `LevelDelta` |
| Health | 0（默认关） | 最大生命 ≥ `HealthMin` |
| Aidt | 10 | 侵略性 ≥ 非常激进 且 自信 = 鲁莽（原脚本的规则 B） |
| Named | 0（默认关） | 有名字 |

硬排除：玩家、死亡、随从/队友、玩家召唤物、所有召唤物、非敌对（可关）、贫血中的 Essential。

> **为什么 DLL 要自己读插件文件？** Skyrim SE 运行时并不保留 EditorID，`GetFormEditorID()`
> 默认返回空串。所以插件在 `kDataLoaded` 时只扫每个插件的 `NPC_` 组（跳过其它 GRUP，很快），
> 建一张「FormID → 是否含 Boss / Unique / AIDT」的表。这张表让 EditorID 规则在没有
> *Native EditorID Fix* 的环境里也能用。

## 游戏内菜单

装了 SKSE Menu Framework 后，F1 打开菜单 → **Boss 动态识别 → 设置**：

* 总开关、判定阈值
* 每个信号的开关与权重滑条
* 等级差、最低血量
* 仅敌对 / 仅战斗 / 调试日志
* **目标（准星优先）→ 评估目标**：对当前准星目标（没指到就用战斗目标）立刻算分并显示明细
* **重新判定目标**：立即清空并重算已加条的 Boss
* **永不 Boss / 永远 Boss**：把当前目标写进黑名单 / 白名单
* **移出名单**：把当前目标从黑名单和白名单中移除
* 本次游戏已添加多少条 Boss 血条

改动约 1.5 秒后自动写回 `BossSense.ini`，也可以点 **立即保存设置**。

> 已加 Boss 条的目标每次被命中都会重算；分数掉到阈值以下会移除血条。
> 如果不想战斗中因为「血量阈值」波动导致条消失，可以把 `[Weights] Health = 0`。

### 界面语言（I18N 外置 JSON）

DLL 里只有英文原文，翻译放在 DLL 同目录的 `BossSense.json`：

* 有 `BossSense.json` → 按里面的词条显示中文；
* **删掉 `BossSense.json` → 立即恢复英文**（改词条也不用重编译）；
* 多语言包见 `dist/lang/<code>/SKSE/Plugins/BossSense.json`（提供 `en`、`zh-CN`、`ru`、`ja`、`ko`、`fr`、`de`、`es`）；
  同一时刻只读 DLL 同目录那一份，切语言 = 换文件。

字体由 SKSE Menu Framework 统一管理，`SKSEMenuFramework.ini` 里 `EnableChinese = true` 即可。

## INI

见 `dist/SKSE/Plugins/BossSense.ini`（**不含注释**，避免被游戏内菜单保存时重写掉；键名含义见上面的判定表）。

`[Lists]` 里用 `Plugin.esp:0xFORMID` 的写法加白名单/黑名单：

```ini
[Lists]
Whitelist = SomeBossMod.esp:0x0012AB
Blacklist = SomeMod.esp:0x0034CD
```

移除已有条目（加载时先应用名单，再按 `Remove*` 移除，并从原始列表删掉，菜单保存不会写回）：

```ini
[Lists]
RemoveBlacklist = SomeMod.esp:0x001234
RemoveWhitelist = SomeMod.esp:0x005678
```

## 从旧版迁移

旧的 `TrueHUD_missing_bosses.ini` 里那一千多条是"EDID 含 Boss + AIDT"的并集，
其中 AIDT 部分把大量普通怪也算进去了。装本 DLL 后请停用那份 ini：
TrueHUD 自带的原版 Boss 名单（龙、龙祭司、地点 Boss 等）仍然有效，Boss Sense 只在此基础上补判定。

## 构建

```powershell
$env:VCPKG_ROOT = "E:\vcpkg"   # 本机；换成你自己的 vcpkg 路径
cmake -B build -S . -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Release
```

产物：`build/Release/BossSense.dll`（+ `.pdb`）。

依赖：CommonLibSSE-NG `ng` 分支（默认用 `E:\CommonLibSSE-NG-alandtse`，不存在时回退到
`lib/commonlibsse-ng`；也可用 `COMMONLIB_SSE_FOLDER` 环境变量指定）、vcpkg manifest 模式
（`vcpkg.json` 已列出依赖，`VCPKG_ROOT=E:\vcpkg`，triplet `x64-windows-static-md`）。

## License

Boss Sense 以 **GNU General Public License v3.0 or later（GPL-3.0-or-later）** 发布。

它链接 [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG)（同为 GPL-3.0-or-later），
因此整体构成 GPL 组合作品：见 `COPYING.txt` 与 `EXCEPTIONS.md`。

## English

**Boss Sense** replaces the static `TrueHUD_missing_bosses.ini` workflow with a runtime
score-based boss detector. Hostile actors are scored when combat starts or when they are hit;
the ones that reach the threshold are pushed into TrueHUD via `IVTrueHUD4::AddBoss`. Signals:
boss race list, location boss markers (`LocRefType Boss` / `DLC2Boss1`), EditorID containing
"boss" (read straight from the plugin files, because Skyrim SE does not keep EditorIDs at
runtime), Unique flag, level, max health, AIDT (aggressive + foolhardy) and having a name.
Everything is tunable from the SKSE Menu Framework page or `BossSense.ini`.

Disable the old generated `TrueHUD_missing_bosses.ini` before using this plugin.
