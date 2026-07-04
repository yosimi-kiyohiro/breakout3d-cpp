#pragma once
#include "raylib.h"

// ボール：位置・速度（Vector3）を持つだけのシンプルなデータクラス
// 衝突判定・反射のロジックは CollisionSystem に集約する
class Ball {
public:
    Vector3 position;
    Vector3 velocity;
    float radius;

    Ball(Vector3 pos, Vector3 vel, float r);

    // 衝突判定を挟まない単純な位置更新（CollisionSystemを使わない用途向け）
    void update(float dt);

    void reset(Vector3 pos, Vector3 vel);
};
