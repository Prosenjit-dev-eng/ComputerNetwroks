// tests/test_nvt.cpp — Unit tests for NvtEncoder and NvtDecoder
#include "nvt.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

static int passed = 0;
static int failed = 0;
// TEST(name, expr) Macro: A custom, lightweight macro that evaluates a boolean condition. If the condition is true, it increments the passed counter and prints "PASS". If false, it increments the failed counter and prints "FAIL".
#define TEST(name, expr) do { \
    if (expr) { std::cout << "PASS: " name "\n"; ++passed; } \
    else { std::cout << "FAIL: " name "\n"; ++failed; } \
} while(0)

int main() {
    NvtEncoder enc;
    NvtDecoder dec;

    // --- NvtEncoder tests ---

    // Plain text passes through unchanged
    {
        auto out = enc.encode("hello");
        std::vector<uint8_t> expected{'h','e','l','l','o'};
        TEST("Encoder: plain text pass-through", out == expected);
    }

    // CR -> CR LF
    {
        auto out = enc.encode("\r");
        std::vector<uint8_t> expected{0x0D, 0x0A};
        TEST("Encoder: CR -> CRLF", out == expected);
    }

    // IAC (0xFF) -> IAC IAC
    {
        auto out = enc.encode(std::string(1, static_cast<char>(0xFF)));
        std::vector<uint8_t> expected{0xFF, 0xFF};
        TEST("Encoder: IAC -> IAC IAC", out == expected);
    }

    // encodeLine appends CRLF
    {
        auto out = enc.encodeLine("test");
        std::vector<uint8_t> expected{'t','e','s','t', 0x0D, 0x0A};
        TEST("Encoder: encodeLine appends CRLF", out == expected);
    }

    // Multiple chars including CR and IAC
    {
        std::string text = "a\rbc";
        text += static_cast<char>(0xFF);
        auto out = enc.encode(text);
        std::vector<uint8_t> expected{'a', 0x0D, 0x0A, 'b', 'c', 0xFF, 0xFF};
        TEST("Encoder: mixed CR and IAC", out == expected);
    }

    // --- NvtDecoder tests ---

    // Plain text pass-through
    {
        dec.reset();
        std::vector<uint8_t> raw{'h','i'};
        auto chunk = dec.decode(raw);
        TEST("Decoder: plain text pass-through", chunk.text == "hi");
    }

    // CR LF -> \n
    {
        dec.reset();
        std::vector<uint8_t> raw{0x0D, 0x0A};
        auto chunk = dec.decode(raw);
        TEST("Decoder: CRLF -> newline", chunk.text == "\n" && chunk.hadCR);
    }

    // CR NUL -> CR
    {
        dec.reset();
        std::vector<uint8_t> raw{0x0D, 0x00};
        auto chunk = dec.decode(raw);
        TEST("Decoder: CR NUL -> CR", chunk.text == "\r" && chunk.hadCR);
    }

    // Incremental decode — CR at end of first chunk, NUL at start of second
    {
        dec.reset();
        std::vector<uint8_t> chunk1{0x0D};
        std::vector<uint8_t> chunk2{0x00};
        auto r1 = dec.decode(chunk1);
        auto r2 = dec.decode(chunk2);
        // After first chunk: CR is pending, text may be empty
        // After second chunk: CR NUL -> CR emitted
        TEST("Decoder: incremental CR NUL across chunks",
             r1.text.empty() && r2.text == "\r" && r2.hadCR);
    }

    // Incremental decode — CR at end of first chunk, LF at start of second
    {
        dec.reset();
        std::vector<uint8_t> c1{'a', 0x0D};
        std::vector<uint8_t> c2{0x0A, 'b'};
        auto r1 = dec.decode(c1);
        auto r2 = dec.decode(c2);
        TEST("Decoder: incremental CRLF across chunks",
             r1.text == "a" && r2.text == "\nb");
    }

    // Multiple lines
    {
        dec.reset();
        std::vector<uint8_t> raw{'l','i','n','e','1', 0x0D, 0x0A, 'l','i','n','e','2'};
        auto chunk = dec.decode(raw);
        TEST("Decoder: multiple lines", chunk.text == "line1\nline2");
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
