#pragma once
#include "raylib.h"

// ブロック：XY平面に配置。Z方向は見た目の奥行きと一致する薄い板（BLOCK_HALF_DEPTH）を
// 半厚みとして持つ（未光レビュー致命傷1対応：旧設計はZ_MIN〜Z_MAX全厚みで見た目と不一致だった）。
// ボールとの衝突判定はX面/Y面/Z面すべてを見る3軸判定にしている
class Block {
public:
    Vector3 position;  // 中心座標（Zは基本0固定。1周目はXY平面配置の簡略化方針のため）
    float halfWidth;
    float halfHeight;
    float halfDepth;  // Z方向の半厚み（見た目の描画サイズと一致。Constants::BLOCK_HALF_DEPTH）
    int durability;
    // 【梨緒レビュー対応・タスク4】durabilityはhit()のたびに減っていくため、
    // 「壊れる直前の値」を見ても多くの場合1にしかならない（耐久値2以上のブロックが
    // 破壊されるのは必ずdurability 1→0のタイミングのため）。生成時の耐久値を
    // 別途保持しておき、パーティクル演出の色選択等「元々何耐久だったか」を
    // 参照したい用途に使う（値はhit()で変化しない）
    int initialDurability;
    bool alive;

    Block(Vector3 pos, float halfWidth_, float halfHeight_, int durability_);

    BoundingBox getBoundingBox() const;

    // 1回被弾。耐久が0になったらalive=falseにする
    void hit();
};
