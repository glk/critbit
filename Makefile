PROG= critbit-test
SRCS= critbit.c critbit.h critbit-test.c

DEBUG_FLAGS=-g
WARNS=6

NO_MAN=

.include <bsd.prog.mk>
