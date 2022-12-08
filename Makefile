all: hw2_exc

hw2_exc: hw2_315393462_321014763.c
	gcc -pthread -g hw2_315393462_321014763.c -o hw2

make clean:
	\rm hw2

