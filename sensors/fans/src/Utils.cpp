#include <Utils.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>

namespace fs = std::experimental::filesystem;

nlohmann::json parse_json(std::string file_name, std::string type) {
  std::ifstream f(file_name);
  nlohmann::json input;
  nlohmann::json ret;
  input << f;
  for (auto it = input.begin(); it != input.end(); it++) {
    auto value = it.value();
    if (value["type"] == type) {
      ret.emplace_back(value);
    }
  }
  return ret;
}

bool find_files(const fs::path dir_path, const std::string& match_string,
                std::vector<fs::path>& found_paths,
                unsigned int symlink_depth) {
  if (!fs::exists(dir_path)) return false;

  fs::directory_iterator end_itr;
  std::regex search(match_string);
  std::smatch match;
  for (auto& p : fs::recursive_directory_iterator(dir_path)) {
    std::string path = p.path().string();
    if (!is_directory(p)) {
      if (std::regex_search(path, match, search))
        found_paths.emplace_back(p.path());
    }
    // since we're using a recursve iterator, these should only be symlink dirs
    else if (symlink_depth) {
      find_files(p.path(), match_string, found_paths, symlink_depth - 1);
    }
  }
  return true;
}