#include <TachSensor.hpp>
#include <dbus/connection.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include "gtest/gtest.h"

TEST(TachSensor, TestTachSensor) {
  boost::asio::io_service io;
  auto system_bus = std::make_shared<dbus::connection>(io, dbus::bus::session);
  dbus::DbusObjectServer object_server(system_bus);
  std::string json_data = R"(  {"connector": {
            "status": "okay",
            "tachs": [
                7,
                8
            ],
            "pwm": 4,
            "type": "IntelFanConnector",
            "name": "1U System Fan connector 4"
        },
        "status": "okay",
        "name": "Fan 4",
        "thresholds": [
            [
                {
                    "direction": "less than",
                    "name": "lower critical",
                    "value": 1080,
                    "severity": 1
                },
                {
                    "direction": "less than",
                    "severity": 1,
                    "value": 1260
                }
            ],
            [
                {
                    "direction": "less than",
                    "name": "lower critical",
                    "value": 1110,
                    "severity": 1
                },
                {
                    "direction": "less than",
                    "name": "lower non critical",
                    "value": 1295,
                    "severity": 1
                }
            ]
        ],
        "type": "AspeedFan",
        "bind_connector": "1U System Fan connector 4"})";

  auto fan_json = nlohmann::json::parse(json_data);

  std::ofstream test_file("test.txt");
  test_file << "10000\n";
  test_file.close();
  auto filename = std::string("test.txt");
  auto fanname = std::string("test fan");
  TachSensor test(filename, object_server, io, fanname, fan_json);

  std::remove("test.txt");
}
