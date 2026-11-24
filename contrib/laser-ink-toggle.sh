#!/usr/bin/env bash
# 开/关 激光笔迹（需 laser-ink 已在 PATH 中）
BIN="${LASER_INK_BIN:-laser-ink}"
if pgrep -x laser-ink >/dev/null 2>&1; then
  pkill -x laser-ink
else
  setsid "$BIN" --trail 600 --radius 4 --color '#ff2b2b' >/dev/null 2>&1 < /dev/null &
fi
