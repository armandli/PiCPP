# Standalone programs that preserve earlier milestones' demos; not part of pi.

# M2: banner + `--raw-chat PROMPT --model ID` raw SSE stream from Ollama.
add_executable(pi_m2_raw_chat m2_raw_chat.cpp)
# pi::modes brings in every module, third-party library and PICPP_VERSION.
target_link_libraries(pi_m2_raw_chat PRIVATE pi::modes)
pi_set_warnings(pi_m2_raw_chat)
pi_set_sanitizers(pi_m2_raw_chat PRIVATE)
