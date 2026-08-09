# 功能:LLM 会话循环 (agent-cpp)

替换闭源 pi-coding-agent 的自研 agent 循环,对接 OpenAI 兼容 `/chat/completions`
SSE 流式接口(opencode.ai/zen 等)。

## 场景:流式文本增量

- 每次 `message_update` 事件携带纯增量 delta
- 累计文本与增量语义正确(支持 cumulative 与 pure-delta 两种 provider)
- 120ms 节流发送面板文本

## 场景:工具调用循环

- 模型返回 tool_calls 时执行对应工具
- 工具结果回填会话,继续下一轮推理
- 无工具调用时发出 `message_end` + `agent_end`
- 工具循环有 32 次上限保护,防止死循环

## 场景:多轮记忆

- 第二轮 prompt 携带第一轮完整对话(含最终 assistant 回复)
- 工具结果中的图片(screenshot)会附到下一轮用户消息

## 场景:SSE 解析健壮性

- `data: [DONE]` 正确结束
- 非 `data:` 前缀的行被忽略
- 畸形 JSON 行被跳过,不中断流
- 缺字段的 chunk 不崩溃(null-safe 访问)

## 场景:错误处理

- HTTP 状态 >= 400 时本轮结束并发出 agent_end
- 工具执行抛异常时捕获并回传错误文案
- provider 未配置时不会崩溃
