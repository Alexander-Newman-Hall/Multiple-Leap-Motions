#pragma once

#include <string>
#ifdef _WIN32
#include <windows.h> // For OutputDebugStringA
#else
#include <iostream>
#endif

// Simple wrapper class for logging via OutputDebugStringA
// Allows the logger to be managed as a service via shared_ptr
class AppLogger {
public:
    virtual ~AppLogger() = default; // Virtual destructor for potential inheritance

    virtual void log(const std::string& message) const {
        std::string logMsg = message + "\n"; // Add newline for readability in debugger
#ifdef _WIN32
        OutputDebugStringA(logMsg.c_str());
#else
        std::clog << logMsg;
#endif
    }
};
