/****************************************************************
 ****************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>

/******************************************************
 * Declarations
 ******************************************************/
#define FRAMES 256
#define FRAME_SIZE 256
#define TABLES_OF_PAGES 256
#define TABLE_SIZE 16
#define PAGE_SIZE 256
#define ADDRESS_SIZE 12

// Memory structures
int pageTable[TABLES_OF_PAGES];
int pageTLB[TABLE_SIZE];
int frameTLB[TABLE_SIZE];
signed char memory[FRAMES][FRAME_SIZE];

// Tracking and stats
int firstFreeFrame = 0;
int pageFaultCount = 0;
int TLBhitCount = 0;
int TLBEntryCount = 0;

// Input and data storage
FILE *backingStore;

/******************************************************
 * Function Implementations
 ******************************************************/

void get_page_and_offset(int logical_address, int *page_num, int *offset) {
    *offset = logical_address & 0xff;
    *page_num = (logical_address >> 8) & 0xff;
}

int get_frame_TLB(int page_num) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (pageTLB[i] == page_num) {
            TLBhitCount++;
            return frameTLB[i];
        }
    }
    return -1; // NOT_FOUND
}

int get_available_frame() {
    return firstFreeFrame++;
}

int get_frame_pagetable(int page_num) {
    return pageTable[page_num];
}

void backing_store_to_memory(int page_num, int frame_num, const char *fname) {
    signed char backValue[PAGE_SIZE];
    fseek(backingStore, page_num * PAGE_SIZE, SEEK_SET);
    fread(backValue, sizeof(signed char), PAGE_SIZE, backingStore);
    for (int i = 0; i < PAGE_SIZE; i++) {
        memory[frame_num][i] = backValue[i];
    }
}

void update_page_table(int page_num, int frame_num) {
    pageTable[page_num] = frame_num;
}

void update_TLB(int page_num, int frame_num) {
    if (TLBEntryCount >= TABLE_SIZE) {
        TLBEntryCount = 0; // FIFO replacement
    }
    pageTLB[TLBEntryCount] = page_num;
    frameTLB[TLBEntryCount] = frame_num;
    TLBEntryCount++;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <BACKING_STORE.bin> <addresses.txt>\n", argv[0]);
        return 1;
    }

    backingStore = fopen(argv[1], "rb");
    FILE *addressFile = fopen(argv[2], "r");
    FILE *outputFile = fopen("correct.txt", "w+");

    if (!backingStore || !addressFile || !outputFile) {
        printf("Error opening files.\n");
        return 1;
    }

    // Initialize page table and TLB
    for (int i = 0; i < TABLES_OF_PAGES; i++) pageTable[i] = -1;
    for (int i = 0; i < TABLE_SIZE; i++) pageTLB[i] = frameTLB[i] = -1;

    char address[ADDRESS_SIZE];
    int logicalAddress, pageNumber, offset, frameNumber;
    int addressCount = 0;
    signed char value;

    while (fgets(address, ADDRESS_SIZE, addressFile) != NULL) {
        logicalAddress = atoi(address);
        get_page_and_offset(logicalAddress, &pageNumber, &offset);

        frameNumber = get_frame_TLB(pageNumber);
        if (frameNumber == -1) { // Not in TLB
            frameNumber = get_frame_pagetable(pageNumber);
            if (frameNumber == -1) { // Page fault
                frameNumber = get_available_frame();
                backing_store_to_memory(pageNumber, frameNumber, argv[1]);
                update_page_table(pageNumber, frameNumber);
                pageFaultCount++;
            }
            update_TLB(pageNumber, frameNumber);
        }

        value = memory[frameNumber][offset];
        fprintf(outputFile, "Virtual address: %d Physical address: %d Value: %d\n",
                logicalAddress, (frameNumber << 8) | offset, value);
        addressCount++;
    }

    double pageFaultRate = (double)pageFaultCount / (double)addressCount;
    double TLBHitRate = (double)TLBhitCount / (double)addressCount;

    fprintf(outputFile, "Number of Translated Addresses = %d\n", addressCount);
    fprintf(outputFile, "Page Faults = %d\n", pageFaultCount);
    fprintf(outputFile, "Page Fault Rate = %.3f\n", pageFaultRate);
    fprintf(outputFile, "TLB Hits = %d\n", TLBhitCount);
    fprintf(outputFile, "TLB Hit Rate = %.3f\n", TLBHitRate);

    fclose(backingStore);
    fclose(addressFile);
    fclose(outputFile);

    return 0;
}
