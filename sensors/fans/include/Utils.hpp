#pragma once

#include <experimental/filesystem>
#include <nlohmann/json.hpp>

nlohmann::json parse_json(std::string file_name, std::string type);

bool find_files(const std::experimental::filesystem::path dir_path,
                const std::string& match_string,
                std::vector<std::experimental::filesystem::path>& found_paths,
                unsigned int symlink_depth = 1);
