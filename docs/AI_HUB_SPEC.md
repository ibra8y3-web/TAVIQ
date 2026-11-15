# TAVIQ AI Hub

## Model asset installation

\`tools/taviq_hub.py\` downloads model assets from Hugging Face without running repository code. A branch or tag is resolved to an immutable 40-character commit SHA before downloads. File extensions are restricted to reviewed model/tokenizer formats such as GGUF, safetensors and tokenizer/config metadata.

Every install requires explicit \`--accept-license\` acknowledgement. SHA-256 is checked against Hub LFS metadata when a digest is supplied; a local manifest records the resolved commit, license and downloaded-file digests.

    python tools/taviq_hub.py org/model --accept-license --file model.gguf

Repeat \`--file\` for each required shard or tokenizer file. Review the model card, license and hardware/memory requirements before installation. Do not commit weights into the language repository.

## Inference choices

Model assets and inference engines are separate. The existing Python client can call a separately installed llama.cpp server bound to loopback. The native provider host is now available through \`include/taviq/ai_provider.h\`, \`ai/ai_provider.cpp\` and the \`ai-generate\` CLI command; a compatible provider library must be supplied and trusted by the user.

The project does not bundle an inference engine, install llama.cpp automatically, or treat downloaded model files as executable code. Native provider plugins run in-process with host privileges and must be reviewed. Streaming, embeddings and unified model lifecycle management remain future work.
