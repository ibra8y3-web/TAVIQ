# TAVIQ Developer Tools

Run commands from the repository root.

## Formatter, linter, inspector and profiler

    python tools/taviq_dev.py format examples/hello.taviq
    python tools/taviq_dev.py format examples/hello.taviq --check
    python tools/taviq_dev.py lint examples/hello.taviq --compiler build/taviq
    python tools/taviq_dev.py inspect examples/hello.taviq --compiler build/taviq
    python tools/taviq_dev.py profile examples/hello.taviq --compiler build/taviq --runs 20

The formatter handles the implemented function-declarations plus one \`flow main\` grammar. Lint delegates to compiler parsing/type checking. Inspect compiles to QVM and prints disassembly. Profile measures repeated process wall-clock latency; it is not an instruction-level sampler.

## Language Server Protocol

    python tools/taviq_lsp.py build/taviq

The LSP uses JSON-RPC over stdio. Current features include document open/change/save diagnostics (through the compiler), keyword/builtin completion, and hover information. It is an initial server, not a complete semantic language service; definition/references/rename, workspace symbols, incremental parse trees and richer semantic completion remain outstanding.

## Debugging and Studio

    build/taviq debug examples/hello.taviq 3,8
    python tools/taviq_debug.py examples/hello.taviq --compiler build/taviq --break 3,8
    python studio/taviq_studio.py --root . --compiler build/taviq --port 8765

The debugger traces QVM instruction indices and can select instruction-index breakpoints. Source-level breakpoints, source maps, stepping UI, variable watches and call-stack navigation are not implemented yet. Studio is a loopback-only local editor with project file actions, compiler diagnostics, formatting, run and debug buttons; full LSP session and interactive debugger integration remain outstanding.
