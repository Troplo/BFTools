//
// Created by troplo on 10/15/26.
//

#ifndef NOMADARCHIVES_LOGGER_H
#define NOMADARCHIVES_LOGGER_H

#include <chrono>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

namespace Nomad::Archives {
    enum class LogLevel { Debug, Info, Warn, Error };

    class Logger {
      public:
        static void SetLogLevel(LogLevel level) { s_minLevel = level; }

        static LogLevel GetLogLevel() { return s_minLevel; }

        template <typename... Args>
        static void Log(LogLevel level, std::string_view tag,
                        std::format_string<Args...> fmt, Args&&... args) {
            if (level < s_minLevel)
                return;

            auto now = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()) %
                      1000;
            std::tm tm{};
#if defined(_WIN32)
            localtime_s(&tm, &timeT);
#else
            localtime_r(&timeT, &tm);
#endif

            const char* levelStr = "INFO ";
            const char* colorCode = "";
            const char* resetCode = "\033[0m";

            switch (level) {
            case LogLevel::Debug:
                levelStr = "DEBUG";
                colorCode = "\033[36m";
                break; // Cyan
            case LogLevel::Info:
                levelStr = "INFO ";
                colorCode = "\033[32m";
                break; // Green
            case LogLevel::Warn:
                levelStr = "WARN ";
                colorCode = "\033[33m";
                break; // Yellow
            case LogLevel::Error:
                levelStr = "ERROR";
                colorCode = "\033[31m";
                break; // Red
            }

            std::string formattedMsg =
                std::format(fmt, std::forward<Args>(args)...);

            std::lock_guard<std::mutex> lock(s_mutex);
            std::ostream& out =
                (level == LogLevel::Error) ? std::cerr : std::cout;
            out << std::format("[{:02d}:{:02d}:{:02d}.{:03d}] {}{}{} [{}] {}\n",
                               tm.tm_hour, tm.tm_min, tm.tm_sec,
                               static_cast<int>(ms.count()), colorCode,
                               levelStr, resetCode, tag, formattedMsg);
            out.flush();
        }

        template <typename... Args>
        static void Debug(std::string_view tag, std::format_string<Args...> fmt,
                          Args&&... args) {
            Log(LogLevel::Debug, tag, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Info(std::string_view tag, std::format_string<Args...> fmt,
                         Args&&... args) {
            Log(LogLevel::Info, tag, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Warn(std::string_view tag, std::format_string<Args...> fmt,
                         Args&&... args) {
            Log(LogLevel::Warn, tag, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Error(std::string_view tag, std::format_string<Args...> fmt,
                          Args&&... args) {
            Log(LogLevel::Error, tag, fmt, std::forward<Args>(args)...);
        }

      private:
        inline static LogLevel s_minLevel = LogLevel::Debug;
        inline static std::mutex s_mutex;
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_LOGGER_H
