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
#include <stdexcept>
#include <string>

const std::string kSentinel = "<|end_conversation|>";

// Fake stdin: returns the given lines, then EOF like Ctrl-D.
class FakeInput : public InputSource {
public:
    FakeInput(const std::string* lines, int count) : lines_(lines), count_(count) {}

    std::string read_line() override {
        if (idx_ < count_) {
            return lines_[idx_++];
        }
        eof_ = true;
        return "";
    }

    bool is_eof() const override { return eof_; }

private:
    const std::string* lines_;
    int count_;
    int idx_ = 0;
    bool eof_ = false;
};

// Fake stdout: saves everything the harness prints.
class CaptureOutput : public OutputSink {
public:
    void write(std::string_view text) override { text_ += text; }
    std::string text_;
};

void write_file(const std::string& path, const std::string& contents) {
    std::ofstream f(path);
    f << contents;
}

// Copy of save_transcript() from main.cpp, since that one isn't accessible here.
void save_transcript(const Conversation& conv, const std::string& path) {
    std::ofstream file(path);
    bool first = true;
    for (const Message* m = conv.begin(); m != conv.end(); ++m) {
        if (!first) file << "---\n";
        first = false;
        std::string role = "assistant";
        if (m->role() == Role::System) role = "system";
        if (m->role() == Role::User) role = "user";
        file << "role: " << role << "\n" << m->content() << "\n";
    }
}

Conversation make_conv(int n) {
    Conversation c;
    for (int i = 0; i < n; ++i) {
        c.append(Message(Role::User, "msg " + std::to_string(i)));
    }
    return c;
}

void test_message_default() {
    Message m;
    assert(m.role() == Role::System);
    assert(m.content() == "");

    Message u(Role::User, "hi");
    assert(u.role() == Role::User);
    assert(u.content() == "hi");
}

// Spec item 1: empty conversation.
void test_empty_conversation() {
    Conversation c;
    assert(c.size() == 0);
    assert(c.begin() == c.end());
    assert(c.begin() == nullptr);

    int count = 0;
    for (const Message& m : c) {
        (void)m;
        count++;
    }
    assert(count == 0);

    bool threw = false;
    try {
        c.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
}

// at() throws one past the end.
void test_at_past_end() {
    Conversation c = make_conv(3);
    assert(c.at(2).content() == "msg 2");

    bool threw = false;
    try {
        c.at(3);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
}

// Spec item 2: system message stays first, even through reallocations and the Harness.
void test_system_message_first() {
    Conversation c;
    c.append(Message(Role::System, "Be concise."));
    for (int i = 0; i < 100; ++i) {
        c.append(Message(Role::User, "u" + std::to_string(i)));
    }
    assert(c.size() == 101);
    assert(c.at(0).role() == Role::System);
    assert(c.at(0).content() == "Be concise.");
    for (std::size_t i = 1; i < c.size(); ++i) {
        assert(c.at(i).role() != Role::System);
    }

    write_file("test_tmp_sys.script",
               "role: assistant\nhi\n---\nrole: assistant\nbye" + kSentinel + "\n");
    HarnessConfig cfg;
    cfg.system_message = "You are a test.";
    Harness h(std::make_unique<ScriptedModelClient>("test_tmp_sys.script"), cfg);
    std::string lines[] = {"hello", "goodbye"};
    FakeInput in(lines, 2);
    CaptureOutput out;
    h.run(in, out);
    std::remove("test_tmp_sys.script");

    assert(h.conversation().at(0).role() == Role::System);
    assert(h.conversation().at(0).content() == "You are a test.");
    assert(h.conversation().at(1).role() == Role::User);
}

// Spec item 3: copies are deep.
void test_copy_is_deep() {
    Conversation a = make_conv(5);

    Conversation b(a);
    assert(b.size() == 5);
    assert(b.begin() != a.begin());
    for (std::size_t i = 0; i < a.size(); ++i) {
        assert(&b.at(i) != &a.at(i));
        assert(b.at(i).content() == a.at(i).content());
    }

    b.append(Message(Role::User, "only in b"));
    assert(a.size() == 5);
    assert(b.size() == 6);

    Conversation c = make_conv(2);
    c = a;
    assert(c.size() == 5);
    assert(c.begin() != a.begin());
    assert(c.at(4).content() == "msg 4");

    Conversation& same = c;
    c = same;
    assert(c.size() == 5);
    assert(c.at(0).content() == "msg 0");

    Conversation empty;
    Conversation empty_copy(empty);
    assert(empty_copy.size() == 0);
    assert(empty_copy.begin() == nullptr);
}

// Spec item 4: moves steal the pointer and leave the source empty but usable.
void test_move_steals() {
    Conversation a = make_conv(4);
    const Message* old_buf = a.begin();

    Conversation b(std::move(a));
    assert(b.begin() == old_buf);
    assert(b.size() == 4);
    assert(a.size() == 0);
    assert(a.begin() == nullptr);

    a.append(Message(Role::User, "reused"));
    assert(a.size() == 1);
    assert(a.at(0).content() == "reused");

    Conversation c = make_conv(7);
    const Message* b_buf = b.begin();
    c = std::move(b);
    assert(c.begin() == b_buf);
    assert(c.size() == 4);
    assert(b.size() == 0);
    assert(b.begin() == nullptr);
}

// Spec item 5: begin() changes on a reallocation, which with doubling only happens when size was 0 or a power of 2.
void test_growth_doubles() {
    Conversation c;
    int reallocations = 0;
    std::size_t next_growth = 0;  // capacity goes 1, 2, 4, 8, ...

    for (std::size_t n = 0; n < 1000; ++n) {
        const Message* before = c.begin();
        c.append(Message(Role::User, std::to_string(n)));
        assert(c.size() == n + 1);

        bool reallocated = (c.begin() != before);
        bool expected = (n == next_growth);
        assert(reallocated == expected);

        if (reallocated) {
            reallocations++;
            next_growth = (n == 0) ? 1 : n * 2;
            for (std::size_t i = 0; i <= n; ++i) {
                assert(c.at(i).content() == std::to_string(i));
            }
        }
    }
    assert(reallocations == 11);
}

// Spec item 6: clean text passes through unchanged.
void test_scanner_clean_text() {
    SentinelScanner s(kSentinel);
    std::string text = "Hello there, this reply has no stop marker at all.";
    auto out = s.feed(text);
    auto rest = s.flush();
    assert(!out.sentinel_found);
    assert(!rest.sentinel_found);
    assert(out.safe_text + rest.safe_text == text);

    SentinelScanner s2(kSentinel);
    auto o2 = s2.feed("hi");
    assert(o2.safe_text == "");
    assert(s2.flush().safe_text == "hi");

    SentinelScanner s3(kSentinel);
    auto o3 = s3.feed("Goodbye." + kSentinel);
    assert(o3.sentinel_found);
    assert(o3.safe_text == "Goodbye.");
}

// Spec item 7: the sample test from the spec.
void test_scanner_every_boundary() {
    const std::string text = "Goodbye." + kSentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(kSentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

// Spec item 7: every chunk size from 1 char up to the whole string.
void test_scanner_every_chunk_size() {
    std::string text = "Hi <|end <" + kSentinel + "trailing junk";
    for (std::size_t chunk = 1; chunk <= text.size(); ++chunk) {
        SentinelScanner s(kSentinel);
        std::string printed;
        bool found = false;
        for (std::size_t i = 0; i < text.size() && !found; i += chunk) {
            auto out = s.feed(text.substr(i, chunk));
            printed += out.safe_text;
            found = out.sentinel_found;
        }
        assert(found);
        assert(printed == "Hi <|end <");
    }
}

// Spec item 8: near-misses don't trigger and still get printed.
void test_scanner_no_false_alarms() {
    std::string near_misses[] = {
        "<|end_world|>",
        "<|end_conversation|",
        "|end_conversation|>",
        "<|END_CONVERSATION|>",
        "<|end_<|end_<|end_",
    };
    for (const std::string& text : near_misses) {
        SentinelScanner s(kSentinel);
        std::string printed;
        for (char ch : text) {
            auto out = s.feed(std::string(1, ch));
            assert(!out.sentinel_found);
            printed += out.safe_text;
        }
        auto rest = s.flush();
        assert(!rest.sentinel_found);
        assert(printed + rest.safe_text == text);
    }
}

// Spec item 9: 4 MB fed one byte at a time; bytes fed minus bytes emitted equals pending_.size().
void test_scanner_bounded_memory() {
    const std::size_t bound = kSentinel.size() - 1;
    const std::string pattern = "<|end_";
    const std::size_t total = 4 * 1024 * 1024;

    SentinelScanner s(kSentinel);
    std::size_t fed = 0;
    std::size_t emitted = 0;
    std::size_t max_held = 0;
    for (std::size_t i = 0; i < total; ++i) {
        auto out = s.feed(std::string(1, pattern[i % pattern.size()]));
        assert(!out.sentinel_found);
        fed++;
        emitted += out.safe_text.size();
        std::size_t held = fed - emitted;
        assert(held <= bound);
        if (held > max_held) max_held = held;
    }
    assert(max_held == bound);

    auto rest = s.flush();
    assert(rest.safe_text.size() == bound);
    assert(emitted + rest.safe_text.size() == total);

    SentinelScanner s2(kSentinel);
    std::string big(100000, 'x');
    auto out = s2.feed(big);
    assert(big.size() - out.safe_text.size() == bound);
}

// Spec item 10: stops with TurnLimit after max_turns.
void test_harness_turn_limit() {
    write_file("test_tmp_turns.script",
               "role: assistant\nr1\n---\nrole: assistant\nr2\n---\n"
               "role: assistant\nr3\n---\nrole: assistant\nr4\n");
    HarnessConfig cfg;
    cfg.max_turns = 2;
    Harness h(std::make_unique<ScriptedModelClient>("test_tmp_turns.script"), cfg);
    std::string lines[] = {"a", "b", "c", "d"};
    FakeInput in(lines, 4);
    CaptureOutput out;
    StopReason r = h.run(in, out);
    std::remove("test_tmp_turns.script");

    assert(r.kind == StopReason::Kind::TurnLimit);
    assert(h.conversation().size() == 4);
    assert(h.conversation().at(1).content() == "r1");
    assert(h.conversation().at(3).content() == "r2");
}

// Spec item 11: halts on the sentinel turn and never prints the sentinel.
void test_harness_sentinel_halt() {
    write_file("test_tmp_sentinel.script",
               "chunk: 3\nrole: assistant\nStill here.\n---\n"
               "chunk: 3\nrole: assistant\nGoodbye." + kSentinel + "IGNORED\n---\n"
               "role: assistant\nnever reached\n");
    Harness h(std::make_unique<ScriptedModelClient>("test_tmp_sentinel.script"), HarnessConfig{});
    std::string lines[] = {"hello", "bye", "extra"};
    FakeInput in(lines, 3);
    CaptureOutput out;
    StopReason r = h.run(in, out);
    std::remove("test_tmp_sentinel.script");

    assert(r.kind == StopReason::Kind::Sentinel);
    assert(h.conversation().size() == 4);
    assert(h.conversation().at(3).content() == "Goodbye." + kSentinel);
    assert(out.text_.find(kSentinel) == std::string::npos);
    assert(out.text_.find("<|") == std::string::npos);
    assert(out.text_.find("IGNORED") == std::string::npos);
    assert(out.text_.find("Goodbye.") != std::string::npos);
}

// EOF gives UserExit and running out of script gives ClientError.
void test_harness_eof_and_client_error() {
    write_file("test_tmp_eof.script", "role: assistant\nonly reply\n");

    Harness h1(std::make_unique<ScriptedModelClient>("test_tmp_eof.script"), HarnessConfig{});
    std::string lines1[] = {"one"};
    FakeInput in1(lines1, 1);
    CaptureOutput out1;
    StopReason r1 = h1.run(in1, out1);
    assert(r1.kind == StopReason::Kind::UserExit);
    assert(h1.conversation().size() == 2);

    Harness h2(std::make_unique<ScriptedModelClient>("test_tmp_eof.script"), HarnessConfig{});
    std::string lines2[] = {"one", "two"};
    FakeInput in2(lines2, 2);
    CaptureOutput out2;
    StopReason r2 = h2.run(in2, out2);
    assert(r2.kind == StopReason::Kind::ClientError);
    assert(h2.conversation().size() == 3);

    std::remove("test_tmp_eof.script");
}

// Spec item 12: save a session, replay it, and check both match.
void test_transcript_round_trip() {
    write_file("test_tmp_rt.script",
               "role: system\nBe concise.\n---\n"
               "chunk: 5\nrole: assistant\nI am doing well, thank you!\n---\n"
               "chunk: 4\nrole: assistant\nSure thing.\n---\n"
               "chunk: 6\nrole: assistant\nGoodbye!" + kSentinel + "\n");
    std::string lines[] = {"hello", "can you help?", "bye"};

    auto scripted = std::make_unique<ScriptedModelClient>("test_tmp_rt.script");
    HarnessConfig cfg1;
    cfg1.system_message = scripted->system_message();
    Harness original(std::move(scripted), cfg1);
    FakeInput in1(lines, 3);
    CaptureOutput out1;
    StopReason r1 = original.run(in1, out1);
    assert(r1.kind == StopReason::Kind::Sentinel);

    save_transcript(original.conversation(), "test_tmp_transcript.txt");

    auto replay = std::make_unique<ReplayModelClient>("test_tmp_transcript.txt");
    HarnessConfig cfg2;
    cfg2.system_message = replay->system_message();
    Harness replayed(std::move(replay), cfg2);
    FakeInput in2(lines, 3);
    CaptureOutput out2;
    StopReason r2 = replayed.run(in2, out2);
    std::remove("test_tmp_rt.script");
    std::remove("test_tmp_transcript.txt");

    assert(r2.kind == StopReason::Kind::Sentinel);
    const Conversation& a = original.conversation();
    const Conversation& b = replayed.conversation();
    assert(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        assert(a.at(i).role() == b.at(i).role());
        assert(a.at(i).content() == b.at(i).content());
    }
    assert(out1.text_ == out2.text_);
}

int main() {
    test_message_default();
    test_empty_conversation();
    test_at_past_end();
    test_system_message_first();
    test_copy_is_deep();
    test_move_steals();
    test_growth_doubles();
    test_scanner_clean_text();
    test_scanner_every_boundary();
    test_scanner_every_chunk_size();
    test_scanner_no_false_alarms();
    test_scanner_bounded_memory();
    test_harness_turn_limit();
    test_harness_sentinel_halt();
    test_harness_eof_and_client_error();
    test_transcript_round_trip();

    std::cout << "All 16 tests passed\n";
    return 0;
}
