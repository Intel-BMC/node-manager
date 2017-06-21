#!/usr/bin/python

import gobject
import dbus
import dbus.service
import dbus.mainloop.glib

from SensorEmitterConfig import WolfPass


class SensorObject(dbus.service.Object):

    def __init__(self, conn, sensor):
        dbus.service.Object.__init__(self, conn, '/xyz/openbmc_project/sensors/{}/{}'.format(sensor.type, sensor.name))
        timeout = sensor.timeout if hasattr(sensor, 'timeout') else 1000
        gobject.timeout_add(timeout, self.poll, sensor)
        self.value = None

    def poll(self, sensor):
        value = sensor.read()
        if value != self.value:
            self.value = value
            self.PropertiesChanged("xyz.openbmc_project.Sensor.Value", {"Value": self.value}, [])
        return True

    @dbus.service.signal('org.freedesktop.DBus.Properties', signature='sa{sv}as')
    def PropertiesChanged(self, property, value, invalid):
        pass

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='v')
    def Get(self, name):
        if name == 'Value':
            return self.value

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='v')
    def Set(self, name):
        if name == 'Value':
            return self.value

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='a{sv}')
    def GetAll(self, name):
        if name == 'Value':
            val = self.value
        if val:
            return {name: val}

    @dbus.service.method('xyz.openbmc_project.sensors',
                         in_signature='', out_signature='')
    def exit(self):
        loop.quit()

if __name__ == '__main__':

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    system_bus = dbus.SystemBus()
    name = dbus.service.BusName('xyz.openbmc_project.sensors', system_bus)
    config = WolfPass()
    objects = []
    for sensor in config.get_sensors():
        objects.append(SensorObject(system_bus, sensor))

    loop = gobject.MainLoop()
    loop.run()
