### Makefile --- 

## Author: Gaspar Fernández <blakeyed@totaki.com>
## Version: $Id: Makefile,v 0.0 2016/10/16 13:12:00 gaspy Exp $
## Keywords: 
## X-URL: 

CC=g++
CFLAGS=-O4 -std=c++11 -g
PLUGINFLAGS=-fPIC -shared
LIBS=-lpthread -lssl -lcrypto -lz -ldl -lsqlite3

SRCFILES =  lib/daemon.cpp	 			\
	lib/daemon_services.cpp					\
	lib/axond_module_info.cpp				\
	lib/axond_auth.cpp							\
	lib/config.cpp									\
	lib/cutils.cpp									\
	lib/glove/glove.cpp							\
  lib/glove/glovecoding.cpp				\
	lib/glove/glovehttpserver.cpp		\
	lib/glove/glovehttpcommon.cpp		\
	lib/glove/glovewebsockets.cpp		\
	lib/data/sqlitebase.cpp					\
	lib/data/safedb.cpp							\
	lib/services/users.cpp					\
	lib/services/groups.cpp					\
	lib/services/modules.cpp				\
	lib/os.cpp

SOURCES=$(SRCFILES)
PLUGINS=plugins/sshkeys.so plugins/messages.so plugins/exec.so plugins/machines.so plugins/files.so
OBJECTS=$(SOURCES:.cpp=.o)
INCLUDES=

all: axond capi $(SOURCES) $(EXECUTABLE) plugins

axond: $(OBJECTS)
	$(CC) axond.cc $(CFLAGS) $(INCLUDES) $(OBJECTS) $(LIBS) -o $@

capi: $(OBJECTS)
	$(CC) capi.cc $(CFLAGS) $(INCLUDES) $(OBJECTS) $(LIBS) -o $@

.cpp.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

.cc.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

plugins: $(PLUGINS)

$(PLUGINS):
	$(CC) $(PLUGINFLAGS) $(CFLAGS) $(@:.so=.cc) -o $@

plugins/exec.so:
	$(CC) $(PLUGINFLAGS) -c lib/os.cpp $(CFLAGS) $(INCLUDES) -o lib/os_fpic.o
	$(CC) $(PLUGINFLAGS) $(CFLAGS) lib/os_fpic.o $(@:.so=.cc) -o $@

plugins/files.so:
	$(CC) $(PLUGINFLAGS) -c lib/glove/glovecoding.cpp $(CFLAGS) $(INCLUDES) -o lib/glove/glovecoding_fpic.o
	$(CC) $(PLUGINFLAGS) -c lib/silicon/silicon.cpp $(CFLAGS) $(INCLUDES) -o lib/silicon/silicon_fpic.o
	$(CC) $(PLUGINFLAGS) $(CFLAGS) lib/silicon/silicon_fpic.o lib/glove/glovecoding_fpic.o $(@:.so=.cc) -o $@

plugins/messages.so:
	$(CC) $(PLUGINFLAGS) -c lib/mailer.cpp $(CFLAGS) $(INCLUDES) -o lib/mailer_fpic.o
	$(CC) $(PLUGINFLAGS) -c lib/cutils.cpp $(CFLAGS) $(INCLUDES) -o lib/cutils_fpic.o
	$(CC) $(PLUGINFLAGS) $(CFLAGS) lib/mailer_fpic.o lib/cutils_fpic.o $(@:.so=.cc) -o $@

clean:
	rm -rf $(OBJECTS)
	rm -rf capi.o
	rm -rf axond.o
	rm -rf capi
	rm -rf axond

### Makefile ends here
