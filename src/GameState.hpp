#pragma once
#include <vector>

#include "Ball.hpp"
#include "Block.hpp"
#include "CollisionSystem.hpp"
#include "Paddle.hpp"

// ③-A：ゲーム全体の状態管理。
// Title（タイトル）→Playing（プレイ中）→Clear（ステージクリア）/GameOver（ゲームオーバー）と
// 遷移する。Playing中はPausedへ一時遷移できる。
// 演出（パーティクル・音声・ハイスコア保存等）は④以降のスコープで、ここでは
// スコア・ライフ・ステージ進行・衝突判定の呼び出しという最小限のゲームループのみを扱う。
enum class GamePhase { Title, Playing, Paused, GameOver, Clear };

// タスク4（⑥）：ブロック破壊イベント1件分（位置・色）。ParticleSystem::emit()の引数に
// そのまま渡せる形にしておく。GameStateはParticleSystemクラス自体には依存しない
// （main.cppがGameStateとParticleSystem両方を知っていて橋渡しする設計）。
struct ParticleEmitEvent {
    Vector3 position;
    Color color;
};

// タスク5（⑦）：このフレームで起きた効果音トリガー用のイベント（一度consumeしたらfalseに戻す）。
// GameStateはAudioManagerクラス自体には依存しない（main.cppが橋渡しする設計）。
struct AudioEvents {
    bool hit = false;        // ボールが何かに当たった（パドル・壁・天井・ブロックいずれか）
    bool blockBreak = false; // ブロックが1個以上破壊された
    bool miss = false;       // MISS_LINE_Yを下回ってライフを失った
};

class GameState {
public:
    GameState();

    // dt秒分のゲーム進行（Playing中のみ衝突判定・ミス判定・クリア判定を行う）
    void update(float dt);
    // キー入力の処理（フェーズ遷移・パドル移動）。パドル移動はdt依存のため引数で受け取る
    void handleInput(float dt);

    // stageIndexのステージでブロックを再構築し、パドル位置・ボールを初期化する
    void resetStage(int stageIndex);
    // ライフを1減らす。0になったらGameOverへ遷移し、そうでなければボールを再配置する
    void loseLife();
    // 次のステージへ進む（stageIndex_をインクリメントしてresetStageを呼ぶ）
    void advanceStage();
    // 生存しているブロックが1つもなければtrue
    bool checkStageClear() const;
    // ライフが0以下ならtrue
    bool checkGameOver() const;

    GamePhase phase() const { return phase_; }
    int score() const { return score_; }
    int lives() const { return lives_; }
    int stageIndex() const { return stageIndex_; }
    // タスク6（⑧）：ScoreFileから読み込んだ／更新されたハイスコア
    int highScore() const { return highScore_; }

    const Ball& ball() const { return ball_; }
    const Paddle& paddle() const { return paddle_; }
    const std::vector<Block>& blocks() const { return blocks_; }

    // タスク4：直近のupdate()で発生したブロック破壊イベントを一度だけ取り出す
    // （呼び出すと内部の保留リストは空になる＝「イベントを一度消費して返す」方式）
    std::vector<ParticleEmitEvent> consumeParticleEmits();
    // タスク5：直近のupdate()で発生した効果音イベントを一度だけ取り出す
    AudioEvents consumeAudioEvents();

private:
    GamePhase phase_;
    int score_;
    int lives_;
    int stageIndex_;
    int highScore_;  // タスク6：ScoreFileで永続化するハイスコア

    Ball ball_;
    Paddle paddle_;
    std::vector<Block> blocks_;

    std::vector<ParticleEmitEvent> pendingParticleEmits_;
    AudioEvents pendingAudio_;

    // ボールをパドル上の定位置に置き、一定の初速（速さはBALL_SPEEDに正規化）で
    // 自動発射状態にする（③-Aの最小実装：パドル上で待機する演出は④以降で検討）
    void resetBallOnPaddle();
    // タスク6：score_がhighScore_を上回っていたら更新し、ScoreFile::save()で永続化する。
    // 毎フレーム呼ぶとファイルI/Oが頻発するため、GameOver確定時・ステージクリア確定時の
    // 「区切りのタイミング」でのみ呼ぶ
    void updateHighScoreIfNeeded();
};
