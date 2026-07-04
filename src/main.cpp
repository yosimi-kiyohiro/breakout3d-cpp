#include <cstdlib>
#include <ctime>

#include "AudioManager.hpp"
#include "GameState.hpp"
#include "ParticleSystem.hpp"
#include "Renderer.hpp"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// ③-A：tetris-cpp方式（静的グローバル＋UpdateDrawFrame()＋PLATFORM_WEB分岐）に揃える。
// Web版はemscripten_set_main_loopがブラウザのrAFを呼び出すため、ネイティブ版のような
// while(!shouldClose())ループは使わない。
// タスク4・5：GameState自体はParticleSystem/AudioManagerに依存させず（疎結合を保つ）、
// main.cppがGameStateのconsumeParticleEmits()/consumeAudioEvents()を橋渡しする設計にした。
static Renderer* g_renderer = nullptr;
static GameState* g_game = nullptr;
static ParticleSystem* g_particles = nullptr;
static AudioManager* g_audio = nullptr;

static void UpdateDrawFrame() {
    float dt = GetFrameTime();

    // タスク5：Webの自動再生ポリシー対策。タイトル画面での最初のキー入力を検知して
    // からAudioManagerを初期化する（handleInput()でフェーズがPlayingへ遷移する前に
    // 判定する必要があるため、handleInput()より先にチェックする）
    if (g_game->phase() == GamePhase::Title && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        g_audio->initAfterUnlock();
    }

    g_game->handleInput(dt);
    g_game->update(dt);

    // タスク4：このフレームで破壊されたブロックの位置・色を消費してパーティクルを発生させる
    for (const auto& emitEvent : g_game->consumeParticleEmits()) {
        g_particles->emit(emitEvent.position, emitEvent.color);
    }
    g_particles->update(dt);

    // タスク5：このフレームのヒット・破壊・ミスイベントを消費して効果音を鳴らす
    AudioEvents audioEvents = g_game->consumeAudioEvents();
    if (audioEvents.blockBreak) {
        g_audio->playBreak();
    } else if (audioEvents.hit) {
        // ブロック破壊音と衝突音が同一フレームで両方鳴ると耳障りなため、
        // 破壊があった時は破壊音を優先し、それ以外の時だけヒット音を鳴らす
        g_audio->playHit();
    }
    if (audioEvents.miss) g_audio->playMiss();

    // タスク5：BGMはPlaying中のみ再生する
    if (g_game->phase() == GamePhase::Playing) {
        g_audio->playBgmLoop();
    } else {
        g_audio->stopBgm();
    }
    g_audio->update(dt);

    g_renderer->beginFrame();

    g_renderer->beginScene3D();
    g_renderer->drawField();
    g_renderer->drawPaddle(g_game->paddle());
    g_renderer->drawBall(g_game->ball());
    g_renderer->drawBlocks(g_game->blocks());
    g_particles->draw();
    g_renderer->endScene3D();

    g_renderer->drawHud(g_game->score(), g_game->lives(), g_game->stageIndex(), g_game->highScore());

    switch (g_game->phase()) {
        case GamePhase::Title:
            g_renderer->drawTitleOverlay(g_game->highScore());
            break;
        case GamePhase::Paused:
            g_renderer->drawPauseOverlay();
            break;
        case GamePhase::GameOver:
            g_renderer->drawGameOverOverlay(g_game->score());
            break;
        case GamePhase::Clear:
            g_renderer->drawClearOverlay(g_game->stageIndex());
            break;
        case GamePhase::Playing:
        default:
            break;
    }

    g_renderer->endFrame();
}

int main() {
    // タスク4：ParticleSystemのrand()を使う乱数生成が起動のたびに同じ列にならないよう、
    // 時刻ベースでシードする（梨緒レビュー指摘対応）
    srand(static_cast<unsigned int>(time(nullptr)));

    g_renderer = new Renderer();
    g_game = new GameState();
    g_particles = new ParticleSystem();
    g_audio = new AudioManager();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!g_renderer->shouldClose()) {
        UpdateDrawFrame();
    }
    delete g_audio;
    delete g_particles;
    delete g_renderer;
    delete g_game;
#endif

    return 0;
}
