#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

int main(void){
    char entry[500];
    fgets(entry, 500, stdin);
    printf("entrée = %s\n", entry);
    return 0;  
}
