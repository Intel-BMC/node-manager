#include <TachSensor.hpp>
#include <Utils.hpp>
#include <experimental/filesystem>
#include <dbus/connection.hpp>

namespace fs = std::experimental::filesystem;

#define JSON_STORE "/var/configuration/sensors.json"

int main(int argc, char** argv) {
  boost::asio::io_service io;
  auto system_bus = std::make_shared<dbus::connection>(io, dbus::bus::system);
  dbus::DbusObjectServer object_server(system_bus);

  std::vector<std::string> tach_paths;
  nlohmann::json fan_json = parse_json(JSON_STORE, "AspeedFan");
  tach_paths.reserve(20);  // systems should have less than 20 fans
  // create tachs
  for (auto& fan : fan_json) {
    auto connector = fan["connector"];
    for (auto tach : *connector.find("tachs")) {
      std::vector<fs::path> paths;
      if (find_files(fs::path("/sys/class/hwmon"),
                     "fan" + std::to_string(tach.get<int>()) + "_input",
                     paths)) {
        for (const auto& path : paths) {  // todo, should only be 1
          tach_paths.emplace_back(path.string());
        }
      }
    }
  }

  if (!fan_json.size()) {
    std::cerr << "No records found in configuration\n";
    return 1;
  }

  int tach_per_fan = tach_paths.size() / fan_json.size();
  std::cout << tach_per_fan << " tach(s) per fan found.\n";

  std::vector<std::unique_ptr<TachSensor>> tachs;
  int ii = 0;
  for (auto& fan : fan_json) {
    for (auto end = ii + tach_per_fan; ii < end; ii++) {
      std::string name = fan["name"];
      // todo(james) if we ever have 'c' fans this will need to be revisited
      if (tach_per_fan > 1) {
        if (ii % 2)
          name += "b";
        else
          name += "a";
      }
      std::cout << name << "\n";
      tachs.emplace_back(std::make_unique<TachSensor>(
          tach_paths.at(ii), object_server, io, name, fan));
    }
  }
  io.run();
}
