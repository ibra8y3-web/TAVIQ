# TAVIQ AI Hub

## Scope

tools/taviq_hub.py downloads model assets from Hugging Face without installing or running repository code. A branch or tag is resolved to a 40-character immutable commit SHA before downloads. Files are restricted to model and tokenizer formats such as GGUF, safetensors and reviewed tokenizer/config metadata.

Every install requires an explicit --accept-license acknowledgement. SHA-256 is checked against Hub LFS metadata when a digest is provided; a local manifest records the resolved commit, license and downloaded-file digests.

Example:

    python tools/taviq_hub.py org/model --accept-license --file model.gguf

Repeat --file for each required shard/tokenizer file. Review the model card and hardware/memory requirements before installation. Do not commit model weights to the language repository.

## Inference

Model assets and inference engines are separate. Start a separately installed llama.cpp llama-server with the chosen model, bound to 127.0.0.1, then use ai/taviq_ai.py to submit prompts to its /completion endpoint. Remote endpoints require the explicit --allow-remote flag.

A C ABI contract is defined in ai/providers/taviq_provider.h; dynamic native provider discovery/loading is not implemented in this release. Streaming, embeddings and native runtime management remain planned.
