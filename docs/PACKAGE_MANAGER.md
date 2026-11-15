# TAVIQ Package Manager and Registry

## Install and lock

\`tools/taviq_pkg.py\` installs versioned package archives from a JSON registry index. It supports install, list, remove and semantic-version dependency resolution:

    python tools/taviq_pkg.py install sample --version 1.0.0 --index https://registry.example/index.json
    python tools/taviq_pkg.py resolve sample --version 1.0.0 --index https://registry.example/index.json
    python tools/taviq_pkg.py list
    python tools/taviq_pkg.py remove sample --version 1.0.0

The resolver supports exact, comparison, caret and tilde constraints and writes a \`taviq.lock\` resolution record. Package installation downloads over HTTPS, checks archive SHA-256, extracts into the project-local package directory, rejects traversal/link/special archive members and size abuse, and never executes package scripts.

For signed indexes, pass \`--require-signature --public-key <ed25519-public-key> --signature <detached-signature>\`.

## Publish and serve an index

\`tools/taviq_registry.py publish\` creates a semantic-versioned index entry with archive SHA-256 and signs the JSON index using an Ed25519 private key through OpenSSL:

    python tools/taviq_registry.py publish --name sample --version 1.0.0 --archive sample-1.0.0.tar.gz --base-url https://packages.example/sample --index registry/index.json --private-key registry/private.pem

\`tools/taviq_registry.py serve\` serves an existing registry directory read-only, defaulting to loopback. Remote exposure requires an explicit flag and a TLS reverse proxy.

## Remaining boundaries

This repository supplies registry tools, not a hosted public registry service or package CDN. Dependency resolution writes a lock plan; automatic transitive download/install orchestration, authenticated publisher accounts, key rotation/revocation, transparency logs and production registry operations remain outstanding.
