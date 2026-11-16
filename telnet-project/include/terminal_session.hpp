#pragma once
#include <string>

// A restricted command interpreter for a single Telnet session.
// NEVER uses system() or popen() with client input.
class TerminalSession {
public:
    TerminalSession();

    // Process a single line of input from the client. Returns response text.
    std::string processCommand(const std::string& line);

    bool shouldExit() const { return exitRequested_; }

private:
    bool        exitRequested_ = false;
    std::string workingDir_;

    std::string executeHelp();
    std::string executeEcho(const std::string& args);
    std::string executeDate();
    std::string executePwd();
    std::string executeClear();
    std::string executeExit();

    static std::string trim(const std::string& s);
};
