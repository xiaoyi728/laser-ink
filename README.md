# laser-ink

Whiteboard-style laser trail for X11 — the cursor leaves a glowing stroke that fades away in a moment.
Click-through, grabs no keys, ~1.7 MB RAM / ~0.5% CPU.

白板风格的激光笔迹（X11 原生）：鼠标划过留下带光晕的轨迹，按时间淡出消失。
窗口点击穿透（XShape 空输入区），不抓任何键盘按键，可与录屏/共享屏幕同时使用。

## 用法

    laser-ink [--trail 600] [--radius 4] [--color '#ff2b2b'] [--opacity 1.0] [--demo]

| 参数 | 默认 | 说明 |
|---|---|---|
| `--trail` | 600 | 轨迹存活毫秒数 |
| `--radius` | 4 | 光斑半径（像素） |
| `--color` | `#ff2b2b` | 主色 |
| `--opacity` | 1.0 | 最大不透明度 |
| `--demo` | - | 合成轨迹自检（用于截图/目视验证） |

退出：`Ctrl+C` 或 `pkill -x laser-ink`。

## 编译与安装

```sh
make            # 生成 ./laser-ink
make install    # 安装到 ~/.local/bin
```

依赖：`libX11-devel libXrender-devel libXext-devel`（Debian/Ubuntu 为 `libx11-dev libxrender-dev libxext-dev`）。

## 配合桌面快捷键

`contrib/laser-ink-toggle.sh` 是一个开/关切换脚本，可绑到桌面环境的全局快捷键
（例如 GNOME：设置 → 键盘 → 自定义快捷键）。

## 说明

- 仅支持 X11（32 位 ARGB visual + XRender）；Wayland 原生不支持。
- 轨迹用径向渐变贴图 + 逐点透明度实现；点沿路径每 1–2 px 插值，细线也连续。
- 与 gromit-mpx / boomer / spotlight 等工具可同时运行，互不干扰。

## License

未指定（All rights reserved）。如需开源授权请自行添加 LICENSE。
