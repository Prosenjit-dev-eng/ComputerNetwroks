// tests/test_session.cpp — Unit tests for TerminalSession
#include "terminal_session.hpp"
#include <iostream>
#include <cassert>
#include <string>

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { std::cout << "PASS: " name "\n"; ++passed; } \
    else { std::cout << "FAIL: " name "\n"; ++failed; } \
} while(0)

int main() {
    // --- help ---
    {
        TerminalSession s;
        auto out = s.processCommand("help");
        TEST("help returns non-empty string", !out.empty());
        TEST("help mentions 'help' keyword", out.find("help") != std::string::npos);
    }

    // --- echo ---
    {
        TerminalSession s;
        auto out = s.processCommand("echo Hello World");
        TEST("echo returns args", out.find("Hello World") != std::string::npos);
    }

    {
        TerminalSession s;
        auto out = s.processCommand("echo");
        TEST("echo with no args returns empty line", out == "\r\n");
    }

    // --- date ---
    {
        TerminalSession s;
        auto out = s.processCommand("date");
        TEST("date returns non-empty string", !out.empty());
        bool hasDigit = false;
        for (char c : out) if (std::isdigit(c)) { hasDigit = true; break; }
        TEST("date output contains digits", hasDigit);
    }

    // --- pwd ---
    {
        TerminalSession s;
        auto out = s.processCommand("pwd");
        TEST("pwd returns non-empty string", !out.empty());
        // Path separator can be '/' on Unix or '\' on Windows
        bool hasSep = (out.find('/') != std::string::npos) || (out.find('\\') != std::string::npos);
        TEST("pwd output contains path separator", hasSep);
    }

    // --- clear ---
    {
        TerminalSession s;
        auto out = s.processCommand("clear");
        bool ok = out.size() >= 4 &&
                  out[0] == '\033' && out[1] == '[' &&
                  out[2] == '2'   && out[3] == 'J';
        TEST("clear returns ANSI escape \\033[2J...", ok);
    }

    // --- exit ---
    {
        TerminalSession s;
        TEST("shouldExit() initially false", !s.shouldExit());
        auto out = s.processCommand("exit");
        TEST("exit sets shouldExit() true", s.shouldExit());
        TEST("exit returns 'Goodbye'", out.find("Goodbye") != std::string::npos);
    }

    // --- Unknown command ---
    {
        TerminalSession s;
        auto out = s.processCommand("nonexistentcommand");
        TEST("unknown command returns error message", !out.empty());
        TEST("unknown command mentions 'Unknown'", out.find("Unknown") != std::string::npos);
    }

    // --- Case-insensitivity ---
    {
        TerminalSession s;
        auto out = s.processCommand("HELP");
        TEST("commands are case-insensitive (HELP)", out.find("Available commands") != std::string::npos);
    }

    // --- Trimming ---
    {
        TerminalSession s;
        auto out = s.processCommand("   help   \r\n");
        TEST("whitespace trimmed around command", !out.empty());
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
