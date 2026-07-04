// 3D衝突判定システムのヘッドレステスト（raylibウィンドウを開かない）
// TC1〜TC11を固定dtでシミュレーションし、実測値をコンソール＋test_results.logへ出力する
//
// 【2周目修正・梨緒HIGH2＋未光致命傷2対応】
// 旧TC2・TC5は「1軸ずつ孤立させたテスト」に過ぎず、vx・vy・vzが同時に非ゼロの
// 複合軌道が一度も出てこないという指摘を受けた。TC2は真の3軸複合反射＋部屋の角への
// 同時衝突検証に書き換え、TC5は正直な名称（Z軸単軸）に改めた上でパドルのX+Z複合
// オフセット反射を追加した。さらにTC7（真の角ヒット）・TC8（隣接ブロックの継ぎ目）・
// TC9（パドル→ブロック→壁の複合軌道）を新設した。
//
// 【3周目修正・未光の再検証で発覚した新たな致命傷への対応】
// 2周目で「9/9 PASS・Go判定」と報告したが、TC1〜TC9は全てz=0固定・vz=0固定のままで、
// ブロックのZ面（実際にボールがぶつかる核心部分）が一度も検証されていなかった。
// TC10（ブロックのZ面単独ヒット）・TC11（X/Y/Zの3軸すべてが同時に外側判定になる
// 真の3方向角ヒット）を新設し、TC9のoffsetZを非ゼロに修正して名前負けを解消した。
//
// 【梨緒レビューLOW指摘対応・TC13新設】
// 現在のブロック密度（BLOCK_MIN_GAP=0.15・BALL_RADIUS=0.3）では、2x2に隣接配置した
// 4ブロックの隙間の交差点（十字の中心）付近でボールが4ブロック全てに同時に重なる
// 状況が幾何学的に起こりうる。TC12は2ブロック隣接（1方向）までしか検証しておらず、
// 4ブロック同時ヒット時にsnapOutsideAxisが各ブロックごとにball.positionを個別に
// 上書きする実装で、同じ軸に複数ブロックが異なるスナップ値を要求した場合に
// 何が起きるかは未検証だった。TC13でこの4ブロック同時ヒットを実測する。
//
// 【修正3：TC13を4方向に拡張】旧版のTC13は「右上から」の1方向のみの検証だった。
// TC7/TC10/TC11のように右上・左上・右下・左下の4方向からの対角アプローチを網羅し、
// 十字の中心という完全対称点でも軸選択が健全（クラッシュしない・生存ブロックへの
// めり込みが残らない）ことを4パターンで実測する。
//
// 【修正1：blockHitCountの追加】StepResultにblockHitCount（このフレームでヒットした
// ブロック数）を追加し、GameState::update()でscore_ += 10 * r.blockHitCountとして
// 破壊数に応じて加算するよう修正した。旧実装はr.hitBlockがtrueなら常に固定10点のみで、
// 同一サブステップで複数ブロックが同時破壊されてもスコアが過小評価されるバグがあった。
// TC8・TC13にblockHitCountの回帰チェック（期待値どおりの個数になっているか）を追加した。
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <vector>

#include "Ball.hpp"
#include "Block.hpp"
#include "CollisionSystem.hpp"
#include "Paddle.hpp"
#include "Stage.hpp"
#include "constants.hpp"
#include "raymath.h"

namespace {

// raylib.h が PI・RAD2DEG をマクロ定義しているため衝突を避けて別名にする
constexpr float MATH_PI = 3.14159265358979323846f;
constexpr float MY_RAD2DEG = 180.0f / MATH_PI;
constexpr float FIXED_DT = 1.0f / 60.0f;

FILE* g_logFile = nullptr;

// 未光の指摘対応：結果がコンソール出力のみで証跡が残らない問題への対応。
// printfと同じ書式でコンソール＋test_results.logの両方に出力する
void dprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    if (g_logFile != nullptr) {
        va_list argsFile;
        va_start(argsFile, fmt);
        vfprintf(g_logFile, fmt, argsFile);
        va_end(argsFile);
    }
}

float radToDeg(float rad) { return rad * MY_RAD2DEG; }

Paddle makeDefaultPaddle() {
    return Paddle({0.0f, Constants::PADDLE_Y, 0.0f}, Constants::PADDLE_HALF_WIDTH, Constants::PADDLE_HALF_HEIGHT,
                  Constants::PADDLE_HALF_DEPTH);
}

bool outOfBounds(const Ball& ball, float tol) {
    return ball.position.x < Constants::X_MIN - tol || ball.position.x > Constants::X_MAX + tol ||
           ball.position.z < Constants::Z_MIN - tol || ball.position.z > Constants::Z_MAX + tol ||
           ball.position.y > Constants::Y_MAX + tol;
}

float speedOf(const Ball& ball) {
    return std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y +
                      ball.velocity.z * ball.velocity.z);
}

// --- TC1: 垂直落下（中央発射・通常速度・50往復） ---
bool testTC1() {
    dprintf("=== TC1: 垂直落下（中央発射・通常速度・50往復） ===\n");
    Paddle paddle = makeDefaultPaddle();
    std::vector<Block> blocks;
    Ball ball({0.0f, 2.0f, 0.0f}, {0.0f, -Constants::BALL_SPEED, 0.0f}, Constants::BALL_RADIUS);

    int paddleHits = 0, ceilingHits = 0, tunnelCount = 0, signErrors = 0;
    const int targetHits = 50;
    int iterations = 0;
    const int maxIterations = 200000;

    while (paddleHits < targetHits && iterations < maxIterations) {
        iterations++;
        float vyBefore = ball.velocity.y;
        CollisionSystem::StepResult r = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);
        if (outOfBounds(ball, 0.01f)) tunnelCount++;
        if (r.hitPaddle) {
            paddleHits++;
            if (!(vyBefore < 0 && ball.velocity.y > 0)) signErrors++;
        }
        if (r.hitCeiling) {
            ceilingHits++;
            if (!(vyBefore > 0 && ball.velocity.y < 0)) signErrors++;
        }
    }

    bool pass = tunnelCount == 0 && signErrors == 0 && paddleHits >= targetHits;
    dprintf("  パドルヒット=%d回 天井ヒット=%d回 すり抜け=%d回 vy符号反転エラー=%d回\n", paddleHits, ceilingHits,
            tunnelCount, signErrors);
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC2: 真の3軸複合斜め反射＋部屋の角への同時衝突（速度保存検証） ---
// 【書き換え理由】旧TC2は名前こそ「45度斜め」だがvy=0固定のXZ平面内の動きに過ぎず、
// 3軸が同時に非ゼロの複合軌道ではなかった（梨緒HIGH2）。角度の符号反転だけを見る
// トートロジー的な検証からも脱却し、反射後の速度ベクトルの大きさ（≒BALL_SPEED）が
// 保存されているかを主軸に据える。
bool testTC2() {
    dprintf("=== TC2: 真の3軸複合斜め反射＋部屋の角への同時衝突（速度保存検証） ===\n");

    // (a) vx・vy・vzすべて非ゼロの持続ループ：X壁・Z壁・天井への複合反射を繰り返し、
    //     速度ベクトルの大きさが常にBALL_SPEEDに保たれ、どの軸も0に縮退しないかを見る
    Paddle paddleA = makeDefaultPaddle();
    std::vector<Block> blocksA;
    float v = Constants::BALL_SPEED / std::sqrt(3.0f);
    Ball ballA({0.0f, 5.0f, 0.0f}, {v, v, v}, Constants::BALL_RADIUS);

    int totalHits = 0, tunnelCount = 0, speedErrors = 0, axisDegenerate = 0;
    float maxSpeedErr = 0.0f;
    const int targetHits = 50;
    int iterations = 0;
    const int maxIterations = 200000;

    while (totalHits < targetHits && iterations < maxIterations) {
        iterations++;
        CollisionSystem::StepResult r = CollisionSystem::update(ballA, paddleA, blocksA, FIXED_DT);
        if (outOfBounds(ballA, 0.01f)) tunnelCount++;

        bool anyWallHit = r.hitWallX || r.hitWallZ || r.hitCeiling;
        if (anyWallHit) {
            totalHits++;
            float speedErr = std::fabs(speedOf(ballA) - Constants::BALL_SPEED);
            if (speedErr > maxSpeedErr) maxSpeedErr = speedErr;
            if (speedErr > 0.01f) speedErrors++;
        }
        if (ballA.velocity.x == 0.0f || ballA.velocity.y == 0.0f || ballA.velocity.z == 0.0f) axisDegenerate++;
    }

    bool passA = tunnelCount == 0 && speedErrors == 0 && axisDegenerate == 0 && totalHits >= targetHits;
    dprintf("  (a)持続3軸反射: 壁/天井ヒット=%d回 すり抜け=%d回 速度保存誤差(最大)=%.5f(基準:0.01) 軸縮退=%d回 -> %s\n",
            totalHits, tunnelCount, maxSpeedErr, axisDegenerate, passA ? "PASS" : "FAIL");

    // (b) 部屋の真の角（X_MAX・Y_MAX・Z_MAX付近）へ同時に接触させ、1回のupdate()呼び出しで
    //     X壁・Z壁・天井の3枚すべてに同時ヒットしても速度の大きさが保存され、境界内に
    //     収まっているかを検証する（複数の独立した壁面判定が同一サブステップで
    //     同時に発火する複合ケース）
    Paddle paddleB = makeDefaultPaddle();
    std::vector<Block> blocksB;
    float r0 = Constants::BALL_RADIUS;
    Ball cornerBall({Constants::X_MAX - r0 - 0.05f, Constants::Y_MAX - r0 - 0.05f, Constants::Z_MAX - r0 - 0.05f},
                     {Constants::BALL_SPEED, Constants::BALL_SPEED, Constants::BALL_SPEED}, r0);
    float speedBefore = speedOf(cornerBall);
    CollisionSystem::StepResult rc = CollisionSystem::update(cornerBall, paddleB, blocksB, FIXED_DT);
    float speedAfter = speedOf(cornerBall);
    bool cornerTripleHit = rc.hitWallX && rc.hitWallZ && rc.hitCeiling;
    bool cornerSpeedOk = std::fabs(speedAfter - speedBefore) < 0.01f;
    bool cornerInBounds = !outOfBounds(cornerBall, 0.01f);
    bool passB = cornerTripleHit && cornerSpeedOk && cornerInBounds;
    dprintf(
        "  (b)部屋の角への同時衝突: X壁=%s Z壁=%s 天井=%s 速度保存前後=%.4f->%.4f 境界内=%s -> %s\n",
        rc.hitWallX ? "true" : "false", rc.hitWallZ ? "true" : "false", rc.hitCeiling ? "true" : "false",
        speedBefore, speedAfter, cornerInBounds ? "true" : "false", passB ? "PASS" : "FAIL");

    bool pass = passA && passB;
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC3: パドル端ヒット（オフセット-0.9〜+0.9・20回・反射角の一致検証） ---
// 【未光の指摘・トートロジー解消の限界】期待値の計算式(offset * PADDLE_MAX_BOUNCE_ANGLE)は
// 本番コード（CollisionSystem.cpp resolvePaddle）と同じ式を再実装している。これは
// パドル反射が「入射方向によらずヒット位置だけで反射方向を決める」設計であるため、
// 古典的な「入射角=反射角」のような物理的反射則が存在せず、設計意図そのものと
// 比較する以外の独立した幾何学的検証手段が無い（完全な独立化はできていない・既知の限界）。
// そのため角度の一致検証に加えて、独立した不変量として「反射後の速さがBALL_SPEedに
// 再正規化されているか」「オフセットの符号と反射方向の符号が一致するか」を追加する。
bool testTC3() {
    dprintf("=== TC3: パドル端ヒット（オフセット±0.9付近・20回） ===\n");
    const int n = 20;
    int passCount = 0;
    float maxErrorDeg = 0.0f;

    for (int k = 0; k < n; ++k) {
        float offsetX = -0.9f + k * (1.8f / (n - 1));
        Paddle paddle = makeDefaultPaddle();
        std::vector<Block> blocks;

        float startX = paddle.position.x + offsetX * paddle.halfWidth;
        BoundingBox box = paddle.getBoundingBox();
        float startY = box.max.y + Constants::BALL_RADIUS * 0.5f;  // 少しめり込んだ位置から開始
        Ball ball({startX, startY, 0.0f}, {0.0f, -Constants::BALL_SPEED, 0.0f}, Constants::BALL_RADIUS);

        CollisionSystem::StepResult r = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);

        float expectedAngleDeg = radToDeg(offsetX * Constants::PADDLE_MAX_BOUNCE_ANGLE);
        float actualAngleDeg = radToDeg(std::atan2(ball.velocity.x, ball.velocity.y));
        float errDeg = std::fabs(actualAngleDeg - expectedAngleDeg);
        if (errDeg > maxErrorDeg) maxErrorDeg = errDeg;

        // 独立した不変量チェック：符号一致・速さの再正規化（本番式のコピーに依存しない）
        bool signOk = (offsetX == 0.0f) || ((offsetX > 0.0f) == (ball.velocity.x > 0.0f));
        bool speedOk = std::fabs(speedOf(ball) - Constants::BALL_SPEED) < 0.01f;

        bool ok = r.hitPaddle && errDeg <= 2.0f && signOk && speedOk;
        if (ok) passCount++;
        dprintf("  [offsetX=%+.3f] 期待角=%.2f度 実測角=%.2f度 誤差=%.3f度 符号一致=%s 速さ再正規化=%s %s\n", offsetX,
                expectedAngleDeg, actualAngleDeg, errDeg, signOk ? "OK" : "NG", speedOk ? "OK" : "NG",
                ok ? "OK" : "NG");
    }

    bool pass = passCount == n;
    dprintf("  合格数=%d/%d 最大誤差=%.5f度（合格基準:±2度）\n", passCount, n, maxErrorDeg);
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC4: 高速球（通常速度の5倍・X壁間・50往復・すり抜け検証） ---
bool testTC4() {
    dprintf("=== TC4: 高速球（通常速度の5倍・50往復・サブステップ検証） ===\n");
    Paddle paddle = makeDefaultPaddle();
    std::vector<Block> blocks;
    float fastSpeed = Constants::BALL_SPEED * 5.0f;
    Ball ball({0.0f, 5.0f, 0.0f}, {fastSpeed, 0.0f, 0.0f}, Constants::BALL_RADIUS);

    int wallHits = 0, tunnelCount = 0, signErrors = 0;
    const int targetHits = 50;
    int iterations = 0;
    const int maxIterations = 200000;
    int observedSubSteps = CollisionSystem::computeSubSteps(ball.velocity, FIXED_DT, ball.radius);

    while (wallHits < targetHits && iterations < maxIterations) {
        iterations++;
        float vxBefore = ball.velocity.x;
        CollisionSystem::StepResult r = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);
        if (outOfBounds(ball, 0.01f)) tunnelCount++;
        if (r.hitWallX) {
            wallHits++;
            if (!((vxBefore < 0 && ball.velocity.x > 0) || (vxBefore > 0 && ball.velocity.x < 0))) signErrors++;
        }
    }

    bool pass = tunnelCount == 0 && signErrors == 0 && wallHits >= targetHits;
    dprintf("  速度=%.1f (通常の%.0f倍) 1フレームあたりサブステップ数=%d\n", fastSpeed,
            fastSpeed / Constants::BALL_SPEED, observedSubSteps);
    dprintf("  壁ヒット=%d回 すり抜け=%d回 vx符号反転エラー=%d回\n", wallHits, tunnelCount, signErrors);
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC5: Z軸壁の単軸反転（正直な名称）＋パドルX+Z複合オフセット反射 ---
// 【改名理由】旧「Z軸斜め入射」は実態としては単軸テストの組み合わせに過ぎず、
// 名前と実態が乖離していた（梨緒HIGH2）。(a)は素直に単軸テストとして名称通り正直に扱い、
// (b)を「X+Zを同時にオフセットさせる真の複合反射」に書き換えて3軸性を持たせた。
bool testTC5() {
    dprintf("=== TC5: (a)Z壁の単軸反転（正直な名称） + (b)パドルX+Z複合オフセット反射 ===\n");

    // (a) Z壁でのvz反転（15回・単軸であることを名称通り正直に記載）
    Paddle paddle = makeDefaultPaddle();
    std::vector<Block> blocksA;
    Ball ballA({0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, Constants::BALL_SPEED}, Constants::BALL_RADIUS);
    int zWallHits = 0, tunnelCountA = 0, signErrorsA = 0;
    int iterations = 0;
    const int maxIterations = 200000;
    while (zWallHits < 15 && iterations < maxIterations) {
        iterations++;
        float vzBefore = ballA.velocity.z;
        CollisionSystem::StepResult r = CollisionSystem::update(ballA, paddle, blocksA, FIXED_DT);
        if (outOfBounds(ballA, 0.01f)) tunnelCountA++;
        if (r.hitWallZ) {
            zWallHits++;
            if (!((vzBefore < 0 && ballA.velocity.z > 0) || (vzBefore > 0 && ballA.velocity.z < 0))) signErrorsA++;
        }
    }
    bool passA = tunnelCountA == 0 && signErrorsA == 0 && zWallHits >= 15;
    dprintf("  (a)Z壁単軸反転: ヒット=%d回 すり抜け=%d回 vz符号反転エラー=%d回 -> %s\n", zWallHits, tunnelCountA,
            signErrorsA, passA ? "PASS" : "FAIL");

    // (b) パドルX+Z複合オフセット反射（X・Zを同時に変化させる・15パターン）
    //     offsetXとoffsetZを逆位相で同時に振ることで、ほぼ全パターンでX・Z両方が
    //     非ゼロオフセットになる真の複合ケースを作る
    const int n = 15;
    int passCountB = 0;
    float maxSpeedErrB = 0.0f;
    for (int k = 0; k < n; ++k) {
        float offsetX = -0.9f + k * (1.8f / (n - 1));
        float offsetZ = 0.9f - k * (1.8f / (n - 1));
        Paddle p = makeDefaultPaddle();
        std::vector<Block> blocksB;

        float startX = p.position.x + offsetX * p.halfWidth;
        float startZ = p.position.z + offsetZ * p.halfDepth;
        BoundingBox box = p.getBoundingBox();
        float startY = box.max.y + Constants::BALL_RADIUS * 0.5f;
        Ball ball({startX, startY, startZ}, {0.0f, -Constants::BALL_SPEED, 0.0f}, Constants::BALL_RADIUS);

        CollisionSystem::StepResult r = CollisionSystem::update(ball, p, blocksB, FIXED_DT);

        // 独立不変量：速さの再正規化・符号一致（X・Z両方）
        bool speedOk = std::fabs(speedOf(ball) - Constants::BALL_SPEED) < 0.01f;
        float speedErr = std::fabs(speedOf(ball) - Constants::BALL_SPEED);
        if (speedErr > maxSpeedErrB) maxSpeedErrB = speedErr;
        bool signXOk = (offsetX == 0.0f) || ((offsetX > 0.0f) == (ball.velocity.x > 0.0f));
        bool signZOk = (offsetZ == 0.0f) || ((offsetZ > 0.0f) == (ball.velocity.z > 0.0f));

        bool ok = r.hitPaddle && speedOk && signXOk && signZOk;
        if (ok) passCountB++;
        dprintf(
            "  [offsetX=%+.3f offsetZ=%+.3f] vx=%+.3f vy=%+.3f vz=%+.3f 速さ=%.4f 符号X=%s 符号Z=%s %s\n", offsetX,
            offsetZ, ball.velocity.x, ball.velocity.y, ball.velocity.z, speedOf(ball), signXOk ? "OK" : "NG",
            signZOk ? "OK" : "NG", ok ? "OK" : "NG");
    }
    bool passB = passCountB == n;
    dprintf("  (b)パドルX+Z複合オフセット: 合格数=%d/%d 速さ再正規化誤差(最大)=%.5f -> %s\n", passCountB, n,
            maxSpeedErrB, passB ? "PASS" : "FAIL");

    bool pass = passA && passB;
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC6: ブロック四隅ギリギリ（前フレーム位置基準のX面/Y面判定・10回） ---
struct CornerTrial {
    const char* label;
    Vector3 blockCenter;
    Vector3 prevPos;
    Vector3 target;
    char expectedAxis;  // 'X' or 'Y'
};

bool testTC6() {
    dprintf("=== TC6: ブロック四隅ギリギリ（前フレーム位置基準のX/Y面判定・10回） ===\n");

    std::vector<CornerTrial> trials = {
        {"block A 左からXフェース", {0, 7, 0}, {-1.05f, 7.0f, 0}, {-0.85f, 7.0f, 0}, 'X'},
        {"block A 右からXフェース", {0, 7, 0}, {1.05f, 7.0f, 0}, {0.85f, 7.0f, 0}, 'X'},
        {"block A 上からYフェース", {0, 7, 0}, {0, 7.75f, 0}, {0, 7.55f, 0}, 'Y'},
        {"block A 下からYフェース", {0, 7, 0}, {0, 6.20f, 0}, {0, 6.45f, 0}, 'Y'},
        {"block A 左下寄りXフェース", {0, 7, 0}, {-1.05f, 6.75f, 0}, {-0.85f, 6.75f, 0}, 'X'},
        {"block A 右上寄りXフェース", {0, 7, 0}, {1.05f, 7.25f, 0}, {0.85f, 7.25f, 0}, 'X'},
        {"block A 上左寄りYフェース", {0, 7, 0}, {-0.6f, 7.75f, 0}, {-0.6f, 7.55f, 0}, 'Y'},
        {"block A 上右寄りYフェース", {0, 7, 0}, {0.6f, 7.75f, 0}, {0.6f, 7.55f, 0}, 'Y'},
        {"block B 左からXフェース", {3, 7, 0}, {1.95f, 7.0f, 0}, {2.15f, 7.0f, 0}, 'X'},
        {"block B 下からYフェース", {3, 7, 0}, {3.0f, 6.20f, 0}, {3.0f, 6.45f, 0}, 'Y'},
    };

    int passCount = 0;
    Paddle paddle = makeDefaultPaddle();

    for (const auto& t : trials) {
        std::vector<Block> blocks;
        blocks.emplace_back(t.blockCenter, Constants::BLOCK_HALF_WIDTH, Constants::BLOCK_HALF_HEIGHT, 1);

        Vector3 displacement = {t.target.x - t.prevPos.x, t.target.y - t.prevPos.y, t.target.z - t.prevPos.z};
        Vector3 velocity = {displacement.x / FIXED_DT, displacement.y / FIXED_DT, displacement.z / FIXED_DT};
        Ball ball(t.prevPos, velocity, Constants::BALL_RADIUS);

        int steps = CollisionSystem::computeSubSteps(ball.velocity, FIXED_DT, ball.radius);
        CollisionSystem::StepResult r = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);

        bool axisXFlipped =
            (velocity.x != 0.0f) && (ball.velocity.x == -velocity.x) && (ball.velocity.y == velocity.y);
        bool axisYFlipped =
            (velocity.y != 0.0f) && (ball.velocity.y == -velocity.y) && (ball.velocity.x == velocity.x);

        bool axisCorrect = (t.expectedAxis == 'X') ? axisXFlipped : axisYFlipped;
        bool ok = r.hitBlock && steps == 1 && axisCorrect && !blocks[0].alive;
        if (ok) passCount++;

        dprintf("  [%s] サブステップ=%d 期待軸=%c ブロックヒット=%s 軸判定=%s %s\n", t.label, steps, t.expectedAxis,
                r.hitBlock ? "true" : "false", axisCorrect ? "一致" : "不一致", ok ? "OK" : "NG");
    }

    bool pass = passCount == static_cast<int>(trials.size());
    dprintf("  合格数=%d/%d\n", passCount, static_cast<int>(trials.size()));
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC7（新設）: 真の角ヒット（ブロックのX面・Y面の両方が「外側」判定になる対角アプローチ） ---
// 未光致命傷2対応：resolveBlocksのフォールバック分岐（角ヒット処理）を実際に通過させて検証する。
// prevPosをブロックの対角外側（X・Y両方とも「外側」判定される領域）に置き、
// 1サブステップでブロック内部へ到達する軌道にすることで、wasOutsideX・wasOutsideYが
// 両方trueになる状況を意図的に発生させる。
bool testTC7() {
    dprintf("=== TC7（新設）: 真の角ヒット（X面・Y面が両方「外側」判定の対角アプローチ・4パターン） ===\n");

    struct DiagonalTrial {
        const char* label;
        Vector3 blockCenter;
        Vector3 prevPos;  // ブロックの対角外側（X・Y両方とも外側判定になる位置）
        Vector3 target;   // ブロック内部
    };

    // TC6と同じ「ボール半径ぎりぎりの小さな変位」スケールで、X面・Y面の両方を同時に
    // 外側判定にする（1サブステップ内で角ヒットのフォールバック分岐を確実に通す）
    std::vector<DiagonalTrial> trials = {
        {"右上から対角進入", {0, 7, 0}, {1.05f, 7.75f, 0}, {0.85f, 7.55f, 0}},
        {"左上から対角進入", {0, 7, 0}, {-1.05f, 7.75f, 0}, {-0.85f, 7.55f, 0}},
        {"右下から対角進入", {0, 7, 0}, {1.05f, 6.20f, 0}, {0.85f, 6.40f, 0}},
        {"左下から対角進入", {0, 7, 0}, {-1.05f, 6.20f, 0}, {-0.85f, 6.40f, 0}},
    };

    int passCount = 0;
    Paddle paddle = makeDefaultPaddle();
    const float r = Constants::BALL_RADIUS;

    for (const auto& t : trials) {
        // 事前に「本当に両軸とも外側判定になる状況」であることを確認する
        // （resolveBlocks内部と同じ判定式をテスト側でも独立に評価し、狙った状況を保証する）
        float halfW = Constants::BLOCK_HALF_WIDTH, halfH = Constants::BLOCK_HALF_HEIGHT;
        float boxMinX = t.blockCenter.x - halfW, boxMaxX = t.blockCenter.x + halfW;
        float boxMinY = t.blockCenter.y - halfH, boxMaxY = t.blockCenter.y + halfH;
        bool wasOutsideX = (t.prevPos.x + r <= boxMinX) || (t.prevPos.x - r >= boxMaxX);
        bool wasOutsideY = (t.prevPos.y + r <= boxMinY) || (t.prevPos.y - r >= boxMaxY);
        bool trueCornerSetup = wasOutsideX && wasOutsideY;

        std::vector<Block> blocks;
        blocks.emplace_back(t.blockCenter, Constants::BLOCK_HALF_WIDTH, Constants::BLOCK_HALF_HEIGHT, 1);

        Vector3 displacement = {t.target.x - t.prevPos.x, t.target.y - t.prevPos.y, t.target.z - t.prevPos.z};
        Vector3 velocity = {displacement.x / FIXED_DT, displacement.y / FIXED_DT, displacement.z / FIXED_DT};
        Ball ball(t.prevPos, velocity, r);

        int steps = CollisionSystem::computeSubSteps(ball.velocity, FIXED_DT, ball.radius);
        CollisionSystem::StepResult res = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);

        // フォールバック分岐が選んだ軸を確認（ちょうど1軸だけが反転しているか＝クラッシュや
        // 二重反転が起きていないか）。exactly-oneをOKの条件にする
        bool xFlipped = (ball.velocity.x == -velocity.x);
        bool yFlipped = (ball.velocity.y == -velocity.y);
        bool exactlyOneFlipped = (xFlipped != yFlipped);  // XOR：片方だけtrue

        bool ok = trueCornerSetup && res.hitBlock && steps == 1 && exactlyOneFlipped && !blocks[0].alive;
        if (ok) passCount++;

        dprintf(
            "  [%s] 真の角ヒット条件成立=%s サブステップ=%d ブロックヒット=%s 反転軸=%s%s フォールバック健全=%s\n",
            t.label, trueCornerSetup ? "true" : "false", steps, res.hitBlock ? "true" : "false",
            xFlipped ? "X" : "", yFlipped ? "Y" : "", exactlyOneFlipped ? "OK" : "NG(二重/無反転)");
        if (!ok) dprintf("       -> %s\n", "NG");
    }

    bool pass = passCount == static_cast<int>(trials.size());
    dprintf("  合格数=%d/%d\n", passCount, static_cast<int>(trials.size()));
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC8（②対応で書き換え）: 隣接ブロックの継ぎ目への同時ヒット ---
// 【②修正・TC12致命傷対応】旧TC8は「1サブステップ1ブロックのみ処理」という当時の
// 仕様をそのまま記録するテストだったが、TC12の実測でこの仕様がステージの実際の密度では
// 視認可能な貫通（オーバーラップ量がボール半径を超える）を引き起こすことが判明し、
// resolveBlocksを「同一サブステップで重なっている生存ブロックを全て処理する」方式に
// 拡張した（①-Bのスコープを超えるが②のブロック密度が判明した時点での対応）。
// TC8はこの新しい期待値（継ぎ目に同時接触する隣接ブロックは両方同時に割れる。
// 速度反転はY軸1回のみで二重反転しない）を検証するテストに書き換える。
bool testTC8() {
    dprintf("=== TC8（②修正）: 隣接ブロックの継ぎ目への同時ヒット（両方同時に割れることを検証） ===\n");

    // block A: x範囲[-0.7, 0.7]、block B: x範囲[0.7, 2.1]（x=0.7でぴったり隣接）
    Vector3 centerA = {0.0f, 7.0f, 0.0f};
    Vector3 centerB = {1.4f, 7.0f, 0.0f};
    std::vector<Block> blocks;
    blocks.emplace_back(centerA, Constants::BLOCK_HALF_WIDTH, Constants::BLOCK_HALF_HEIGHT, 1);
    blocks.emplace_back(centerB, Constants::BLOCK_HALF_WIDTH, Constants::BLOCK_HALF_HEIGHT, 1);

    Paddle paddle = makeDefaultPaddle();

    // 継ぎ目（x=0.7）の真上から両ブロックに同時に触れる形で降下させる
    Vector3 prevPos = {0.7f, 7.75f, 0.0f};
    Vector3 target = {0.7f, 7.55f, 0.0f};
    Vector3 displacement = {target.x - prevPos.x, target.y - prevPos.y, target.z - prevPos.z};
    Vector3 velocity = {displacement.x / FIXED_DT, displacement.y / FIXED_DT, displacement.z / FIXED_DT};
    Ball ball(prevPos, velocity, Constants::BALL_RADIUS);

    CollisionSystem::StepResult r = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);

    bool bothBlocksHit = r.hitBlock && !blocks[0].alive && !blocks[1].alive;
    bool singleAxisFlip = (ball.velocity.y == -velocity.y) && (ball.velocity.x == velocity.x);
    // 【修正1回帰チェック】2ブロック同時破壊なのでblockHitCountは2になるはず
    // （旧実装のバグ再発防止：score_ += 10固定ではなく破壊数に応じて加算する前提の検証）
    bool hitCountOk = (r.blockHitCount == 2);

    // ②修正後の期待挙動：継ぎ目に同時接触する隣接ブロックは両方同時に割れる。
    // Y軸の速度反転は（2ブロック分検出されても）1回だけに抑えられ、二重反転で
    // 元の符号に戻ってしまう（=反射しない）事故が起きていないことを確認する。
    bool pass = bothBlocksHit && singleAxisFlip && hitCountOk;

    dprintf("  ブロックA(alive=%s) ブロックB(alive=%s) ヒットblockIndex=%d blockHitCount=%d\n",
            blocks[0].alive ? "true" : "false", blocks[1].alive ? "true" : "false", r.blockIndex, r.blockHitCount);
    dprintf("  ②修正後の仕様: 同一サブステップで重なっている生存ブロックは全て処理する -> 継ぎ目では両方同時に割れる\n");
    dprintf("  両方ヒットして破壊=%s 反転軸は1軸のみ（二重反転なし）=%s blockHitCount=2=%s\n",
            bothBlocksHit ? "OK" : "NG", singleAxisFlip ? "OK" : "NG", hitCountOk ? "OK" : "NG");
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC9（新設・3周目で offsetZ 追加修正）: 壁・パドル・ブロックを跨ぐ複合軌道 ---
// 【3周目修正・未光「名前負け」指摘対応】旧TC9はoffsetZが未設定（常に0）で、パドル反射後も
// vzが終始0のままのXY平面内の動きに過ぎず、「複合軌道」という名前に実態が伴っていなかった。
// offsetZ=0.4を設定してパドル反射直後からvx・vy・vzすべてが非ゼロになるようにし、
// ブロックもZ方向にオフセットした位置（z=1.2）に配置することで、本当にZ軸が関与する
// 3軸複合軌道にした。着弾点はパドル反射直後の速度ベクトルから解析的に見積もった近似値。
bool testTC9() {
    dprintf("=== TC9（3周目修正）: パドル→ブロック→壁を跨ぐ複合軌道（offsetZ導入・Z軸関与を実測） ===\n");

    Paddle paddle = makeDefaultPaddle();
    std::vector<Block> blocks;
    // パドルでX+Zオフセット反射させた球がぶつかる位置にブロックを配置する
    // （offsetX=0.6, offsetZ=0.4で反射した後の進行方向をy=3.0地点まで解析的に延長した近似値）
    blocks.emplace_back(Vector3{2.2f, 3.0f, 1.2f}, Constants::BLOCK_HALF_WIDTH, Constants::BLOCK_HALF_HEIGHT, 1);

    float offsetX = 0.6f;
    float offsetZ = 0.4f;
    float startX = paddle.position.x + offsetX * paddle.halfWidth;
    float startZ = paddle.position.z + offsetZ * paddle.halfDepth;
    Ball ball({startX, 2.0f, startZ}, {0.0f, -Constants::BALL_SPEED, 0.0f}, Constants::BALL_RADIUS);

    bool sawPaddle = false, sawBlock = false, sawWall = false, sawWallZ = false, sawNonZeroVz = false;
    int tunnelCount = 0, speedErrors = 0;
    float maxSpeedErr = 0.0f;
    const int maxIterations = 6000;  // 100秒相当。十分な観測時間を確保する

    for (int i = 0; i < maxIterations; ++i) {
        CollisionSystem::StepResult r = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);
        if (outOfBounds(ball, 0.01f)) tunnelCount++;

        float speedErr = std::fabs(speedOf(ball) - Constants::BALL_SPEED);
        if (speedErr > maxSpeedErr) maxSpeedErr = speedErr;
        if (speedErr > 0.05f) speedErrors++;

        if (std::fabs(ball.velocity.z) > 0.01f) sawNonZeroVz = true;
        if (r.hitPaddle) sawPaddle = true;
        if (r.hitBlock) sawBlock = true;
        if (r.hitWallX || r.hitWallZ || r.hitCeiling) sawWall = true;
        if (r.hitWallZ) sawWallZ = true;

        if (sawPaddle && sawBlock && sawWall) break;
    }

    bool pass = sawPaddle && sawBlock && sawWall && sawNonZeroVz && tunnelCount == 0 && speedErrors == 0;
    dprintf("  パドルヒット観測=%s ブロックヒット観測=%s 壁/天井ヒット観測=%s（うちZ壁=%s） vz非ゼロ観測=%s\n",
            sawPaddle ? "あり" : "なし", sawBlock ? "あり" : "なし", sawWall ? "あり" : "なし",
            sawWallZ ? "あり" : "なし", sawNonZeroVz ? "あり" : "なし");
    dprintf("  すり抜け=%d回 速度保存誤差(最大)=%.5f(基準:0.05) 速度保存エラー回数=%d\n", tunnelCount, maxSpeedErr,
            speedErrors);
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC10（新設・3周目・最優先致命傷対応）: ブロックのZ面単独ヒット ---
// 未光3周目レビュー致命傷対応：TC1〜TC9はすべてz=0固定・vz=0固定で、
// ブロックのZ面（box.min.z / box.max.z、position.z ± BLOCK_HALF_DEPTH）に
// ボールが実際に当たるケースが一度も検証されていなかった。X面・Y面は外側判定されず
// Z面だけが外側判定される軌道を作り、(1)vzの反転 (2)snapOutsideAxisによる
// Z軸の位置補正 の両方を実測で確認する。
struct ZFaceTrial {
    const char* label;
    Vector3 blockCenter;
    Vector3 prevPos;
    Vector3 target;
    int sign;  // +1: 手前(+Z)側からbox.max.zへ衝突 / -1: 奥(-Z)側からbox.min.zへ衝突
};

bool testTC10() {
    dprintf("=== TC10（新設）: ブロックのZ面単独ヒット（X/Y面は外側判定されない軌道・4パターン） ===\n");

    std::vector<ZFaceTrial> trials = {
        {"block A 手前(+Z)からZフェース・中心軌道", {0.0f, 7.0f, 0.0f}, {0.0f, 7.0f, 0.85f}, {0.0f, 7.0f, 0.65f}, +1},
        {"block A 奥(-Z)からZフェース・中心軌道", {0.0f, 7.0f, 0.0f}, {0.0f, 7.0f, -0.85f}, {0.0f, 7.0f, -0.65f}, -1},
        {"block B 手前(+Z)からZフェース・XオフセットありY", {3.0f, 7.0f, 0.0f}, {3.3f, 7.0f, 0.85f}, {3.3f, 7.0f, 0.65f}, +1},
        {"block C 奥(-Z)からZフェース・XYオフセットあり", {-2.0f, 7.0f, 0.0f}, {-2.0f, 7.2f, -0.85f}, {-2.0f, 7.2f, -0.65f}, -1},
    };

    int passCount = 0;
    Paddle paddle = makeDefaultPaddle();
    const float r = Constants::BALL_RADIUS;
    const float halfW = Constants::BLOCK_HALF_WIDTH;
    const float halfH = Constants::BLOCK_HALF_HEIGHT;
    const float halfD = Constants::BLOCK_HALF_DEPTH;

    for (const auto& t : trials) {
        // 独立判定：X面・Y面は外側判定されず、Z面だけが外側判定される軌道であることを確認する
        float boxMinX = t.blockCenter.x - halfW, boxMaxX = t.blockCenter.x + halfW;
        float boxMinY = t.blockCenter.y - halfH, boxMaxY = t.blockCenter.y + halfH;
        float boxMinZ = t.blockCenter.z - halfD, boxMaxZ = t.blockCenter.z + halfD;
        bool wasOutsideX = (t.prevPos.x + r <= boxMinX) || (t.prevPos.x - r >= boxMaxX);
        bool wasOutsideY = (t.prevPos.y + r <= boxMinY) || (t.prevPos.y - r >= boxMaxY);
        bool wasOutsideZ = (t.prevPos.z + r <= boxMinZ) || (t.prevPos.z - r >= boxMaxZ);
        bool zOnlySetup = wasOutsideZ && !wasOutsideX && !wasOutsideY;

        std::vector<Block> blocks;
        blocks.emplace_back(t.blockCenter, halfW, halfH, 1);

        Vector3 displacement = {t.target.x - t.prevPos.x, t.target.y - t.prevPos.y, t.target.z - t.prevPos.z};
        Vector3 velocity = {displacement.x / FIXED_DT, displacement.y / FIXED_DT, displacement.z / FIXED_DT};
        Ball ball(t.prevPos, velocity, r);

        int steps = CollisionSystem::computeSubSteps(ball.velocity, FIXED_DT, ball.radius);
        CollisionSystem::StepResult res = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);

        bool zFlippedOnly =
            (ball.velocity.z == -velocity.z) && (ball.velocity.x == velocity.x) && (ball.velocity.y == velocity.y);

        // 位置補正（snapOutsideAxis）の検証：Z面外側へスナップされているか
        float expectedSnapZ = (t.sign > 0) ? (boxMaxZ + r) : (boxMinZ - r);
        float snapErr = std::fabs(ball.position.z - expectedSnapZ);
        bool snapOk = snapErr < 1e-3f;

        bool ok = zOnlySetup && res.hitBlock && steps == 1 && zFlippedOnly && snapOk && !blocks[0].alive;
        if (ok) passCount++;

        dprintf(
            "  [%s] Z単独外側判定=%s サブステップ=%d ブロックヒット=%s vz反転のみ=%s Zスナップ誤差=%.5f %s\n",
            t.label, zOnlySetup ? "true" : "false", steps, res.hitBlock ? "true" : "false",
            zFlippedOnly ? "OK" : "NG", snapErr, ok ? "OK" : "NG");
    }

    bool pass = passCount == static_cast<int>(trials.size());
    dprintf("  合格数=%d/%d\n", passCount, static_cast<int>(trials.size()));
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC11（新設・3周目・致命傷対応）: X・Y・Zの3軸すべてが同時に外側判定になる真の3方向角ヒット ---
// 未光3周目レビュー致命傷対応：TC7は「X面・Y面の2軸」の角ヒットのみで、resolveBlocksの
// フォールバック分岐がX/Y/Zの3軸すべてが同時に「外側」判定になる本当の立体角ヒットでは
// 一度も検証されていなかった。前フレーム位置をブロックの3次元的な角の外側（X・Y・Zの
// 3面すべてから見て外側）に置き、1サブステップでブロック内部へ到達させることで
// wasOutsideX・wasOutsideY・wasOutsideZが全てtrueになる状況を意図的に作る。
// ΔX<ΔY<ΔZという不等な食い込み量にすることで「めり込みが最も浅い軸を優先」という
// フォールバックの優先順位ロジックも一意に検証できるようにしている。
bool testTC11() {
    dprintf("=== TC11（新設）: X・Y・Zの3軸すべてが同時に外側判定になる真の3方向角ヒット（4パターン） ===\n");

    struct CornerTrial3D {
        const char* label;
        Vector3 blockCenter;
        int sx, sy, sz;  // 各軸の接近方向（+1 or -1）
    };

    std::vector<CornerTrial3D> trials = {
        {"右上手前(+++)から立体角進入", {0.0f, 7.0f, 0.0f}, +1, +1, +1},
        {"左下奥(---)から立体角進入", {2.0f, 7.0f, 2.0f}, -1, -1, -1},
        {"右下手前(+-+)から立体角進入", {-2.0f, 7.0f, -2.0f}, +1, -1, +1},
        {"左上奥(-+-)から立体角進入", {4.0f, 7.0f, 4.0f}, -1, +1, -1},
    };

    int passCount = 0;
    Paddle paddle = makeDefaultPaddle();
    const float r = Constants::BALL_RADIUS;
    const float halfW = Constants::BLOCK_HALF_WIDTH;
    const float halfH = Constants::BLOCK_HALF_HEIGHT;
    const float halfD = Constants::BLOCK_HALF_DEPTH;
    // 各軸で不等な食い込み量にする（tie-break ロジックを一意に検証するため）。
    // 0.1268 < Δ < 0.1732 の範囲であれば「前フレームは外側」かつ「移動後は衝突」を両立できる
    // （半径r=0.3・3軸同時食い込みの幾何から導出した範囲。詳細はコメント外の設計メモ参照）
    const float dx = 0.13f, dy = 0.15f, dz = 0.17f;  // dx最小 -> X軸がフォールバックで優先されるはず

    for (const auto& t : trials) {
        Vector3 prevPos = {t.blockCenter.x + t.sx * (halfW + r), t.blockCenter.y + t.sy * (halfH + r),
                            t.blockCenter.z + t.sz * (halfD + r)};
        Vector3 target = {prevPos.x - t.sx * dx, prevPos.y - t.sy * dy, prevPos.z - t.sz * dz};

        float boxMinX = t.blockCenter.x - halfW, boxMaxX = t.blockCenter.x + halfW;
        float boxMinY = t.blockCenter.y - halfH, boxMaxY = t.blockCenter.y + halfH;
        float boxMinZ = t.blockCenter.z - halfD, boxMaxZ = t.blockCenter.z + halfD;
        bool wasOutsideX = (prevPos.x + r <= boxMinX) || (prevPos.x - r >= boxMaxX);
        bool wasOutsideY = (prevPos.y + r <= boxMinY) || (prevPos.y - r >= boxMaxY);
        bool wasOutsideZ = (prevPos.z + r <= boxMinZ) || (prevPos.z - r >= boxMaxZ);
        bool trueCorner3DSetup = wasOutsideX && wasOutsideY && wasOutsideZ;

        std::vector<Block> blocks;
        blocks.emplace_back(t.blockCenter, halfW, halfH, 1);

        Vector3 displacement = {target.x - prevPos.x, target.y - prevPos.y, target.z - prevPos.z};
        Vector3 velocity = {displacement.x / FIXED_DT, displacement.y / FIXED_DT, displacement.z / FIXED_DT};
        Ball ball(prevPos, velocity, r);

        int steps = CollisionSystem::computeSubSteps(ball.velocity, FIXED_DT, ball.radius);
        CollisionSystem::StepResult res = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);

        bool xFlipped = (ball.velocity.x == -velocity.x);
        bool yFlipped = (ball.velocity.y == -velocity.y);
        bool zFlipped = (ball.velocity.z == -velocity.z);
        int flipCount = (xFlipped ? 1 : 0) + (yFlipped ? 1 : 0) + (zFlipped ? 1 : 0);
        bool exactlyOneFlipped = (flipCount == 1);
        // ΔXが最小なのでフォールバックはX軸を選ぶはず（優先順位ロジックの一意性検証）
        bool chosenAxisIsX = xFlipped;

        bool ok = trueCorner3DSetup && res.hitBlock && steps == 1 && exactlyOneFlipped && chosenAxisIsX &&
                  !blocks[0].alive;
        if (ok) passCount++;

        dprintf(
            "  [%s] 3軸同時外側判定=%s(X=%s Y=%s Z=%s) サブステップ=%d ブロックヒット=%s "
            "反転軸数=%d 選択軸=%s%s%s %s\n",
            t.label, trueCorner3DSetup ? "true" : "false", wasOutsideX ? "T" : "F", wasOutsideY ? "T" : "F",
            wasOutsideZ ? "T" : "F", steps, res.hitBlock ? "true" : "false", flipCount, xFlipped ? "X" : "",
            yFlipped ? "Y" : "", zFlipped ? "Z" : "", ok ? "OK" : "NG");
    }

    bool pass = passCount == static_cast<int>(trials.size());
    dprintf("  合格数=%d/%d（すり抜け・クラッシュなし、常にちょうど1軸のみ反転、優先順位ロジックも一致）\n",
            passCount, static_cast<int>(trials.size()));
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

// --- TC12（新設・②対応）: ②で決めたステージ密度における複数ブロック同時ヒットの
// 見た目上のすり抜け実測 ---
// AGENTS.mdの既知の限界「resolveBlocksは1サブステップにつき1ブロックしか処理しない」が、
// ②で実際に決めたブロック密度（Stage::buildBlocksの隙間・行列数）において、
// 「本来同時に割れるべき隣接ブロックのうち割れなかった方をボールが貫通して通過して見える
// （オーバーラップ量がボール半径を超える）」ケースを引き起こすかどうかを実測する。
// 最も密度が高いステージ（隙間が下限BLOCK_MIN_GAPまで詰まるステージ）の全隣接ペアに対し、
// 代表速度（通常・高速球）×代表軌道（垂直・斜め）の4パターンをぶつけ、100件以上の
// 試行を確保する。
bool testTC12() {
    dprintf("=== TC12（新設）: ②のステージ密度における複数ブロック同時ヒットのすり抜け実測 ===\n");

    // 隙間がBLOCK_MIN_GAPまで詰まりきる高ステージ番号を「最も密なステージ」として採用。
    // 耐久値はStage::buildBlocksの生成値のまま使うと（高ステージほど4など複数耐久になり）
    // 1回ヒットしても壊れず何度もバウンドし続けてしまい、「幾何学的にすり抜けるか」という
    // 本来の検証目的から外れた「耐久値による滞留」を測ってしまう。そのためこのテストでは
    // 位置・サイズ・間隔（＝密度）だけをStageから受け継ぎ、耐久値は1回ヒットで壊れる値に
    // 強制的に揃えて計測する。
    const int denseStageIndex = 10;
    std::vector<Block> rawStageBlocks = Stage::buildBlocks(denseStageIndex);
    std::vector<Block> refBlocks;
    refBlocks.reserve(rawStageBlocks.size());
    for (const auto& b : rawStageBlocks) {
        refBlocks.emplace_back(b.position, b.halfWidth, b.halfHeight, 1);
    }
    dprintf("  対象ステージ=%d ブロック数=%d（耐久値は検証目的のため1に統一）\n", denseStageIndex,
            static_cast<int>(refBlocks.size()));

    // 隣接ペア（同じ行でXが隣・同じ列でYが隣）を全列挙する
    struct NeighborPair {
        int a, b;
        char axis;  // 'X'=横並びの継ぎ目, 'Y'=縦並びの継ぎ目
    };
    std::vector<NeighborPair> neighbors;
    const float eps = 0.05f;
    for (size_t i = 0; i < refBlocks.size(); ++i) {
        for (size_t j = i + 1; j < refBlocks.size(); ++j) {
            const Block& a = refBlocks[i];
            const Block& b = refBlocks[j];
            if (std::fabs(a.position.z - b.position.z) > eps) continue;  // 同じZレイヤーのみ対象

            float dx = std::fabs(a.position.x - b.position.x);
            float dy = std::fabs(a.position.y - b.position.y);
            bool sameRow = dy < eps;
            bool sameCol = dx < eps;

            if (sameRow && dx > 0.0f && dx < (a.halfWidth + b.halfWidth) * 2.2f) {
                neighbors.push_back({static_cast<int>(i), static_cast<int>(j), 'X'});
            } else if (sameCol && dy > 0.0f && dy < (a.halfHeight + b.halfHeight) * 2.2f) {
                neighbors.push_back({static_cast<int>(i), static_cast<int>(j), 'Y'});
            }
        }
    }
    dprintf("  隣接ペア数=%d\n", static_cast<int>(neighbors.size()));

    struct Variant {
        const char* label;
        float speedMul;  // 1.0=通常速度、4.0=高速球
        bool diagonal;
    };
    std::vector<Variant> variants = {
        {"通常・垂直", 1.0f, false},
        {"通常・斜め", 1.0f, true},
        {"高速・垂直", 4.0f, false},
        {"高速・斜め", 4.0f, true},
    };

    int trialCount = 0;
    int violationCount = 0;
    float maxOverlapSeen = 0.0f;

    for (const auto& nb : neighbors) {
        Vector3 posA = refBlocks[nb.a].position;
        Vector3 posB = refBlocks[nb.b].position;
        Vector3 seamMid = {(posA.x + posB.x) * 0.5f, (posA.y + posB.y) * 0.5f, (posA.z + posB.z) * 0.5f};

        for (const auto& variant : variants) {
            trialCount++;

            Paddle paddle = makeDefaultPaddle();
            std::vector<Block> blocks = refBlocks;  // このトライアル専用にコピー

            // 継ぎ目の下方(Y方向)から接近させる。斜めパターンは継ぎ目の向きと直交する
            // 方向にも速度成分を持たせ、真の斜め軌道にする
            const float approach = Constants::BALL_RADIUS * 3.0f;
            Vector3 startPos = seamMid;
            startPos.y -= approach;

            Vector3 dir;
            if (!variant.diagonal) {
                dir = {0.0f, 1.0f, 0.0f};
            } else if (nb.axis == 'X') {
                dir = {0.25f, 1.0f, 0.15f};
            } else {
                dir = {0.15f, 1.0f, 0.25f};
            }
            dir = Vector3Normalize(dir);
            float speed = Constants::BALL_SPEED * variant.speedMul;
            Ball ball(startPos, Vector3Scale(dir, speed), Constants::BALL_RADIUS);

            // 継ぎ目を通過しきるまで数フレーム進める（最大60フレーム=1秒あれば十分到達する）
            float localMaxOverlap = 0.0f;
            for (int f = 0; f < 60; ++f) {
                CollisionSystem::update(ball, paddle, blocks, FIXED_DT);

                for (int idx : {nb.a, nb.b}) {
                    if (!blocks[idx].alive) continue;
                    BoundingBox box = blocks[idx].getBoundingBox();
                    if (!CheckCollisionBoxSphere(box, ball.position, ball.radius)) continue;

                    float overlapX = std::min(ball.position.x + ball.radius - box.min.x,
                                               box.max.x - (ball.position.x - ball.radius));
                    float overlapY = std::min(ball.position.y + ball.radius - box.min.y,
                                               box.max.y - (ball.position.y - ball.radius));
                    float overlapZ = std::min(ball.position.z + ball.radius - box.min.z,
                                               box.max.z - (ball.position.z - ball.radius));
                    float minOverlap = std::min(overlapX, std::min(overlapY, overlapZ));
                    if (minOverlap > localMaxOverlap) localMaxOverlap = minOverlap;
                }

                bool bothBroken = !blocks[nb.a].alive && !blocks[nb.b].alive;
                if (bothBroken) break;
            }

            if (localMaxOverlap > maxOverlapSeen) maxOverlapSeen = localMaxOverlap;
            if (localMaxOverlap > Constants::BALL_RADIUS) violationCount++;
        }
    }

    dprintf(
        "  試行数=%d（隣接ペア%d組 × 4パターン） 違反件数=%d 最大オーバーラップ=%.5f（ボール半径%.2f基準）\n",
        trialCount, static_cast<int>(neighbors.size()), violationCount, maxOverlapSeen, Constants::BALL_RADIUS);

    bool pass = (trialCount >= 100) && (violationCount == 0);
    dprintf("  判定: %s（%s）\n\n", pass ? "PASS" : "FAIL",
            violationCount == 0 ? "resolveBlocks拡張後は貫通なし・現在の実装を採用"
                                 : "貫通が顕在化・resolveBlocksのさらなる拡張が必要");
    return pass;
}

// --- TC13（新設・梨緒レビューLOW指摘対応 → 修正3で4方向に拡張）: 2x2隣接ブロックの
// 十字の交点における4ブロック同時重なりの検証 ---
// 現在のブロック密度（BLOCK_MIN_GAP=0.15・BLOCK_HALF_WIDTH=0.7・BLOCK_HALF_HEIGHT=0.35・
// BALL_RADIUS=0.3）では、2x2で隣接配置した4ブロックの隙間の交差点（十字の中心）付近で
// ボール（直径0.6）が4ブロック全てに同時に重なる状況が幾何学的に起こりうる。
// resolveBlocksは各ブロックを順番に処理し、位置補正（snapOutsideAxis）はブロックごとに
// ball.positionを直接上書きするため、複数ブロックが同じ軸で異なるスナップ値を要求した
// 場合に後から処理したブロックの値が前のブロックのスナップ結果を上書きしてしまう懸念が
// ある。このテストでは実際にその懸念が起きるか・起きた場合に致命的か（生存ブロックへの
// めり込みが残るか・座標が異常値になるか）を実測する。
//
// 【修正3：4方向への拡張】旧版は「十字の中心よりわずかにTR側へ寄った前フレーム位置から
// 中心へ向かう」1方向のみの検証だった。TC7/TC10/TC11と同様、十字の中心という
// 完全対称点（理論上は全軸の重なり量が同値になり得るtie-breakの最も厳しい条件）に対して、
// 右上・左上・右下・左下の4方向から接近しても軸選択が健全（クラッシュしない・二重反転や
// 無反転が起きない・生存ブロックへのめり込みが残らない）であることを実測する。
bool testTC13() {
    dprintf("=== TC13（修正3で4方向に拡張）: 2x2隣接ブロックの十字の交点における4ブロック同時重なり（4パターン） ===\n");

    const float halfW = Constants::BLOCK_HALF_WIDTH;
    const float halfH = Constants::BLOCK_HALF_HEIGHT;
    const float gap = Constants::BLOCK_MIN_GAP;
    const float pitchX = halfW * 2.0f + gap;  // 隣接ブロック中心間の距離（X方向）
    const float pitchY = halfH * 2.0f + gap;  // 隣接ブロック中心間の距離（Y方向）
    const float cx = pitchX / 2.0f;
    const float cy = pitchY / 2.0f;
    // ブロック群をY=7付近（他のTC6・TC7・TC10・TC11と同じ高さ）に配置する。
    // 【実測で判明した設計ミスの修正（③-C時点）】当初Y=0付近に配置したところ、パドル
    // （PADDLE_Y=0・halfHeight=0.3）の当たり判定と重なってしまい、
    // resolveBlocksより先にresolvePaddleが割り込んでボールを弾き飛ばし、
    // 本来検証したい4ブロック同時ヒットが実測できていなかった（TL/TRのみ検出、
    // BL/BRは未検出という誤った結果が出た）。ブロック群をパドルから十分離れた
    // 高さに移動して解消する。
    const float offsetY = 7.0f;
    const float r = Constants::BALL_RADIUS;
    const Vector3 target = {0.0f, offsetY, 0.0f};  // 十字の中心（4ブロック全てに重なる点）
    float bound = pitchX + halfW + r;              // 2x2クラスタ全体+余裕を持たせた範囲

    // 十字の中心へ、4つの象限（右上・左上・右下・左下）それぞれから接近させる。
    // sx/syは前フレーム位置を中心からどちら向きにずらすかの符号（TC7の対角進入パターンと
    // 同じ考え方）。変位を0.3(=BALL_RADIUS)未満に抑え、computeSubStepsがsteps==1を
    // 返すようにする（TC6・TC7と同じ「小さな変位でsteps==1を保証する」手法）
    struct CrossApproachTrial {
        const char* label;
        float sx;
        float sy;
    };
    std::vector<CrossApproachTrial> trials = {
        {"右上から対角進入", +1.0f, +1.0f},
        {"左上から対角進入", -1.0f, +1.0f},
        {"右下から対角進入", +1.0f, -1.0f},
        {"左下から対角進入", -1.0f, -1.0f},
    };

    int passCount = 0;
    Paddle paddle = makeDefaultPaddle();

    for (const auto& t : trials) {
        // 前トライアルで4ブロックとも破壊されているため、トライアルごとに新規生成する
        Vector3 centerTL = {-cx, offsetY + cy, 0.0f};
        Vector3 centerTR = {cx, offsetY + cy, 0.0f};
        Vector3 centerBL = {-cx, offsetY - cy, 0.0f};
        Vector3 centerBR = {cx, offsetY - cy, 0.0f};

        std::vector<Block> blocks;
        blocks.emplace_back(centerTL, halfW, halfH, 1);  // index0: 左上（TL）
        blocks.emplace_back(centerTR, halfW, halfH, 1);  // index1: 右上（TR）
        blocks.emplace_back(centerBL, halfW, halfH, 1);  // index2: 左下（BL）
        blocks.emplace_back(centerBR, halfW, halfH, 1);  // index3: 右下（BR）

        Vector3 prevPos = {t.sx * 0.1f, offsetY + t.sy * 0.1f, 0.0f};

        // 事前確認：本当にtargetが4ブロック全てに重なるか（実測：距離約0.106<半径0.3）を独立に検証する
        int overlapCountAtTarget = 0;
        for (const auto& b : blocks) {
            BoundingBox box = b.getBoundingBox();
            if (CheckCollisionBoxSphere(box, target, r)) overlapCountAtTarget++;
        }

        Vector3 displacement = {target.x - prevPos.x, target.y - prevPos.y, target.z - prevPos.z};
        Vector3 velocity = {displacement.x / FIXED_DT, displacement.y / FIXED_DT, displacement.z / FIXED_DT};
        Ball ball(prevPos, velocity, r);

        int steps = CollisionSystem::computeSubSteps(ball.velocity, FIXED_DT, ball.radius);
        CollisionSystem::StepResult res = CollisionSystem::update(ball, paddle, blocks, FIXED_DT);

        bool allHit = !blocks[0].alive && !blocks[1].alive && !blocks[2].alive && !blocks[3].alive;

        // TC6と同じガード：velocity.?の元の値が0.0fの場合、0.0f == -0.0f はIEEE754上
        // trueになってしまい「反転した」という誤検出になる（本テストはvelocity.zが
        // 常に0のため、このガードなしではzFlippedが常にtrueになってしまう）
        bool xFlipped = (velocity.x != 0.0f) && (ball.velocity.x == -velocity.x);
        bool yFlipped = (velocity.y != 0.0f) && (ball.velocity.y == -velocity.y);
        bool zFlipped = (velocity.z != 0.0f) && (ball.velocity.z == -velocity.z);
        int flipCount = (xFlipped ? 1 : 0) + (yFlipped ? 1 : 0) + (zFlipped ? 1 : 0);
        bool exactlyOneAxisFlipped = (flipCount == 1);

        // 【修正1回帰チェック】4ブロック同時破壊なのでblockHitCountは4になるはず
        bool hitCountOk = (res.blockHitCount == 4);

        // 生存ブロックへのめり込みが残っていないか（このケースは4ブロックとも1発で
        // 破壊されるため、生存ブロックは0個になり検証は事実上「対象なしでOK」になるはず。
        // durabilityが2以上のブロックが混在するケースでは意味を持つチェックのため、
        // 将来の変更に備えてロジックは汎用的に書いておく）
        int survivorOverlapCount = 0;
        for (const auto& b : blocks) {
            if (!b.alive) continue;
            BoundingBox box = b.getBoundingBox();
            if (CheckCollisionBoxSphere(box, ball.position, r)) survivorOverlapCount++;
        }

        // 位置補正が幾何学的に妥当な範囲に収まっているか（NaN・2x2クラスタから大きく
        // 外れた異常値になっていないか）をtarget（十字の中心）からのズレ量で判定する
        bool positionSane = std::isfinite(ball.position.x) && std::isfinite(ball.position.y) &&
                             std::isfinite(ball.position.z) && std::fabs(ball.position.x - target.x) <= bound &&
                             std::fabs(ball.position.y - target.y) <= bound &&
                             std::fabs(ball.position.z - target.z) <= bound;

        bool ok = overlapCountAtTarget == 4 && steps == 1 && res.hitBlock && allHit && exactlyOneAxisFlipped &&
                  hitCountOk && survivorOverlapCount == 0 && positionSane;
        if (ok) passCount++;

        dprintf(
            "  [%s] 事前重なり数=%d/4 サブステップ=%d(期待値:1) 4ブロック全滅=%s 反転軸数=%d(選択軸=%s%s%s) "
            "blockHitCount=%d(期待値:4) 生存ブロックへのめり込み=%d件 最終位置妥当性=%s %s\n",
            t.label, overlapCountAtTarget, steps, allHit ? "true" : "false", flipCount, xFlipped ? "X" : "",
            yFlipped ? "Y" : "", zFlipped ? "Z" : "", res.blockHitCount, survivorOverlapCount,
            positionSane ? "OK" : "NG(異常値)", ok ? "OK" : "NG");
    }

    bool pass = passCount == static_cast<int>(trials.size());
    dprintf("  合格数=%d/%d（十字の中心という完全対称点でも、4方向いずれからの接近でも軸選択が健全）\n", passCount,
            static_cast<int>(trials.size()));
    dprintf("  判定: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

}  // namespace

int main() {
    g_logFile = fopen("test_results.log", "w");
    if (g_logFile == nullptr) {
        fprintf(stderr, "警告: test_results.log を開けませんでした。ログはコンソール出力のみになります。\n");
    }

    dprintf("########################################\n");
    dprintf("# CollisionSystem ヘッドレステスト開始 #\n");
    dprintf("########################################\n\n");

    bool r1 = testTC1();
    bool r2 = testTC2();
    bool r3 = testTC3();
    bool r4 = testTC4();
    bool r5 = testTC5();
    bool r6 = testTC6();
    bool r7 = testTC7();
    bool r8 = testTC8();
    bool r9 = testTC9();
    bool r10 = testTC10();
    bool r11 = testTC11();
    bool r12 = testTC12();
    bool r13 = testTC13();

    int total = (r1 ? 1 : 0) + (r2 ? 1 : 0) + (r3 ? 1 : 0) + (r4 ? 1 : 0) + (r5 ? 1 : 0) + (r6 ? 1 : 0) +
                (r7 ? 1 : 0) + (r8 ? 1 : 0) + (r9 ? 1 : 0) + (r10 ? 1 : 0) + (r11 ? 1 : 0) + (r12 ? 1 : 0) +
                (r13 ? 1 : 0);
    const int totalTests = 13;

    dprintf("========================================\n");
    dprintf("総合結果: %d / %d PASS\n", total, totalTests);
    dprintf("  TC1  垂直落下:                              %s\n", r1 ? "PASS" : "FAIL");
    dprintf("  TC2  真の3軸複合反射+部屋の角:              %s\n", r2 ? "PASS" : "FAIL");
    dprintf("  TC3  パドル端ヒット:                        %s\n", r3 ? "PASS" : "FAIL");
    dprintf("  TC4  高速球:                                %s\n", r4 ? "PASS" : "FAIL");
    dprintf("  TC5  Z壁単軸+パドルX+Z複合オフセット:       %s\n", r5 ? "PASS" : "FAIL");
    dprintf("  TC6  ブロック四隅:                          %s\n", r6 ? "PASS" : "FAIL");
    dprintf("  TC7  真の角ヒット（X/Y面・健全性のみ検証）: %s\n", r7 ? "PASS" : "FAIL");
    dprintf("  TC8  隣接ブロックの継ぎ目（現仕様の記録）:  %s\n", r8 ? "PASS" : "FAIL");
    dprintf("  TC9  パドル→ブロック→壁の複合軌道（offsetZ導入）: %s\n", r9 ? "PASS" : "FAIL");
    dprintf("  TC10 ブロックのZ面単独ヒット（新設・3周目）: %s\n", r10 ? "PASS" : "FAIL");
    dprintf("  TC11 X/Y/Z 3軸同時角ヒット（新設・3周目）:   %s\n", r11 ? "PASS" : "FAIL");
    dprintf("  TC12 ②ステージ密度での複数ブロック貫通実測（新設）: %s\n", r12 ? "PASS" : "FAIL");
    dprintf("  TC13 2x2隣接4ブロック同時重なり（修正3で4方向に拡張）: %s\n", r13 ? "PASS" : "FAIL");
    dprintf("----------------------------------------\n");
    dprintf("総合判定: %s\n", total == totalTests ? "Go（③へ進める）" : "No-Go（代替案の検討が必要）");
    dprintf("========================================\n");

    if (g_logFile != nullptr) {
        fclose(g_logFile);
    }

    return total == totalTests ? 0 : 1;
}
