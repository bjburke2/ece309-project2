# Design Log — Project 2

## Growth factor and amortized cost

Conversation starts at capacity 4 and doubles whenever append() is called on a full buffer (grow() in conversation.cpp). Doubling
keeps the total copying work across N appends bounded by a constant multiple of N, instead of growing with N itself.

**Proof.** Start at capacity C, doubling each time the buffer fills. After N appends, resizes occur at sizes C, 2C, 4C, ..., 2^k * C,
where 2^k * C is the first capacity >= N. Each resize of size s costs O(s) (every existing element is moved into the new buffer).
Total resize work is:


C + 2C + 4C + ... + 2^k C = C * (2^(k+1) - 1) < 2 * (2^k * C) < 2N


(the last step holds because 2^k * C < N by definition of k, so doubling it is still < 2N). Total copying work across all N
appends is therefore O(N), meaning each append is O(1) amortized — even though any single append that triggers a resize is O(current
size) in the worst case. A fixed additive growth scheme (e.g. capacity_ += 4 each time) would instead cost O(N^2) total, since
every append past the initial capacity would trigger a full copy of an ever-larger array.

test_growth_behavior checks this indirectly from outside the class (capacity_ is private, with no public accessor): it appends 200
messages one at a time and counts how many times begin()'s address changes. With doubling, that happens roughly log2(200) ≈ 8 times;
the test asserts fewer than 20 reallocations — impossible under a linear/additive growth scheme, which would reallocate closer to 200
times.

## Rule of Five evidence

Conversation is the only class holding a raw pointer, so it's the only one needing a hand-written Rule of Five. Message only owns a
std::string, so its compiler-generated copy/move operations are already correct and deep — no hand-written Rule of Five needed there.

- **Copy constructor / copy assignment** allocate an entirely new buffer and copy each Message across using Message's own copy
  assignment. test_rule_of_five_copy asserts copy.begin() != original.begin() (proving separate underlying arrays) and that
  appending to the copy doesn't change the original's size() — only possible if the two objects own independent memory.
- **Move constructor / move assignment** steal data_, size_, and capacity_ directly, then zero the source's fields. test_rule_of_
  five_move asserts the moved-to object's begin() pointer is identical to the original's pointer *before* the move (the buffer
  was reused, not copied), and that the source's size() is 0 and begin() is nullptr afterward, so its destructor (delete[]
  nullptr is a no-op) or a later reassignment is safe.
- Both assignment operators guard against self-assignment, and copy assignment builds the new buffer before freeing the old one, so a
  failed allocation never leaves *this half-destroyed.
- The entire suite, including repeated construct/destroy cycles inside test_harness_turn_limit and test_harness_sentinel_halt, was run
  under -fsanitize=address,undefined (the flags the provided CMakeLists.txt already sets) with zero leaks and no undefined
  behavior reported.

## Sentinel scanner: bounded pending_ proof

pending_ only ever holds the longest suffix of everything fed so far that is *also a prefix of the sentinel*. Anything longer would already
have matched the full sentinel (and been consumed as a match instead); anything shorter is released immediately as safe text. Since a proper
prefix of the sentinel can be at most sentinel_.size() - 1 characters long — a "prefix" the full length of the sentinel is just the whole
sentinel, which is caught by buffer.find(sentinel_) before the suffix logic ever runs — pending_.size() can never reach
sentinel_.size().

This is enforced directly in longest_matching_suffix(): its candidate length is capped at pattern.size() - 1 before any
comparison happens, so it is structurally impossible for it to return a length equal to the sentinel's full size. Each call to feed()
builds one temporary string (pending_ + chunk), uses it, and discards it — pending_ is *reassigned* to a short suffix of that
temporary each time, never appended to its previous value, so nothing accumulates across calls.

test_scanner_bounded_memory verifies this directly: it feeds 4 MB of adversarial input ("<|end_" repeated, since every 6-character chunk
looks like it could be starting the sentinel) one byte at a time, asserting pending_size() <= sentinel.size() - 1 after every byte. It
completes instantly, confirming the bound holds under sustained adversarial pressure, not just on a few hand-picked examples.

## What I would change differently

If I could redo it, I'd use a smarter way to check for the sentinel that remembers what it already matched instead of re-checking from scratch 
every time — it would run faster on tricky inputs. Right now it works fine, but it's not the most efficient way to do it.
