#pragma once

// タスク6（⑧）：ハイスコアをファイルに保存・読み込みするユーティリティ。
// tetris-cpp（src/ScoreFile.hpp）・space-invaders-cpp（src/ScoreFile.hpp）と同じ
// 「load()/save()の静的メソッドのみ」というシンプルなパターンを踏襲する。
//
// 【Web版の挙動差について】
// tetris-cpp・space-invaders-cppはいずれもWeb版で無効化する分岐コードを書いておらず、
// emscriptenの仮想ファイルシステム（MEMFS）上にそのままファイルを読み書きしている。
// 本プロジェクトもこの方式を踏襲し、無条件でファイルI/Oを呼ぶ（Web版だけ処理を
// スキップするような分岐は入れない）。この結果、Web版はページをリロードすると
// MEMFSが初期化され直すため、ハイスコアがリセットされるという挙動差が生まれる
// （ネイティブ版はhighscore.txtがディスクに残るので永続化される）。
// これは既知の仕様上の割り切りとしてAGENTS.mdにも明記する。
struct SaveData {
    int highScore = 0;
};

class ScoreFile {
public:
    // ファイルからハイスコアを読み込む（ファイルが存在しない・壊れている場合は0のまま返す）
    static SaveData load();
    // ハイスコアをファイルに保存する
    static void save(int highScore);

private:
    static constexpr const char* FILE_PATH = "highscore.txt";
};
