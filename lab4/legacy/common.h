#pragma once
#include "lab2.h"

#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <random>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// DATA STRUCTURES

// CONSOLE AND UTF-8 (locale-independent: custom u32string helpers)

void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

u32string utf8ToU32(const string& s) {
    u32string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        int len;
        if (c < 0x80)              { cp = c;        len = 1; }
        else if ((c >> 5) == 0x6)  { cp = c & 0x1F; len = 2; }
        else if ((c >> 4) == 0xE)  { cp = c & 0x0F; len = 3; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; len = 4; }
        else { ++i; continue; }                      // stray byte
        if (i + len > s.size()) break;
        bool ok = true;
        for (int k = 1; k < len; ++k) {
            unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc >> 6) != 0x2) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { ++i; continue; }
        out.push_back(cp);
        i += len;
    }
    return out;
}

string u32ToUtf8(const u32string& u) {
    string out;
    out.reserve(u.size() * 2);
    for (char32_t cp : u) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

bool isLetterCp(char32_t c) {
    return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
           (c >= 0x410 && c <= 0x44F) || c == 0x401 || c == 0x451;   // A-ya, Yo (upper and lower case)
}

char32_t toUpperCp(char32_t c) {
    if (c >= U'a' && c <= U'z') return c - 32;
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    if (c == 0x451) return 0x401;
    return c;
}

char32_t toLowerCp(char32_t c) {
    if (c >= U'A' && c <= U'Z') return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c == 0x401) return 0x451;
    return c;
}

string trim(const string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// FILE I/O

// Reads the whole file into a string. Returns false if the file cannot be opened
inline bool readTextFile(const string& path, string& out) {
    ifstream f(path, ios::binary);
    if (!f) return false;
    ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    if (out.compare(0, 3, "\xEF\xBB\xBF") == 0) out.erase(0, 3);   // strip the BOM if present
    return true;
}

// Writes a string to a file (UTF-8). Returns false on error
inline bool writeTextFile(const string& path, const string& text) {
    ofstream f(path, ios::binary);
    if (!f) return false;
    f << text;
    return static_cast<bool>(f);
}

// Converts a UTF-8 string to lower case (Latin + Cyrillic)
#include <string_view>
inline string lowerUtf8View(std::string_view s) {
    string out(s);
    for (size_t i=0;i<out.size();) {
        unsigned char a=static_cast<unsigned char>(out[i]);
        if(a<128) {if(a>='A'&&a<='Z')out[i]=static_cast<char>(a+32);++i;continue;}
        if((a==0xd0||a==0xd1)&&i+1<out.size()) {
            unsigned char b=static_cast<unsigned char>(out[i+1]);
            if(b>=0x80&&b<=0xbf) {
                if(a==0xd0&&b>=0x90&&b<=0x9f)out[i+1]=static_cast<char>(b+32);
                else if(a==0xd0&&b>=0xa0&&b<=0xaf){out[i]=static_cast<char>(0xd1);out[i+1]=static_cast<char>(b-32);}
                else if(a==0xd0&&b==0x81){out[i]=static_cast<char>(0xd1);out[i+1]=static_cast<char>(0x91);}
                i+=2;continue;
            }
        }
        u32string u=utf8ToU32(string(s));
        for(char32_t& cp:u)cp=toLowerCp(cp);
        return u32ToUtf8(u);
    }
    return out;
}
inline string toLowerUtf8(const string& s) {return lowerUtf8View(s);}
