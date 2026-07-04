#!/bin/sh
# ビルド＆起動スクリプト（ネイティブ確認用）
# w64devkit で実行：sh build.sh

# w64devkit の PATH を通す
export PATH="/c/w64devkit/w64devkit/bin:$PATH"

# プロジェクトルートに移動
cd "$(dirname "$0")" || exit 1

echo "ビルド中（breakout3d.exe）..."
g++ src/main.cpp src/Ball.cpp src/Paddle.cpp src/Block.cpp src/CollisionSystem.cpp src/Stage.cpp \
    src/GameState.cpp src/Renderer.cpp src/ParticleSystem.cpp src/AudioManager.cpp src/ScoreFile.cpp \
    -o breakout3d.exe \
    -I C:/raylib/raylib-6.0_win64_mingw-w64/include \
    -L C:/raylib/raylib-6.0_win64_mingw-w64/lib \
    -std=c++17 -Wall -Wextra \
    -lraylib -lopengl32 -lgdi32 -lwinmm

if [ $? -eq 0 ]; then
    echo "ビルド成功！起動します..."
    ./breakout3d.exe
else
    echo "ビルド失敗。エラーを確認してください。"
    exit 1
fi
