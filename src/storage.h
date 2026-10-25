#pragma once
#include "model.h"
#include <filesystem>
namespace paper {
struct LoadResult { State state; bool recovered = false; };
class Store {
public:
    explicit Store(std::filesystem::path directory);
    LoadResult Load() const;
    void Save(const State& state) const;
    const std::filesystem::path& Directory() const { return directory_; }
private:
    std::filesystem::path directory_;
};
std::filesystem::path DefaultDirectory();
}

