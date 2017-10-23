#pragma once

#include <dbus/properties.hpp>
#include <nlohmann/json.hpp>

class TachSensor {
 private:
  std::string path;
  dbus::DbusObjectServer &object_server;
  boost::asio::io_service &io;
  std::string name;
  nlohmann::json &json_config;
  std::shared_ptr<dbus::DbusObject> dbus_object;
  std::shared_ptr<dbus::DbusInterface> dbus_interface;
  boost::asio::posix::stream_descriptor input_dev;
  boost::asio::deadline_timer wait_timer;
  boost::asio::streambuf read_buf;
  float value;
  int err_count;
  double max_value;
  double min_value;
  void setup_read(void);
  void handle_response(const boost::system::error_code &err);
  void check_thresholds(float new_value);
  void update_value(float new_value);
  void set_initial_properties(void);

 public:
  TachSensor(std::string &path, dbus::DbusObjectServer &object_server,
             boost::asio::io_service &io, std::string &fan_name,
             nlohmann::json &json_config);
};
