# TAVIQ Developer Tools

Run commands from the repository root.

    python tools/taviq_dev.py format examples/hello.taviq
    python tools/taviq_dev.py format examples/hello.taviq --check
    python tools/taviq_dev.py lint examples/hello.taviq --compiler build/taviq
    python tools/taviq_dev.py inspect examples/hello.taviq --compiler build/taviq
    python tools/taviq_dev.py profile examples/hello.taviq --compiler build/taviq --runs 20

The formatter handles the currently implemented single flow-main grammar and rejects incomplete statements or unbalanced parentheses. Lint delegates to the compiler parser and type checker. Inspect compiles to QVM and prints disassembly. Profile repeatedly runs the program and reports wall-clock process latency; it is not an instruction-level profiler.
