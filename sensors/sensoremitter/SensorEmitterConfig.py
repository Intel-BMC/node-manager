import subprocess
import re
import os
import random

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

    def __init__(self, tach):
        self.hwmon = find_hwmon('1e786000.pwm-tacho-controller')
        self.path = os.path.join(self.hwmon, 'fan{}_input'.format(tach))
        self.type = 'tach'
        self.name = 'tach{}'.format(tach)
        self.units = '' # rpm
        self.scale = 0

    def read(self):
        with open(self.path) as f:
            val = f.read().strip()
        return int(val)


class BaseboardTempSensor:

    def __init__(self, dev, input, name, scale=-3):
        self.hwmon = find_hwmon(dev)
        self.path = os.path.join(self.hwmon, 'temp{}_input'.format(input))
        self.type = 'temperature'
        self.scale = scale
        self.units = 'C'
        self.name = name

    def read(self):
        with open(self.path) as f:
            val = f.read().strip()

        return int(val) * (10 ** self.scale)


class ADCSensor:

    def __init__(self, input, name, scale=-3):
        self.hwmon = find_hwmon('iio-hwmon')
        self.path = os.path.join(self.hwmon, 'in{}_input'.format(input))
        self.type = 'voltage'
        self.scale = scale
        self.name = name
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
        self.TachSensors = [TachSensor(tach) for tach in range(0, 12)]
        self.TempSensors = [BaseboardTempSensor('1-004d', 1, 'front_panel'),
                            #BaseboardTempSensor('2-0048', 1, 'riser1'),
                            #BaseboardTempSensor('2-0049', 2, 'riser2'),
                            BaseboardTempSensor('6-0048', 1, 'vrd1'),
                            BaseboardTempSensor('6-0049', 1, 'lr_brd'),
                            BaseboardTempSensor('6-004a', 1, 'bmc_temp'),
                            BaseboardTempSensor('6-004b', 1, 'vrd2'),
                            BaseboardTempSensor('6-004d', 1, 'rr_brd'),
                            #BaseboardTempSensor('13-0056', 1, 'riser3')
                            ]
        ADC_names = ['BB12V', 'P3V3', 'PVNN_PCH_AUX', 'P105_PCH_AUX', 'P12V_AUX', 'P1V8_PCH',
                     'P3VBAT', 'PVCCIN_CPU0', 'PVCCIN_CPU1', 'PVDQ_ABC_CPU0', 'PVDQ_DEF_CPU0',
                     'PVDQ_ABC_CPU1', 'PVDQ_DEF_CPU1', 'PVCCIO_CPU0', 'PVCCIO_CPU1'
        ]
        self.ADCSensors = []
        ii = 1
        for ADC_name in ADC_names:
            self.ADCSensors.append(ADCSensor(ii, ADC_name))
            ii += 1

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
