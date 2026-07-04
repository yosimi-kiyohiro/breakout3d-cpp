#pragma once
#include <cstdint>
#include <vector>

#include "raylib.h"

// タスク5（⑦）：効果音・BGMをraylibのWave/Sound APIでコード生成する
// （外部音声ファイルは使わない）オーディオマネージャ。
// 参考：space-invaders-cpp（src/AudioManager.cpp）のサイン波・矩形波減衰の生成の考え方を、
// SDL3（push型のAudioStream）からraylib（Wave全体を作ってLoadSoundFromWave()する方式）向けに
// 書き換えたもの。
//
// 【Webの自動再生ポリシー対策】
// ブラウザは「ユーザー操作なしの自動再生」を許可しないため、Web版はページ読み込み直後に
// InitAudioDevice()を呼んでも実際には音が鳴らない（鳴らせないまま無音で進む）ことがある。
// initAfterUnlock()を用意し、タイトル画面での最初のキー入力（main.cpp側でトリガー）から
// 呼び出すことで、確実にユーザー操作の後に初期化されるようにする。
// ネイティブ版はこの制約が無いため、コンストラクタで即座に初期化する
// （space-invaders-cppのAudioManagerと同じ方針：`#if !defined(PLATFORM_WEB)`で分岐）。
class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Web: タイトル画面の最初のキー入力をトリガーに呼ぶ。ネイティブは既に初期化済みなら何もしない
    void initAfterUnlock();
    bool isReady() const { return ready_; }

    void playHit();      // ボールが何かに当たった時の短い音
    void playBreak();    // ブロックが破壊された時の音（矩形波の降下音）
    void playMiss();     // ミス（ライフ喪失）時の低い音
    void playBgmLoop();  // BGMループ再生を開始する（既に再生中なら何もしない）
    void stopBgm();      // BGMを止める

    // 毎フレーム呼ぶ：BGMが鳴り終わっていたら再度再生してループさせる
    // （raylibのSoundにはネイティブのループ機構が無いため、手動での再トリガー方式を採る）
    void update(float dt);

private:
    static constexpr int SAMPLE_RATE = 44100;

    bool ready_ = false;
    bool bgmActive_ = false;

    Sound hitSound_{};
    Sound breakSound_{};
    Sound missSound_{};
    Sound bgmSound_{};

    Sound makeSineSound(float freqHz, float durationSec, float amplitude);
    Sound makeSquareDecaySound(float startFreq, float endFreq, float durationSec, float amplitude);
    Sound makeSoundFromSamples(const std::vector<int16_t>& samples);
};
