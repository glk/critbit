CFLAGS:= -std=gnu99 -Wall -Wno-unused -fno-strict-aliasing -g -I.

TARGETS:= critbit-test

all: ${TARGETS}

.PHONY: clean
clean:
	rm -f ${TARGETS}

critbit-test: critbit.c critbit.h critbit-test.c
	${CC} ${CFLAGS} -o $@ $(filter %.c,$^)
