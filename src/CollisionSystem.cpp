#include "CollisionSystem.hpp"

#include <algorithm>
#include <cmath>

#include "constants.hpp"
#include "raymath.h"

namespace CollisionSystem {

namespace {

// 壁（X_MIN/X_MAX, Z_MIN/Z_MAX, 天井Y_MAX）との衝突解決
// MISS_LINE_Yは壁ではない（パドルを外した時のミス判定はupdate()側で行う）
void resolveWalls(Ball& ball, StepResult& result) {
    const float r = ball.radius;

    if (ball.position.x - r < Constants::X_MIN) {
        ball.position.x = Constants::X_MIN + r;
        if (ball.velocity.x < 0) ball.velocity.x = -ball.velocity.x;
        result.hitWallX = true;
    } else if (ball.position.x + r > Constants::X_MAX) {
        ball.position.x = Constants::X_MAX - r;
        if (ball.velocity.x > 0) ball.velocity.x = -ball.velocity.x;
        result.hitWallX = true;
    }

    if (ball.position.z - r < Constants::Z_MIN) {
        ball.position.z = Constants::Z_MIN + r;
        if (ball.velocity.z < 0) ball.velocity.z = -ball.velocity.z;
        result.hitWallZ = true;
    } else if (ball.position.z + r > Constants::Z_MAX) {
        ball.position.z = Constants::Z_MAX - r;
        if (ball.velocity.z > 0) ball.velocity.z = -ball.velocity.z;
        result.hitWallZ = true;
    }

    if (ball.position.y + r > Constants::Y_MAX) {
        ball.position.y = Constants::Y_MAX - r;
        if (ball.velocity.y > 0) ball.velocity.y = -ball.velocity.y;
        result.hitCeiling = true;
    }
}

// パドルとの衝突解決：ヒット位置のX/Zオフセットに応じて反射方向を決め、
// 速さはBALL_SPEEDに再正規化する（入射速度に関わらず一定の打ち返し速度にする設計）
void resolvePaddle(Ball& ball, const Paddle& paddle, StepResult& result) {
    if (ball.velocity.y >= 0) return;  // 上昇中はパドルに当たらない

    BoundingBox box = paddle.getBoundingBox();
    if (!CheckCollisionBoxSphere(box, ball.position, ball.radius)) return;

    // パドル上面より上に押し戻す（めり込み防止）
    ball.position.y = box.max.y + ball.radius;

    float offsetX = (ball.position.x - paddle.position.x) / paddle.halfWidth;
    offsetX = Clamp(offsetX, -1.0f, 1.0f);
    float offsetZ = (ball.position.z - paddle.position.z) / paddle.halfDepth;
    offsetZ = Clamp(offsetZ, -1.0f, 1.0f);

    float angleX = offsetX * Constants::PADDLE_MAX_BOUNCE_ANGLE;
    float angleZ = offsetZ * Constants::PADDLE_MAX_BOUNCE_ANGLE;

    Vector3 dir = {sinf(angleX), cosf(angleX) * cosf(angleZ), sinf(angleZ)};
    dir = Vector3Normalize(dir);

    ball.velocity = Vector3Scale(dir, Constants::BALL_SPEED);
    result.hitPaddle = true;
}

// ヒットしたブロックの外側へボールをスナップする（めり込み防止・二重ヒット防止）。
// 梨緒HIGH1対応：壁・パドルは押し戻しをしているのにブロックだけ速度反転のみで
// 位置補正が無かった。将来の多段耐久ブロックで同一フレーム内の二重反転・
// 耐久値の二重減算が起きないよう、壁・パドルと同じ「ヒット面の外側へスナップ」に揃える。
// refPos：どちら側へスナップするかを判定する基準位置。
// 【②修正】同一サブステップで複数ブロックを処理する際、先に処理したブロックの
// スナップでball.positionが動いた後の値を基準にしてしまうと、まだ未処理の隣接ブロックの
// 判定基準がブロックごとにブレてしまう（TC8で実際に発生：Aをスナップした後の位置を
// 基準にBを判定したところ、境界の浮動小数点誤差でBが「衝突なし」と判定されてしまった）。
// そのため方向判定にはこのサブステップ開始時点で固定したrefPosを使い、実際に書き込む
// 座標（ball.position）だけを更新する。
void snapOutsideAxis(Ball& ball, const Vector3& refPos, const BoundingBox& box, const Vector3& blockCenter,
                      char axis) {
    const float r = ball.radius;
    switch (axis) {
        case 'X':
            ball.position.x = (refPos.x < blockCenter.x) ? (box.min.x - r) : (box.max.x + r);
            break;
        case 'Y':
            ball.position.y = (refPos.y < blockCenter.y) ? (box.min.y - r) : (box.max.y + r);
            break;
        case 'Z':
        default:
            ball.position.z = (refPos.z < blockCenter.z) ? (box.min.z - r) : (box.max.z + r);
            break;
    }
}

// ブロックとの衝突解決：前フレーム（このサブステップ開始前）の位置を基準に
// X面/Y面/Z面のどれに当たったかを判定する（未光致命傷1対応でブロックのZ厚みを
// 見た目と一致する薄い板にしたため、Z面ヒットも実際に起こりうる3軸判定になった）。
// ちょうど1軸だけ「外側」と判定できる場合はその軸を採用し、2軸以上が同時に
// 外側（真の角ヒット・稜線ヒット）または0軸（判定不能）の場合のみ、
// めり込み量が最も浅い軸を優先するヒューリスティックでフォールバックする
// （設計上の既知の限界：厳密な衝突時刻計算ではない。TC7で実際にこの分岐を検証する）。
//
// 【②対応・TC12致命傷への修正】
// 旧実装は1サブステップにつき1ブロックのみ処理して即break していたが、
// ②で決めたステージの隙間（BLOCK_MIN_GAP=0.15）がボール直径（BALL_RADIUS*2=0.6）より
// 狭いため、隣接ブロックの継ぎ目ではボールが両方に同時に重なる状況が幾何学的に
// 避けられないことがTC12の実測で判明した（違反112/568件・最大オーバーラップ0.45が
// ボール半径0.3を超過）。1ブロックだけ割れて隣は無傷のまま次サブステップに持ち越すと、
// その間ボールが割れなかった方のブロックに深くめり込んだまま見た目上貫通して見える。
// このため、同一サブステップ内で現在ボールに重なっている「生存中の全ブロック」を
// まとめて処理する方式に拡張した。各軸（X/Y/Z）の速度反転は軸ごとに最大1回のみ
// （同じ軸を複数ブロックが要求しても二重反転で相殺しないようflippedX/Y/Zで管理）、
// 位置補正（snapOutsideAxis）は各ブロックごとに順番に適用する。
void resolveBlocks(Ball& ball, const Vector3& prevPos, std::vector<Block>& blocks, StepResult& result) {
    const float r = ball.radius;
    // このサブステップの解決を開始する時点の位置を固定して使う（②修正：後述）
    const Vector3 entryPos = ball.position;

    bool flippedX = false;
    bool flippedY = false;
    bool flippedZ = false;
    int firstHitIndex = -1;
    // 【修正1・梨緒レビュー対応】このサブステップで実際に「破壊された」ブロック数
    // （block.hit()を呼んだ回数ではなく、その結果alive=falseになった回数）。
    // 耐久値2以上のブロックを1回叩いただけ（まだ生存）では数えない。
    // 複数ブロックが同時に破壊された場合、全て加算してGameState側のスコア計算に渡す。
    int destroyedThisSubstep = 0;

    for (size_t i = 0; i < blocks.size(); ++i) {
        Block& block = blocks[i];
        if (!block.alive) continue;

        BoundingBox box = block.getBoundingBox();
        // 【②修正】ここで ball.position（既に前のブロックのスナップで動いている可能性がある）
        // ではなく entryPos（このサブステップ開始時点の位置）を基準に判定する。
        // 隣接ブロックは同じ面を共有していることが多く、片方をスナップした後の位置で
        // もう片方を判定すると境界の浮動小数点誤差で「衝突なし」に転んでしまうことがある
        // （TC8で実際に発生：Aをスナップ後の位置でBを判定したらBが検出されなかった）。
        if (!CheckCollisionBoxSphere(box, entryPos, r)) continue;

        bool wasOutsideX = (prevPos.x + r <= box.min.x) || (prevPos.x - r >= box.max.x);
        bool wasOutsideY = (prevPos.y + r <= box.min.y) || (prevPos.y - r >= box.max.y);
        bool wasOutsideZ = (prevPos.z + r <= box.min.z) || (prevPos.z - r >= box.max.z);
        int outsideCount = (wasOutsideX ? 1 : 0) + (wasOutsideY ? 1 : 0) + (wasOutsideZ ? 1 : 0);

        char hitAxis;
        if (outsideCount == 1) {
            hitAxis = wasOutsideX ? 'X' : (wasOutsideY ? 'Y' : 'Z');
        } else {
            // 角ヒット・稜線ヒット・判定不能：めり込みが最も浅い軸を反転（フォールバック）
            float overlapX = std::min(entryPos.x + r - box.min.x, box.max.x - (entryPos.x - r));
            float overlapY = std::min(entryPos.y + r - box.min.y, box.max.y - (entryPos.y - r));
            float overlapZ = std::min(entryPos.z + r - box.min.z, box.max.z - (entryPos.z - r));
            if (overlapX <= overlapY && overlapX <= overlapZ) {
                hitAxis = 'X';
            } else if (overlapY <= overlapZ) {
                hitAxis = 'Y';
            } else {
                hitAxis = 'Z';
            }
        }

        switch (hitAxis) {
            case 'X':
                if (!flippedX) {
                    ball.velocity.x = -ball.velocity.x;
                    flippedX = true;
                }
                break;
            case 'Y':
                if (!flippedY) {
                    ball.velocity.y = -ball.velocity.y;
                    flippedY = true;
                }
                break;
            case 'Z':
                if (!flippedZ) {
                    ball.velocity.z = -ball.velocity.z;
                    flippedZ = true;
                }
                break;
            default:
                break;
        }
        snapOutsideAxis(ball, entryPos, box, block.position, hitAxis);

        block.hit();
        // 【修正1・梨緒レビュー対応】「当たった」ではなく「破壊された」かどうかで数える
        if (!block.alive) destroyedThisSubstep++;
        if (firstHitIndex < 0) firstHitIndex = static_cast<int>(i);
        // break しない：同一サブステップで重なっている生存ブロックを全て処理する（②対応）
    }

    if (firstHitIndex >= 0) {
        result.hitBlock = true;
        result.blockIndex = firstHitIndex;
        // 【修正1】resolveBlocksはサブステップごとに呼ばれるため、+=で1フレーム分を積算する
        result.blockHitCount += destroyedThisSubstep;
    }
}

}  // namespace

int computeSubSteps(const Vector3& velocity, float dt, float radius) {
    if (radius <= 0.0f) return 1;
    float speed = Vector3Length(velocity);
    int n = static_cast<int>(std::ceil((speed * dt) / radius));
    return std::max(1, std::min(Constants::MAX_SUBSTEPS, n));
}

StepResult update(Ball& ball, const Paddle& paddle, std::vector<Block>& blocks, float dt) {
    StepResult result;
    int steps = computeSubSteps(ball.velocity, dt, ball.radius);
    float subDt = dt / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i) {
        Vector3 prevPos = ball.position;
        ball.position = Vector3Add(ball.position, Vector3Scale(ball.velocity, subDt));

        resolveWalls(ball, result);
        resolvePaddle(ball, paddle, result);
        resolveBlocks(ball, prevPos, blocks, result);

        if (ball.position.y - ball.radius < Constants::MISS_LINE_Y) {
            result.missed = true;
        }
        // 梨緒レビュー軽微指摘対応：ミス確定後はそのフレームの残りサブステップを
        // 計算しても意味がない（GameState::update側でこのフレームの結果は破棄される）
        // ため、無駄な計算を避けて打ち切る
        if (result.missed) break;
    }

    return result;
}

}  // namespace CollisionSystem
