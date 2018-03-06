import subprocess
import re
import os
import random
import glob

HWMON_PATH = '/sys/class/hwmon'

# these are only whitelisted until a user space application has been written
TEMP_SENSOR_WHITELIST = ['peci-hwmon']


class HwmonTempSensor:
    def __init__(self, sensor):
        self.hwmon = os.path.dirname(sensor)
        self.path = sensor
        self.type = 'temperature'
        self.units = 'C'
        index_m = re.search('temp(\d+)_input', os.path.basename(sensor))
        index = index_m.group(1)
        self.name = get_oemname_from_sys(self.hwmon, index)
        if self.name is None:
            self.name = os.path.basename(sensor)
        self.scale = get_scale_from_sys(self.hwmon)

    def read(self):
        try:
            with open(self.path) as f:
                val = f.read().strip()
        except IOError:
            val = 0

        return int(val) * (10 ** self.scale)


class RandomSensor:
    def __init__(self):
        self.type = 'random'
        self.float = random.randrange(0, 2)
        self.name = 'random{}{}'.format(
            'float' if self.float else '', random.randrange(0, 10000))

    def read(self):
        if self.float:
            return random.uniform(0, 100000)
        return random.randrange(0, 100000)


class WolfPass:
    def __init__(self):
        self.TempSensors = [HwmonTempSensor(temp) for temp in get_temp_sensors()]
        self.sensors = self.TempSensors

    def get_sensors(self):
        return self.sensors


class Test:
    def __init__(self):
        self.sensors = [RandomSensor() for _ in range(0, 100)]

    def get_sensors(self):
        return self.sensors


def find_hwmon(name):
    subdirs = os.listdir(HWMON_PATH)
    subdirs = [os.path.join(HWMON_PATH, subdir) for subdir in subdirs]
    for subdir in subdirs:
        device = os.path.join(subdir, 'device')
        device_name = os.path.basename(os.path.realpath(device))
        if device_name == name:
            return subdir


def get_oemname_from_sys(hwmon_path, index=1):
    oemname = 'oemname{}'.format(index)
    try:
        with open(os.path.join(hwmon_path, 'of_node', oemname)) as namefile:
            name = namefile.read().rstrip('\0')
    except IOError:
        name = None
    return name


def get_scale_from_sys(hwmon_path, default=-3):
    try:
        with open(os.path.join(hwmon_path, 'of_node', 'scale')) as scalefile:
            scale = scalefile.read().rstrip('\0')
            scale = int(scale)
    except (IOError, ValueError):
        return default

    return scale


def get_temp_sensors():
    sensors = []
    hwmons = glob.glob(os.path.join(HWMON_PATH, 'hwmon*'))
    for h in hwmons:
        name_file = os.path.join(h, 'of_node', 'name')
        try:
            with open(name_file) as f:
                name = f.read()
        except IOError:
            continue
        name = name.rstrip('\0')
        if name in TEMP_SENSOR_WHITELIST:
            sensors += glob.glob(os.path.join(h, "temp*input"))
    return sensors
