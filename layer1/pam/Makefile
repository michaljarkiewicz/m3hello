CC=gcc
CFLAGS=-O2 -Wall -Wextra -fPIC
LDFLAGS=-shared
TARGET=pam_m3hello.so

all: $(TARGET)

pam_m3hello.o: pam_m3hello.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): pam_m3hello.o
	$(CC) $(LDFLAGS) -o $@ $^ -lpam

clean:
	rm -f *.o $(TARGET)

install: $(TARGET)
	install -D -m 0644 $(TARGET) /usr/lib/$(shell dpkg-architecture -qDEB_HOST_MULTIARCH)/security/$(TARGET)

.PHONY: all clean install
