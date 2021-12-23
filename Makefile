CFLAGS := -g -Wall
LINKS := -lwayland-client -lrt
LINKS2 := -lwayland-client -lwayland-cursor -lrt

CCMD := $(CC) $(CFLAGS)

TARGETS := a.out

####

.PHONY: all clean

all: $(TARGETS)

clean:
	-rm -f $(TARGETS) *.o
	rm xdg-shell-client-protocol.h
	rm xdg-shell-protocol.c
	rm text-input-unstable-v3-client-protocol.h
	rm text-input-unstable-v3-protocol.c

%.o: %.c
	$(CCMD) -c -o $@ $<

a.out: main.c client.o imagebuf.o
	$(CCMD) -o $@ $^ $(LINKS) xdg-shell-protocol.o text-input-unstable-v3-protocol.o

protocols: xdg-shell-protocol.o text-input-unstable-v3-protocol.o

xdg-shell-protocol.o:
	wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
	wayland-scanner public-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
	$(CC) -c xdg-shell-protocol.c

text-input-unstable-v3-protocol.o:
	wayland-scanner client-header /usr/share/wayland-protocols/unstable/text-input/text-input-unstable-v3.xml text-input-unstable-v3-client-protocol.h
	wayland-scanner public-code /usr/share/wayland-protocols/unstable/text-input/text-input-unstable-v3.xml text-input-unstable-v3-protocol.c
	$(CC) -c text-input-unstable-v3-protocol.c

