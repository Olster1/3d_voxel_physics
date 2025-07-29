struct ResizeArrayHeader {
    size_t sizeOfElement;
    int elementsCount;
    int maxCount;
    Arena *arena;
};

#define initResizeArray(type) (type *)initResizeArray_(sizeof(type))
#define initResizeArrayArena(type, arena) (type *)initResizeArray_(sizeof(type), arena)

u8 *initResizeArray_(size_t sizeOfElement, Arena *arena = 0) {
    ResizeArrayHeader *header = 0;

    size_t startSize = sizeOfElement + sizeof(ResizeArrayHeader);

    if(arena) {
        header = (ResizeArrayHeader *)pushSize(arena, startSize);
    } else {
        header =(ResizeArrayHeader *)easyPlatform_allocateMemory(startSize, EASY_PLATFORM_MEMORY_ZERO);
    }
     
    u8 *array = ((u8 *)header) + sizeof(ResizeArrayHeader);

    header->sizeOfElement = sizeOfElement;
    header->elementsCount = 0;
    header->maxCount = 1;
    header->arena = arena;

    return array;
}

ResizeArrayHeader *getResizeArrayHeader(u8 *array) {
    ResizeArrayHeader *header = (ResizeArrayHeader *)(((u8 *)array) - sizeof(ResizeArrayHeader));
    return header;
}

void freeResizeArray(void *array_) {
    u8 *array = (u8 *)array_;
    ResizeArrayHeader *header = getResizeArrayHeader(array);
    if(header->arena) {
        assert(false);
    } else {
        easyPlatform_freeMemory(header);
    }
}

void clearResizeArray(void *array) {
     if(!array) {
        return;
    }
    ResizeArrayHeader *header = getResizeArrayHeader((u8 *)array);
    header->elementsCount = 0;
    
}

u8 *getResizeArrayContents(ResizeArrayHeader *header) {
    u8 *array = ((u8 *)header) + sizeof(ResizeArrayHeader);
    return array;
}

int getArrayLength(void *array) {
    if(!array) {
        return 0;
    }
    ResizeArrayHeader *header = getResizeArrayHeader((u8 *)array);
    assert(header->elementsCount <= header->maxCount);
    int result = header->elementsCount;
    return result;
}

// bool removeArrayIndex(void *array, int index) {
//     ResizeArrayHeader *header = getResizeArrayHeader((u8 *)array);
//     assert(header->elementsCount <= header->maxCount);
//     assert(index < header->elementsCount);
//     return result;
// }

#define pushArrayItem(array_, data, type)  (type *)pushArrayItem_((void **)array_, &data)
void *pushArrayItem_(void **array_, void *data) {
    u8 *array = *((u8 **)array_);
    u8 *newPos = 0;
    if(array) {
        ResizeArrayHeader *header = getResizeArrayHeader(array);

        if(header->elementsCount == header->maxCount) {
            //NOTE: Resize array
            size_t oldSize = header->maxCount*header->sizeOfElement + sizeof(ResizeArrayHeader);
            float resizeFactor = 1.5; //NOTE: Same as MSVC C++ Vector. x2 on GCC c++ Vector
            header->maxCount = round(header->maxCount*resizeFactor); 
            size_t newSize = header->maxCount*header->sizeOfElement + sizeof(ResizeArrayHeader);
            if(header->arena) {
                MemoryPiece *piece = getCurrentMemoryPiece(header->arena);
                
                u8 *endOfArray = (u8 *)header + oldSize;
                u8 *endOfArena = (u8 *)piece->memory + piece->currentSize;
                bool atEndOfArena = (endOfArray == endOfArena);

                size_t deltaBytes = newSize - oldSize;
                if(atEndOfArena && ((piece->currentSize + deltaBytes) <= piece->totalSize)) {
                    //NOTE: Still at the end of the arena and has room for more
                    piece->currentSize += deltaBytes;
                    assert(piece->currentSize <= piece->totalSize);
                } else {
                    u8 *newAddress = (u8 *)pushSize(header->arena, newSize);
                    easyPlatform_copyMemory(newAddress, header, oldSize);
                    header = (ResizeArrayHeader *)newAddress;
                }
                
            } else {
                header = (ResizeArrayHeader *)easyPlatform_reallocMemory(header, oldSize, newSize);
            }
            array = getResizeArrayContents(header);
        } 

        newPos = array + (header->elementsCount * header->sizeOfElement);
        header->elementsCount++;

        easyPlatform_copyMemory(newPos, data, header->sizeOfElement);
    }

    *array_ = array;

    return newPos;
}

bool doesArrayContain(void *array, void *itemToCheck) {
      if(!array) {
        return 0;
    }
    ResizeArrayHeader *header = getResizeArrayHeader((u8 *)array);
    assert(header->elementsCount <= header->maxCount);
    int length = header->elementsCount;

    bool result = false;

    for(int i = 0; i < length; ++i) {
        u8 *a = ((u8 *)array) + i*header->sizeOfElement;
        u8 *b = (u8 *)itemToCheck;
        if(memcmp(a, b, header->sizeOfElement) == 0) {
            result = true;
            break;
        }
    }
    
    return result;
    
}

struct TestStruct {
    int x;
    int y; 
    int z;
};

void test_standard_resize_array() {
    TestStruct *blocks = initResizeArray(TestStruct);  // Heap allocation
    TestStruct b = {0};

    pushArrayItem(&blocks, b, TestStruct);
    assert(getArrayLength(blocks) == 1);

    pushArrayItem(&blocks, b, TestStruct);
    assert(getArrayLength(blocks) == 2);

    b.y = 2;
    TestStruct *t = pushArrayItem(&blocks, b, TestStruct);
    assert(getArrayLength(blocks) == 3);
    assert(t->y == 2);

    printf("✅ test_standard_resize_array passed\n");
}

void test_arena_inplace_resize() {
    Arena arena = createArena(1024 * 1024); // 1MB

    TestStruct *blocks = initResizeArrayArena(TestStruct, &arena);
    TestStruct b = {0};

    for (int i = 0; i < 8; ++i) {
        pushArrayItem(&blocks, b, TestStruct);
    }

    ResizeArrayHeader *headerBefore = getResizeArrayHeader((u8 *)blocks);
    void *addressBefore = (void *)headerBefore;

    pushArrayItem(&blocks, b, TestStruct); // Should resize in-place

    ResizeArrayHeader *headerAfter = getResizeArrayHeader((u8 *)blocks);
    void *addressAfter = (void *)headerAfter;

    assert(addressBefore == addressAfter && "Expected in-place arena growth");
    printf("✅ test_arena_inplace_resize passed\n");
}

void test_arena_non_tail_resize() {
    Arena arena = createArena(1024 * 1024);

    pushSize(&arena, 128);  // Break "at end of arena" condition

    TestStruct *blocks = initResizeArrayArena(TestStruct, &arena);
    TestStruct b = {0};

    for (int i = 0; i < 8; ++i) {
        pushArrayItem(&blocks, b, TestStruct);
    }

    ResizeArrayHeader *headerBefore = getResizeArrayHeader((u8 *)blocks);
    pushSize(&arena, 64);  // Further break tail alignment

    pushArrayItem(&blocks, b, TestStruct); // Forces move

    ResizeArrayHeader *headerAfter = getResizeArrayHeader((u8 *)blocks);
    assert(headerBefore != headerAfter && "Expected arena move due to non-tail allocation");

    printf("✅ test_arena_non_tail_resize passed\n");
}

void test_arena_insufficient_space_resize() {
    size_t arenaSize = 64;
    size_t testSize = sizeof(TestStruct);
    Arena arena = createArena(arenaSize); 
    MemoryPiece *p = getCurrentMemoryPiece(&arena);
    int accountForAlignment = 4;
    pushSize(&arena, p->totalSize - (sizeof(ResizeArrayHeader) + testSize) - accountForAlignment);
    MemoryPiece *p1 = getCurrentMemoryPiece(&arena);
    assert(p == p1);

    TestStruct *blocks = initResizeArrayArena(TestStruct, &arena);
    TestStruct b = {0};
    MemoryPiece *p2 = getCurrentMemoryPiece(&arena);
    assert(p1 == p2);

    pushArrayItem(&blocks, b, TestStruct);
    ResizeArrayHeader *headerBefore = getResizeArrayHeader((u8 *)blocks);
    pushArrayItem(&blocks, b, TestStruct);  // Forces reallocation

    ResizeArrayHeader *headerAfter = getResizeArrayHeader((u8 *)blocks);
    assert(headerBefore != headerAfter && "Expected arena move due to insufficient space");

    printf("✅ test_arena_insufficient_space_resize passed\n");
}

void test_does_array_contain() {
    TestStruct *array = initResizeArray(TestStruct);

    TestStruct **array1 = initResizeArray(TestStruct *);

    TestStruct a = {1, 2, 3};
    TestStruct b = {4, 5, 6};
    TestStruct c = {7, 8, 9};
    TestStruct d = {1, 2, 3}; // same as a
    TestStruct e = {0, 0, 0};

    TestStruct *f = &e;

    // Initially empty
    assert(!doesArrayContain(array, &a));
    assert(!doesArrayContain(array, &b));
    
    pushArrayItem(&array, a, TestStruct);
    assert(doesArrayContain(array, &a));
    assert(doesArrayContain(array, &d)); // same contents as a
    assert(!doesArrayContain(array, &b));

    pushArrayItem(&array, b, TestStruct);
    assert(doesArrayContain(array, &b));

    pushArrayItem(&array, c, TestStruct);
    assert(doesArrayContain(array, &c));
    assert(!doesArrayContain(array, &e));

    assert(!doesArrayContain(array1, &f));
    pushArrayItem(&array1, f, TestStruct *);
    assert(doesArrayContain(array1, &f));
    
    printf("✅ test_does_array_contain passed\n");
}

void DEBUG_ArrayTests() {
    test_standard_resize_array();
    test_arena_inplace_resize();
    test_arena_non_tail_resize();
    test_arena_insufficient_space_resize();
    test_does_array_contain();
    exit(0);
}
