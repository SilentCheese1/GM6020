# GM6020 正弦速度与位置控制（RoboMaster C 板）

这是 STM32F407 的完整 CMake 源码工程。当前电机采用 **CAN 电流控制**，电机拨码 ID 为 1；速度和位置两种模式共用 `Tasks/` 中的控制代码。默认编译选项也是电流控制。旧电压控制 ELF 留在本机，仅供以后需要时查阅，不能用于当前电流模式电机。

## Ozone 烧录哪个文件

当前源码编译出的两种电流控制固件如下；修改参数后重新编译对应模式，并在 Ozone 下载、复位：

| 模式 | 固定 ELF 路径 | 当前参数 |
| --- | --- | --- |
| 速度正弦 | `build-current-speed/C_project.elf` | 速度 P=375、I=D=0，滤波 50 ms，摩擦补偿 350 |
| 位置正弦 | `build-current-position/C_project.elf` | 位置 P=6；速度内环 P=375、I=D=0，滤波 50 ms，摩擦补偿 350 |

两种模式的 PID 电流指令限幅均为 ±1000，叠加摩擦补偿后的最终限幅为 ±1350。位置模式在 Ozone 中观察到约 75 秒时目标与反馈基本重合，见[位置跟踪截图](results/position_tracking.png)；此前曾出现延迟抖动，仍应在实际使用时继续观察。速度模式调参时的曲线见[速度跟踪截图](results/speed_tracking.png)。截图是特定电机和负载下的试车记录，不代表其他硬件条件也能直接复用参数。

以下 `firmware/` 仅是本机保存的历史试车固件，已被 `.gitignore` 排除：

| 用途 | 已经实机试过的固件 | 参数 |
| --- | --- | --- |
| 速度正弦 | `firmware/C_project_current_speed_p375_filter50ms_limit1000.elf` | 速度 P=375，I=D=0，滤波 50 ms，电流限幅 ±1000 |
| 位置正弦 | `firmware/C_project_current_position_kp6_limit1000.elf` | 位置 P=6，速度内环 P=400，I=D=0，滤波 20 ms，电流限幅 ±1000 |

仅在 Ozone 打开 ELF 不会更新板上程序，须执行下载/复位。

两份旧文件 `firmware/C_project_speed.elf`、`firmware/C_project_position.elf` 是电压模式，不用于当前电机，也不需要编辑 ELF。

## 在 VSCode 改参数

只改 `Tasks/inc/SineConfig.h`。这个文件包含曲线频率/幅值、电流限幅、速度反馈滤波、速度 PID 和位置 P。`#if SINE_DEFAULT_MODE == 0` 内是**独立速度模式**的速度环；`#else` 内是**位置模式**的速度内环。修改哪种模式，就只重编译那种模式。`SINE_CURRENT_SPEED_KP/KI/KD` 是电流模式速度环参数；`SINE_SPEED_KP/KI/KD` 是旧电压模式参数，当前不用。

现阶段 I、D 为 0 是刻意保留的已验证基线。不要直接改 `.elf`；ELF 是编译结果。

`SINE_STATIC_FRICTION_COMMAND` 用于速度内环的低速起步补偿，当前两种模式分别设为 350。它随目标速度平滑过零；最终指令最多为 ±1350（PID 限幅 1000 加补偿 350，约 0.25 A）。速度模式实机对比中 Kp=350 未改善快速波动，因此保留 Kp=375、Ki=0、Kd=0。位置模式采用同一组速度内环参数，位置外环 P=6；已保存的基线 ELF 不受影响。若电机仍不转且 `gm6020_current_fdb_a` 仍接近零，停止增加此值，先检查电机是否执行了电流指令。

在本目录执行：

```sh
cmake --build build-current-speed
cmake --build build-current-position
```

首次在另一台电脑配置构建目录时，先执行：

```sh
cmake -S . -B build-current-speed -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake -DGM6020_CURRENT_CONTROL=ON -DGM6020_DEMO_MODE=0
cmake -S . -B build-current-position -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake -DGM6020_CURRENT_CONTROL=ON -DGM6020_DEMO_MODE=1
```

需要已安装的 ARM GNU 工具链、CMake 和 Ninja。两个 `build-current-*` 目录分别保存两种编译配置；以后编译会覆盖各自固定路径的 ELF，不再按参数另存文件。源码里的 `GM6020_DEMO_MODE` 通过 CMake 设定：0 为速度，1 为位置。

## GitHub 保留哪些文件

以本目录 `C_project_overlay` 为仓库根目录上传。保留 `README.md`、`.gitignore`、`CMakeLists.txt`、`CMakePresets.json`、`C_project.ioc`、`Core/`、`Drivers/`、`Tasks/`、`cmake/`、`results/`、启动文件和链接脚本。`build*/`、`firmware/`、`Diagnostics/`、ELF/HEX/BIN、macOS 临时文件由 `.gitignore` 排除；它们是本地生成、排查或试车文件，不属于必须上传的源码。

## Ozone 观察变量

在 Data Sampling 采样以下变量，至少看两个完整的 5 秒正弦周期：

- 速度：`gm6020_speed_ref_rad_s`（目标）、`gm6020_speed_filtered_rad_s`（控制器使用的滤波反馈）、`gm6020_speed_fdb_rad_s`（原始反馈）。
- 位置：`gm6020_position_ref_rad`（目标）、`gm6020_position_fdb_rad`（反馈）。
- 通用：`gm6020_feedback_ok`（反馈有效时为 1）、`gm6020_current_command`（协议原始电流指令，不是安培数）。
- 排查不转：重新编译的速度版提供 `gm6020_current_fdb_a`（从电机 CAN 反馈计算的实际转矩电流，单位 A，换算为近似值）和 `gm6020_tx_fail_count`（连续调用 HAL CAN 发送入队失败次数；为 0 不等于电机已经收到指令）。把它们与 `gm6020_current_command` 一起观察；指令变化不代表电机已经执行。

电机供电、CAN 连接、拨码 ID 和电流/电压模式必须与固件一致。若异常转动、碰撞或发热，立即切断电机电源。烧录速度版和位置版时，应先停止当前运行并重新下载选定固件。

## 附录：实机跟踪结果

### 1. 位置正弦跟踪

红色为目标位置，绿色为实际位置；本次截图中两条曲线基本重合。

![位置正弦跟踪结果](results/position_tracking.png)

### 2. 速度正弦跟踪

红色为目标速度，绿色为滤波后的速度反馈。反馈存在一定采样波动，但整体跟随正弦目标。

![速度正弦跟踪结果](results/speed_tracking.png)
