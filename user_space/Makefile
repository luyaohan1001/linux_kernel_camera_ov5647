CC=gcc

CFLAGS+=-Wall 


target:
	$(CC) $(CFLAGS) main.c -o main

.PHONY: setup run clean flip-vertical flip-horizontal

# Setup build environment.
setup:
	apt-get install libv4l-dev -y
	apt install doxygen -y
	apt install clang-format -y

flip-vertical:
	v4l2-ctl --set-ctrl vertical_flip=1

flip-horizontal:
	v4l2-ctl --set-ctrl horizontal_flip=1

take-photo: target flip-vertical flip-horizontal
	sudo modprobe ov5647
	./main

clean:
	rm -rf main

format:
	clang-format -i ./*.[ch]
