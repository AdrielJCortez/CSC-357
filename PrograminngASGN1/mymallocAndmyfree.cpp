#include <unistd.h> 
#include <stdio.h>    
#include <stdlib.h>   
#include <string.h>   
#include <stdint.h>   
#include <iostream>

typedef unsigned char BYTE;

using namespace std;

void *heap_start = NULL; // first chunk
void *heap_end = NULL; // second chunk

struct chunkinfo
{
    int size; // size of the complete chunk including the chunck info (4)
    int occ; // 0 if free, one if occupied (4)
    BYTE *next; // next chunk (8)
    BYTE *prev; // prev chunk (8)
};

chunkinfo* get_last_chunk() //you can change it when you aim for performance
{
if(!heap_start)
return NULL;
chunkinfo* ch = (chunkinfo*)heap_start;
for (; ch->next; ch = (chunkinfo*)ch->next);
return ch;
}

void analyze()
{
printf("\n--------------------------------------------------------------\n");
if(heap_start == 0)
{
printf("no heap ");
printf("program break on address: %x\n",sbrk(0));
return;
}
chunkinfo* ch = (chunkinfo*)heap_start;
for (int no=0; ch; ch = (chunkinfo*)ch->next,no++)
{
printf("%d | current addr: %x |", no, ch);
printf("size: %d | ", ch->size);
printf("info: %x | ", ch->occ);
printf("next: %x | ", ch->next);
printf("prev: %x", ch->prev);
printf(" \n");
}
printf("program break on address: %x\n",sbrk(0));
}

BYTE *mymalloc(int size)
{
    int sizeCiandPadd = ((size + sizeof(chunkinfo) + 4095) / 4096) * 4096;

    if (heap_start == 0) // if there a no chunks
    {   
        // sbrk returns me to start program break BEFORE moving the program break down
        void* start_addr = sbrk(sizeCiandPadd); 
        if (start_addr == (void*)-1)
        {
            cout << "Allocation failed!" << endl;
            return NULL;
        }

        chunkinfo *ci = (chunkinfo*)start_addr; // set the chunk info pointer to the start addr from sbrk
        ci ->size = sizeCiandPadd; // size with padding, chunk info, and
        ci ->occ = 1; // chunk is now occupied
        ci ->next = ci ->prev = NULL; // since it is the first chunk being created 

        heap_start = start_addr; // the start heap and last heap will be the start
        heap_end = start_addr;

        return (BYTE*)(ci + 1); // return the memory after the chunk info
    }

    else  // otherwise traverse through the chunks and try to find an open one if not create a new one
    {
        chunkinfo* curr_chunk= (chunkinfo*)heap_start; // starting chunk
        curr_chunk = (chunkinfo*)curr_chunk->next;
        while(curr_chunk != NULL)
        {
            // check if the chunk is unoccupied and that the current chunk can fit the wished memory
            if (curr_chunk ->occ == 0 && (curr_chunk->size >= (sizeCiandPadd))) 
            {
                if ((curr_chunk->size - sizeCiandPadd) > 4096) // check if the chunk needs to be split
                {
                    chunkinfo* new_chunk = (chunkinfo*)((BYTE*)curr_chunk + sizeCiandPadd);
                    new_chunk->size = curr_chunk->size - sizeCiandPadd;  // Remaining space
                    new_chunk->occ = 0;  // Mark it as free
                    new_chunk->next = curr_chunk->next;  // Link to next chunk
                    new_chunk->prev = (BYTE*)curr_chunk;  // Link back to current chunk

                    if (new_chunk->next) 
                    {
                        ((chunkinfo*)new_chunk->next)->prev = (BYTE*)new_chunk;// Update next chunk’s prev
                    }

                    curr_chunk->size = sizeCiandPadd;  // Adjust the current chunk size
                    curr_chunk->next = (BYTE*)new_chunk;  // Link to new chunk

                }
                // after slipting the chunk or not, set the current chunk to occupied
                curr_chunk->occ = 1;
                return (BYTE*)(curr_chunk + 1);            
            }
            curr_chunk = (chunkinfo*)curr_chunk->next;
        }
        // after trying to find an open chunk with no luck, move the program break and create a new chunk
        void* addr = sbrk(sizeCiandPadd); // allocate space for size of chunk and padding
        if (addr == (void*)-1)
        {
            cout << "Allocation failed!" << endl;
            return NULL;
        }
        chunkinfo* last_chunk = get_last_chunk();
        BYTE* last_chunk_addr = (BYTE*)last_chunk;

        chunkinfo *ci = (chunkinfo*)addr; // create new chunk
        ci->size = sizeCiandPadd;
        ci ->occ = 1;
        ci->next = NULL;
        ci->prev = last_chunk_addr;

        // now we need to modify next of the last chunk
        last_chunk->next = (BYTE*)ci;

        return (BYTE*)(ci +1);
    }
}

void myfree(BYTE* addr) 
{
    if (!addr) return;  // Null check

    
    chunkinfo* ci = (chunkinfo*)(addr-sizeof(chunkinfo));

    
    ci->occ = 0; // set the current chunk to free

    chunkinfo* nchunk = (chunkinfo*)ci->next; // next chunk
    chunkinfo* pchunk = (chunkinfo*)ci->prev; // previous chunk

    // check if the next chunk is also free, merge foward
    if (ci->next && nchunk->occ == 0)
    {
        ci->size += nchunk->size; // combine both the sizes
        ci->next = nchunk->next; // make the next of the current chunk the next of the next

        if (ci->next) // check if the new next has a next because we have to update the prev
        {
            ((chunkinfo*)ci->next)->prev = (BYTE*) ci;
        }
    }

    // merge backwards
    if (ci->prev && pchunk->occ == 0)
    {
        // this logic is different since we want to add to the chunk that is in front
        pchunk->size += ci ->size;  
        pchunk->next = ci ->next;

        if(pchunk->next)
        {
            ((chunkinfo*)ci->next)->prev = (BYTE*) pchunk;
        }

        ci = pchunk; // we do this since ci no longer exits while pchunk does
    }

    if (!ci->next && ((BYTE*)ci + ci->size) == sbrk(0)) // check if it is the last chunk
    {
        int goback = -(ci->size);
        sbrk(goback);

        heap_start = 0;
        heap_end = 0;
    }
}



int main() 
{
BYTE*a[100];
analyze();//50% points
for(int i=0;i<100;i++)
a[i] = mymalloc(1000);
for(int i=0;i<90;i++)
myfree(a[i]);
analyze(); //50% of points if this is correct
myfree(a[95]);
// analyze() // see what it looks like with the one freed
a[95] = mymalloc(1000);
analyze();//25% points, this new chunk should fill the smaller free one
//(best fit)
for(int i=90;i<100;i++)
myfree(a[i]);
analyze();// 25% should be an empty heap now with the start address
//from the program start
return 1;
}
