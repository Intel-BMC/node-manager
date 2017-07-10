import os 
import subprocess
import json 

unknown_i2c_devices = []
fru_devices = []


class FRU


def run_command(command):
    print command
    return subprocess.check_output(command, shell=True)
    
for bus_index in range(1,8):
    try:
        result = run_command("i2cdetect -y -r {}".format(bus_index))
        print result 
        device_index = 0
        for line in result.split("\n")[1:]:
            line = line[4:]
            for device in [line[i:i+3] for i in range(0, len(line), 3)]:
                
                if device == "-- ":
                    pass
                elif device == "   ":
                    pass
                else:
                    print("found {:02X}".format(device_index))
                    unknown_i2c_devices.append((bus_index, device_index))
                device_index += 1
    except subprocess.CalledProcessError:
        pass  #TODO(ed) only call valid busses

items_to_remove = []
for element in unknown_i2c_devices:
    i2c_bus_index = element[0]
    i2c_device_index = element[1]
    try:
        print "querying bus: {} device {:02X}".format(i2c_bus_index,i2c_device_index)
        result = run_command("i2cget -f -y {} 0x{:02X} 0x00 i 0x8".format(i2c_bus_index, i2c_device_index))
        print "got {}".format(result)
        result_bytes = [int(x[2:], 16) for x in result.split(" ")[1:]]
        csum = 256 - sum(result_bytes[0:7]) & 0xFF
        print "result_bytes {}".format(result_bytes)
        out = {}
        if csum == result_bytes[7]:
            print "found valid fru bus: {} device {:02X}".format(i2c_bus_index,i2c_device_index)
            for index, area in enumerate(("INTERNAL", "CHASSIS", "BOARD", "PRODUCT", "MULTIRECORD")):                   
                area_offset = result_bytes[index + 1]
                if area_offset != 0:
                    area_offset = area_offset * 8
                    print "offset 0x{:02X}".format(area_offset)
                    result = run_command("i2cget -f -y {} 0x{:02X} 0x{:02X} i 0x2".format(i2c_bus_index, i2c_device_index, area_offset))
                    print "got {}".format(result)
                    area_bytes = [int(x[2:], 16) for x in result.split(" ")[1:]]
                    format = area_bytes[0]
                    length = area_bytes[1] * 8
                    if length != 0 :
                        area_bytes = []
                        while (length > 0):
                            to_get = min(0x20, length)
                            result = run_command("i2cget -f -y {} 0x{:02X} 0x{:02X} i 0x{:02X}".format(i2c_bus_index, i2c_device_index, area_offset, to_get))
                            area_offset += to_get
                            area_bytes.extend([int(x[2:], 16) for x in result.split(" ")[1:]])
                            length -= to_get

                        offset = 2
                        if area == "CHASSIS":                                                                               
                            field_names = ("PART_NUMBER", "SERIAL_NUMBER", "CHASSIS_INFO_AM1", "CHASSIS_INFO_AM2")          
                            out["CHASSIS_TYPE"] = area_bytes[offset]                                                         
                            offset += 1                                                                                     
                        elif area == "BOARD":                                                                               
                            field_names = ("MANUFACTURER", "PRODUCT_NAME", "SERIAL_NUMBER", "PART_NUMBER", "VERSION_ID")    
                            out["BOARD_LANGUAGE_CODE"] = area_bytes[offset]                                                  
                            offset += 1                                                                                     
                            out["BOARD_MANUFACTURE_DATE"] = area_bytes[offset:offset+4]                                      
                            offset += 3                                                                                     
                        elif area == "PRODUCT":                                                                             
                            field_names = ("MANUFACTURER", "PRODUCT_NAME", "PART_NUMBER", "PRODUCT_VERSION",                
                                        "PRODUCT_SERIAL_NUMBER", "ASSET_TAG")                                            
                            out["PRODUCT_LANGUAGE_CODE"] = area_bytes[offset]                                                
                            offset += 1                                                                                     
                        else:                                                                                               
                            field_names = ()                                                                                
                                                                                                                            
                        for field in field_names:                                                                           
                            length = area_bytes[offset] & 0x3f                                                               
                            out[area + "_" + field] = ''.join(chr(x) for x in area_bytes[offset+1:offset+1+length]).rstrip()
                            offset += length + 1 
                            if offset >= len(area_bytes):
                                print "Not long enough"
                                break

            print json.dumps(out, sort_keys=True, indent=4)
            fru_devices.append((i2c_bus_index, i2c_device_index, out))
            items_to_remove.append(element)
        print ""

    except subprocess.CalledProcessError:
        pass  #TODO(ed) only call valid busses


unknown_i2c_devices = filter(lambda x: x not in items_to_remove, unknown_i2c_devices)

for element in unknown_i2c_devices:
    print("unknown bus:{} device:{:02X}".format(element[0], element[1]))