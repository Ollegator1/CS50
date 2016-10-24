/********************************
 * resize.c
 *
 * CS 50
 * Problem Set 4
 * 
 * Ollegator1
 * 23/10/2016
 * 
 * Resize a bmp by a factor of n
 *******************************
 */
#include <stdio.h>
#include <stdlib.h>

#include "bmp.h"

int main(int argc, char* argv[])
{
    // ensure proper usage
    if (argc != 4)
    {
        printf("Usage: ./resize factor infile outfile\n");
        return 1;
    }

    // remember factor and filenames
    int factor = atoi(argv[1]);
    char* infile = argv[2];
    char* outfile = argv[3];

    // open input file 
    FILE* inptr = fopen(infile, "r");
    if (inptr == NULL)
    {
        printf("Could not open %s.\n", infile);
        return 2;
    }

    // open output file
    FILE* outptr = fopen(outfile, "w");
    if (outptr == NULL)
    {
        fclose(inptr);
        fprintf(stderr, "Could not create %s.\n", outfile);
        return 3;
    }

    // read infile's BITMAPFILEHEADER
    BITMAPFILEHEADER bf;
    fread(&bf, sizeof(BITMAPFILEHEADER), 1, inptr);

    // read infile's BITMAPINFOHEADER
    BITMAPINFOHEADER bi;
    fread(&bi, sizeof(BITMAPINFOHEADER), 1, inptr);

    // ensure infile is (likely) a 24-bit uncompressed BMP 4.0
    if (bf.bfType != 0x4d42 || bf.bfOffBits != 54 || bi.biSize != 40 || 
        bi.biBitCount != 24 || bi.biCompression != 0)
    {
        fclose(outptr);
        fclose(inptr);
        fprintf(stderr, "Unsupported file format.\n");
        return 4;
    }

    // copy bf and bi into bf_mod & bi_mod to preserve original header values
    BITMAPFILEHEADER bf_mod = bf;
    BITMAPINFOHEADER bi_mod = bi;

    // get original file padding, if any
    int padding =  (4 - (bi.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;

    // save original scanline "width" in number of bytes
    int scan_bytes = padding + bi.biWidth * sizeof(RGBTRIPLE);

    // change new width & heght by factor
    bi_mod.biWidth *= factor;
    bi_mod.biHeight *= factor ;

    // determine new padding for scanlines
    int padding_mod =  (4 - (bi_mod.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;
    
    // set new biSizeImage
    bi_mod.biSizeImage = ((bi_mod.biWidth * sizeof(RGBTRIPLE)) + padding_mod) * abs(bi_mod.biHeight);

    // set new bfSize
    bf_mod.bfSize = bi_mod.biSizeImage + 54;

    // write outfile's BITMAPFILEHEADER
    fwrite(&bf_mod, sizeof(BITMAPFILEHEADER), 1, outptr);

    // write outfile's BITMAPINFOHEADER
    fwrite(&bi_mod, sizeof(BITMAPINFOHEADER), 1, outptr);

    // iterate over infile's scanlines
    for (int i = 0, biHeight = abs(bi.biHeight); i < biHeight; i++)
    {
        // loop to repeat going over each scanline factor times
        for (int m = 0; m < factor; m++)
        {
            // iterate over pixels in scanline
            for (int j = 0; j < bi.biWidth; j++)
            {
                // temporary storage
                RGBTRIPLE triple;
    
                // read RGB triple from infile
                fread(&triple, sizeof(RGBTRIPLE), 1, inptr);
    
                // write RGB triple to outfile factor times
                for (int l = 0; l < factor; l++)
                {
                    fwrite(&triple, sizeof(RGBTRIPLE), 1, outptr);
                }
            }
    
            // skip over padding, if any
            fseek(inptr, padding, SEEK_CUR);
    
            // then add it back (to demonstrate how)
            for (int k = 0; k < padding_mod; k++)
            {
                fputc(0x00, outptr);
            }
            
            // check if m < factor - 1 and fseek scan_bytes back if it is
            if (m < factor - 1)
            {
                fseek(inptr, scan_bytes * -1, SEEK_CUR);
            }
            
        }
    }

    // close infile
    fclose(inptr);

    // close outfile
    fclose(outptr);

    // that's all folks
    return 0;
}