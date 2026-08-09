# 功能:音频与语音 (agent-cpp)

基于 miniaudio 的采集/播放/回环,以及 MiMo TTS 的 SSE 流式 PCM。

## 场景:音频工具函数

- PCM16 与 float32 双向转换无损往返
- WAV 头(Riff/WAVE/fmt/data)正确生成
- 环形缓冲先进先出、跨块拼接正确

## 场景:MiMo TTS SSE

- 从 `data:` SSE 行解析 audio.data 的 base64 PCM
- 多个 chunk 按序累积
- `data: [DONE]` 正常结束
- 网络错误时返回失败而非崩溃
- abort 后不再追加 chunk

## 场景:文本清洗

- `sanitizeForTts` 去除 think 块、代码围栏、emoji、markdown 符号
- `isTtsJunk` 判定纯标点片段
- `splitSentences` 按 ASCII 与 CJK 标点分句,标点完整归属前句

## 场景:播放串行

- 一句播放未结束,不会开始下一句
- 播放被打断(stop)后立即停止
