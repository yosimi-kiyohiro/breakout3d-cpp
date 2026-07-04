#!/bin/sh
# 衝突判定ヘッドレステストのビルド＆実行
# w64devkit で実行：sh build_test.sh

export PATH="/c/w64devkit/w64devkit/bin:$PATH"
cd "$(dirname "$0")" || exit 1

echo "ビルド中（test_collision.exe）..."
g++ src/test_collision.cpp src/Ball.cpp src/Paddle.cpp src/Block.cpp src/CollisionSystem.cpp src/Stage.cpp \
    -o test_collision.exe \
    -I C:/raylib/raylib-6.0_win64_mingw-w64/include \
    -L C:/raylib/raylib-6.0_win64_mingw-w64/lib \
    -std=c++17 -Wall -Wextra \
    -lraylib -lopengl32 -lgdi32 -lwinmm

if [ $? -eq 0 ]; then
    echo "ビルド成功！テストを実行します..."
    ./test_collision.exe
else
    echo "ビルド失敗。エラーを確認してください。"
    exit 1
fi
