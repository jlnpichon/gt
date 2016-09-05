CFLAGS=-g -I./ -D_GNU_SOURCE
all:
	gcc -Wall $(CFLAGS) -c common.c -o common.o
	gcc -Wall $(CFLAGS) -c index.c -o index.o
	gcc -Wall $(CFLAGS) cat-file.c -o cat-file common.o index.o -lcrypto -lz
	gcc -Wall $(CFLAGS) hash-blob.c -o hash-blob common.o index.o -lcrypto -lz
	gcc -Wall $(CFLAGS) update-index.c -o update-index common.o index.o -lcrypto -lz

clean:
	rm *.o update-index hash-blob
