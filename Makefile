all: clean demod

clean:
	rm -f demod

demod:
	gcc -Wall -Wextra -O3 -march=native -mtune=native demod.c -o demod -lm
