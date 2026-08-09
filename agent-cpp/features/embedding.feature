# 功能:嵌入式 Agent 生命周期与线程模型

Agent 以 `aoi_agent.dll` 嵌入 Unity 进程,通过 C ABI 驱动。
核心要求:agent 运行在**独立的子线程**,绝不阻塞 Unity 主线程(渲染)。
消息传递为**进程内直连**(无命名管道),彻底避免管道被占用/冲突。

## 场景:DLL 加载与导出

- `AoiAgent_SetEnv` 设置进程环境变量(API Key / 工作目录)
- `AoiAgent_Start` 返回 0 表示成功,重复调用返回非 0
- `AoiAgent_IsRunning` 在启动后返回 1
- 六个导出符号均可从外部 GetProcAddress 解析(含 SetMessageCallback/SendJson)

## 场景:不阻塞调用方

- `AoiAgent_Start` 在 1000ms 内返回(agent 在子线程启动)
- 启动后调用方线程继续可执行(未被 agent 占用)
- `AoiAgent_Stop` 阻塞直到 agent 退出
- `AoiAgent_Stop` 后 `AoiAgent_IsRunning` 返回 0

## 场景:进程内消息传递(无管道)

- 注册 `AoiAgent_SetMessageCallback` 后,agent 出站消息(greeting 等)经回调送达
- `AoiAgent_SendJson` 可将入站消息(StateChange/UserInput/ScreenshotResponse 等)送入 agent
- `ScreenshotResponse` 快速路径即时 resolve(不等 LLM 结束)
- 畸形 JSON 被安全拒绝,不崩溃
- 模型生成期间 `TtsStop` / 新 `StateChange` 仍可处理

## 场景:稳定性

- 恶意/畸形/超长输入洪流下宿主进程存活
- 反复 Start/Stop 生命周期 churn 下宿主进程存活

## 场景:无管道冲突

- 不再使用任何命名管道(`aoi-unity` / `aoi-agent-cpp` 均不再使用)
- 同机其它进程占用任意管道名不影响本 agent
