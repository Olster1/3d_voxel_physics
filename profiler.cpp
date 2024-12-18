struct ProfileData {
    char *name;
    float totalTime;
};

struct Profiler {
    ProfileData *data;
};  

static Profiler *global_profiler;

void initProfiler(Profiler *p) {
    p->data = initResizeArray(ProfileData);
}

struct ProfileBlock {
    char *name;
    uint32_t start;
    
    ProfileBlock(char *title) {
        name = title;
        start = SDL_GetTicks();
        
    }

    ~ProfileBlock() {
        uint32_t end = SDL_GetTicks();
        assert(end > start);
        float seconds = ((end - start) / 1000.0f);
        printf("%f seconds\n", seconds);

        if(!global_profiler) {
            global_profiler = pushStruct(&globalLongTermArena, Profiler);
            initProfiler(global_profiler);
        }

        ProfileData *d = 0;

        for(int i = 0; i < getArrayLength(global_profiler->data) && !d; i++) {
            if(easyString_stringsMatch_nullTerminated(global_profiler->data[i].name, name)) {
                d = &global_profiler->data[i];
            }
        }

        if(!d) {
            ProfileData a = {};
            d = (ProfileData *)pushArrayItem(&global_profiler->data, a, ProfileData);
        } 

        d->name = name;
        d->totalTime += seconds;

    }

};

void clearProfiler() {
    if(global_profiler) {
        ResizeArrayHeader *header = getResizeArrayHeader((u8 *)global_profiler->data);
        header->elementsCount = 0;
    }

}

#define PROFILE_FUNC(name) ProfileBlock profileBlock_##name = ProfileBlock(#name);