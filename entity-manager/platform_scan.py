#!/usr/bin/python

import os
from fru_device import FruDeviceProbe
import json
import overlay_gen
import glob
import traceback
from utils.generic import prep_json, dict_template_replace

TEMPLATE_CHAR = '$'
CONFIGURATION_DIR = os.environ.get('JSON_STORE', '/var/configuration/')


class PlatformScan(object):
    def __init__(self):
        self.fru = None
        self.found_devices = []
        self.configuration_dir = os.environ.get('CONFIGURATION_DIR', '/usr/share/configurations')

    def parse_fru(self):
        self.fru = FruDeviceProbe()

    @staticmethod
    def find_bind(d):
        for x in list(d):
            if x.startswith('bind'):
                return x
        return False

    @staticmethod
    def has_update(d):
        return 'update' in list(d)

    def apply_update(self, item):
        if 'update' not in list(item):
            return False
        for _, entity in self.found_devices.iteritems():
            for child in entity['exposes']:
                if child['name'] == item['update']:
                    item.pop('update', None)
                    child.update(item)
                    return True

    def apply_bind(self, item):
        bind = self.find_bind(item)
        if not bind:
            return False
        for _, entity in self.found_devices.iteritems():
            for child in entity['exposes']:
                if child['name'] == item[bind]:
                    bind_name = bind.split('_')[1]
                    item[bind_name] = child
                    item['status'] = item[bind_name]['status'] = 'okay'  # todo, should this go here?
                    return True

    def read_config(self):
        try:
            with open(os.path.join(CONFIGURATION_DIR, 'system.json')) as config:
                self.found_devices = json.load(config)
        except IOError:
            return False
        return self.found_devices

    def write_config(self):
        if not os.path.exists(os.path.dirname(CONFIGURATION_DIR)):
            os.makedirs(os.path.dirname(CONFIGURATION_DIR))
        with open(os.path.join(CONFIGURATION_DIR, 'system.json'), 'w') as config:
            json.dump(self.found_devices, config, indent=4)
        sensors = []
        for _, value in self.found_devices.iteritems():
            if isinstance(value, dict) and value.get('exposes', None):
                sensors += value['exposes']
        with open(os.path.join(CONFIGURATION_DIR, 'sensors.json'), 'w') as sensor_config:
            json.dump(sensors, sensor_config, indent=4)

        # todo make this a dbus endpoint in the fru_device
        with open(os.path.join(CONFIGURATION_DIR, 'frus.json'), 'w') as fru_config:
            json.dump(self.fru.get_all(), fru_config, indent=4)

    def parse_configuration(self):

        available_entity_list = []
        self.found_devices = {}

        for json_filename in glob.glob(os.path.join(self.configuration_dir, "*.json")):
            with open(os.path.join(self.configuration_dir, json_filename)) as json_file:
                clean_file = prep_json(json_file)
                try:
                    entities = json.load(clean_file)
                    if isinstance(entities, list):
                        available_entity_list += entities
                    else:
                        available_entity_list.append(entities)
                except ValueError as e:
                    traceback.format_exc(e)
                    print("Failed to parse {} error was {}".format(json_file, e))
        while True:
            num_devices = len(self.found_devices)
            for entity in available_entity_list[:]:
                probe_command = entity.get("probe", None)
                if not probe_command:
                    available_entity_list.remove(entity)
                    print "entity {} doesn't have a probe function".format(entity.get("name", "unknown"))
                    if not entity.get("name", False):
                        print json.dumps(entity, sort_keys=True, indent=4)
                elif entity['name'] not in (key for key in list(self.found_devices)):
                    found_dev = eval(probe_command, {'fru': self.fru, 'found_devices': list(self.found_devices)})
                    # TODO(ed)  Calling eval on a string is bad practice.  Come up with a better
                    # probing structure
                    if found_dev:
                        try:
                            count = len(found_dev)
                        except TypeError:
                            count = 1

                        entity['type'] = 'entity'
                        entity['status'] = 'okay'

                        idx = 0
                        for item in entity.get('exposes', [])[:]:
                            if self.find_bind(item):
                                assert (self.apply_bind(item))
                            elif self.has_update(item):
                                assert (self.apply_update(item))
                                entity['exposes'].remove(item)
                            else:
                                if TEMPLATE_CHAR in str(item):
                                    # todo populate this dict smarter
                                    for ii in range(0, count):
                                        replaced = dict_template_replace(item, {'bus': found_dev[0]['bus'],
                                                                                'fruaddress': hex(found_dev[0]['device']),
                                                                                'index': ii})
                                        if 'status' not in replaced:
                                            replaced['status'] = 'okay'
                                        entity['exposes'][idx] = replaced
                                else:
                                    replaced = item
                                    if 'status' not in replaced:
                                        replaced['status'] = 'okay'
                                    entity['exposes'][idx] = replaced
                            self.found_devices[entity['name']] = entity
                            idx += 1

            if len(self.found_devices) == num_devices:
                break  # exit after looping without additions
        self.write_config()
        return self.found_devices


if __name__ == '__main__':
    platform_scan = PlatformScan()
    platform_scan.parse_fru()
    found_devices = platform_scan.parse_configuration()

    overlay_gen.unload_overlays()  # start fresh

    for pk, pvalue in found_devices.iteritems():
        for element in pvalue.get('exposes', []):
            if not isinstance(element, dict) or element.get('status', 'disabled') != 'okay':
                continue
            element['oem_name'] = element.get('name', 'unknown').replace(' ', '_')
            if element.get("type", "") == "TMP75":
                element["reg"] = element.get("address").lower()
                # todo(ed) find a better escape function to use.
                overlay_gen.load_entity(**element)

            elif element.get("type", "") == "TMP421":
                element["reg"] = element.get("address").lower()
                element["oem_name1"] = element.get("name1").replace(" ", "_")
                overlay_gen.load_entity(**element)

            elif element.get("type", "") == "ADC":
                overlay_gen.load_entity(**element)

            elif element.get("type", "") == "AspeedFan":
                connector = element['connector']
                element.update(connector)
                element["type"] = 'aspeed_pwmtacho'
                overlay_gen.load_entity(**element)

            elif element.get("type", "") == "SkylakeCPU":
                element["type"] = 'aspeed_peci_hwmon'
                overlay_gen.load_entity(**element)

            elif element.get("type", "") == "IntelFruDevice":
                element["type"] = 'eeprom'
                element["reg"] = element.get("address").lower()
                overlay_gen.load_entity(**element)
