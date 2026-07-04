# Breakout 3D C++

「ブロックくずし」を3D空間（X/Y/Z軸）に拡張した C++ / raylib 学習プロジェクトです。
tetris-cpp・space-invaders-cpp に続く3作目で、C++・raylib・3D数学（衝突判定・反射）を
学ぶことを目的としています。

- 🎮 プレイ版（GitHub Pages）: https://yosimi-kiyohiro.github.io/breakout3d-cpp/

## 技術検証の経緯（3D衝突判定というリスクにどう向き合ったか）

このプロジェクトで最もリスクが高かったのは「3D空間でのボール⇔パドル⇔ブロック⇔壁の
衝突判定と反射が破綻しないこと」でした。誇張せずに、実際に起きたことを順番に書きます。

1. **1周目**：まず衝突判定だけを切り出したスパイク実装を作り、TC1〜TC6の6項目でPASSを
   確認しました。しかしレビューで「ブロックの見た目の奥行き（1.0）と、実際の当たり判定
   （Z_MIN〜Z_MAX全厚み）が一致しておらず、実質2D判定になっている」「複合3軸の軌道が
   一度も検証されていない」という致命的な指摘を受けました。
2. **2周目**：ブロックのZ方向の当たり判定を見た目と一致させる設計変更を行い、TC7〜TC9を
   新設して「9/9 PASS」と報告しましたが、これも**過大評価でした**。改めて確認したところ、
   ブロックのZ面（実際にボールがぶつかる面）に一度もボールを当てるテストを書いていない
   ことが判明しました。
3. **3周目**：TC10（ブロックのZ面単独ヒット）・TC11（X/Y/Zの3軸すべてが同時に外側判定に
   なる真の立体角ヒット）を新設し、ようやく3軸判定の全体が実際に動作することを確認しました。

この後もステージ密度を上げた際にブロックの継ぎ目でボールがすり抜ける不具合（TC12で実測・
修正）や、コードレビューで見つかったスコア計算バグ（TC8・TC13に回帰チェックを追加）が
ありました。その都度テストケースを追加し、実測で直ったことを確認しています。

**現在、`src/test_collision.cpp` に TC1〜TC13（13項目）のヘッドレステストがあり、
全てPASSすることを実測で確認しています**（`test_results.log` に実測ログを記録）。
このテストは衝突判定ロジックの単体的な健全性チェックであり、「ゲームとして完成度が
高い」ことを保証するものではありません（詳細は下記「既知の限界」を参照してください）。

## 遊び方・操作方法

| キー | 動作 |
|---|---|
| ← / A | パドルを左へ移動 |
| → / D | パドルを右へ移動 |
| P / Esc | 一時停止 ⇔ 再開 |
| Enter / Space | タイトル→プレイ開始／ゲームオーバー→タイトルへ／ステージクリア→次のステージへ |

- ボールはミス後・ステージ開始後、パドル上から自動的に発射されます（発射待ちの演出はなし）。
- パドルはX軸方向にのみ移動します。ヒットした位置（X/Zオフセット）に応じてボールの反射方向が変わります。
- ライフは3。ブロックをすべて壊すとステージクリア、ライフが0になるとゲームオーバーです。
- ハイスコアは `highscore.txt` に自動保存されます（ネイティブ版のみ永続化。Web版はページ再読み込みでリセットされます）。

## ビルド方法

### ⚠️ 前提条件（必ず先にお読みください）

**このプロジェクトは単体では Web ビルドできません。** `build_web.sh` は
`emsdk`（Emscripten SDK）と Web 用 raylib 静的ライブラリ（`libraylib.web.a`）を
`tetris-cpp` プロジェクトと共用しており、次のように **兄弟ディレクトリ構成が前提** です。

```
Projects/                      ← 共通の親ディレクトリ
├── breakout3d-cpp/            ← 本リポジトリ
├── tetris-cpp/
│   └── emsdk/                 ← Emscripten SDK（tetris-cppと共用）
└── raylib-src/
    └── src/
        └── libraylib.web.a    ← Web用raylib静的ライブラリ（raylib-srcと共用）
```

`tetris-cpp`・`raylib-src` が `breakout3d-cpp` と同じ階層（`Projects/` 直下）に
存在しない場合、`build_web.sh` はエラーで停止します（ネイティブビルド `build.sh` は
raylibのWindows版ライブラリのみ使用するため、この前提は不要です）。

### ネイティブビルド（Windows / w64devkit）

```bash
cd /path/to/breakout3d-cpp
sh build.sh          # ネイティブ確認（breakout3d.exe）
sh build_test.sh      # 衝突判定ヘッドレステスト（test_collision.exe・TC1〜TC13）
```

- コンパイラ：w64devkit（MinGW-GCC）
- ライブラリ：raylib 6.0 MinGW版（`C:\raylib\raylib-6.0_win64_mingw-w64\`）
- CMake・vcpkgは使用しません

### Web ビルド（emscripten）

上記の兄弟ディレクトリ構成が整っている前提で：

```bash
cd /path/to/breakout3d-cpp
sh build_web.sh       # web/index.html・index.js・index.wasm を生成
emrun web/index.html  # ローカルサーバーを起動してブラウザで確認
```

### CI（GitHub Actions）

`.github/workflows/web-build.yml` で以下を自動実行します。

1. **test job**：raylib（PLATFORM_DESKTOP）をソースからビルドし、衝突判定ヘッドレス
   テスト（TC1〜TC13）を実行して全PASSを確認する
2. **web-deploy job**：test成功後のみ実行。raylib（PLATFORM_WEB）をソースからビルドし、
   `build_web.sh` でWebビルドしてGitHub Pagesへデプロイする

CI環境ではローカルの兄弟ディレクトリの代わりに、ワークフロー内で同じ相対パス構成
（`tetris-cpp/emsdk`・`raylib-src/src`）を一時的に再現しています。

## 技術スタック

| 項目 | 内容 |
|---|---|
| 言語 | C++17 |
| グラフィックスライブラリ | raylib 6.0 |
| ネイティブコンパイラ | w64devkit（MinGW-GCC） |
| Webビルド | emscripten（emsdk） |
| ビルドツール | シェルスクリプト（CMake・vcpkgは不使用） |

## 既知の限界（正直な報告）

開発中のレビューで見つかり、現時点で未解決・または割り切って受け入れている点です。

- **パドル反射の期待値検証（TC3・TC5b）は、本番コードと同じ式を再実装した比較が中心**です。
  パドル反射は「入射方向によらずヒット位置だけで反射方向を決める」設計のため、物理的な
  「入射角=反射角」則と比較する独立した検証手段がありません。
- **TC7・TC13で検証しているのは「反転軸がちょうど1軸だけ選ばれ、クラッシュ・二重反転が
  起きない」という健全性のみ**です。「選ばれた軸が幾何学的に正しいか」までは検証していません。
- **ブロックの複数同時ヒット処理（同一サブステップで重なっている生存ブロックを全て処理する
  方式）は、TC12・TC13で耐久値1の同時破壊ケースまでは実測しましたが、一部のブロックだけ
  耐久値が2以上で生き残る構成は未検証**です。理論上、上書きされたスナップ位置が生存ブロックの
  内部に残ってしまう可能性が認識されていますが、実測・修正のいずれも行っていません。
- **Webビルドは、ビルド成功とHTTP配信（emrun経由でindex.html/js/wasmがHTTP 200）までは
  確認済みですが、ブラウザでの実際の目視確認（画面描画・キーボード操作）は本開発環境
  （ヘッドレスCLI環境）では実施できていません。** ブラウザでの動作確認は今後の課題です。
- **Web版の音声自動再生対策（`AudioManager::initAfterUnlock()`）はロジック上は実装済み**
  ですが、実際にブラウザで「タイトル画面の最初のキー入力後に音が鳴るか」の実聴確認は
  未実施です。
- **Web版のパフォーマンス（実測FPS）は本開発環境では計測できていません。** ネイティブ版は
  60FPS想定・Web版はブラウザで平均30FPS以上を目標としていますが、実機・実ブラウザでの
  計測が必要です。
- **ハイスコアの保存タイミングは「ゲームオーバー確定時」「ステージクリア確定時」のみ**
  のため、ウィンドウを閉じる等で不正終了した場合、その回の最高得点が保存されないことが
  あります（意図的な割り切り）。
- Web版のハイスコアはページをリロードするとリセットされます（emscriptenの仮想ファイル
  システム上にのみ保存されるため）。ネイティブ版は `highscore.txt` に永続化されます。

より詳細な検証過程・実測ログは `AGENTS.md`・`test_results.log` を参照してください。

## ファイル構成

```
breakout3d-cpp/
├── src/
│   ├── constants.hpp             プレイフィールド境界・速度・パドル/ブロック/ステージ配置の定数
│   ├── Ball.hpp/.cpp             ボール（位置・速度・半径）
│   ├── Paddle.hpp/.cpp           パドル（X軸移動のみ・Z方向に厚みあり）
│   ├── Block.hpp/.cpp            ブロック（XY平面配置＋奥レイヤー・Z方向は見た目と一致する薄い板）
│   ├── CollisionSystem.hpp/.cpp  衝突判定・反射ロジック（X/Y/Z 3軸判定・同一サブステップ複数ブロック処理）
│   ├── Stage.hpp/.cpp            ステージ番号に応じたブロック配置の生成
│   ├── GameState.hpp/.cpp        ゲーム状態管理（GamePhase・スコア/ライフ/ステージ進行）
│   ├── Renderer.hpp/.cpp         raylib描画ラップ（CAMERA_CUSTOM固定カメラ）
│   ├── ParticleSystem.hpp/.cpp   ブロック破壊時の3Dパーティクル演出
│   ├── AudioManager.hpp/.cpp     raylibのWave/SoundでPCM生成する効果音・BGM
│   ├── ScoreFile.hpp/.cpp        ハイスコアのファイル永続化
│   ├── main.cpp                  エントリポイント（静的グローバル＋UpdateDrawFrame＋PLATFORM_WEB分岐）
│   └── test_collision.cpp        衝突判定ヘッドレステスト（TC1〜TC13）
├── web/
│   └── shell.html                Webビルド用シェル（ビルド成果物のindex.html等は.gitignore対象）
├── .github/workflows/
│   └── web-build.yml             CI（衝突判定テスト→Webビルド→GitHub Pagesデプロイ）
├── build.sh                      ネイティブビルド（breakout3d.exe）
├── build_test.sh                 衝突判定テストビルド（test_collision.exe）
├── build_web.sh                  Webビルド（emscripten）
├── AGENTS.md                     開発経緯・設計判断の詳細記録
└── .gitignore
```

## 関連プロジェクト

- [tetris-cpp](https://github.com/yosimi-kiyohiro/tetris-cpp)（同じ作者の1作目・emsdk/raylib-srcを共用）
- [space-invaders-cpp](https://github.com/yosimi-kiyohiro/space-invaders-cpp)（同じ作者の2作目・SDL3版）
