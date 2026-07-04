#include "AudioManager.hpp"

#include <cmath>

namespace {
// raylib.h が PI をマクロ定義しているため衝突を避けて別名にする（他ファイルと同じ方針）
constexpr float AUDIO_PI = 3.14159265358979323846f;
}  // namespace

AudioManager::AudioManager() {
#if !defined(PLATFORM_WEB)
    // ネイティブはブラウザの自動再生ポリシーの制約が無いため、起動直後に初期化する
    initAfterUnlock();
#endif
    // Webはタイトル画面での最初のキー入力（main.cpp側）からinitAfterUnlock()を呼ぶまで
    // 音声デバイスを開かない
}

AudioManager::~AudioManager() {
    if (!ready_) return;
    UnloadSound(hitSound_);
    UnloadSound(breakSound_);
    UnloadSound(missSound_);
    UnloadSound(bgmSound_);
    CloseAudioDevice();
}

void AudioManager::initAfterUnlock() {
    if (ready_) return;  // 二重初期化防止

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        CloseAudioDevice();
        return;
    }

    hitSound_ = makeSineSound(660.0f, 0.05f, 0.35f);
    breakSound_ = makeSquareDecaySound(300.0f, 80.0f, 0.18f, 0.4f);
    missSound_ = makeSquareDecaySound(180.0f, 60.0f, 0.35f, 0.4f);
    bgmSound_ = makeSineSound(220.0f, 1.0f, 0.12f);  // シンプルなループ用の単音BGM

    ready_ = true;
}

Sound AudioManager::makeSoundFromSamples(const std::vector<int16_t>& samples) {
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(samples.size());
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    // LoadSoundFromWave()は内部でwave.dataをコピーするため、constのローカルバッファへの
    // ポインタを渡しても関数呼び出しが終わるまでの間だけ有効であれば問題ない
    wave.data = const_cast<int16_t*>(samples.data());

    return LoadSoundFromWave(wave);
}

Sound AudioManager::makeSineSound(float freqHz, float durationSec, float amplitude) {
    int numSamples = static_cast<int>(SAMPLE_RATE * durationSec);
    std::vector<int16_t> buf(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float fade = 1.0f - (t / durationSec);  // 末尾にフェードアウト（プチノイズ防止）
        buf[i] = static_cast<int16_t>(std::sin(2.0f * AUDIO_PI * freqHz * t) * amplitude * fade * 32767.0f);
    }
    return makeSoundFromSamples(buf);
}

Sound AudioManager::makeSquareDecaySound(float startFreq, float endFreq, float durationSec, float amplitude) {
    int numSamples = static_cast<int>(SAMPLE_RATE * durationSec);
    std::vector<int16_t> buf(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        // 指数補間：音が自然なカーブで降下する（線形より耳に心地よい。space-invaders-cppと同じ考え方）
        float freq = startFreq * std::pow(endFreq / startFreq, t / durationSec);
        float fade = 1.0f - (t / durationSec);
        float phase = std::fmod(t * freq, 1.0f);
        float wave = (phase < 0.5f) ? 1.0f : -1.0f;
        buf[i] = static_cast<int16_t>(wave * amplitude * fade * 32767.0f);
    }
    return makeSoundFromSamples(buf);
}

void AudioManager::playHit() {
    if (!ready_) return;
    PlaySound(hitSound_);
}

void AudioManager::playBreak() {
    if (!ready_) return;
    PlaySound(breakSound_);
}

void AudioManager::playMiss() {
    if (!ready_) return;
    PlaySound(missSound_);
}

void AudioManager::playBgmLoop() {
    if (!ready_) return;
    bgmActive_ = true;
    if (!IsSoundPlaying(bgmSound_)) PlaySound(bgmSound_);
}

void AudioManager::stopBgm() {
    bgmActive_ = false;
    if (ready_) StopSound(bgmSound_);
}

void AudioManager::update(float dt) {
    (void)dt;  // 現状は時間経過そのものを使わないが、将来のフェード等のためdt引数は残す
    if (!ready_ || !bgmActive_) return;
    // raylibのSoundにはネイティブのループ機構が無いため、鳴り終わっていたら
    // 再度PlaySound()して手動でループさせる（シームレスではないが簡易ループとして許容する）
    if (!IsSoundPlaying(bgmSound_)) PlaySound(bgmSound_);
}
