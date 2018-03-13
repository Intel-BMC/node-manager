#!/usr/bin/python

import gobject
import dbus
import dbus.service
import dbus.mainloop.glib
import threading
import time
import random

from SensorEmitterConfig import WolfPass

FAIL_TIMEOUT = 5


class SensorObject(dbus.service.Object):

    def __init__(self, conn, sensor):
        self.object_name = '/xyz/openbmc_project/Sensors/{}/{}'.format(
            sensor.type, sensor.name)
        dbus.service.Object.__init__(self, conn, self.object_name)
        self.timeout = sensor.timeout if hasattr(sensor, 'timeout') else 1000
        self.value = None
        self.sensor = sensor
        self.poll_num = 0
        self.maxValue = 0xFF  # default, updated via config if needed
        self.minValue = 0x0
        self.InterfacesAdded(self.object_name, {
                             "xyz.openbmc_project.Sensor": {"Value": ""}})

    def poll(self):
        value = self.sensor.read()
        # require 1 non 0 read first
        if value == 0 and self.value == None:
            return False
        self.poll_num += 1
        if value != self.value or self.poll_num > 10:
            self.value = value
            self.poll_num = 0

            self.PropertiesChanged("xyz.openbmc_project.Sensor", {
                                   "Value": self.value}, [])
        return True

    @dbus.service.signal('org.freedesktop.DBus.Properties', signature='sa{sv}as')
    def PropertiesChanged(self, property, value, invalid):
        pass

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ss', out_signature='v')
    def Get(self, interface, property):
        if interface == "xyz.openbmc_project.Sensor":
            if property == 'Value':
                return self.value if self.value else 0
        return None

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ssv', out_signature='v')
    def Set(self, interface, property, value):
        return None

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != "xyz.openbmc_project.Sensor":
            return None
        ret_dict = {}
        if self.value:
            ret_dict["Value"] = self.value if self.value else 0
        return ret_dict

    @dbus.service.method('org.freedesktop.DBus.ObjectManager', in_signature='', out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        return {self.object_name: {"xyz.openbmc_project.Sensor": {"Value": self.value if self.value else 0,
                                                                  "MaxValue": self.maxValue, "MinValue": self.minValue},
                                   "xyz.openbmc_project.Sensor.Threshold.Warning": {
            "WarningHigh": self.maxValue * .8, "WarningLow": self.maxValue * .1},
            "xyz.openbmc_project.Sensor.Threshold.Critical": {
            "CriticalHigh": self.maxValue * .9, "CriticalLow": self.maxValue * .05}}}

    @dbus.service.signal('org.freedesktop.DBus.ObjectManager', signature='oa{sa{sv}}')
    def InterfacesAdded(self, object_name, interfaces):
        pass

    @dbus.service.method('xyz.openbmc_project.Sensor',
                         in_signature='', out_signature='')
    def exit(self):
        loop.quit()


class RootObject(dbus.service.Object):
    def __init__(self):
        self.object_name = '/'
        self.sensors = []
        dbus.service.Object.__init__(self, dbus.SystemBus(), self.object_name)

    def add_sensor(self, sensor):
        self.sensors.append(sensor)

    @dbus.service.method('org.freedesktop.DBus.ObjectManager', in_signature='', out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        managed_object = {}
        for sensor in self.sensors:
            if sensor.value is None:
                continue
            for key, val in sensor.GetManagedObjects().iteritems():
                managed_object[key] = val
        return managed_object


def sensor_thread(sensor, root):
    system_bus = dbus.SystemBus()
    time.sleep(random.uniform(0, 1))  # to stagger sensors
    if isinstance(sensor, list):
        objs = []
        for sens in sensor:
            objs.append(SensorObject(system_bus, sens))
            root.add_sensor(objs[-1])
        while True:
            for obj in objs:
                obj.poll()
            time.sleep(obj.timeout * (10 ** -3))

    else:
        obj = SensorObject(system_bus, sensor)
        root.add_sensor(obj)
        while True:
            if not obj.poll():
                time.sleep(FAIL_TIMEOUT)
            else:
                time.sleep(obj.timeout * (10 ** -3))


if __name__ == '__main__':
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    name = dbus.service.BusName(
        "org.openbmc.SensorEmitter", dbus.SystemBus())
    gobject.threads_init()
    config = WolfPass()
    root = RootObject()
    for sensor in config.get_sensors():
        thread = threading.Thread(target=sensor_thread, args=[sensor, root])
        thread.start()
    loop = gobject.MainLoop()
    loop.run()
