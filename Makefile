#
# Zap Copyright (C) 2004-2009 Ruckus Wireless, Inc.
#

PLATFORM := $(shell echo "$${OSTYPE}" | sed 's/\(^[a-zA-Z]*\)[0-9.]*/\1/')
TARGET_DIR:=linux
ifeq ($(PLATFORM),darwin)
	TARGET_DIR:=macintosh
endif

all:	bin/$(TARGET_DIR) bin/$(TARGET_DIR)/zap bin/$(TARGET_DIR)/zapd

CFLAGS= -O2 -m32 -static
CC=	$(TOOLPREFIX)gcc $(CFLAGS)

bin/$(TARGET_DIR):
	rm -rf bin/$(TARGET_DIR)
	mkdir -p bin
	mkdir -p bin/$(TARGET_DIR)

bin/$(TARGET_DIR)/zap : zap/zap.c zaplib/zaplib.c zaplib/zaplib.h
	$(CC)  -o $@ zap/zap.c zaplib/zaplib.c zaplib/error.c -Izap -Izaplib

bin/$(TARGET_DIR)/zapd : zapd/zapd.c zaplib/zaplib.c zaplib/zaplib.h
	$(CC)  -o $@ zapd/zapd.c zaplib/zaplib.c zaplib/error.c -Izap -Izaplib 



install : bin/linux/zap 
	cp bin/$(TARGET_DIR)/zap 	$(DESTDIR)/usr/bin
	cp bin/$(TARGET_DIR)/zapd	$(DESTDIR)/usr/bin
	chmod +s	$(DESTDIR)/usr/bin/zap
	chmod +s	$(DESTDIR)/usr/bin/zapd

clean :
	rm -rf bin/$(TARGET_DIR)
