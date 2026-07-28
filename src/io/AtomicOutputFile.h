#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fpt {

class AtomicOutputFile {
public:
    explicit AtomicOutputFile(
        const std::filesystem::path& target,
        std::size_t bufferBytes = 0)
        : target_(target),
          temporary_(makeTemporaryPath(target)),
          buffer_(bufferBytes) {
        if (!buffer_.empty()) {
            stream_.rdbuf()->pubsetbuf(
                buffer_.data(),
                static_cast<std::streamsize>(buffer_.size()));
        }
        stream_.open(temporary_, std::ios::out | std::ios::trunc);
        if (!stream_) {
            throw std::runtime_error(
                "Failed to open temporary output file: " +
                temporary_.generic_string());
        }
    }

    AtomicOutputFile(const AtomicOutputFile&) = delete;
    AtomicOutputFile& operator=(const AtomicOutputFile&) = delete;

    ~AtomicOutputFile() {
        if (!committed_) {
            stream_.close();
            std::error_code ignored;
            std::filesystem::remove(temporary_, ignored);
        }
    }

    std::ostream& stream() {
        return stream_;
    }

    void commit() {
        stream_.flush();
        if (!stream_) {
            throw std::runtime_error(
                "Failed while writing temporary output file: " +
                temporary_.generic_string());
        }
        stream_.close();
        if (stream_.fail()) {
            throw std::runtime_error(
                "Failed to close temporary output file: " +
                temporary_.generic_string());
        }

#ifdef _WIN32
        if (!MoveFileExW(
                temporary_.c_str(),
                target_.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const DWORD error = GetLastError();
            throw std::runtime_error(
                "Failed to replace output file atomically (Windows error " +
                std::to_string(static_cast<unsigned long>(error)) + "): " +
                target_.generic_string());
        }
#else
        std::error_code error;
        std::filesystem::rename(temporary_, target_, error);
        if (error) {
            throw std::runtime_error(
                "Failed to replace output file atomically: " +
                target_.generic_string() + " (" + error.message() + ")");
        }
#endif
        committed_ = true;
    }

private:
    static std::uint64_t processId() {
#ifdef _WIN32
        return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
        return static_cast<std::uint64_t>(getpid());
#endif
    }

    static std::filesystem::path makeTemporaryPath(
        const std::filesystem::path& target) {
        static std::atomic<std::uint64_t> sequence{0};
        const std::filesystem::path directory =
            target.has_parent_path() ? target.parent_path() : std::filesystem::current_path();
        const std::wstring base = target.filename().wstring();
        for (int attempt = 0; attempt < 64; ++attempt) {
            const std::uint64_t id = ++sequence;
            const std::filesystem::path candidate =
                directory /
                (base + L".fpt-tmp-" + std::to_wstring(processId()) +
                 L"-" + std::to_wstring(id));
            std::error_code error;
            if (!std::filesystem::exists(candidate, error)) {
                return candidate;
            }
        }
        throw std::runtime_error(
            "Unable to allocate a unique temporary output path for: " +
            target.generic_string());
    }

    std::filesystem::path target_;
    std::filesystem::path temporary_;
    std::vector<char> buffer_;
    std::ofstream stream_;
    bool committed_ = false;
};

} // namespace fpt
