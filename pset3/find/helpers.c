/**
 * helpers.c
 *
 * Computer Science 50
 * Problem Set 3
 *
 * Helper functions for Problem Set 3.
 */
       
#include <cs50.h>

#include "helpers.h"
/**
 * Search for value in an array using binary method
 */
 bool binarySearch(int value, int values[], int left, int right)
{
    if (left > right)
    {
        return false;
    }

    int middle = (left + right) / 2;

    if (values[middle] < value)
    {
        return binarySearch(value, values, middle + 1, right); 
    }
    else if (values[middle] > value)
    {
        return binarySearch(value, values, left, middle - 1);
    }
    else
    {
        return true;
    }
}

/**
 * Swaps two values
 */
void swap(int *a, int *b)
{
    int temp;
    temp = *b;
    *b = *a;
    *a = temp;
}

/**
 * Returns true if value is in array of n values, else false.
 */
bool search(int value, int values[], int n)
{
    return binarySearch(value, values, 0, n - 1);
}

/**
 * Sorts array of n values.
 */
void sort(int values[], int n)
{
    bool swapped;
    int counter = 0;

    do
    {
        swapped = false;
        for (int i = 0; i < n - counter - 1; i++)
        {
            if (values[i] > values[i + 1])
            {
                swap(&values[i], &values[i + 1]);
                swapped = true;
            }
        }
    } while (swapped == true);

    counter++;
}