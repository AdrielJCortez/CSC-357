#include <stdio.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int LONG;

struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
};

struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
};

// Bilinear interpolation to get color
void getColor_bilinear(BYTE* img, int width, int height, float fx, float fy, int &r, int &g, int &b) {

    // the four surrounding pixels
    // eg. fx = 10.7, fy = 5.3
    int x_small = (int)(fx); // 10
    int y_small = (int)(fy); // 5
    int x_big = min(x_small + 1, width - 1); // 11
    int y_big = min(y_small + 1, height - 1); // 6

    float dx = fx - x_small; // 0.7 "70%"
    float dy = fy - y_small; // 0.3 "30%"

    // get each index positions
    int index_tl = (y_small * width + x_small) * 3; // index of top left
    int index_tr = (y_small * width + x_big) * 3; // index of top right
    int index_bl = (y_big * width + x_small) * 3; // index of bottom left
    int index_br = (y_big * width + x_big) * 3; // index of bottom right

    // get each color and each index position r,g,b
    int r_tl = img[index_tl + 2], g_tl = img[index_tl + 1], b_tl = img[index_tl];
    int r_tr = img[index_tr + 2], g_tr = img[index_tr + 1], b_tr = img[index_tr];
    int r_bl = img[index_bl + 2], g_bl = img[index_bl + 1], b_bl = img[index_bl];
    int r_br = img[index_br + 2], g_br = img[index_br + 1], b_br = img[index_br];

    // Interpolate horizontally
    float r_top = r_tl * (1 - dx) + r_tr * dx;
    float g_top = g_tl * (1 - dx) + g_tr * dx;
    float b_top = b_tl * (1 - dx) + b_tr * dx;

    float r_bottom = r_bl * (1 - dx) + r_br * dx;
    float g_bottom = g_bl * (1 - dx) + g_br * dx;
    float b_bottom = b_bl * (1 - dx) + b_br * dx;

    // Interpolate Vertically
    r = (int)(r_top * (1 - dy) + r_bottom * dy);
    g = (int)(g_top * (1 - dy) + g_bottom * dy);
    b = (int)(b_top * (1 - dy) + b_bottom * dy);
}

// Blend images using bilinear interpolation
void blend_bilinear(BYTE* small_img, int small_width, int small_height, BYTE* big_img, 
int big_width, int big_height, BYTE* output, float ratio, int startingHeight, int endingHeight, int outputWidth, int outputHeight) {
    
    for (int y = startingHeight; y < endingHeight; y++) { // for each clm
        for (int x = 0; x < outputWidth; x++) { // for each row

            // get fx and fy for small and get the color for r, g, b
            int r_small, g_small, b_small;

            float fx_small = x * (small_width / float(outputWidth)); 
            float fy_small = y * (small_height / float(outputHeight));

            getColor_bilinear(small_img, small_width, small_height, fx_small, fy_small, r_small, g_small, b_small);

            // get fx and fy for big and get the color for r, g, b
            int r_big, g_big, b_big;

            float fx_big = x * (big_width / float(outputWidth));
            float fy_big = y * (big_height / float(outputHeight));
            
            getColor_bilinear(big_img, big_width, big_height, fx_big, fy_big, r_big, g_big, b_big);

            // get the new estimated r,b,g values applied with the ratio
            int r_new = (int)(r_small * ratio + r_big * (1 - ratio));
            int g_new = (int)(g_small * ratio + g_big * (1 - ratio));
            int b_new = (int)(b_small * ratio + b_big * (1 - ratio));

            int index = (y * outputWidth + x) * 3;
            output[index + 2] = max(0, min(r_new, 255));
            output[index + 1] = max(0, min(g_new, 255));
            output[index] = max(0, min(b_new, 255));
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        cout << "Usage: " << argv[0] << " <inputfile1> <inputfile2> <blend_ratio> <Process#> <outputfile>" << endl;
        return 1;
    }

    string inputFileName1 = argv[1];
    string inputFileName2 = argv[2];
    float ratio = atof(argv[3]);
    int processNumber = atoi(argv[4]);
    string outputFileName = argv[5];


    FILE* inputFile1 = fopen(inputFileName1.c_str(), "rb");
    FILE* inputFile2 = fopen(inputFileName2.c_str(), "rb");

    if (!inputFile1 || !inputFile2) {
        cout << "Error opening input files" << endl;
        return 1;
    }

    // Read BITMAPFILEHEADER for image 1
    tagBITMAPFILEHEADER fileHeader1;
    fread(&fileHeader1.bfType, sizeof(WORD), 1, inputFile1);
    fread(&fileHeader1.bfSize, sizeof(DWORD), 1, inputFile1);
    fread(&fileHeader1.bfReserved1, sizeof(WORD), 1, inputFile1);
    fread(&fileHeader1.bfReserved2, sizeof(WORD), 1, inputFile1);
    fread(&fileHeader1.bfOffBits, sizeof(DWORD), 1, inputFile1);


    // Read BITMAPINFOHEADER for image 1
    tagBITMAPINFOHEADER infoHeader1;
    fread(&infoHeader1.biSize, sizeof(DWORD), 1, inputFile1);
    fread(&infoHeader1.biWidth, sizeof(LONG), 1, inputFile1);
    fread(&infoHeader1.biHeight, sizeof(LONG), 1, inputFile1);
    fread(&infoHeader1.biPlanes, sizeof(WORD), 1, inputFile1);
    fread(&infoHeader1.biBitCount, sizeof(WORD), 1, inputFile1);
    fread(&infoHeader1.biCompression, sizeof(DWORD), 1, inputFile1);
    fread(&infoHeader1.biSizeImage, sizeof(DWORD), 1, inputFile1);
    fread(&infoHeader1.biXPelsPerMeter, sizeof(LONG), 1, inputFile1);
    fread(&infoHeader1.biYPelsPerMeter, sizeof(LONG), 1, inputFile1);
    fread(&infoHeader1.biClrUsed, sizeof(DWORD), 1, inputFile1);
    fread(&infoHeader1.biClrImportant, sizeof(DWORD), 1, inputFile1);


    // Read BITMAPFILEHEADER for image 2
    tagBITMAPFILEHEADER fileHeader2;
    fread(&fileHeader2.bfType, sizeof(WORD), 1, inputFile2);
    fread(&fileHeader2.bfSize, sizeof(DWORD), 1, inputFile2);
    fread(&fileHeader2.bfReserved1, sizeof(WORD), 1, inputFile2);
    fread(&fileHeader2.bfReserved2, sizeof(WORD), 1, inputFile2);
    fread(&fileHeader2.bfOffBits, sizeof(DWORD), 1, inputFile2);


    // Read BITMAPINFOHEADER for image 2
    tagBITMAPINFOHEADER infoHeader2;
    fread(&infoHeader2.biSize, sizeof(DWORD), 1, inputFile2);
    fread(&infoHeader2.biWidth, sizeof(LONG), 1, inputFile2);
    fread(&infoHeader2.biHeight, sizeof(LONG), 1, inputFile2);
    fread(&infoHeader2.biPlanes, sizeof(WORD), 1, inputFile2);
    fread(&infoHeader2.biBitCount, sizeof(WORD), 1, inputFile2);
    fread(&infoHeader2.biCompression, sizeof(DWORD), 1, inputFile2);
    fread(&infoHeader2.biSizeImage, sizeof(DWORD), 1, inputFile2);
    fread(&infoHeader2.biXPelsPerMeter, sizeof(LONG), 1, inputFile2);
    fread(&infoHeader2.biYPelsPerMeter, sizeof(LONG), 1, inputFile2);
    fread(&infoHeader2.biClrUsed, sizeof(DWORD), 1, inputFile2);
    fread(&infoHeader2.biClrImportant, sizeof(DWORD), 1, inputFile2);

    int width1 = infoHeader1.biWidth;
    int height1 = abs(infoHeader1.biHeight);
    int pad1 = (4 - (width1 * 3) % 4) % 4;
    int rowSize1 = (width1 * 3 + 3) / 4 * 4;
    int imageSize1 = rowSize1 * height1;

    int width2 = infoHeader2.biWidth;
    int height2 = abs(infoHeader2.biHeight);
    int pad2 = (4 - (width2 * 3) % 4) % 4;
    int rowSize2 = (width2 * 3 + 3) / 4 * 4;
    int imageSize2 = rowSize2 * height2;

    // Determine output image dimensions
    int outputWidth = max(width1, width2);
    int outputHeight = max(height1, height2);
    int outputPadding = (4 - (outputWidth * 3) % 4) % 4;
    int outputRowSize = (outputWidth * 3 + 3) / 4 * 4;
    int outputImageSize = outputRowSize * outputHeight;

    // Create output file headers
    tagBITMAPFILEHEADER fileHeaderOut = fileHeader1;
    tagBITMAPINFOHEADER infoHeaderOut = infoHeader1;

    // Modify output headers to reflect the new image size
    infoHeaderOut.biWidth = outputWidth;
    infoHeaderOut.biHeight = outputHeight;
    infoHeaderOut.biSizeImage = outputImageSize;
    fileHeaderOut.bfSize = sizeof(tagBITMAPFILEHEADER) + sizeof(tagBITMAPINFOHEADER) + outputImageSize;


    // Read image data row by row with padding handling
    BYTE* imageData1 = (BYTE*)mmap(NULL, imageSize1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    BYTE* imageData2 = (BYTE*)mmap(NULL, imageSize2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    for (int y = 0; y < height1; y++) {
        fread(imageData1 + y * width1 * 3, 3, width1, inputFile1);
        fseek(inputFile1, pad1, SEEK_CUR);  // Skip padding
    }

    for (int y = 0; y < height2; y++) {
        fread(imageData2 + y * width2 * 3, 3, width2, inputFile2);
        fseek(inputFile2, pad2, SEEK_CUR);  // Skip padding
    }

    fclose(inputFile1);
    fclose(inputFile2);

    // Allocate output image memory
    BYTE* outputImageData = (BYTE*)mmap(NULL, outputImageSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    timespec time, res;

    clock_gettime(CLOCK_MONOTONIC, &time);

    pid_t child_pid, wpid;
    int status = 0;

    for (int id = 0; id<processNumber; id++)
    {
        if((child_pid = fork()) == 0)
        {
            int startHeight = (outputHeight / processNumber) * (id); // start at 0
            int endHeight = (outputHeight / processNumber) * (id + 1); // go to 1/2 for example
            blend_bilinear(imageData1, width1, height1, imageData2, width2, height2, outputImageData, ratio, startHeight, endHeight, outputWidth, outputHeight);

            exit(0);
        }
    }

    while ((wpid = wait(&status))>0);
    
    clock_gettime(CLOCK_MONOTONIC, &res);

    // Calculate elapsed time in milliseconds
    double elapsedTimeMs = (res.tv_sec - time.tv_sec) * 1000.0 + (res.tv_nsec - time.tv_nsec) / 1e6;

    cout << "Time taken with " << processNumber << " proccess is " << abs(elapsedTimeMs) << "miliseconds" << endl;

    // Open output file
    FILE* outputFile = fopen(outputFileName.c_str(), "wb");
    if (!outputFile) {
        cout << "Error: Could not create output file." << endl;
        return 1;
    }

    // Write headers
    fwrite(&fileHeaderOut.bfType, sizeof(WORD), 1, outputFile);
    fwrite(&fileHeaderOut.bfSize, sizeof(DWORD), 1, outputFile);
    fwrite(&fileHeaderOut.bfReserved1, sizeof(WORD), 1, outputFile);
    fwrite(&fileHeaderOut.bfReserved2, sizeof(WORD), 1, outputFile);
    fwrite(&fileHeaderOut.bfOffBits, sizeof(DWORD), 1, outputFile);

    fwrite(&infoHeaderOut.biSize, sizeof(DWORD), 1, outputFile);
    fwrite(&infoHeaderOut.biWidth, sizeof(LONG), 1, outputFile);
    fwrite(&infoHeaderOut.biHeight, sizeof(LONG), 1, outputFile);
    fwrite(&infoHeaderOut.biPlanes, sizeof(WORD), 1, outputFile);
    fwrite(&infoHeaderOut.biBitCount, sizeof(WORD), 1, outputFile);
    fwrite(&infoHeaderOut.biCompression, sizeof(DWORD), 1, outputFile);
    fwrite(&infoHeaderOut.biSizeImage, sizeof(DWORD), 1, outputFile);
    fwrite(&infoHeaderOut.biXPelsPerMeter, sizeof(LONG), 1, outputFile);
    fwrite(&infoHeaderOut.biYPelsPerMeter, sizeof(LONG), 1, outputFile);
    fwrite(&infoHeaderOut.biClrUsed, sizeof(DWORD), 1, outputFile);
    fwrite(&infoHeaderOut.biClrImportant, sizeof(DWORD), 1, outputFile);

    // Write image data row by row, inserting padding
    BYTE padding[3] = {0, 0, 0}; // BMP padding is always zero (I had to look this up because the padding was my only issue)
    for (int y = 0; y < outputHeight; y++) {
        fwrite(outputImageData + y * outputWidth * 3, 3, outputWidth, outputFile);
        fwrite(padding, 1, outputPadding, outputFile);  // Add correct padding
    }

    fclose(outputFile);

    // Free allocated memory
    munmap(imageData1, imageSize1);
    munmap(imageData2, imageSize2);
    munmap(outputImageData, outputImageSize);

    cout << "Blended image written successfully!" << endl;
    return 0;

}


