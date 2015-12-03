CC = gcc
CFLAGS = -fno-stack-protector -Wall -Iutil -g -Iatm -Ibank -Irouter -I. -DLINUX -I/usr/local/ssl/include -L/usr/local/ssl/lib
LIBS = -lssl -lcrypto -ldl

all: bin/atm bin/bank bin/router bin/init test

bin/atm : atm/atm-main.c atm/atm.c util/list.c util/hash_table.c
	${CC} ${CFLAGS} atm/atm.c atm/atm-main.c -o bin/atm  util/list.c util/hash_table.c ${LIBS}

bin/bank : bank/bank-main.c bank/bank.c util/list.c util/hash_table.c
	${CC} ${CFLAGS} bank/bank.c bank/bank-main.c -o bin/bank util/list.c util/hash_table.c ${LIBS}

bin/router : router/router-main.c router/router.c
	${CC} ${CFLAGS} router/router.c router/router-main.c -o bin/router ${LIBS}

bin/init : init.c
	${CC} ${CFLAGS} init.c -o bin/init ${LIBS}

test : util/list.c util/list_example.c util/hash_table.c util/hash_table_example.c
	${CC} ${CFLAGS} util/list.c util/list_example.c -o bin/list-test ${LIBS}
	${CC} ${CFLAGS} util/list.c util/hash_table.c util/hash_table_example.c -o bin/hash-table-test ${LIBS}

clean:
	cd bin && rm -f atm bank router list-test hash-table-test
