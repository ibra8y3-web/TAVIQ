# TAVIQ Package Manager

tools/taviq_pkg.py installs versioned package archives using a JSON registry index. The index schema is:

    {
      "packages": {
        "sample": {
          "versions": {
            "1.0.0": {
              "url": "https://packages.example.org/sample-1.0.0.tar.gz",
              "sha256": "64-character-lowercase-hex-digest"
            }
          }
        }
      }
    }

Commands:

    python tools/taviq_pkg.py install sample --version 1.0.0 --index registry-index.json
    python tools/taviq_pkg.py list
    python tools/taviq_pkg.py remove sample --version 1.0.0

Archives are downloaded over HTTPS, checked against SHA-256, extracted into the local project package directory and recorded in taviq.lock. Absolute paths, traversal, links, special files, oversized archives and excessive extracted content are rejected. Package hooks are never executed.

The current tool provides local registry-index installation. A hosted public registry, signed metadata, dependency graph resolution, semantic version solver and package publishing service remain future work.
