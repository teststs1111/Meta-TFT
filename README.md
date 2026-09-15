# MetaTFT Viewer for PS Vita

PS Vita向けのMetaTFTデータビューアです。MetaTFT Explorer APIからユニットの順位・勝率・Top4率を取得し、vita2dで一覧表示します。

## ビルド

GitHub Actionsの `Build Vita VPK` がpush時に自動実行されます。成功すると `metatft-vita-vpk` Artifact に `metatft_vita.vpk` が生成されます。

> APIはMetaTFTの公開公式APIではなく、観測された内部APIを利用しています。レスポンス仕様や利用可否が変更される可能性があります。
