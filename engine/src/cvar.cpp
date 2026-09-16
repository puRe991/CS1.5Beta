#include "cvar.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

Cvar::Cvar(std::string name, std::string defaultValue, unsigned flags, std::string help)
    : name_(std::move(name)), help_(std::move(help)), flags_(flags), value_(std::move(defaultValue)) {
    syncFloatCache();
    CvarSystem::Get().Register(this);
}

void Cvar::syncFloatCache() {
    char* end = nullptr;
    float v = std::strtof(value_.c_str(), &end);
    floatValue_ = (end != value_.c_str()) ? v : 0.0f;
}

void Cvar::SetString(const std::string& v) {
    value_ = v;
    syncFloatCache();
}

void Cvar::SetFloat(float v) {
    std::ostringstream oss;
    oss << v;
    value_ = oss.str();
    floatValue_ = v;
}

CvarSystem& CvarSystem::Get() {
    static CvarSystem instance;
    return instance;
}

void CvarSystem::Register(Cvar* cvar) {
    byName_[cvar->Name()] = cvar;
    order_.push_back(cvar);
}

Cvar* CvarSystem::Find(const std::string& name) {
    auto it = byName_.find(name);
    return it != byName_.end() ? it->second : nullptr;
}

bool CvarSystem::Set(const std::string& name, const std::string& value) {
    Cvar* cvar = Find(name);
    if (!cvar) return false;
    cvar->SetString(value);
    return true;
}

namespace {

std::string trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

// Splits a config line into (name, value), honoring a leading/trailing pair
// of double quotes around the value (e.g. sensitivity "3.5"). Strips '//'
// line comments first.
bool parseLine(const std::string& rawLine, std::string& outName, std::string& outValue) {
    std::string line = rawLine;
    if (size_t commentAt = line.find("//"); commentAt != std::string::npos) {
        line = line.substr(0, commentAt);
    }
    line = trim(line);
    if (line.empty()) return false;

    size_t split = line.find_first_of(" \t");
    if (split == std::string::npos) return false;

    outName = trim(line.substr(0, split));
    outValue = trim(line.substr(split + 1));
    if (outValue.size() >= 2 && outValue.front() == '"' && outValue.back() == '"') {
        outValue = outValue.substr(1, outValue.size() - 2);
    }
    return !outName.empty();
}

} // namespace

bool CvarSystem::LoadConfig(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line, name, value;
    while (std::getline(in, line)) {
        if (parseLine(line, name, value)) {
            Set(name, value); // silently ignores unknown names
        }
    }
    return true;
}

bool CvarSystem::SaveConfig(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "// auto-generated config file -- edit while the game is closed\n";
    for (const Cvar* cvar : order_) {
        if (!(cvar->Flags() & CVAR_ARCHIVE)) continue;
        out << cvar->Name() << " \"" << cvar->AsString() << "\"\n";
    }
    return true;
}
