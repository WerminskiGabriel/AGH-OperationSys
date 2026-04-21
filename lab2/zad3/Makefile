CC = gcc
CFLAGS = -Wall -fPIC
LDFLAGS = -L. -Wl,-rpath,.
LDLIBS = -lsignals
LIB_SRCS = sig_default.c sig_handle.c sig_ignore.c sig_mask.c
OBJS = $(LIB_SRCS:.c=.o)

.PHONY: all clean

all: main child_static child_shared child_dynamic

libsignals.a: $(OBJS)
	ar rcs $@ $^

libsignals.so: $(OBJS)
	$(CC) -shared -o $@ $^

main: main.c
	$(CC) $(CFLAGS) $< -o $@

child_static: child.c libsignals.a
	$(CC) $(CFLAGS) $< libsignals.a -o $@

child_shared: child.c libsignals.so
	$(CC) $(CFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

child_dynamic: child.c libsignals.so
	$(CC) $(CFLAGS) -DDLL $< -o $@ -ldl

clean:
	rm -f *.o *.a *.so main child_static child_shared child_dynamic