#include <limits.h>
#include <unistd.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <dbus/connection.hpp>
#include <dbus/endpoint.hpp>
#include <dbus/message.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>

#define JSON_STORE "/var/configuration/sensors.json"
#define PWM_POLL_MS 500

namespace fs = std::experimental::filesystem;

bool find_files(const fs::path dir_path, const std::string& match_string,
                std::vector<fs::path>& found_paths,
                unsigned int symlink_depth = 1) {
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

class Tach {
 public:
  Tach(std::string& path, std::shared_ptr<dbus::connection> system_bus,
       boost::asio::io_service& io, std::string& fan_name,
       nlohmann::json& json_config)
      : path(path),
        system_bus(system_bus),
        io(io),
        name(boost::replace_all_copy(fan_name, " ", "_")),
        json_config(json_config),
        dbus_endpoint("", "/xyz/openbmc_project/sensors/tach/" + name,
                      "org.freedesktop.Dbus.Properties"),
        input_dev(io, open(path.c_str(), O_RDONLY)),
        wait_timer(io) {
    setup_read();
  }

 private:
  std::string path;
  std::shared_ptr<dbus::connection> system_bus;
  boost::asio::io_service& io;
  std::string name;
  nlohmann::json& json_config;
  dbus::endpoint dbus_endpoint;
  boost::asio::posix::stream_descriptor input_dev;
  boost::asio::deadline_timer wait_timer;
  boost::asio::streambuf read_buf;

  float value =
      FLT_MAX;  // max so we don't get a threshold uncrossed event at start
  int err_count = 0;

  void setup_read(void) {
    boost::asio::async_read_until(
        input_dev, read_buf, '\n',
        [&](const boost::system::error_code& ec,
            std::size_t /*bytes_transfered*/) { handle_response(ec); });
  }

  void handle_response(const boost::system::error_code& err) {
    if (!err) {
      err_count = 0;
      std::istream response_stream(&read_buf);
      std::string response;
      std::getline(response_stream, response);
      float nvalue = std::stof(response);
      response_stream.clear();
      if (nvalue != value) update_value(nvalue);
    } else {
      // todo(james) this happens mostly on poweroff, where we shouldn't be
      // reading anyway

      std::cerr << "Failure to read sensor " << name << " at " << path << "\n";

      if (err_count >= 10) {
        // only send value update once
        if (err_count == 10) {
          update_value(0);
          err_count++;
        }
      } else
        err_count++;
    }
    input_dev.close();
    input_dev.assign(open(path.c_str(), O_RDONLY));
    wait_timer.expires_from_now(boost::posix_time::milliseconds(PWM_POLL_MS));
    wait_timer.async_wait(
        [&](const boost::system::error_code&) { setup_read(); });
  }

  void check_thresholds(float new_value) {
    int index = boost::ends_with(name, "b") ? 1 : 0;
    auto thresholds = json_config["thresholds"].at(index);
    for (auto& threshold : thresholds) {
      if (threshold["direction"] == "less than") {
        if (value >= threshold["value"] && new_value < threshold["value"]) {
          std::cout << "threshold crossed!\n";  // todo(james) assert on dbus
        } else if (value < threshold["value"] &&
                   new_value >= threshold["value"]) {
          std::cout << "threshold uncrossed!\n";
        }
      } else if (threshold["direction"] == "greater than") {
        if (value <= threshold["value"] && new_value > threshold["value"]) {
          std::cout << "threshold crossed!\n";  // todo(james) assert on dbus
        } else if (value > threshold["value"] &&
                   new_value <= threshold["value"]) {
          std::cout << "threshold uncrossed!\n";
        }

      } else {
        std::cerr << "Invalid threshold " << threshold["direction"] << ".\n";
      }
    }
  }

  void update_value(float new_value) {
    check_thresholds(new_value);

    auto signal_name = std::string("PropertiesChanged");
    auto m = dbus::message::new_signal(dbus_endpoint, signal_name);
    value = new_value;

    std::vector<std::pair<std::string, dbus::dbus_variant>> map2;
    map2.emplace_back("Value", value);
    static auto removed = std::vector<uint32_t>();

    m.pack("xyz.openbmc_project.Sensor.Value", map2, removed);

    system_bus->async_send(
        m, [&](boost::system::error_code ec, dbus::message s) {});
  }
};

nlohmann::json parse_json(void) {
  std::ifstream f(JSON_STORE);
  nlohmann::json input;
  nlohmann::json ret;
  input << f;
  for (auto it = input.begin(); it != input.end(); it++) {
    auto value = it.value();
    if (value["type"] == "AspeedFan") {
      ret.emplace_back(value);
    }
  }
  return ret;
}

int main(int argc, char** argv) {
  boost::asio::io_service io;
  auto system_bus = std::make_shared<dbus::connection>(io, dbus::bus::system);

  std::vector<std::string> tach_paths;
  nlohmann::json fan_json = parse_json();
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

  std::vector<std::unique_ptr<Tach>> tachs;
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
      tachs.emplace_back(
          std::make_unique<Tach>(tach_paths.at(ii), system_bus, io, name, fan));
    }
  }
  io.run();
}
