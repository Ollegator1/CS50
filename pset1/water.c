#include <stdio.h>
#include <cs50.h>


int main(void){
    printf("how many time do you spend in shower? in minutes: ");
    int t = GetInt();
    
    printf("minutes: %d \nbottles: %d\n", t, t*12);
}