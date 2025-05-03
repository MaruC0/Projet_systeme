#include <stdio.h>
#include <stdlib.h>

int main(void){
    char entry[500];
    fgets(entry, 500, stdin);
    printf("entrée = %s\n", entry);
    return 0;  
}
