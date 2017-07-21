import subprocess
import re
import os
import random
import glob

HWMON_PATH = '/sys/class/hwmon'


class DimmSensor:

    def __init__(self, cpu, dimm):
        self.path = '/usr/bin/dimmsensor'
        self.cpu = cpu
        self.dimm = dimm
        self.type = 'temperature'
        self.name = 'cpu{}dimm{}'.format(cpu, dimm)
        self.units = 'C'
        self.scale = 0

    def read(self):
        try:
            out = subprocess.check_output('{} {} {}'.format(self.path, self.cpu, self.dimm), shell=True)
        except subprocess.CalledProcessError:
            return 0
        if out[0].isdigit():
            m = re.search('\d+', out)
            if m:
                return int(m.group(0))
        return 0


class TachSensor:

    def __init__(self, sensor):
        self.hwmon = os.path.dirname(sensor)
        self.path = sensor
        self.type = 'tach'
        name_count = len(glob.glob(os.path.join(self.hwmon, 'of_node', 'oemname*')))
        max_temps = len(glob.glob(os.path.join(self.hwmon, 'fan*input')))
        two_to_one = name_count < max_temps

        index_m = re.search('fan(\d+)_input', os.path.basename(sensor))
        index = int(index_m.group(1))
        b_sensor = False
        if two_to_one:
            b_sensor = index % 2 == 0
            index = int((index + 1) / 2)  # todo, will we ever have more than 2:1 ?

        self.name = get_oemname_from_sys(self.hwmon, index)
        if self.name is None:
            self.name = os.path.basename(sensor)
        if b_sensor:
            self.name += 'b'

        self.units = ''  # rpm
        self.scale = 0
        self.timeout = 2000

    def read(self):
        try:
            with open(self.path) as f:
                val = f.read().strip()
        except IOError:
            val = 0
        return int(val)


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


class ADCSensor:

    def __init__(self, sensor):
        self.hwmon = os.path.dirname(sensor)
        self.path = sensor
        self.type = 'voltage'
        self.scale = get_scale_from_sys(self.hwmon)
        index_m = re.search('in(\d+)_input', os.path.basename(sensor))
        index = index_m.group(1)
        self.name = get_oemname_from_sys(self.hwmon, index)
        if self.name is None:
            self.name = os.path.basename(sensor)
        self.units = 'V'

    def read(self):
        with open(self.path) as f:
            val = f.read().strip()

        return int(val) * (10 ** self.scale)


class RandomSensor:

    def __init__(self):
        self.type = 'random'
        self.float = random.randrange(0, 2)
        self.name = 'random{}{}'.format('float' if self.float else '', random.randrange(0, 10000))

    def read(self):
        if self.float:
            return random.uniform(0, 100000)
        return random.randrange(0, 100000)


class WolfPass:

    def __init__(self):
        self.DimmSensors = [DimmSensor(0, dimm) for dimm in range(0, 12)] + \
                           [DimmSensor(1, dimm) for dimm in range(0, 12)]
        self.TachSensors = [TachSensor(tach) for tach in glob.glob(os.path.join(HWMON_PATH, 'hwmon*', 'fan*input'))]
        self.TempSensors = [HwmonTempSensor(temp) for temp in glob.glob(os.path.join(HWMON_PATH, 'hwmon*', 'temp*input'))]
        self.ADCSensors = [ADCSensor(adc) for adc in glob.glob(os.path.join(find_hwmon('iio-hwmon'), 'in*input'))]

        self.sensors = [self.DimmSensors] + self.TachSensors + self.TempSensors + self.ADCSensors

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
