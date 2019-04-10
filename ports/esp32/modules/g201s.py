# -*- coding: utf-8 -*-
import upygatt
from binascii import hexlify
import time

class G201S():
    def __init__(self, address):
        self.address = address
        self.adapter = upygatt.GATTToolBackend()
        self.index = 0
        self.cur_temp = 0
        self.bt_is_started = False

    def write_handle(self, handle, value, increment=True):
        #global index
        val = []
        if handle == 0x000e:
            val.append(0x55)
            val.append(self.index)
            for i in value:
                val.append(i)
            val.append(0xAA)
        else:
            for i in value:
                val.append(i)

        val = bytearray(val)
        self.adapter.char_write_handle(handle=handle, value=val, wait_for_response=True)
        if increment: self.index += 1

    def turn_on(self):
        self.index = 0
        try:
            self.adapter.start()
            time.sleep_ms(500)
            self.bt_is_started = True
            while self.adapter.connect(self.address) not 0:
                print('Connection not 0')
                pass

        finally:
            self.write_handle(0x000e, [0xFF, 0xDF, 0x24, 0x0E, 0xC6, 0x94, 0xD1, 0x97, 0x43], False)
            self.write_handle(0x000c, [0x01, 0x00])
            self.write_handle(0x000e, [0x01])
            self.write_handle(0x000e, [0x05, 0x00, 0x00, int(hex(95), 16), 0x00])
            self.write_handle(0x000e, [0x03])
            self.adapter.disconnect(self.address)
            self.adapter.stop()

    def turn_off(self):
        self.index = 0
        try:
            self.adapter.start()
            time.sleep_ms(500)
            self.bt_is_started = True
            while self.adapter.connect(self.address) not 0:
                print('Connection not 0')
                pass

        finally:
            self.write_handle(0x000e, [0xFF, 0xDF, 0x24, 0x0E, 0xC6, 0x94, 0xD1, 0x97, 0x43], False)
            self.write_handle(0x000c, [0x01, 0x00])
            self.write_handle(0x000e, [0x01])
            self.write_handle(0x000e, [0x04])
            #self.cur_temp = self.get_temperature()
            #print("Tried to switch off, returned {}".format(self.cur_temp))
            self.adapter.disconnect(self.address)
            self.adapter.stop()

    def set_temperature(self, value):
        self.index = 0
        print("set temperature {}".format(value))
        # try:
        #     adapter.start()
        #     time.sleep_ms(500)
        #     device = adapter.connect(self.address, address_type=ADDRESS_TYPE)
        #     self.write_handle(device, 0x000e, [0xFF, 0xDF, 0x24, 0x0E, 0xC6, 0x94, 0xD1, 0x97, 0x43], False)
        #     self.write_handle(device, 0x000c, [0x01, 0x00])
        #     self.write_handle(device, 0x000e, [0x01])
        #     self.write_handle(device, 0x000e, [0x05, 0x00, 0x00, int(hex(value), 16), 0x00])
        #     self.write_handle(device, 0x000e, [0x03])
        #     self.get_temperature()

        # finally:
        #     adapter.stop()
        if self.get_temperature() < value:
            self.turn_on()
            while True:
                if self.get_temperature() == value:
                    self.turn_off()
                    break
        else:
            print('{}°C'.format(self.get_temperature()))

    def get_temperature(self):
        self.index = 0
        try:
            self.adapter.start()
            time.sleep_ms(500)
            self.bt_is_started = True
            while self.adapter.connect(self.address) not 0:
                print('Connection not 0')
                pass
        finally:
            self.write_handle(0x000e, [0xFF, 0xDF, 0x24, 0x0E, 0xC6, 0x94, 0xD1, 0x97, 0x43], False)
            self.write_handle(0x000c, [0x01, 0x00])
            self.write_handle(0x000e, [0x01])
            self.write_handle(0x000e, [0x06])
            print("get temperature")
            value = self.adapter.char_read("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
            value = hexlify(value)
            value = value[16:18]
            self.cur_temp = int(value, 16)
            print("Current temperature of kettle is {}".format(self.cur_temp))
            self.adapter.disconnect(self.address)
            self.adapter.stop()
            return self.cur_temp

    def _update_sensor_data(self):
        print("update")
        return self.cur_temp
