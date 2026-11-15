# AI subsystem

Model asset installation is provided by tools/taviq_hub.py. The client in taviq_ai.py calls a separately running llama.cpp server over loopback HTTP. The C ABI contract is in providers/taviq_provider.h. An in-process native model runtime and plugin loader are not yet included.
