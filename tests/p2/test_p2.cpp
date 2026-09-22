// tests/p2/test_p2.cpp
//
// YOUR test suite goes here. At least 12 assert-based test cases — see
// spec §5 for the required categories and the sample test for the
// expected level of rigor.
//
// This file is a stub so the project builds out of the box; replace the
// body of main() with your own tests.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

class VectorInputSource : public InputSource {
public:
    explicit VectorInputSource(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::string read_line() override {
        if (index_ >= lines_.size()) {
            eof_ = true;
            return "";
        }
        return lines_[index_++];
    }

    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> lines_;
    std::size_t              index_ = 0;
    bool                     eof_   = false;
};

class StringOutputSink : public OutputSink {
public:
    void write(std::string_view text) override { captured_.append(text); }
    const std::string& text() const noexcept { return captured_; }

private:
    std::string captured_;
};

// Writes a temporary .script or transcript file
void write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
}

// 1. Empty Conversation Bounds
void test_empty_conversation_bounds() {
    Conversation conv;
    assert(conv.size() == 0);
    assert(conv.begin() == conv.end());

    bool threw = false;
    try {
        conv.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
}


// 2. System Message Ordering
void test_system_message_ordering() {
    const std::string script_path = "test2_ordering.script";
    write_file(script_path,
        "role: system\nBe concise.\n---\n"
        "role: assistant\nReply 1\n---\n"
        "role: assistant\nReply 2\n");

    auto scripted = std::make_unique<ScriptedModelClient>(script_path);
    HarnessConfig cfg;
    cfg.max_turns       = 2;
    cfg.system_message  = scripted->system_message();
    Harness harness(std::move(scripted), cfg);

    VectorInputSource in({"hi", "again"});
    StringOutputSink  out;
    harness.run(in, out);

    const Conversation& conv = harness.conversation();
    assert(conv.size() == 5); 
    assert(conv.at(0).role() == Role::System);
    assert(conv.at(0).content() == "Be concise.");
    assert(conv.at(1).role() == Role::User); 

    std::remove(script_path.c_str());
}


// 3. Rule of Five (Copy)
void test_rule_of_five_copy() {
    Conversation original;
    original.append(Message(Role::User, "a"));
    original.append(Message(Role::User, "b"));

    Conversation copy(original);
    assert(copy.size() == original.size());
    assert(copy.begin() != original.begin()); 
    assert(copy.at(0).content() == "a");
    assert(copy.at(1).content() == "b");

    copy.append(Message(Role::User, "c"));
    assert(original.size() == 2);
    assert(copy.size() == 3);
}


// 4. Rule of Five (Move)
void test_rule_of_five_move() {
    Conversation original;
    original.append(Message(Role::User, "x"));
    const Message* original_ptr = original.begin();

    Conversation moved(std::move(original));
    assert(moved.begin() == original_ptr); 
    assert(moved.size() == 1);

    assert(original.size() == 0);          
    assert(original.begin() == nullptr);

    Conversation target;
    target.append(Message(Role::User, "placeholder"));
    target = std::move(moved);
    assert(target.size() == 1);
    assert(target.at(0).content() == "x");
    assert(moved.size() == 0);
}


// 5. Growth behavior
void test_growth_behavior() {
    Conversation conv;
    const Message* last_ptr        = conv.begin();
    int            reallocations   = 0;
    const int      n               = 200;

    for (int i = 0; i < n; ++i) {
        conv.append(Message(Role::User, std::to_string(i)));

        assert(conv.size() == static_cast<std::size_t>(i + 1));
        for (int j = 0; j <= i; ++j) {
            assert(conv.at(static_cast<std::size_t>(j)).content() == std::to_string(j));
        }

        if (conv.begin() != last_ptr) {
            ++reallocations;
            last_ptr = conv.begin();
        }
    }

    assert(reallocations < 20);
}


// 6. Scanner (Clean Text)
void test_scanner_clean_text() {
    SentinelScanner scanner("<|end_conversation|>");
    std::string     text = "Hello, world! Nothing special here.";

    auto result  = scanner.feed(text);
    auto flushed = scanner.flush();

    assert(!result.sentinel_found);
    assert(result.safe_text + flushed.safe_text == text);
}


// 7. Scanner (Split Sentinel) -- every possible boundary
void test_scanner_split_sentinel() {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text     = "Goodbye." + sentinel;

    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(sentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));

        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}


// 8. Scanner (False Alarms)
void test_scanner_false_alarms() {
    SentinelScanner scanner("<|end_conversation|>");
    std::string     text = "This has <|end_world|> in it but not the real sentinel.";

    auto result  = scanner.feed(text);
    auto flushed = scanner.flush();

    assert(!result.sentinel_found);
    assert(result.safe_text + flushed.safe_text == text); // nothing dropped
}


// 9. Scanner (Bounded Memory)
void test_scanner_bounded_memory() {
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner   scanner(sentinel);

    const std::string  unit      = "<|end_"; 
    std::size_t         total_fed = 0;
    const std::size_t   target    = 4 * 1024 * 1024; 

    while (total_fed < target) {
        for (char c : unit) {
            scanner.feed(std::string_view(&c, 1));
            ++total_fed;
            assert(scanner.pending_size() <= sentinel.size() - 1);
            if (total_fed >= target) break;
        }
    }
}


// 10. Harness (Turn Limit)
void test_harness_turn_limit() {
    const std::string script_path = "test10_turn_limit.script";
    write_file(script_path,
        "role: assistant\nok 1\n---\n"
        "role: assistant\nok 2\n---\n"
        "role: assistant\nok 3\n");

    HarnessConfig cfg;
    cfg.max_turns = 3;
    Harness harness(std::make_unique<ScriptedModelClient>(script_path), cfg);

    VectorInputSource in({"a", "b", "c", "d", "e"}); 
    StringOutputSink  out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(harness.conversation().size() == 6); 

    std::remove(script_path.c_str());
}


// 11. Harness (Sentinel Halt)
void test_harness_sentinel_halt() {
    const std::string script_path = "test11_sentinel_halt.script";
    write_file(script_path,
        "role: assistant\nHi there.\n---\n"
        "chunk: 5\nrole: assistant\nGoodbye.<|end_conversation|>\n");

    HarnessConfig cfg;
    cfg.max_turns = 10;
    Harness harness(std::make_unique<ScriptedModelClient>(script_path), cfg);

    VectorInputSource in({"hello", "bye"});
    StringOutputSink  out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::Sentinel);
    assert(out.text().find("<|end_conversation|>") == std::string::npos); 
    assert(harness.conversation().size() == 4);                           
    assert(harness.conversation().at(3).content() == "Goodbye.<|end_conversation|>");

    std::remove(script_path.c_str());
}


// 12. Transcript Round-Trip
void test_transcript_round_trip() {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    conv.append(Message(Role::User, "hello"));
    conv.append(Message(Role::Assistant, "Hi! What can I do for you today?"));
    conv.append(Message(Role::User, "goodbye"));
    conv.append(Message(Role::Assistant, "Goodbye.<|end_conversation|>"));

    const std::string transcript_path = "test12_roundtrip.txt";
    {
        std::ofstream file(transcript_path);
        bool          first = true;
        for (const Message* m = conv.begin(); m != conv.end(); ++m) {
            if (!first) file << "---\n";
            first = false;
            const char* role_str = (m->role() == Role::System)   ? "system"
                                  : (m->role() == Role::User)     ? "user"
                                                                   : "assistant";
            file << "role: " << role_str << "\n";
            file << m->content() << "\n";
        }
    }

    ReplayModelClient replay(transcript_path);
    assert(replay.system_message() == "Be concise.");

    Conversation dummy; 
    Message      reply1 = replay.generate(dummy); 
    assert(reply1.content() == "Hi! What can I do for you today?");

    Message reply2 = replay.generate(dummy);
    assert(reply2.content() == "Goodbye.<|end_conversation|>");

    std::remove(transcript_path.c_str());
}

}  // namespace

int main() {
    struct NamedTest {
        const char* name;
        void (*fn)();
    };
    NamedTest tests[] = {
        {"empty_conversation_bounds", test_empty_conversation_bounds},
        {"system_message_ordering",   test_system_message_ordering},
        {"rule_of_five_copy",         test_rule_of_five_copy},
        {"rule_of_five_move",         test_rule_of_five_move},
        {"growth_behavior",           test_growth_behavior},
        {"scanner_clean_text",        test_scanner_clean_text},
        {"scanner_split_sentinel",    test_scanner_split_sentinel},
        {"scanner_false_alarms",      test_scanner_false_alarms},
        {"scanner_bounded_memory",    test_scanner_bounded_memory},
        {"harness_turn_limit",        test_harness_turn_limit},
        {"harness_sentinel_halt",     test_harness_sentinel_halt},
        {"transcript_round_trip",     test_transcript_round_trip},
    };

    for (const NamedTest& t : tests) {
        t.fn();
        std::cout << "[PASS] " << t.name << "\n";
    }
    std::cout << "All " << (sizeof(tests) / sizeof(tests[0])) << " tests passed.\n";
    return 0;
}
