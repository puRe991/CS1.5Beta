#include "console.h"
#include "cvar.h"
#include "ui/ui.h"

#include <algorithm>
#include <sstream>

ConsoleCommand::ConsoleCommand(std::string name, Fn fn, std::string help)
    : name_(std::move(name)), help_(std::move(help)), fn_(std::move(fn)) {
    ConsoleCommandRegistry::Get().Register(this);
}

ConsoleCommandRegistry& ConsoleCommandRegistry::Get() {
    static ConsoleCommandRegistry instance;
    return instance;
}

void ConsoleCommandRegistry::Register(ConsoleCommand* cmd) {
    byName_[cmd->Name()] = cmd;
    order_.push_back(cmd);
}

ConsoleCommand* ConsoleCommandRegistry::Find(const std::string& name) {
    auto it = byName_.find(name);
    return it != byName_.end() ? it->second : nullptr;
}

namespace {

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

constexpr size_t kMaxLogLines = 200;

} // namespace

Console& Console::Get() {
    static Console instance;
    return instance;
}

void Console::Open() { open_ = true; }

void Console::Close() {
    open_ = false;
    historyPos_ = -1;
}

void Console::Toggle() {
    if (open_) Close(); else Open();
}

void Console::HandleTextInput(const std::string& text) {
    // '`' both toggles the console and (on some layouts) generates a
    // simultaneous SDL_TEXTINPUT event; never let it leak into the buffer.
    for (char c : text) {
        if (c == '`') continue;
        inputLine_.push_back(c);
    }
}

void Console::HandleBackspace() {
    if (!inputLine_.empty()) inputLine_.pop_back();
}

void Console::HandleHistory(int direction) {
    if (history_.empty()) return;
    if (direction < 0) {
        if (historyPos_ == -1) historyPos_ = (int)history_.size() - 1;
        else if (historyPos_ > 0) --historyPos_;
    } else {
        if (historyPos_ == -1) return;
        ++historyPos_;
        if (historyPos_ >= (int)history_.size()) {
            historyPos_ = -1;
            inputLine_.clear();
            return;
        }
    }
    if (historyPos_ >= 0 && historyPos_ < (int)history_.size()) inputLine_ = history_[historyPos_];
}

void Console::HandleEnter() {
    std::string line = inputLine_;
    inputLine_.clear();
    historyPos_ = -1;
    if (line.empty()) return;

    if (history_.empty() || history_.back() != line) history_.push_back(line);
    execute(line);
}

void Console::Print(const std::string& line) {
    log_.push_back(line);
    if (log_.size() > kMaxLogLines) log_.erase(log_.begin(), log_.begin() + (log_.size() - kMaxLogLines));
}

void Console::Clear() { log_.clear(); }

void Console::execute(const std::string& line) {
    Print("] " + line);

    std::vector<std::string> tokens = tokenize(line);
    if (tokens.empty()) return;

    const std::string& name = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    if (ConsoleCommand* cmd = ConsoleCommandRegistry::Get().Find(name)) {
        cmd->Invoke(args);
        return;
    }

    if (Cvar* cvar = CvarSystem::Get().Find(name)) {
        if (args.empty()) {
            Print(cvar->Name() + " = \"" + cvar->AsString() + "\"" + (cvar->Help().empty() ? "" : "  (" + cvar->Help() + ")"));
        } else {
            cvar->SetString(args[0]);
            Print(cvar->Name() + " = \"" + cvar->AsString() + "\"");
        }
        return;
    }

    Print("unknown command: " + name);
}

void Console::Draw(int screenW, int screenH) {
    if (!open_) return;

    constexpr float kHeightFrac = 0.5f;
    float consoleH = screenH * kHeightFrac;
    constexpr float kLineH = 16.0f;
    constexpr float kTextScale = 1.4f;
    constexpr float kPadding = 8.0f;

    uiDrawRect(0, 0, (float)screenW, consoleH, Color{0.05f, 0.05f, 0.08f, 0.88f});
    uiDrawRect(0, consoleH, (float)screenW, 2, Color{0.4f, 0.4f, 0.5f, 1.0f});

    // Scrollback: newest line just above the input line, growing upward.
    float y = consoleH - kPadding - kLineH * 2; // leave room for input line
    for (auto it = log_.rbegin(); it != log_.rend() && y > -kLineH; ++it) {
        uiDrawText(kPadding, y, *it, kColorWhite, kTextScale);
        y -= kLineH;
    }

    float inputY = consoleH - kPadding - kLineH;
    uiDrawRect(0, inputY - 2, (float)screenW, kLineH + 4, Color{0.1f, 0.1f, 0.14f, 0.9f});
    std::string prompt = "] " + inputLine_ + "_";
    uiDrawText(kPadding, inputY, prompt, Color{0.6f, 1.0f, 0.6f, 1.0f}, kTextScale);
}

// --- Built-in commands ---
namespace {

ConsoleCommand cmd_quit("quit", [](const std::vector<std::string>&) {
    Console::Get().RequestQuit();
}, "exits the game");

ConsoleCommand cmd_clear("clear", [](const std::vector<std::string>&) {
    Console::Get().Clear();
}, "clears the console scrollback");

ConsoleCommand cmd_echo("echo", [](const std::vector<std::string>& args) {
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) out += ' ';
        out += args[i];
    }
    Console::Get().Print(out);
}, "echo <text>: prints text to the console");

ConsoleCommand cmd_exec("exec", [](const std::vector<std::string>& args) {
    if (args.empty()) {
        Console::Get().Print("usage: exec <file>");
        return;
    }
    if (CvarSystem::Get().LoadConfig(args[0])) {
        Console::Get().Print("executed " + args[0]);
    } else {
        Console::Get().Print("couldn't exec " + args[0]);
    }
}, "exec <file>: runs a config file's cvar settings");

ConsoleCommand cmd_help("help", [](const std::vector<std::string>&) {
    Console::Get().Print("commands:");
    for (const ConsoleCommand* c : ConsoleCommandRegistry::Get().All()) {
        Console::Get().Print("  " + c->Name() + (c->Help().empty() ? "" : " - " + c->Help()));
    }
    Console::Get().Print("cvars (type name to view, 'name value' to set):");
    for (const Cvar* c : CvarSystem::Get().All()) {
        Console::Get().Print("  " + c->Name() + (c->Help().empty() ? "" : " - " + c->Help()));
    }
}, "lists commands and cvars");

} // namespace
