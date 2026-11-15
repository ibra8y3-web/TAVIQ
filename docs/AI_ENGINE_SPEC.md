# TAVIQ Intelligence Engine — Implementation Boundary

The repository currently provides three separate pieces:

1. A C ABI contract in ai/providers/taviq_provider.h for future native provider modules.
2. tools/taviq_hub.py to fetch model/tokenizer assets from Hugging Face at an immutable revision.
3. ai/taviq_ai.py to call a separately managed llama.cpp server over HTTP, loopback by default.

These pieces are not yet unified into an in-process inference engine or language-level AI syntax. The Python client does not install llama.cpp automatically. The model hub does not execute downloaded code. The native ABI is not dynamically loaded by the C++ runtime.

The target API includes model load/unload, prompt generation, cancellation, streaming, embeddings, tokenizer access, usage metrics and resource limits. Native ABI loader, streaming, embeddings, model lifecycle and TAVIQ-language integration remain unimplemented.
