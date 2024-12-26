#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <deque>
#include <unordered_map>
#include <vector>
#include <climits>

#define FRAME_COUNT 128
#define FRAME_SIZE 256
#define PAGE_COUNT 256
#define TLB_SIZE 16
#define ADDRESS_SIZE 12

// Memory structures
int pageTable[PAGE_COUNT];
signed char memory[FRAME_COUNT][FRAME_SIZE];

// FIFO and LRU structures
std::deque<int> fifoQueue;
std::unordered_map<int, int> lruMap; // To track page usage

// TLB structure
std::vector<std::pair<int, int>> TLB;

// Tracking and stats
int pageFaultCount = 0;
int tlbHitCount = 0;
int addressCount = 0;
int firstFreeFrame = 0;
FILE *backingStore;

// Helper functions
void initializeStructures() {
    memset(pageTable, -1, sizeof(pageTable));
    TLB.clear();
    fifoQueue.clear();
    lruMap.clear();
}

void get_page_and_offset(int logical_address, int *page_num, int *offset) {
    *offset = logical_address & 0xff;
    *page_num = (logical_address >> 8) & 0xff;
}

void backing_store_to_memory(int page_num, int frame_num) {
    signed char backValue[FRAME_SIZE];
    fseek(backingStore, page_num * FRAME_SIZE, SEEK_SET);
    fread(backValue, sizeof(signed char), FRAME_SIZE, backingStore);
    memcpy(memory[frame_num], backValue, FRAME_SIZE);
}

int get_available_frame(const char *policy) {
    if (firstFreeFrame < FRAME_COUNT) {
        return firstFreeFrame++;
    }

    // If no free frame, apply replacement policy
    if (strcmp(policy, "fifo") == 0) {
        if (!fifoQueue.empty()) {
            int evictedPage = fifoQueue.front();
            fifoQueue.pop_front();
            int evictedFrame = pageTable[evictedPage];
            pageTable[evictedPage] = -1;
            return evictedFrame;
        } else {
            fprintf(stderr, "Error: FIFO queue is empty during eviction.\n");
            exit(1);
        }
    } else if (strcmp(policy, "lru") == 0) {
        // Find least recently used page
        int lruPage = -1, minUsage = INT_MAX;
        for (auto &entry : lruMap) {
            if (entry.second < minUsage) {
                minUsage = entry.second;
                lruPage = entry.first;
            }
        }
        if (lruPage != -1) {
            int evictedFrame = pageTable[lruPage];
            lruMap.erase(lruPage);
            pageTable[lruPage] = -1;
            return evictedFrame;
        } else {
            fprintf(stderr, "Error: LRU map is empty during eviction.\n");
            exit(1);
        }
    }

    fprintf(stderr, "Error: Unknown replacement policy.\n");
    exit(1);
}

void update_page_table(int page_num, int frame_num, const char *policy) {
    if (strcmp(policy, "fifo") == 0) {
        if (pageTable[page_num] == -1) {
            fifoQueue.push_back(page_num);
        }
    } else if (strcmp(policy, "lru") == 0) {
        // Reset usage for the current page
        lruMap[page_num] = addressCount;
    }

    pageTable[page_num] = frame_num;
}

void update_tlb(int page_num, int frame_num) {
    for (auto &entry : TLB) {
        if (entry.first == page_num) {
            entry.second = frame_num; // Update existing mapping
            return;
        }
    }

    if (TLB.size() == TLB_SIZE) {
        TLB.erase(TLB.begin()); // Remove oldest entry
    }

    TLB.emplace_back(page_num, frame_num); // Add new entry
}

int search_tlb(int page_num) {
    for (const auto &entry : TLB) {
        if (entry.first == page_num) {
            return entry.second;
        }
    }
    return -1; // Not found
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <BACKING_STORE.bin> <addresses.txt> <policy>\n", argv[0]);
        return 1;
    }

    backingStore = fopen(argv[1], "rb");
    FILE *addressFile = fopen(argv[2], "r");
    FILE *outputFile = fopen("correct.txt", "w+");

    if (!backingStore || !addressFile || !outputFile) {
        fprintf(stderr, "Error opening files.\n");
        return 1;
    }

    initializeStructures();

    char address[ADDRESS_SIZE];
    int logicalAddress, pageNumber, offset, frameNumber;
    signed char value;

    while (fgets(address, ADDRESS_SIZE, addressFile) != NULL) {
        logicalAddress = atoi(address);
        get_page_and_offset(logicalAddress, &pageNumber, &offset);

        frameNumber = search_tlb(pageNumber);
        if (frameNumber != -1) {
            tlbHitCount++;
        } else {
            frameNumber = pageTable[pageNumber];
            if (frameNumber == -1) { // Page fault
                frameNumber = get_available_frame(argv[3]);
                backing_store_to_memory(pageNumber, frameNumber);
                pageFaultCount++;
            }
            update_page_table(pageNumber, frameNumber, argv[3]);
            update_tlb(pageNumber, frameNumber);
        }

        // Update LRU usage for the page accessed
        if (strcmp(argv[3], "lru") == 0) {
            lruMap[pageNumber] = addressCount;
        }

        value = memory[frameNumber][offset];
        fprintf(outputFile, "Virtual address: %d Physical address: %d Value: %d\n",
                logicalAddress, (frameNumber * FRAME_SIZE) + offset, value);
        addressCount++;
    }

    double pageFaultRate = (double)pageFaultCount / (double)addressCount;
    double tlbHitRate = (double)tlbHitCount / (double)addressCount;

    fprintf(outputFile, "Number of Translated Addresses = %d\n", addressCount);
    fprintf(outputFile, "Page Faults = %d\n", pageFaultCount);
    fprintf(outputFile, "Page Fault Rate = %.3f\n", pageFaultRate);
    fprintf(outputFile, "TLB Hits = %d\n", tlbHitCount);
    fprintf(outputFile, "TLB Hit Rate = %.3f\n", tlbHitRate);

    fclose(backingStore);
    fclose(addressFile);
    fclose(outputFile);

    return 0;
}
