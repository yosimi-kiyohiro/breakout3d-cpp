#include "GameState.hpp"

#include "ScoreFile.hpp"
#include "Stage.hpp"
#include "constants.hpp"
#include "raylib.h"
#include "raymath.h"

GameState::GameState()
    : phase_(GamePhase::Title),
      score_(0),
      lives_(3),
      stageIndex_(1),
      highScore_(0),
      ball_({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, Constants::BALL_RADIUS),
      paddle_({0.0f, Constants::PADDLE_Y, 0.0f}, Constants::PADDLE_HALF_WIDTH, Constants::PADDLE_HALF_HEIGHT,
              Constants::PADDLE_HALF_DEPTH),
      blocks_() {
    // タスク6：起動時に保存済みハイスコアを読み込む（ファイルが無ければ0のまま）
    highScore_ = ScoreFile::load().highScore;
    // タイトル画面の背景にステージ1のブロックを表示しておく（見た目確認用）
    resetStage(stageIndex_);
}

void GameState::resetStage(int stageIndex) {
    stageIndex_ = stageIndex;
    blocks_ = Stage::buildBlocks(stageIndex_);
    paddle_.position = {0.0f, Constants::PADDLE_Y, 0.0f};
    resetBallOnPaddle();
}

void GameState::resetBallOnPaddle() {
    Vector3 pos = {paddle_.position.x, paddle_.position.y + paddle_.halfHeight + Constants::BALL_RADIUS + 0.05f,
                   paddle_.position.z};
    // 自動発射：やや斜め上方向・速さはBALL_SPEEDに正規化（③-Aの最小実装。
    // パドル上で待機してからプレイヤー操作で発射する演出は④以降で検討する）
    Vector3 dir = Vector3Normalize(Vector3{0.3f, 1.0f, 0.0f});
    Vector3 vel = Vector3Scale(dir, Constants::BALL_SPEED);
    ball_.reset(pos, vel);
}

void GameState::loseLife() {
    lives_ -= 1;
    if (checkGameOver()) {
        phase_ = GamePhase::GameOver;
        // タスク6：ゲームオーバー確定はプレイの区切りなので、ここでハイスコアを確定・保存する
        updateHighScoreIfNeeded();
    } else {
        resetBallOnPaddle();
    }
}

void GameState::updateHighScoreIfNeeded() {
    if (score_ > highScore_) {
        highScore_ = score_;
        ScoreFile::save(highScore_);
    }
}

std::vector<ParticleEmitEvent> GameState::consumeParticleEmits() {
    // std::moveだけだと移動後の状態がis-emptyであることを規格上保証されないため、
    // コピー＋明示clear()で「一度消費したら必ず空になる」を保証する
    std::vector<ParticleEmitEvent> events = pendingParticleEmits_;
    pendingParticleEmits_.clear();
    return events;
}

AudioEvents GameState::consumeAudioEvents() {
    AudioEvents events = pendingAudio_;
    pendingAudio_ = AudioEvents();
    return events;
}

void GameState::advanceStage() {
    stageIndex_ += 1;
    resetStage(stageIndex_);
}

bool GameState::checkStageClear() const {
    for (const auto& block : blocks_) {
        if (block.alive) return false;
    }
    return true;
}

bool GameState::checkGameOver() const { return lives_ <= 0; }

void GameState::update(float dt) {
    if (phase_ != GamePhase::Playing) return;

    // タスク4：衝突判定の前に各ブロックの生存状態を記録しておき、判定後に
    // 「生存→非生存」へ変わったブロックを「このフレームで破壊された」と判定する
    // （CollisionSystem自体はレンダリング色の概念を持たないため、GameState側で判定する）。
    std::vector<bool> aliveBefore(blocks_.size());
    for (size_t i = 0; i < blocks_.size(); ++i) {
        aliveBefore[i] = blocks_[i].alive;
    }

    CollisionSystem::StepResult r = CollisionSystem::update(ball_, paddle_, blocks_, dt);

    // 【修正1】旧実装はr.hitBlockがtrueなら常に固定10点のみで、同一サブステップで
    // 複数ブロックが同時破壊されてもスコアが過小評価されるバグがあった。
    // r.blockHitCount（このフレームで実際にヒットしたブロック数）に応じて加算する。
    if (r.hitBlock) {
        score_ += 10 * r.blockHitCount;
    }

    // タスク4・5：ブロック破壊イベントを検出し、パーティクル発生位置・色と
    // 効果音トリガーを保留リストに積む（main.cpp側がconsume*()で取り出して使う）
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (aliveBefore[i] && !blocks_[i].alive) {
            // 【梨緒レビュー対応】durability（壊れる直前は耐久2以上のブロックでもほぼ常に1に
            // なっている）ではなく、生成時から変化しないinitialDurabilityを使う。
            // Renderer::drawBlocksの3段階配色ルール（1=ORANGE、2=MAROON、3以上=DARKPURPLE）と
            // 一致させる
            const Block& destroyedBlock = blocks_[i];
            Color color = (destroyedBlock.initialDurability >= 3)
                               ? DARKPURPLE
                               : (destroyedBlock.initialDurability == 2 ? MAROON : ORANGE);
            pendingParticleEmits_.push_back({destroyedBlock.position, color});
        }
    }
    if (!pendingParticleEmits_.empty()) pendingAudio_.blockBreak = true;
    if (r.hitPaddle || r.hitWallX || r.hitWallZ || r.hitCeiling || r.hitBlock) pendingAudio_.hit = true;

    // 梨緒レビューHIGH対応：checkStageClear()をr.missedより先に評価する。
    // CollisionSystem::update()は1フレーム内で最大8サブステップ回るため、
    // 「最後のブロックをX面ヒットで割った直後、同じフレーム内でミスラインを
    // 越える」ケースが起こりえる。旧実装ではr.missedを先に見てloseLife()して
    // returnしてしまい、本来ステージクリアのはずがライフが減る・最悪その場で
    // GameOverになるバグがあった。クリアと同時にミスが起きた場合はクリアを
    // 優先する仕様とする。
    if (checkStageClear()) {
        phase_ = GamePhase::Clear;
        // タスク6：ステージクリアもプレイの区切りなので、ここでハイスコアを確定・保存する
        updateHighScoreIfNeeded();
        return;
    }

    if (r.missed) {
        pendingAudio_.miss = true;
        loseLife();  // 内部でゲームオーバー判定・ボール再配置まで処理済み
    }
}

void GameState::handleInput(float dt) {
    switch (phase_) {
        case GamePhase::Title:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                score_ = 0;
                lives_ = 3;
                resetStage(1);
                phase_ = GamePhase::Playing;
            }
            break;

        case GamePhase::Playing:
            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) paddle_.moveX(-Constants::PADDLE_SPEED * dt);
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) paddle_.moveX(Constants::PADDLE_SPEED * dt);
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) phase_ = GamePhase::Paused;
            break;

        case GamePhase::Paused:
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) phase_ = GamePhase::Playing;
            break;

        case GamePhase::GameOver:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) phase_ = GamePhase::Title;
            break;

        case GamePhase::Clear:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                advanceStage();
                phase_ = GamePhase::Playing;
            }
            break;
    }
}
