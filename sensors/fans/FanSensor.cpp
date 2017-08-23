#include <unistd.h>
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

namespace fs = std::experimental::filesystem;
int sensor_count = 0;  // todo move this elseware

bool find_files(const fs::path dir_path, const std::string& match_string,
                std::vector<fs::path>& found_paths,
                unsigned int symlink_depth = 1) {
  if (!fs::exists(dir_path)) return false;

  fs::directory_iterator end_itr;
  std::regex search(match_string);
  std::smatch match;
  for (auto& p : fs::recursive_directory_iterator(dir_path)) {
    std::string path = p.path().string();
    if (!is_directory(p)){
      if(std::regex_search(path, match, search))
        found_paths.emplace_back(p.path());
    }
    // since we're using a recursve iterator, these should only be symlink dirs
    else if (symlink_depth) {
      find_files(p.path(), match_string, found_paths, symlink_depth - 1);
    }
  }
  return true;
}

std::string find_name(const std::string& dir_path, int count) {
  fs::path p(dir_path);
  fs::path dir = p.parent_path();
  std::string of_node = dir.string() + "/of_node";
  std::vector<fs::path> oemfiles;
  find_files(of_node, "oemname\\d+", oemfiles);
  std::regex search("fan(\\d+)_input");
  std::smatch match;
  int index = -1;
  int ii = 0;
  bool b_sensor = false;
  std::string name;

  if (!std::regex_search(dir_path, match, search))
    return std::string("unknown");

  for (auto x : match) {
    if (ii == 0)
      ii++;
    else {
      index = std::stoi(x);
      break;
    }
  }

  if (index == -1) return std::string("unknown");

  if (oemfiles.size() < count) {
    if (index % 2 == 0)
      b_sensor = true;  // todo(james) will we ever have 'c' sensors?
    index = (index + 1) / 2;
  }
  std::ifstream namefile(of_node + std::string("/oemname") +
                         std::to_string(index));
  if(!namefile.good())
   return std::string("unknown");

  std::getline(namefile, name);
  namefile.close();
  name.pop_back();
  if (b_sensor) name.append("b");
  return name;
}

class Tach {
 public:
  Tach(std::string& path, std::shared_ptr<dbus::connection> system_bus,
       boost::asio::io_service& io)
      : path(path),
        system_bus(system_bus),
        io(io),
        name(find_name(path, sensor_count)),
        dbus_endpoint("", "/xyz/openbmc_project/sensors/tach/" + name,
                      "org.freedesktop.Dbus.Properties"),
        input_dev(io, open(path.c_str(), O_RDONLY)) {
    setup_read();
  }

 private:
  std::string path;
  std::shared_ptr<dbus::connection> system_bus;
  boost::asio::io_service& io;
  std::string name;
  dbus::endpoint dbus_endpoint;
  boost::asio::posix::stream_descriptor input_dev;
  boost::asio::streambuf read_buf;

  float value = 0;

  void setup_read(void) {
    boost::asio::async_read_until(
        input_dev, read_buf, '\n',
        [this](const boost::system::error_code& ec,
               std::size_t /*bytes_transfered*/) { handle_response(ec); });
  }

  void handle_response(const boost::system::error_code& err) {
    if (!err) {
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
    }
    input_dev.close();
    input_dev.assign(open(path.c_str(), O_RDONLY));
    setup_read();
  }

  void update_value(float new_value) {
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

nlohmann::json parse_json(void){
 std::ifstream f(JSON_STORE);
 nlohmann::json input;
 nlohmann::json ret;
 input << f;
 for(auto it = input.begin(); it != input.end(); it++){
  auto value = it.value();
  if(value["type"] == "AspeedFan"){
   ret.emplace_back(value);
  }
 }
  return ret;
}

int main(int argc, char** argv) {
  boost::asio::io_service io;
  auto system_bus = std::make_shared<dbus::connection>(io, dbus::bus::system);

  std::vector<std::string> tach_paths;
  nlohmann::json j = parse_json();
  tach_paths.reserve(20); //systems should have less than 20 fans
  // create tachs
  for(auto& fan : j){
       for(auto tach : *fan.find("tachs")){
        std::vector<fs::path> paths;
        if(find_files(fs::path("/sys/class/hwmon"), "fan" + std::to_string(tach.get<int>()) + "_input", paths)){
         for(const auto& path : paths){  // todo, should only be 1
           tach_paths.emplace_back(path.string());
          }
        }
      }

  }
  sensor_count = tach_paths.size();

  std::vector<std::unique_ptr<Tach>> tachs;
  for(auto& tach_path : tach_paths){
    tachs.emplace_back(std::make_unique<Tach>(tach_path, system_bus, io));
  }
  io.run();
}
