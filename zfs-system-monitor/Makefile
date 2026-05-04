CC = clang
CFLAGS = `pkg-config --cflags gtk4 libadwaita-1 gtk4-layer-shell-0` -O3
LIBS = `pkg-config --libs gtk4 libadwaita-1 gtk4-layer-shell-0` -lm
TARGET = zfs-system-monitor-layer
SRC = zfs-system-monitor.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)

install: all
	sudo cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
