#include <stdio.h>

int main(void) {
    printf("Hello world!\n");
    
    while(1) {
        sleep(5);
        printf("wakeup\n");
    }


    return 0;
}