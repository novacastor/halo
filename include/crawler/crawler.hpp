#pragma once
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>
#include "engine/types.hpp"

struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
};

namespace Engine {
    
    const std::unordered_set<std::string, StringHash, std::equal_to<>> EXTENSION_WHITELIST = {
        // C/C++
        ".c", ".h", ".cpp", ".cc", ".cxx", ".hpp", ".hh", ".hxx", ".inl", ".ipp",
        ".tcc", ".cppm", ".ixx",
    
        // Rust / Go / Zig
        ".rs", ".go", ".zig",
    
        // Python
        ".py", ".pyi", ".pyx", ".pxd",
    
        // JVM languages
        ".java", ".kt", ".kts", ".scala", ".groovy", ".gradle",
    
        // .NET
        ".cs", ".fs", ".fsx", ".vb",
    
        // Web / JS ecosystem
        ".js", ".jsx", ".mjs", ".cjs", ".ts", ".tsx", ".html", ".htm", ".css",
        ".scss", ".sass", ".less", ".vue", ".svelte", ".astro",
    
        // Scripting / shell
        ".sh", ".bash", ".zsh", ".fish", ".ps1", ".psm1", ".bat", ".cmd",
    
        // Other languages
        ".rb", ".php", ".pl", ".pm", ".lua", ".swift", ".m", ".mm", ".dart",
        ".ex", ".exs", ".erl", ".hrl", ".hs", ".ml", ".mli", ".clj", ".cljs",
        ".r", ".jl", ".nim", ".v", ".d", ".cr", ".sol",
    
        // Config / data / markup
        ".json", ".jsonc", ".json5", ".yaml", ".yml", ".toml", ".xml", ".ini",
        ".cfg", ".conf", ".env", ".properties", ".editorconfig",
    
        // Build / project files
        "CMakeLists.txt", ".cmake", "Makefile", ".mk", ".ninja", ".bazel",
        ".bzl", "BUILD", "WORKSPACE", ".gyp", ".gn", ".gni",
    
        // Docs / text
        ".md", ".mdx", ".txt", ".rst", ".adoc", ".tex", ".org", ".log",
    
        // SQL / data query
        ".sql", ".graphql", ".gql",
    
        // Misc dev-relevant
        ".proto", ".thrift", ".avsc", ".patch", ".diff", ".gitignore",
        ".gitattributes", ".dockerfile", "Dockerfile", ".tf", ".tfvars"
    };
    
    const std::unordered_set<std::string, StringHash, std::equal_to<>> FOLDER_BLACKLIST = {
        // version control
        ".git", ".svn", ".hg", ".bzr", ".fossil-settings", "CVS", "_darcs", "_FOSSIL_",
    
        // editors/IDEs
        ".vscode", ".vscode-server", ".idea", ".vs", ".eclipse", ".settings", ".project",
        ".classpath", ".vim", ".nvim", ".emacs.d", ".fleet", ".nova", ".zed",
        ".spyderproject", ".spyproject", ".atom", ".metals", ".bloop", ".ensime_cache",
        ".history", "nbproject", ".lazy", ".luarocks", ".helix",
    
        // AI coding tools / assistants
        ".antigravity", ".claude", ".cursor", ".cursor-server", ".continue", ".aider",
        ".copilot", ".windsurf", ".codeium", ".tabnine", ".github-copilot", ".amazonq",
        ".cody", ".warp", ".replit",
    
        // build/output (general)
        "build", "builds", "cmake-build-debug", "cmake-build-release",
        "cmake-build-relwithdebinfo", "cmake-build-minsizerel", "out", "dist", "target",
        "bin", "obj", "_build", "CMakeFiles", "Debug", "Release", "x64", "x86",
        "cbuild", "_deps", "cbuild-debug", "Testing", "artifacts", "output", ".output",
        "builddir", "install_manifest",
    
        // C++ package managers / tooling
        "vcpkg_installed", "buildtrees", "downloads", ".conan", ".conan2", "conan-cache",
        ".ccls-cache", ".clangd", ".cquery_cache", ".ycm_extra_conf_cache", "ipch",
    
        // JS/web/Node ecosystem
        "node_modules", ".npm", ".pnpm-store", ".yarn", "bower_components", ".next",
        ".nuxt", ".svelte-kit", ".turbo", ".parcel-cache", ".webpack", ".vite",
        ".cache-loader", ".angular", "coverage", ".nyc_output", ".docusaurus",
        ".vercel", ".netlify", ".astro", ".gatsby", ".yarn-cache", ".pnp", ".rush",
        ".verdaccio", "storybook-static",
    
        // Python
        "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".tox", ".venv",
        "venv", "env", ".eggs", ".ipynb_checkpoints", ".hypothesis", "site-packages",
        ".pyre", ".pytype", ".nox", ".pdm-build", ".conda", "conda-meta", "__pypackages__",
    
        // Rust
        ".cargo", ".rustup",
    
        // Java/JVM/Android
        ".gradle", ".m2", "gradle", ".android", "captures", ".externalNativeBuild",
        ".cxx", ".kotlin", ".sts4-cache", ".apt_generated", ".ivy2",
    
        // Ruby
        ".bundle", ".rbenv", ".rvm", ".yardoc",
    
        // iOS/macOS dev
        "Pods", "DerivedData", ".build", "xcuserdata", ".swiftpm", "Carthage", ".accio",
    
        // .NET
        "packages", "TestResults",
    
        // PHP / Go
        "vendor", ".phpunit.cache", "Godeps",
    
        // Dart/Flutter
        ".dart_tool", ".pub-cache", ".flutter-plugins", ".flutter-plugins-dependencies",
        ".pub", "ephemeral",
    
        // Haskell/OCaml/Elixir/Elm
        ".stack-work", "_opam", ".opam", "deps", ".elixir_ls", "_checkouts",
        "elm-stuff", ".elm",
    
        // containers/VMs/infra/cloud CLIs
        ".docker", ".vagrant", ".terraform", ".serverless", ".aws-sam", ".kube",
        ".minikube", ".helm", ".terragrunt-cache", ".pulumi", ".aws", ".azure",
        ".gcloud", "cdk.out", ".chef",
    
        // OS/filesystem noise
        ".cache", ".local", ".Trash", ".sass-cache", "System Volume Information",
        "lost+found", "$RECYCLE.BIN", ".Spotlight-V100", ".fseventsd",
        ".DocumentRevisions-V100", ".TemporaryItems", "__MACOSX", ".AppleDouble",
        ".apdisk", ".Trashes", ".overlay-store", ".wine",
    
        // noisy subfolders inside app/tool config dirs (keeps parent dir indexable)
        "Cache", "GPUCache", "Code Cache", "DawnCache", "ShaderCache", "blob_storage",
        "Service Worker", "IndexedDB", "Session Storage", "Local Storage",
        "CacheStorage", "Crashpad", "Crash Reports", "logs", "log", "Logs", "tmp",
        "temp", "CachedData", "extensions", "Extensions", "Sessions", "WebStorage",
        "databases",
    
        // misc dev tooling / test caches
        ".terraform.d", ".sonar", ".nx", ".yalc", ".direnv", ".pre-commit-cache",
        ".husky", ".changeset", ".rollup.cache", ".swc", ".esbuild", ".jest-cache",
        ".karma-cache", ".puppeteer-cache", ".playwright-cache", ".cypress-cache"
    };
    
    const std::unordered_set<std::string, StringHash, std::equal_to<>> GLOBAL_FOLDER_BLACKLIST = { 
        ".cache", ".Trash", ".local/share" 
    };

    class Crawler {
        public:
        CrawlBatch run_crawler(const std::string &target_path);
        CrawlBatch process_filesystem_crawl(const std::string &target_path);
        bool check_extention(const std::string &ext) {
            return EXTENSION_WHITELIST.find(ext) != EXTENSION_WHITELIST.end();
        }

    };
}
