# RD ENGINE 2.0

`old/` の自作エンジン（DirectX 12 / Actor + Component）を、設計を整理しながら作り直したものです。

## ビルドと実行

1. `RdEngine.sln` を Visual Studio 2026（v145 ツールセット）で開く
2. 構成 `Debug|x64` か `Release|x64` でビルド
3. スタートアッププロジェクト `Sandbox` を実行（作業ディレクトリは自動でこのフォルダになります）

初回起動時に `Assets/Sample/Scenes/Sample.scene` が自動生成されます。

### コマンドライン

| 引数 | 内容 |
| --- | --- |
| `-noeditor` | エディタなしでゲームとして起動 |
| `-play` | エディタでも起動直後にプレイ開始 |
| `-scene <path>` | 開くシーン |
| `-capture <frame> [path]` | 指定フレームの画面を PNG に保存して終了（自動テスト用） |
| `-novsync` | 垂直同期オフ |

## フォルダ構成

```
claude/
├─ RdEngine.sln
├─ Build/Common.props        全プロジェクト共通の設定（出力先・C++20・定義）
├─ External/                 ImGui(docking) / ImGuizmo / DirectXTex / assimp / nlohmann-json / magic_enum
│  └─ ThirdParty.vcxproj     ImGui と DirectXTex をまとめた静的ライブラリ
├─ Engine/                   エンジン本体（静的ライブラリ）
│  └─ Source/
│     ├─ Core/        ログ・アサート・JobSystem・リフレクション・JSON・時間
│     ├─ Math/        ベクトル・行列・クォータニオン・幾何（AABB/Ray/Frustum）
│     ├─ Platform/    ウィンドウ・入力（キーボード/マウス/XInput）
│     ├─ RHI/         D3D12 デバイス・デスクリプタ・テクスチャ・パイプライン・シェーダコンパイラ
│     ├─ Asset/       非同期アセット管理・テクスチャ・モデル(assimp)・アニメーション
│     ├─ Render/      レンダラー（PBR・影・空・パーティクル・ブルーム・トーンマップ・スプライト・デバッグ線）
│     ├─ Scene/       Actor・Component・Transform・Scene・参照（ActorRef）・SceneSystem
│     ├─ Components/  MeshRenderer / Animator / Camera / Lights / Sprite / Particle / AudioSource
│     ├─ Physics/     コライダー・剛体・物理ワールド（SceneSystem として動く）
│     ├─ Audio/       XAudio2
│     ├─ Runtime/     シーン切替・プレイ状態・フレームの組み立て・組み込みコンポーネント登録
│     ├─ Editor/      ImGui エディタ（コマンド式 Undo/Redo・ギズモ・インスペクタ・アセットブラウザ）
│     └─ Engine.h/.cpp サブシステムの所有とメインループ
├─ Tools/CheckDependencies.ps1  モジュール間の依存が層の決まりを守っているか調べる
├─ Sandbox/                  サンプルゲーム（エンジンの使い方の例）
└─ Assets/
   ├─ Engine/   シェーダ・フォント・アイコン
   └─ Sample/   サンプル用のモデル・テクスチャ・音・シーン
```

## 設計のポイント

### モジュールの層
下の層は上の層を `#include` しない。`Tools/CheckDependencies.ps1` で確認できる（違反があると終了コード 1）。

```
Math / Core → Platform / RHI → Asset → Render / Audio → Scene → Components → Physics → Runtime → Editor
```
- `Scene` はコンポーネントや物理の具体的な型を知らない。物理ワールドは `SceneSystem` として `scene.GetSystem<PhysicsWorld>()` で使う。
- `Runtime` はエディタを知らない。エディタは `FrameOverlay` インターフェースを実装して、カメラ・ギズモ・UI を差し込む。
- エディタは作るときに `Engine&` を受け取り、`Engine::Get()` に頼らない。

### コンポーネントの更新
アクターを全部回して仮想関数を呼ぶのではなく、シーンが**型ごとのリスト**と**コールバックごとのリスト**を持つ。
- `RD_COMPONENT` が、そのコンポーネントが `Update` / `FixedUpdate` / `LateUpdate` / `OnRender` などを上書きしているかを自動で調べる。上書きしていないもの（ライトの `Update` など）は呼ばない。
- `scene.FindComponents<T>()` は型ごとのリストから集めるので、全アクターを調べない。順番は追加した順で毎回同じ。

### Undo / Redo（コマンドパターン）
操作ごとに `EditorCommand` を作って `CommandHistory` に積む。変更した部分だけを保存する。
| コマンド | 内容 |
|---|---|
| `ActorStateCommand` | アクター 1 つの変更前後（名前・Transform・コンポーネントの値・追加・削除・並び替え） |
| `ActorLifetimeCommand` | アクターの作成 / 削除（子ごと。ID はそのままなので参照も復活する） |
| `ReparentCommand` | 親子の付け替え（兄弟の順番も戻す） |
| `SceneSettingsCommand` | 環境・重力の変更 |
| `CompositeCommand` | 複数のコマンドを 1 回の操作にまとめる |

短い間隔で同じ操作が続いたら 1 つにまとめる。プレイを停止しても履歴は残る。保存した位置を覚えていて、タイトルの `*` に反映する。

### ログ
```cpp
Log::Info(LogCat::Render, "Loaded {}", path);   // カテゴリを省略すると General
```
- レベル: Trace / Debug / Info / Warning / Error / Fatal。`-loglevel debug` で実行時に変更。Game ビルドでは Debug 以下をコンパイル時に除く。
- 出力先（`LogSink`）: Visual Studio の出力ウィンドウ（Warning 以上はダブルクリックで該当行へ）、ファイル、エディタのコンソール。書き込みはログ専用スレッドで行い、Error 以上は書き出しまで待つ。
- ファイル: `Logs/RdEngine.log`。前回分は `Logs/History/` に日時付きで 10 個まで残す。各行に時刻・経過秒・レベル・カテゴリ・スレッド・発生場所が付く。
- クラッシュ時: 未処理の例外で Fatal ログを書き出し、`Logs/Crash_日時.dmp` を残す。

## 旧エンジンからの主な変更

### 設計の整理
- **型情報を整数 enum で保存するのをやめた**: 旧版は `Actor::Type` / `Component::Type` の番号を JSON に保存していたため、enum の順番を変えると既存データが壊れた。新版は型名の文字列で保存し、`ComponentRegistry` で生成する。
- **リフレクションの一本化**: コンポーネントは `Reflect(PropertyVisitor&)` に一度プロパティを書くだけで、JSON 保存・読み込み・インスペクタ表示・Undo に対応する（旧版は `Load` / `Save` / `UpdateForDev` を個別に手書き）。
- **レンダラーとエディタの分離**: 旧 `Renderer` は ImGui や `gEngine` を直接触っていた。新版の Renderer は描画リスト（`RenderList`）を受け取って描くだけで、エディタのことを知らない。
- **所有権の明確化**: Actor / Component は `unique_ptr` で Scene / Actor が所有。破棄はフレーム末に遅延実行。GPU リソースはフェンスを待ってから解放（`DeferRelease`）。
- **アニメーション状態の分離**: 旧版は姿勢を共有の `Mesh` に書き込んでいたため、同じモデルを複数置くと全員が同じ動きになった。新版は `Animator` コンポーネントごとに姿勢を持つ。
- **グローバル変数の削減**: `gEngine` / `gGraphicsEngine` / エディタの extern 群を整理した。エディタ・シーン切替・フレームの組み立ては必要なものを受け取る形にし、`Xxx::Get()` は描画やアセットなど、コンポーネントから使うサービスに限っている。

### 見つけて直した旧版のバグ
- `Matrix3` の行列式・逆行列の計算式の誤り（積が引き算になっていた）
- `Matrix4::operator*` がヘッダでは非 inline、cpp では inline という不整合
- `Mouse::GetButtonUp` / `GetButtonDown` の判定が逆
- `Matrix4` の逆行列がアフィン変換専用だった（一般の 4x4 に対応）
- 毎フレーム GPU の完了を待っていた（後述のフレーム多重化で解消）

### パワーアップした機能
| 分野 | 内容 |
| --- | --- |
| JobSystem | ワーカースレッドプール、優先度付きキュー、`ParallelFor`、待機中のスレッドもジョブを手伝う Wait、メインスレッドへの処理の戻し |
| 非同期読み込み | テクスチャ・モデル・音声はワーカーでデコードし、次フレーム先頭で GPU へ転送。読み込み中は代替表示 |
| RHI | 2 フレーム同時進行（CPU と GPU の並列化）、フレームごとの一時アップロード領域、遅延解放、フリーリスト式デスクリプタヒープ |
| 描画 | PBR（GGX）、平行光源のシャドウマップ（PCF・テクセルスナップ）、点光源/スポットライト最大 32/16 灯、法線マップ、空のグラデーション + 太陽、フォグ、HDR + ブルーム + ACES トーンマップ、視錐台カリング、インスタンシングのパーティクル、スプライトのバッチ描画 |
| アニメーション | ジョブで並列に姿勢計算、クロスフェード、別ファイルのアニメーション流用（単位の違いを自動補正） |
| 物理 | 剛体（質量・慣性・重力・抵抗）、球/箱/カプセル/メッシュ（BVH）の衝突、スイープ&プルーンの広域判定、逐次インパルス法（反発・摩擦）、Enter/Stay/Exit イベント、トリガー、レイキャスト、レイヤー |
| オーディオ | BGM / SE のバス音量、ループ、距離減衰とパンによる 3D 定位 |
| エディタ | ドッキング UI、Undo/Redo、移動/回転/拡縮ギズモ（Ctrl でスナップ）、クリック選択、階層のドラッグ&ドロップで親子付け、アセットブラウザからのドラッグ&ドロップ、コンポーネント追加メニュー（検索付き）、プレイ/一時停止/ステップ/停止（停止で元に戻る）、プレハブ保存、コンソール、統計、シェーダのホットリロード（Ctrl+R） |

## ゲームコードの書き方

```cpp
// MyComponent.h
class Spinner final : public rd::Component
{
    RD_COMPONENT(Spinner)
public:
    float speed = 90.0f;
    void Reflect(rd::PropertyVisitor& v) override { v.Property("Speed", speed); }
    void Update(float dt) override
    {
        GetTransform().Rotate(rd::Quaternion::AngleAxis(rd::Vector3::kUnitY, rd::math::ToRadians(speed * dt)), false);
    }
};

// MyComponent.cpp
RD_REGISTER_COMPONENT(Spinner, "Game");// これだけでエディタの「Add Component」に出る
```

## エディタの操作

| 操作 | 内容 |
| --- | --- |
| 右ドラッグ + WASD/QE | カメラ移動（Shift で加速、ホイールで速度） |
| 中ドラッグ / Alt + 左ドラッグ | パン / 回り込み |
| F | 選択中のアクターへフォーカス |
| W / E / R | 移動 / 回転 / 拡縮ギズモ |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+S / Ctrl+O | シーン保存 / 開く |
| Ctrl+D / Delete | 複製 / 削除 |
| Ctrl+P | プレイ / 停止 |

## 座標系の規約
左手座標系・行ベクトル（`v * M`）・行列は「先に適用するものを左」（`S * R * T`）。旧エンジンと同じ。

## 既知の制限
- 旧版のレベルファイル（`.rdlv`）と互換はない（型を番号で保存していたため）
- シャドウは最初の平行光源の 1 枚のみ（カスケードなし）
- メッシュコライダーは静的な物体専用
- 音声は WAV のみ
