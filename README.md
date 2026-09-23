# ECE 309 — Project 2

This is my Project 2 submission: a small command-line "harness" that
runs a back-and-forth conversation between a user and a model. It
streams replies in, watches for a stop sentinel so the conversation
knows when to end, and can save/replay transcripts.

## Building it

```bash
cmake -S . -B build
cmake --build build
```

If you're on Windows and CMake tries to use MSVC instead of g++, force
MinGW instead:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Running it

```bash
./build/miniharness --script scripts/greeting.script
```

Type messages at the `you>` prompt, Ctrl-D to quit early. Add `--save
transcript.txt` if you want to save the conversation, or `--max-turns
N` to cap how long it goes.

## Running the tests

```bash
./build/test_p2
```

Should print 12 `[PASS]` lines and end with "All 12 tests passed."

## What I wrote vs. what was given

Everything in `include/model/`, `include/harness/`, and most of `src/`
(model_client, scripted_client, replay_client, harness, main) was
provided starter code. I wrote `Message`, `Conversation`,
`SentinelScanner`, the test suite, and the design log.
