#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])

{
	printf("short is %d chars\n", sizeof(short));
	printf("int is %d chars\n", sizeof(int));
	printf("unsigned int is %d chars\n", sizeof(unsigned int));
	printf("long is %d chars\n", sizeof(long));
	printf("float is %d chars\n", sizeof(float));
	printf("double is %d chars\n", sizeof(double));

	printf("in_addr_t is %d chars\n", sizeof(in_addr_t));
}
