//
//  helperfunctions.cpp
//  Kufar Telegram Notifier
//
//  Created by Macintosh on 04.06.2022.
//

#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iterator>
#include <algorithm>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include "helperfunctions.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <libgen.h>
#endif
#include <limits.h>
#include <iostream>

using namespace std;

bool fileExists(const string &path) {
    ifstream f(path);
    return f.good();
}

uint64_t getFileSize(const string &path) {
    ifstream f(path, ios::binary | ios::ate);
    if (!f) { throw runtime_error("Cannot read file size"); }
    return static_cast<uint64_t>(f.tellg());
}

string getTextFromFile(const string &path) {
    ifstream ifs(path, ios::binary);
    if (!ifs) { throw runtime_error("Cannot read file"); }
    return string((istreambuf_iterator<char>(ifs)),
                  (istreambuf_iterator<char>()));
}

time_t zuluToTimestamp(const string &zuluDate) {
    tm t{};
    istringstream stringStream(zuluDate);
    
    stringStream >> get_time(&t, "%Y-%m-%dT%H:%M:%S");
    if (stringStream.fail()) {
        throw runtime_error{"failed to parse time string"};
    }
    
    const auto suffix = zuluDate.substr(19);
    if (suffix != "Z" && (suffix.size() < 3 || suffix.front() != '.' || suffix.back() != 'Z' ||
        suffix.substr(1, suffix.size() - 2).find_first_not_of("0123456789") != string::npos)) {
        throw runtime_error("Expected a UTC timestamp ending in Z");
    }
    const tm original = t;
#ifdef _WIN32
    const auto result = _mkgmtime(&t);
#else
    const auto result = timegm(&t);
#endif
    if (t.tm_year != original.tm_year || t.tm_mon != original.tm_mon || t.tm_mday != original.tm_mday ||
        t.tm_hour != original.tm_hour || t.tm_min != original.tm_min || t.tm_sec != original.tm_sec) {
        throw runtime_error("Invalid UTC date");
    }
    return result;
}

string joinIntVector(const vector<int> &nums, const string &delim) {
    ostringstream result;
    for (size_t i = 0; i < nums.size(); ++i) {
        if (i) { result << delim; }
        result << nums[i];
    }
    return result.str();
}

time_t timestampShift(const time_t &timestamp, int shift) {
    return timestamp + (3600 * shift);
}

bool stringHasPrefix(const string &originalString, const string &prefix) {
    return originalString.rfind(prefix, 0) == 0;
}

void saveFile(const string &path, const string &contents) {
    const filesystem::path target(path);
    const filesystem::path temporary(path + ".tmp");
    if (filesystem::exists(temporary) || filesystem::is_symlink(temporary)) {
        throw runtime_error("Cache temporary file already exists; check it before restarting");
    }
    ofstream ofs(temporary, ios::binary | ios::trunc);
    ofs << contents;
    ofs.flush();
    if (!ofs) { throw runtime_error("Cannot write cache temporary file"); }
    ofs.close();
    if (!ofs) { throw runtime_error("Cannot close cache temporary file"); }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw runtime_error("Cannot replace cache file");
    }
#else
    filesystem::rename(temporary, target);
#endif
}

nlohmann::json parseJSON(const string &text) {
    return nlohmann::json::parse(text, [](int depth, nlohmann::json::parse_event_t, nlohmann::json &) {
        if (depth > 64) { throw runtime_error("JSON nesting exceeds 64 levels"); }
        return true;
    });
}

#ifdef __APPLE__
    #include <mach-o/dyld.h>
    #include <filesystem>

    optional<string> getWorkingDirectory() {
        char buffer[PATH_MAX];
        uint32_t buffsize = PATH_MAX;
        
        if (_NSGetExecutablePath(buffer, &buffsize) == 0) {
            return dirname(buffer);
        }
        
        return nullopt;
    }
#elif __linux__
    #include <linux/limits.h>

    optional<string> getWorkingDirectory() {
        char result[PATH_MAX];
        const auto count = readlink("/proc/self/exe", result, sizeof(result) - 1);
        if (count > 0 && count < static_cast<ssize_t>(sizeof(result) - 1)) {
            result[count] = '\0';
            return filesystem::path(result).parent_path().string();
        }
        return nullopt;
    }
#elif defined(_WIN32)
    optional<string> getWorkingDirectory() {
        wchar_t buffer[32768];
        const auto count = GetModuleFileNameW(nullptr, buffer, 32768);
        if (count == 0 || count >= 32768) { return nullopt; }
        return filesystem::path(buffer).parent_path().string();
    }
#else
    optional<string> getWorkingDirectory() { return nullopt; }
#endif
