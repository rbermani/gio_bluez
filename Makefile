GDBUS_CODEGEN := gdbus-codegen
CC := gcc
TARGET := gatt-example
PREF := /usr
PKG_CONFIG := pkg-config
PKG := gio-unix-2.0 glib-2.0 gobject-2.0 gthread-2.0 gio-2.0 gmodule-2.0
CFLAGS := -c -Werror -Wall $(shell $(PKG_CONFIG) --cflags $(PKG))
LDFLAGS := $(shell $(PKG_CONFIG) --libs $(PKG))

C_FILES := \
	bluez-gdbus.c \
	gatt-example.c

OBJECTS := $(C_FILES:.c=.o)

all: $(C_FILES) $(TARGET)
	
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

.PHONY: codegen
codegen: bluez-gdbus.c
bluez-gdbus.c bluez-gdbus.h: bluez-gdbus.xml 
	$(GDBUS_CODEGEN) --c-namespace= --generate-c-code=bluez-gdbus --c-generate-object-manager --interface-prefix=org.bluez $<

clean:
	rm bluez-gdbus.c bluez-gdbus.h gatt-example

