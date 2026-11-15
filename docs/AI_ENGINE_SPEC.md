# TAVIQ Intelligence Engine — Implementation Boundary

## Implemented provider host

- \`include/taviq/ai_provider.h\` defines ABI v1: provider metadata, initialization, single-prompt generation, response release and shutdown.
- \`ai/ai_provider.cpp\` implements a cross-platform dynamic-library loader. It validates ABI version and callbacks, bounds configuration and prompt sizes, caps generation tokens and temperature, and releases provider-owned responses.
- \`taviq ai-generate <provider-library> <prompt> [tokens] [temperature]\` loads a trusted provider and invokes generation.
- \`tests/fixtures/ai_provider.cpp\` is a contract fixture for ABI tests, not an inference model.

## Separate model workflow

\`tools/taviq_hub.py\` downloads model/tokenizer assets from Hugging Face at an immutable revision after license acknowledgement. \`ai/taviq_ai.py\` remains a separate HTTP client for a separately installed llama.cpp server.

## What is not included

The repository does not bundle model weights, llama.cpp, an in-process inference engine, tokenizer runtime, GPU kernels or a production AI provider. The native provider ABI is a trusted native-code extension point and is not a sandbox. The current ABI exposes one-shot generation only; streaming, embeddings, cancellation, batching, model discovery/installation integration, usage accounting and language-level AI syntax remain outstanding.
