import unittest2 as unittest
from .. import platform_scan
from .. import fru_device
import os

THIS_DIR = os.path.dirname(os.path.realpath(__file__))


class TestPlatformScan(unittest.TestCase):

    def setUp(self):
        self.fru_dev = fru_device.FruDeviceProbe()
        #self.fru_dev.children = [{'device': 87, 'bus': 1, 'fru': bytearray(b'\x01\x01\x00\x02\x00\x00\x00\xfc\x01\x00\x00\x00\x00\x00\x00\x01\x01\t\x00\xc8U\xa6\xd1Intel Corporation\xc7FFPANEL\xccBQRU60201108\xcaG28538-252\xccFRU Ver 0.01\xc1\x00s')}, {'device': 113, 'bus': 2, 'fru': bytearray(b'\x00\x00\x00\x00\x00\x00\x00\x00')}, {'device': 114, 'bus': 2, 'fru': bytearray(b'\x00\x00\x00\x00\x00\x00\x00\x00')}, {'device': 115, 'bus': 2, 'fru': bytearray(b'\x00\x00\x00\x00\x00\x00\x00\x00')}, {'device': 80, 'bus': 7, 'fru': bytearray(b'\x01\x00\x00\x00\x01\x0c\x00\xf2\x01\x0b\x19\xe1SAMSUNG ELECTRO-MECHANICS CO.,LTD\xcbPSSF132202A\xcaH79286-001\xc3S02\xd1CNS1322A4SG1U0112\x00\x00\xc1\x00\x00\xa5\x00\x02\x18\x9dI\x14\x05t\x08#\x05(#\xb06P')}, {'device': 81, 'bus': 7, 'fru': bytearray(b'\x01\x00\x00\x00\x01\x0c\x00\xf2\x01\x0b\x19\xe1SAMSUNG ELECTRO-MECHANICS CO.,LTD\xcbPSSF132202A\xcaH79286-001\xc3S02\xd1CNS1322A4SG1U0425\x00\x00\xc1\x00\x00\x9e\x00\x02\x18\x9dI\x14\x05t\x08#\x05(#\xb06P')}]
        self.fru_dev.children = [{'device': 80, 'bus': '7', 'formatted': {'PRODUCT_PRODUCT_VERSION': 'S02', 'PRODUCT_PRODUCT_SERIAL_NUMBER': 'CNS1322A4SG1U0112', 'PRODUCT_PRODUCT_NAME': 'PSSF132202A', 'PRODUCT_MANUFACTURER': 'SAMSUNG ELECTRO-MECHANICS CO.,LTD', 'PRODUCT_ASSET_TAG': '', 'PRODUCT_PART_NUMBER': 'H79286-001', 'PRODUCT_LANGUAGE_CODE': '25', 'Common Format Version': '1'}, 'fru': bytearray(b'\x01\x00\x00\x00\x01\x0c\x00\xf2\x01\x0b\x19\xe1SAMSUNG ELECTRO-MECHANICS CO.,LTD\xcbPSSF132202A\xcaH79286-001\xc3S02\xd1CNS1322A4SG1U0112\x00\x00\xc1\x00\x00\xa5\x00\x02\x18\x9dI\x14\x05t\x08#\x05(#\xb06P')}, {'device': 81, 'bus': '7', 'formatted': {'PRODUCT_PRODUCT_VERSION': 'S02', 'PRODUCT_PRODUCT_SERIAL_NUMBER': 'CNS1322A4SG1U0425', 'PRODUCT_PRODUCT_NAME': 'PSSF132202A', 'PRODUCT_MANUFACTURER': 'SAMSUNG ELECTRO-MECHANICS CO.,LTD', 'PRODUCT_ASSET_TAG': '', 'PRODUCT_PART_NUMBER': 'H79286-001', 'PRODUCT_LANGUAGE_CODE': '25', 'Common Format Version': '1'}, 'fru': bytearray(b'\x01\x00\x00\x00\x01\x0c\x00\xf2\x01\x0b\x19\xe1SAMSUNG ELECTRO-MECHANICS CO.,LTD\xcbPSSF132202A\xcaH79286-001\xc3S02\xd1CNS1322A4SG1U0425\x00\x00\xc1\x00\x00\x9e\x00\x02\x18\x9dI\x14\x05t\x08#\x05(#\xb06P')}, {'device': 80, 'bus': '2', 'formatted': {'BOARD_MANUFACTURER': '', 'BOARD_PART_NUMBER': 'Intel Corporation', 'BOARD_SERIAL_NUMBER': '', 'BOARD_VERSION_ID': 'F1UL16', 'Common Format Version': '1', 'BOARD_LANGUAGE_CODE': '0', 'BOARD_PRODUCT_NAME': '', 'BOARD_MANUFACTURE_DATE': '\x00\x00\x00\x00'}, 'fru': bytearray(b'\x01\x01\x00\x02\x00\x00\x00\xfc\x10\x00\x00\x00\x00\x00\x00\x01\x10\n\x00\x00\x00\x00\x00\x00\x00\xd1Intel Corporation\xc6F1UL16\xcc............\xca..........\xccFRU Ver 0.04\xc0\xc1\xff\xff\xff\xff\xff\xff\xff')}, {'device': 81, 'bus': '2', 'formatted': {'BOARD_MANUFACTURER': '', 'BOARD_PART_NUMBER': 'Intel Corporation', 'BOARD_SERIAL_NUMBER': '', 'BOARD_VERSION_ID': 'F1UL16', 'Common Format Version': '1', 'BOARD_LANGUAGE_CODE': '0', 'BOARD_PRODUCT_NAME': '', 'BOARD_MANUFACTURE_DATE': '\x00\x00\x00\x00'}, 'fru': bytearray(b'\x01\x01\x00\x02\x00\x00\x00\xfc\x10\x00\x00\x00\x00\x00\x00\x01\x10\n\x00\x00\x00\x00\x00\x00\x00\xd1Intel Corporation\xc6F1UL16\xcc............\xca..........\xccFRU Ver 0.04\xc0\xc1\xff\xff\xff\xff\xff\xff\xff')}, {'device': 113, 'bus': '2', 'formatted': {'Common Format Version': '0'}, 'fru': bytearray(b'\x00\x00\x00\x00\x00\x00\x00\x00')}, {'device': 114, 'bus': '2', 'formatted': {'Common Format Version': '0'}, 'fru': bytearray(b'\x00\x00\x00\x00\x00\x00\x00\x00')}, {'device': 115, 'bus': '2', 'formatted': {'Common Format Version': '0'}, 'fru': bytearray(b'\x00\x00\x00\x00\x00\x00\x00\x00')}, {'device': 87, 'bus': '1', 'formatted': {'BOARD_MANUFACTURER': 'Intel Corporation', 'BOARD_PART_NUMBER': 'G28538-252', 'BOARD_SERIAL_NUMBER': 'BQRU60201108', 'BOARD_VERSION_ID': 'FRU Ver 0.01', 'Common Format Version': '1', 'BOARD_LANGUAGE_CODE': '0', 'BOARD_PRODUCT_NAME': 'FFPANEL', 'BOARD_MANUFACTURE_DATE': '\xc8U\xa6\xd1'}, 'fru': bytearray(b'\x01\x01\x00\x02\x00\x00\x00\xfc\x01\x00\x00\x00\x00\x00\x00\x01\x01\t\x00\xc8U\xa6\xd1Intel Corporation\xc7FFPANEL\xccBQRU60201108\xcaG28538-252\xccFRU Ver 0.01\xc1\x00s')}]
        self.fru_dev.append_baseboard_fru(os.path.join(THIS_DIR, 'S2600WFT.fru.bin'))
        for fru in self.fru_dev.children:
            fru['formatted'] = fru_device.read_fru(fru['fru'])

    def test_parse_configuration(self):
        p_scan = platform_scan.PlatformScan()
        p_scan.fru = self.fru_dev
        records = p_scan.parse_configuration()
        enabled = 0
        #for key, rec in records.iteritems():
        #    if isinstance(rec, dict) and rec['status'] == 'okay':
        #        enabled += 1
        print 'found {} records, {} enabled'.format(len(records), enabled)

    def test_read_config(self):
        self.test_parse_configuration()
        self.assertTrue(platform_scan.PlatformScan().read_config())
