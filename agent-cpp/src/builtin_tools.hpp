#pragma once
#include <string>
#include <vector>

#include "llm_client.hpp"

namespace aoi {

// Built-in file/command tools mirroring the pi-coding-agent read/bash/edit/write
// set. Implemented as simple, safe versions.
ToolDefinition makeReadTool();
ToolDefinition makeBashTool();
ToolDefinition makeEditTool();
ToolDefinition makeWriteTool();

} // namespace aoi
