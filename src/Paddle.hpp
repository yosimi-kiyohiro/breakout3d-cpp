#pragma once
#include "raylib.h"

// パドル：X軸移動のみ。Z方向にも厚みを持たせ、ヒット位置のZオフセットで
// ボールのvzを変化させる反射に使う（CollisionSystem側で計算）
class Paddle {
public:
    Vector3 position;  // 中心座標（Yは常にPADDLE_Y、Zは固定値）
    float halfWidth;
    float halfHeight;
    float halfDepth;

    Paddle(Vector3 pos, float halfWidth_, float halfHeight_, float halfDepth_);

    BoundingBox getBoundingBox() const;

    // dxだけX移動し、プレイフィールド境界内にクランプする
    void moveX(float dx);
};
