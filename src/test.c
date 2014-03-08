#include <stdio.h>

void main(void) {

	int pids[] = { 33, 66, 99, 0};
	void **data = (void *)pids;
	
	printf("second num %d", ((int *)data)[2]);

	
}
