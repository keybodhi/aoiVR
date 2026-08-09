# 功能:消息协议 (agent-cpp <-> Unity)

Agent 与 Unity 通过**进程内 C ABI** 传递消息(不再使用命名管道)。
消息内容为 4 字节小端长度前缀 + UTF-8 JSON,经 AoiAgent_SendJson /
AoiAgent_SetMessageCallback 在进程内收发。

## 场景:编码与解码

- `encodeMessage` 输出 4 字节长度头 + JSON
- 长度字段等于 JSON 字节数
- `decodeMessage` 对合法 JSON 还原为 Message 对象
- `decodeMessage` 对非法 JSON 返回失败
- 消息往返(tryReadMessage)保持 type/payload/id/timestamp 不变

## 场景:流式读取(帧缓冲)

- 分段到达的数据能拼齐一条完整消息
- 长度 <= 0 或 > 64MB 时返回协议错误
- 缓冲不足时返回"需要更多数据"
- 一个缓冲内含多条消息时依次全部解析

## 场景:消息类型

- 全部 20 种 MessageType 字符串可双向转换
- 未知字符串回退为 Acknowledge
- 非对象 JSON(字符串/数字/null/数组)安全拒绝

## 场景:进程内传输(C ABI)

- `AoiAgent_SendJson` 接收合法消息并投递给 agent
- 畸形 JSON 返回失败且不崩溃
- 出站消息经注册的 C 回调送达

