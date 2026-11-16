#include "terminal_session.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

TerminalSession::TerminalSession() {
    try {
        workingDir_ = std::filesystem::current_path().string();
    } catch (...) {
        char buf[4096] = {};
        if (::getcwd(buf, sizeof(buf))) workingDir_ = buf;
        else workingDir_ = "/";
    }
}

std::string TerminalSession::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string TerminalSession::processCommand(const std::string& line) {
    std::string trimmed = trim(line);
    if (trimmed.empty()) return "";

    // Split into command + args on first whitespace
    std::string cmd, args;
    size_t sp = trimmed.find_first_of(" \t");
    if (sp == std::string::npos) {
        cmd = trimmed;
    } else {
        cmd  = trimmed.substr(0, sp);
        args = trim(trimmed.substr(sp + 1));
    }

    // Lowercase command
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "help")  return executeHelp();
    if (cmd == "echo")  return executeEcho(args);
    if (cmd == "date")  return executeDate();
    if (cmd == "pwd")   return executePwd();
    if (cmd == "clear") return executeClear();
    if (cmd == "exit" || cmd == "quit") return executeExit();

    return "Unknown command: " + cmd + "\r\nType 'help' for available commands.\r\n";
}

std::string TerminalSession::executeHelp() {
    return
        "Available commands:\r\n"
        "  help          - Show this help message\r\n"
        "  echo <args>   - Print arguments back\r\n"
        "  date          - Show current date and time\r\n"
        "  pwd           - Print working directory\r\n"
        "  clear         - Clear the terminal screen\r\n"
        "  exit          - Close the session\r\n";
}

std::string TerminalSession::executeEcho(const std::string& args) {
    return args + "\r\n";
}

std::string TerminalSession::executeDate() {
    std::time_t now = std::time(nullptr);
    char buf[64] = {};
    struct tm* tm_info = std::localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buf) + "\r\n";
}

std::string TerminalSession::executePwd() {
    try {
        return std::filesystem::current_path().string() + "\r\n";
    } catch (...) {
        char buf[4096] = {};
        if (::getcwd(buf, sizeof(buf))) return std::string(buf) + "\r\n";
        return workingDir_ + "\r\n";
    }
}

std::string TerminalSession::executeClear() {
    return "\033[2J\033[H";
}

std::string TerminalSession::executeExit() {
    exitRequested_ = true;
    return "Goodbye!\r\n";
}
