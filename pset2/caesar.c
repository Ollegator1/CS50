/**
 * CS50 Problem set 2
 * Caesar Ciphers
 * User: Oleg Konov
 * Date: 20.09.2016
 */

#include <cs50.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, string argv[])  
{
    int k = 0;
    string p;
    
    // Check if correct # of arguments given
    if (argc != 2 && !isdigit(argv[1]))
    {
        printf ("Wrong number of arguments. Please try again.\n");
        return 1;
    }
    else 
    {// Convert input to int type
    k = atoi(argv[1]);
    }
    
    // Get text to encode
    p = GetString();

    // Loop through text
    for (int i = 0, n = strlen(p); i < n; i++)
    {            
        // Keep case of letter
        if (isupper(p[i]))
        {
            // Get modulo number and add to appropriate case
            printf("%c", 'A' + (p[i] - 'A' + k) % 26);
        }
        else if (islower(p[i]))
        {
            printf("%c", 'a' + (p[i] - 'a' + k) % 26);
        }
        else
        {
            // return unchanged
            printf("%c", p[i]);
        }
    }

    printf("\n");

    return 0;
}