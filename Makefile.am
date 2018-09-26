sbin_PROGRAMS = nodemanagerd

nodemanagerd_SOURCES = nodemanagerd.cpp
nodemanagerd_LDFLAGS = $(SYSTEMD_LIBS) \
                  $(SDBUSPLUS_LIBS)
nodemanagerd_CFLAGS = $(SYSTEMD_CFLAGS) \
                 $(SDBUSPLUS_CFLAGS)
