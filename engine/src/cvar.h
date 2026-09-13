#pragma once

// Minimal GoldSrc-style config/cvar system: named, typed variables that can
// be declared anywhere (as static globals, HL-engine style) and are then
// loadable/saveable from a simple "name value" text config file.
//
// Usage:
//   static Cvar sensitivity("sensitivity", "3.0", CVAR_ARCHIVE, "mouse sensitivity");
//   ...
//   float s = sensitivity.AsFloat();
//   CvarSystem::Get().LoadConfig("config.cfg");   // once at startup
//   CvarSystem::Get().SaveConfig("config.cfg");   // once at shutdown

#include <string>
#include <unordered_map>
#include <vector>

enum CvarFlags : unsigned {
    CVAR_NONE = 0,
    CVAR_ARCHIVE = 1 << 0, // persisted to the config file
    CVAR_CHEAT = 1 << 1,   // informational only; nothing enforces it yet
};

class Cvar {
public:
    Cvar(std::string name, std::string defaultValue, unsigned flags = CVAR_NONE, std::string help = "");

    const std::string& Name() const { return name_; }
    const std::string& Help() const { return help_; }
    unsigned Flags() const { return flags_; }

    const std::string& AsString() const { return value_; }
    float AsFloat() const { return floatValue_; }
    int AsInt() const { return (int)floatValue_; }
    bool AsBool() const { return floatValue_ != 0.0f; }

    void SetString(const std::string& v);
    void SetFloat(float v);

private:
    std::string name_;
    std::string help_;
    unsigned flags_;
    std::string value_;
    float floatValue_ = 0.0f; // parsed cache of value_, 0 if non-numeric

    void syncFloatCache();
};

// Global registry. Cvars register themselves here on construction, so any
// statically-constructed Cvar anywhere in the program is automatically
// findable/settable/persistable without extra wiring.
class CvarSystem {
public:
    static CvarSystem& Get();

    void Register(Cvar* cvar);
    Cvar* Find(const std::string& name);

    // Sets a cvar by name if it exists. Returns false if no such cvar is registered.
    bool Set(const std::string& name, const std::string& value);

    // Parses a text config file of "name value" lines (quotes optional around
    // value, '//' starts a line comment, blank lines ignored) and applies
    // each line via Set(). Unknown cvar names are ignored. Returns false if
    // the file could not be opened.
    bool LoadConfig(const std::string& path);

    // Writes every CVAR_ARCHIVE cvar as a `name "value"` line. Overwrites the
    // file at path. Returns false if the file could not be written.
    bool SaveConfig(const std::string& path) const;

    const std::vector<Cvar*>& All() const { return order_; }

private:
    std::unordered_map<std::string, Cvar*> byName_;
    std::vector<Cvar*> order_; // preserves registration order for SaveConfig output
};
