#!/usr/bin/python

import os
from fru_device import FruDeviceProbe
import json
import overlay_gen
import glob
import traceback
from utils.generic import prep_json, dict_template_replace
import warnings

TEMPLATE_CHAR = '$'
CONFIGURATION_STORE = os.environ.get('JSON_STORE', '/var/configuration/system.json')


class PlatformScan(object):
    def __init__(self):
        self.fru = None
        self.found_devices = []
        self.configuration_dir = os.environ.get('CONFIGURATION_DIR', '/usr/share/configurations')

    def parse_fru(self):
        self.fru = FruDeviceProbe()

    @staticmethod
    def is_overlay(d):
        return 'connector' in list(d)

    def apply_overlay(self, d, name):
        if 'connector' not in list(d):
            return False
        for _, entity in self.found_devices.iteritems():
            for key, child in entity['exposes'].iteritems():
                if key == d['connector']:
                    for k, val in d.iteritems():
                        child[k] = val
                    child['name'] = name
                    child['status'] = 'okay'  # todo, should this go here?
                    return True
        return False

    def read_config(self):
        try:
            with open(CONFIGURATION_STORE) as config:
                self.found_devices = json.load(config)
        except IOError:
            return False
        return self.found_devices

    def write_config(self):
        try:
            if not os.path.exists(os.path.dirname(CONFIGURATION_STORE)):
                os.makedirs(os.path.dirname(CONFIGURATION_STORE))
            with open(CONFIGURATION_STORE, 'w') as config:
                json_config = json.dumps(self.found_devices, indent=4)
                config.write(json_config)
        except IOError:
            warnings.warn('Failed to write configuration.')

    def parse_configuration(self):

        available_entity_list = []
        self.found_devices = {}

        for json_filename in glob.glob(os.path.join(self.configuration_dir, "*.json")):
            with open(os.path.join(self.configuration_dir, json_filename)) as json_file:
                clean_file = prep_json(json_file)
                try:
                    available_entity_list.append(json.load(clean_file))
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
                        removals = set()
                        for key, item in entity.get('exposes', []).iteritems():
                            if self.is_overlay(item):
                                assert (self.apply_overlay(item, key))
                                removals.add(key)
                            else:
                                if TEMPLATE_CHAR in str(item):
                                    # todo populate this dict smarter
                                    for ii in range(0, count):
                                        replaced = dict_template_replace(item, {'bus': found_dev[0]['bus'],
                                                                                'fruaddress': hex(found_dev[0]['device']),
                                                                                'index': ii})
                                        if 'status' not in replaced:
                                            replaced['status'] = 'okay'
                                        entity['exposes'][key] = replaced
                                else:
                                    replaced = item
                                    if 'status' not in replaced:
                                        replaced['status'] = 'okay'
                                    entity['exposes'][key] = replaced
                        self.found_devices[entity['name']] = entity
                        for r in removals:
                            entity['exposes'].pop(r)

            if len(self.found_devices) == num_devices:
                break  # exit after looping without additions
        self.write_config()
        return self.found_devices


if __name__ == '__main__':
    platform_scan = PlatformScan()
    found_devices = platform_scan.read_config()
    if not found_devices:
        platform_scan.parse_fru()
        found_devices = platform_scan.parse_configuration()

    overlay_gen.unload_overlays()  # start fresh

    for pk, pvalue in found_devices.iteritems():
        for key, element in pvalue.get('exposes', {}).iteritems():
            if not isinstance(element, dict) or element.get('status', 'disabled') != 'okay':
                continue
            element['oem_name'] = element.get('name', key).replace(' ', '_')
            if element.get("type", "") == "TMP75":
                element["reg"] = element.get("address").lower()
                # todo(ed) find a better escape function to use.
                overlay_gen.load_entity(**element)

            elif element.get("type", "") == "TMP421":
                element["reg"] = element.get("address").lower()
                element["oem_name1"] = element.get("name1").replace(" ", "_").lower()
                overlay_gen.load_entity(**element)

            elif element.get("type", "") == "ADC":
                overlay_gen.load_entity(**element)

            elif element.get("type", "") == "AspeedFan":
                element["type"] = 'aspeed_pwmtacho'
                overlay_gen.load_entity(**element)
