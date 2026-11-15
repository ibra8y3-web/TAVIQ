# TAVIQ Studio — Initial Local Editor

Run the local browser editor after building the CLI:

    python studio/taviq_studio.py --root . --compiler build/taviq

Studio binds only to 127.0.0.1. It lists project .taviq files, reads/saves source and launches the configured compiler only when the user presses Run. File paths are constrained to the project root, requests are size limited, browser origins are checked and subprocesses are invoked without a shell.

This is an initial lightweight editor, not yet a full IDE. Syntax highlighting, language-server protocol, diagnostics while typing, project/package panels, debugger, breakpoints, profiler and integrated model management remain future work.
