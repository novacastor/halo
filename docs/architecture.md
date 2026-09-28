# How Halo works

Halo is a local, single-machine search application. It discovers eligible files beneath a root directory, stores a catalog and token index in SQLite, and runs queries against that index. The main implementation is in `include/` and `src/`; `tests/` contains component and integration tests.

## Runtime components

`Engine::App` coordinates the subsystems. On startup, `main()` initializes logging, constructs the app, and calls `App::init()`. Initialization opens/configures SQLite, creates the schema and prepared statements, loads indexed modification times into memory, initializes the query engine, and starts the Linux filesystem watcher. The UI calls into the app and query engine, but it does not implement crawling or search itself.

The `Engine::Config` defaults the scan root to `$HOME` and the database filename to `search_engine.db`. The database filename is relative, so it resolves against the process's working directory. The log file is stored under `$XDG_STATE_HOME/halo/halo.log`, falling back to `~/.local/state/halo/halo.log`.

## Initial crawl and indexing

When indexing starts, `App` runs these stages:

1. **Crawl.** `Crawler` walks the configured root recursively, skips inaccessible directories, and prunes blacklisted folders such as version-control, build, dependency, and cache directories. It accepts a broad whitelist of source-code and text extensions and filters common temporary/backup files. The crawl returns eligible files, candidate files with modification times, and traversed directories.
2. **Synchronize the filesystem catalog.** Each eligible file's name, extension, and path are upserted into `filesystem_index`. This catalog supports filename search independently of content indexing.
3. **Index changed content.** `IndexerPipeline` compares each candidate's modification time with the in-memory map loaded from SQLite. Unchanged files are skipped. Changed files are read, tokenized, and sent to a dedicated SQLite writer thread. Up to four worker threads handle file reads and tokenization; a single writer batches database work in transactions of up to 200 jobs.
4. **Watch directories.** After the initial pass, `FileWatcher` registers an `inotify` watch for each crawled directory so later filesystem events can update the catalog and content index.

For larger rebuilds (at least 1,000 changed candidates), the database temporarily drops the token lookup index while writing, then recreates/optimizes indexes after the batch. This reduces index maintenance overhead during bulk ingestion.

## Tokenization and content search

`Tokenizer` splits text into runs of letters, digits, and underscores, converts tokens to lowercase, and drops tokens of two characters or fewer as well as a built-in set of common programming-language terms. Each retained token is paired with its 1-based source line number. The same tokenizer is used for file contents and query text, so query terms follow the same normalization rules.

`QueryEngine::search_terms()` tokenizes the query and asks SQLite for lines that contain **every distinct query term**. Term order does not matter. Results are grouped by document and line, limited to 200, and ordered using a score that rewards matching terms and gives a path preference to selected common content directories while downranking hidden-directory paths. The result identifies the file path, line number, and score; it does not return a source-line excerpt.

Filename search is separate: `search_filename()` performs a case-insensitive SQLite `LIKE` substring lookup over cataloged file names. The user-provided text is escaped as a literal pattern, so `%` and `_` are treated as characters rather than wildcards.

## SQLite data model

The database currently contains four main tables:

| Table | Purpose |
| --- | --- |
| `filesystem_index` | File path, name, and extension used by filename search. |
| `documents` | Content-indexed file paths and their last modification times. |
| `tokens` | Unique normalized token strings. |
| `inverted_index` | A row for each token occurrence, linking token and document IDs to a line number. |

The inverted index lets a content query find documents/lines by token ID without rereading files. SQLite runs in WAL mode, with prepared statements and indexes for token and document lookups. A mutex protects database operations because crawling/indexing and query/watch work may overlap.

## Live updates

The watcher has one thread that reads `inotify` events and queues them, and another that processes the queue. New directories are added to the watch set. Eligible file create/modify events refresh the filesystem catalog and enqueue a content index job; temporary or unsupported files are removed from both indexes. Delete events remove the corresponding catalog/document rows and token postings. This keeps the index responsive between full startup crawls.

## Operational notes and current boundaries

- The watcher is Linux-specific because it uses `inotify`.
- A startup crawl refreshes/updates discovered catalog entries. The current crawl synchronization path upserts discovered entries; it does not reconcile every previously cataloged path that disappeared while the app was stopped.
- The token index stores token-to-line associations, not source text or token positions. Search therefore returns matching paths and line numbers, not snippets or phrase matches.
- The crawler's extension and directory filters are defined in `include/crawler/crawler.hpp`; adjust them there to change what is indexed.
- Database schema setup and query SQL are in `src/database/`; the overall startup and indexing sequence is coordinated by `src/engine/engineApp.cpp`.
