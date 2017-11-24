#pragma once
int configGpio(const char *gpioName, int *fd, sdbusplus::bus::bus &bus);
int closeGpio(int fd);