#include <cs50.h>
#include <stdio.h>

int main(void)
{
   // Declare and intialize variables
	int height;
	int i, j, k;
	
	// Grab user input, check it and ask for retry if not correct  
	do
	{
		printf("Height: ");
		height = GetInt();
		if (height == 0)
		{
		    return 0;
		}
    }
	while (height < 0 || height > 23);

	// Print lines
	for(i = 0; i < height; i++) 
	{
		// Print the required spaces
        for(j = 0; j < (height-i-1); j++)
        {
            printf("%s", " ");
        }
        // Print the '#' character.
		for(k = 0; k < i+2; k++)
		{
			printf("#");
		}
		printf("\n");
	}	
	return 0;
}