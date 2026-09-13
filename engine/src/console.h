#pragma once

// In-engine developer console (Quake/GoldSrc-style): a dropdown text overlay
// bound to the ` (backquote) key. Typed lines are dispatched to either a
// registered ConsoleCommand or, if no command matches, treated as a cvar
// get/set against CvarSystem (see cvar.h) — so any Cvar declared anywhere in
// the engine is automatically console-settable with no extra wiring.
//
// Usage:
//   static ConsoleCommand cmd_foo("foo", [](const std::vector<std::string>& args) {
//       Console::Get().Print("did foo");
//   }, "does foo");
//   ...
//   Console::Get().Draw(screenW, screenH); // call within a ui frame

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class ConsoleCommand {
public:
    using Fn = std::function<void(const std::vector<std::string>& args)>;

    ConsoleCommand(std::string name, Fn fn, std::string help = "");

    const std::string& Name() const { return name_; }
    const std::string& Help() const { return help_; }
    void Invoke(const std::vector<std::string>& args) const { fn_(args); }

private:
    std::string name_;
    std::string help_;
    Fn fn_;
};

// Global registry, mirroring CvarSystem: commands register themselves on
// construction so any statically-constructed ConsoleCommand is automatically
// findable from the console.
class ConsoleCommandRegistry {
public:
    static ConsoleCommandRegistry& Get();

    void Register(ConsoleCommand* cmd);
    ConsoleCommand* Find(const std::string& name);
    const std::vector<ConsoleCommand*>& All() const { return order_; }

private:
    std::unordered_map<std::string, ConsoleCommand*> byName_;
    std::vector<ConsoleCommand*> order_;
};

class Console {
public:
    static Console& Get();

    bool IsOpen() const { return open_; }
    void Open();
    void Close();
    void Toggle();

    // Input plumbing: call from the SDL event loop while IsOpen().
    void HandleTextInput(const std::string& text);
    void HandleBackspace();
    void HandleEnter();
    void HandleHistory(int direction); // -1 = older, +1 = newer

    // Appends a line to the console's scrollback (also visible in-game, not
    // just while the console is open).
    void Print(const std::string& line);
    void Clear();

    // Draws the dropdown overlay + scrollback + input line. No-op if closed.
    // Must be called between uiBeginFrame()/uiEndFrame().
    void Draw(int screenW, int screenH);

    // Set once by the built-in "quit" command; the main loop should check
    // this each frame and exit if true.
    bool WantsQuit() const { return wantsQuit_; }
    void RequestQuit() { wantsQuit_ = true; }

private:
    Console() = default;
    void execute(const std::string& line);

    bool open_ = false;
    bool wantsQuit_ = false;
    std::string inputLine_;
    std::vector<std::string> log_;
    std::vector<std::string> history_;
    int historyPos_ = -1; // -1 = not browsing history
};
