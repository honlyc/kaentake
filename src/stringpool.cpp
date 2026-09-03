#include "pch.h"
#include "hook.h"
#include "ztl/ztl.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

#define REPLACE_STRING(INDEX, NEW_STRING) \
    do { \
        static char sEncoded[GetLength(NEW_STRING) + 2]; \
        EncodeString(INDEX, NEW_STRING, sEncoded); \
    } while (0)


class StringPool {
public:
    inline static auto ms_aKey = reinterpret_cast<const unsigned char*>(0x00B001EC);
    inline static auto ms_aString = reinterpret_cast<const char**>(0x00BDC9D4);

    class Key {
    public:
        ZArray<unsigned char> m_aKey;
        Key(const unsigned char* pKey, unsigned int nKeySize, unsigned int nSeed) {
            reinterpret_cast<void(__thiscall*)(Key*, const unsigned char*, unsigned int, unsigned int)>(0x0079E780)(this, pKey, nKeySize, nSeed);
        }
    };
    static_assert(sizeof(Key) == 0x4);
};

constexpr size_t GetLength(const char* s) {
    size_t n = 0;
    while (s[n]) {
        ++n;
    }
    return n;
}

void EncodeString(int nIdx, const char* sSource, char* sDestination) {
    StringPool::Key keygen(StringPool::ms_aKey, 0x10, 0);
    size_t n = strlen(sSource);
    for (size_t i = 0; i < n; ++i) {
        unsigned char key = keygen.m_aKey[i % 0x10];
        sDestination[i + 1] = sSource[i] ^ key;
        if (static_cast<uint8_t>(sSource[i]) == static_cast<uint8_t>(key)) {
            sDestination[i + 1] = key;
        }
    }
    sDestination[0] = 0;
    sDestination[n + 1] = 0;
    StringPool::ms_aString[nIdx] = sDestination;
}

// Persistent encoded buffers to ensure pointers remain valid for the process lifetime.
static std::vector<std::vector<char>> g_encodedBuffers;

static void ReplaceStringRuntime(int nIdx, const std::string& sSource) {
    if (nIdx < 0) return;
    if (g_encodedBuffers.size() <= (size_t)nIdx) {
        g_encodedBuffers.resize(nIdx + 1);
    }
    // Allocate n+2 bytes (EncodeString writes n+2 bytes)
    g_encodedBuffers[nIdx].assign(sSource.size() + 2, 0);
    EncodeString(nIdx, sSource.c_str(), g_encodedBuffers[nIdx].data());
}

// trim helpers
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}
static inline void trim(std::string &s) { ltrim(s); rtrim(s); }

// Process C-style escape sequences in a string read from file.
// Supports: \n \r \t \\ \0
static std::string UnescapeString(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n':  result += '\n'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case '0':  result += '\0'; ++i; break;
                default:   result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

// Simple translations loader. Format: each non-comment line is "index=translation"
// File must be saved as UTF-8. If the client expects another encoding, add conversion here.
static void LoadTranslationsFromFile(const char* path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        LOG_DEBUG("LoadTranslationsFromFile: failed to open %s", path);
        return;
    }
    std::string line;
    unsigned int lineNo = 0;
    while (std::getline(ifs, line)) {
        ++lineNo;
        if (line.empty()) continue;
        // remove UTF-8 BOM if present (0xEF,0xBB,0xBF)
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
            line.erase(0, 3);
        }
        // comment
        if (line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        trim(key);
        // allow empty translation value (will set empty string)
        // ignore invalid keys
        if (key.empty()) continue;
        try {
            int idx = std::stoi(key);
            std::string unescaped = UnescapeString(val);
            ReplaceStringRuntime(idx, unescaped);
        } catch (...) {
            LOG_DEBUG("LoadTranslationsFromFile: invalid key on line %u: %s", lineNo, key.c_str());
            continue;
        }
    }
    LOG_DEBUG("LoadTranslationsFromFile: finished loading %s", path);
}


void AttachStringPoolMod() {
    // Keep existing static replacements if desired
    REPLACE_STRING(1163, "Kaentake");

    // Try load translations from translations/zh_CN.txt relative to working directory
    LoadTranslationsFromFile("translations/zh_CN.txt");
}
