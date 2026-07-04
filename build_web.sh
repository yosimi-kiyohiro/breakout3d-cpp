#!/bin/sh
# Web ビルドスクリプト（emscripten）v0草案（③-B 最小疎通スパイク）
# w64devkit で実行：sh build_web.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

# emsdk・Web用raylib静的libはtetris-cppプロジェクトと共用。
# 自分のプロジェクトからの相対パス（兄弟ディレクトリ）で解決する
# （他プロジェクトへの絶対パスハードコードを避けるため）。
EMSDK_DIR="$SCRIPT_DIR/../tetris-cpp/emsdk"
RAYLIB_SRC_DIR="$SCRIPT_DIR/../raylib-src/src"

if [ -f "$EMSDK_DIR/emsdk_env.sh" ]; then
    . "$EMSDK_DIR/emsdk_env.sh" 2>/dev/null
else
    echo "emsdk が見つかりません: $EMSDK_DIR"
    echo "tetris-cpp/emsdk が存在するか確認してください。"
    exit 1
fi

if [ ! -f "$RAYLIB_SRC_DIR/libraylib.web.a" ]; then
    echo "Web用raylib静的ライブラリが見つかりません: $RAYLIB_SRC_DIR/libraylib.web.a"
    exit 1
fi

mkdir -p web

echo "Web ビルド中（③-B 最小疎通スパイク）..."
em++ src/main.cpp src/Ball.cpp src/Paddle.cpp src/Block.cpp src/CollisionSystem.cpp \
     src/Stage.cpp src/GameState.cpp src/Renderer.cpp src/ParticleSystem.cpp src/AudioManager.cpp src/ScoreFile.cpp \
    -o web/index.html \
    -I "$RAYLIB_SRC_DIR" "$RAYLIB_SRC_DIR/libraylib.web.a" \
    -s USE_GLFW=3 \
    -s TOTAL_MEMORY=67108864 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s WASM=1 \
    --shell-file web/shell.html \
    -std=c++17 \
    -DPLATFORM_WEB \
    -Os

if [ $? -eq 0 ]; then
    echo "Web ビルド成功！ web/index.html を生成しました。"
    echo "ブラウザで確認する場合: emrun web/index.html"
else
    echo "Web ビルド失敗。エラーを確認してください。"
    exit 1
fi
