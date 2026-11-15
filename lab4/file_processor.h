#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <stdexcept>

struct OperationCancelled : std::runtime_error {
    OperationCancelled() : std::runtime_error("Операция отменена") {}
};
struct ProcessingStats { std::uint64_t valid = 0, invalid = 0; };
ProcessingStats processFile(const std::filesystem::path& input,
                            const std::filesystem::path& partial,
                            const std::atomic<bool>& cancel,
                            std::atomic<std::uint64_t>& bytes);
