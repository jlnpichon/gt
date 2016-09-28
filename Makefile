CFLAGS=-g -I./ -D_GNU_SOURCE
all:
	gcc -Wall $(CFLAGS) -c buffer.c -o buffer.o
	gcc -Wall $(CFLAGS) -c common.c -o common.o
	gcc -Wall $(CFLAGS) -c index.c -o index.o
	gcc -Wall $(CFLAGS) cat-file.c -o cat-file common.o index.o -lcrypto -lz
	gcc -Wall $(CFLAGS) commit-tree.c -o commit-tree buffer.o common.o index.o -lcrypto -lz
	gcc -Wall $(CFLAGS) diff.c -o diff common.o index.o -lcrypto -lz
	gcc -Wall $(CFLAGS) hash-blob.c -o hash-blob common.o index.o -lcrypto -lz
	gcc -Wall $(CFLAGS) ls-files.c -o ls-files common.o index.o -lcrypto -lz
	gcc -Wall $(CFLAGS) update-index.c -o update-index common.o index.o -lcrypto -lz
	gcc -Wall $(CFLAGS) write-tree.c -o write-tree buffer.o common.o index.o -lcrypto -lz

clean:
	-@rm -f *.o cat-file commit-tree diff hash-blob ls-files update-index write-tree
