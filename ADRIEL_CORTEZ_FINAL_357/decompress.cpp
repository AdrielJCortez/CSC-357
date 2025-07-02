#include <stdio.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <queue>
#include <bitset>
#include <fstream>
#include <sstream>

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

struct batch
{
    int start; // x-start position of the batch
    int end; // x-end position of the batch
    BYTE shade; // color shade of the batch
    int line; // y-position of the batch
};

// helper function that asigns the colors to the image data
void writeBatch(BYTE* imageData, const vector<batch> colorBatch, int colorIndex, int width, int height)
{
    int rowSize = (width * 3 + 3) / 4 * 4;
    for (const batch& b : colorBatch)
    {
        int y = b.line;
        for (int x = b.start; x < b.end && x < width; x++)
        {
            int pixelIndex = y * rowSize + x * 3;
            imageData[pixelIndex + colorIndex] = BYTE(b.shade);
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cout << "please enter as: ./decompress compressed.eck output.bmp" << endl;
        return 0;
    }

    string inputFile = argv[1];
    string outputFileName = argv[2];

    FILE* file = fopen(inputFile.c_str(), "rb");
    if (!file) 
    {
        cerr << "Error opening file" << endl;
        return 1;
    }
    // create the file headers that we will be writing back to
    tagBITMAPFILEHEADER fileHeaderOut;
    tagBITMAPINFOHEADER infoHeaderOut;

    // read the File Header
    fread((&fileHeaderOut.bfType), sizeof(fileHeaderOut.bfType), 1, file);
    fread((&fileHeaderOut.bfSize), sizeof(fileHeaderOut.bfSize), 1, file);
    fread((&fileHeaderOut.bfReserved1), sizeof(fileHeaderOut.bfReserved1), 1, file);
    fread((&fileHeaderOut.bfReserved2), sizeof(fileHeaderOut.bfReserved2), 1, file);
    fread((&fileHeaderOut.bfOffBits), sizeof(fileHeaderOut.bfOffBits), 1, file);

    if (fileHeaderOut.bfType != 0x4D42) {
        cout << "file not read correctly" << endl;
        return 0;
    }

    // read the Info Header
    fread((&infoHeaderOut.biSize), sizeof(infoHeaderOut.biSize), 1, file);
    fread((&infoHeaderOut.biWidth), sizeof(infoHeaderOut.biWidth), 1, file);
    fread((&infoHeaderOut.biHeight), sizeof(infoHeaderOut.biHeight), 1, file);
    fread((&infoHeaderOut.biPlanes), sizeof(infoHeaderOut.biPlanes), 1, file);
    fread((&infoHeaderOut.biBitCount), sizeof(infoHeaderOut.biBitCount), 1, file);
    fread((&infoHeaderOut.biCompression), sizeof(infoHeaderOut.biCompression), 1, file);
    fread((&infoHeaderOut.biSizeImage), sizeof(infoHeaderOut.biSizeImage), 1, file);
    fread((&infoHeaderOut.biXPelsPerMeter), sizeof(infoHeaderOut.biXPelsPerMeter), 1, file);
    fread((&infoHeaderOut.biYPelsPerMeter), sizeof(infoHeaderOut.biYPelsPerMeter), 1, file);
    fread((&infoHeaderOut.biClrUsed), sizeof(infoHeaderOut.biClrUsed), 1, file);
    fread((&infoHeaderOut.biClrImportant), sizeof(infoHeaderOut.biClrImportant), 1, file);

    // Display header info
    cout << "File Size: " << fileHeaderOut.bfSize << " bytes" << endl;
    cout << "Image Dimensions: " << infoHeaderOut.biWidth << "x" << infoHeaderOut.biHeight << endl;

    int amtRedBatch, amtGreenBatch, amtBlueBatch;
    fread((&amtRedBatch), sizeof(amtRedBatch),1, file);
    fread((&amtGreenBatch), sizeof(amtGreenBatch), 1, file);
    fread((&amtBlueBatch), sizeof(amtBlueBatch), 1, file);

    cout << "size of Red: " << amtRedBatch << endl;
    cout << "size of Green: " << amtGreenBatch << endl;
    cout << "size of Blue: " << amtBlueBatch << endl;

    vector<batch> redBatches;
    vector<batch> greenBatches;
    vector<batch> blueBatches;

    // read each of the batches and add them all into a list (vector)

    // read the red batches
    for (int i = 0; i < amtRedBatch; i++)
    {
        batch newRedBatch;
        fread((&newRedBatch), 16, 1, file);
        redBatches.push_back(newRedBatch);
    }
    // read the green batches
    for (int i = 0; i < amtGreenBatch; i++)
    {
        batch newGreenBatch;
        fread((&newGreenBatch), 16, 1, file);
        greenBatches.push_back(newGreenBatch);
    }
    // read the blue batches
    for (int i = 0; i < amtBlueBatch; i++)
    {
        batch newBlueBatch;
        fread((&newBlueBatch), 16, 1, file);
        blueBatches.push_back(newBlueBatch);
    }

    int width = infoHeaderOut.biWidth;
    int height = abs(infoHeaderOut.biHeight);
    int pad = (4 - (width * 3) % 4) % 4;
    int rowSize = (width * 3 + 3) / 4 * 4;
    int imageSize = rowSize * height;

    BYTE* imageData = (BYTE*)mmap(NULL, imageSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (imageData == MAP_FAILED)
    {
        cerr << "mem failed" << endl;
        return 1;
    }
    for (int y = 0; y < height; y++) {
    fread(imageData + y * rowSize, 1, rowSize, file);  // Read full row, including padding
    }
    fclose(file);

    timespec time, res;

    clock_gettime(CLOCK_MONOTONIC, &time);

    // now asign the batches to the output, and fork for each batch
    pid_t redPID = fork();
    if (redPID == 0){
        writeBatch(imageData, redBatches, 2, width, height);
        exit(0);
    }
    pid_t greenPID = fork();
    if (greenPID == 0){
        writeBatch(imageData, greenBatches, 1, width, height);
        exit(0);
    }
    writeBatch(imageData, blueBatches, 0, width, height);

    waitpid(redPID, NULL, 0);
    waitpid(greenPID, NULL, 0);

    clock_gettime(CLOCK_MONOTONIC, &res);
    double elapsedTimeMs = (res.tv_sec - time.tv_sec) * 1000.0 + (res.tv_nsec - time.tv_nsec) / 1e6;

    cout << "Batches written in: " << elapsedTimeMs << "ms"<<endl;

    FILE* outputBMP = fopen(outputFileName.c_str(), "wb");
    fwrite(&fileHeaderOut.bfType, sizeof(WORD), 1, outputBMP);
    fwrite(&fileHeaderOut.bfSize, sizeof(DWORD), 1, outputBMP);
    fwrite(&fileHeaderOut.bfReserved1, sizeof(WORD), 1, outputBMP);
    fwrite(&fileHeaderOut.bfReserved2, sizeof(WORD), 1, outputBMP);
    fwrite(&fileHeaderOut.bfOffBits, sizeof(DWORD), 1, outputBMP);

    fwrite(&infoHeaderOut.biSize, sizeof(DWORD), 1, outputBMP);
    fwrite(&infoHeaderOut.biWidth, sizeof(LONG), 1, outputBMP);
    fwrite(&infoHeaderOut.biHeight, sizeof(LONG), 1, outputBMP);
    fwrite(&infoHeaderOut.biPlanes, sizeof(WORD), 1, outputBMP);
    fwrite(&infoHeaderOut.biBitCount, sizeof(WORD), 1, outputBMP);
    fwrite(&infoHeaderOut.biCompression, sizeof(DWORD), 1, outputBMP);
    fwrite(&infoHeaderOut.biSizeImage, sizeof(DWORD), 1, outputBMP);
    fwrite(&infoHeaderOut.biXPelsPerMeter, sizeof(LONG), 1, outputBMP);
    fwrite(&infoHeaderOut.biYPelsPerMeter, sizeof(LONG), 1, outputBMP);
    fwrite(&infoHeaderOut.biClrUsed, sizeof(DWORD), 1, outputBMP);
    fwrite(&infoHeaderOut.biClrImportant, sizeof(DWORD), 1, outputBMP);
   
    // Write image data row by row, inserting padding
    BYTE padding[3] = {0, 0, 0}; // BMP padding is always zero (I had to look this up because the padding was my only issue)
    // Align to 4-byte boundary
    for (int y = 0; y < height; y++) {
        fwrite(imageData + y * rowSize, 1, rowSize, outputBMP);
    }
    cout << "image written YAY!!!!" << endl;
    fclose(outputBMP);

    munmap(imageData, imageSize);
    return 0;
}