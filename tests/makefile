all: tests

tests: tape_test.o
	$(CC) -o tape_test ../tape.o tape_test.o
	./tape_test

tape_test.o: tape_test.c

clean:
	rm *.o *~

