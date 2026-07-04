#pragma once
#include <vector>

#include "Ball.hpp"
#include "Block.hpp"
#include "Paddle.hpp"
#include "raylib.h"

// 3D衝突判定・反射ロジックを集約するモジュール（採用案C：
// raylibのCheckCollisionBoxSphereベース + サブステップ分割のハイブリッド方式）
namespace CollisionSystem {

// 1フレーム分の衝突解決の結果（テスト・演出トリガー用）
struct StepResult {
    bool hitWallX = false;
    bool hitWallZ = false;
    bool hitCeiling = false;
    bool hitPaddle = false;
    bool hitBlock = false;
    bool missed = false;  // MISS_LINE_Yを下回った
    int blockIndex = -1;  // 何番目のブロックに当たったか（当たらなければ-1）
    // 【修正1】このフレーム（複数サブステップ合計）で実際に「破壊された」ブロック数。
    // 旧実装はhitBlockがtrueかどうかしか見ておらず、同一サブステップで複数ブロックが
    // 同時破壊されてもGameState側でscore_ += 10しか加算されない過小評価バグがあった。
    // 【梨緒レビュー対応】単に「当たった回数」ではなく「破壊された回数」を数える
    // （耐久値2以上のブロックを1回叩いただけでは加点しない。壊れて初めて加点する）。
    // resolveBlocks側でブロックが破壊されるたびに加算し、GameStateはscore_ += 10 *
    // blockHitCountで破壊数に応じて加算する。
    int blockHitCount = 0;
};

// サブステップ数の算出：ceil(|v|*dt / radius) を 1〜MAX_SUBSTEPS にクランプ
int computeSubSteps(const Vector3& velocity, float dt, float radius);

// 1フレーム分（dt秒）の位置更新＋衝突解決。内部でサブステップに分割する
StepResult update(Ball& ball, const Paddle& paddle, std::vector<Block>& blocks, float dt);

}  // namespace CollisionSystem
