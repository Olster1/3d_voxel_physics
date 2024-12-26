struct ProfileData {
    char *name;
    float totalTime;
};

struct Profiler {
    ProfileData *data;
};  

u64 _rdtsc(void)
{
    u64 val;

    /*
     * According to ARM DDI 0487F.c, from Armv8.0 to Armv8.5 inclusive, the
     * system counter is at least 56 bits wide; from Armv8.6, the counter
     * must be 64 bits wide.  So the system counter could be less than 64
     * bits wide and it is attributed with the flag 'cap_user_time_short'
     * is true.
     */
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));

    return val;
}

static Profiler *global_profiler;

void initProfiler(Profiler *p) {
    p->data = initResizeArray(ProfileData);
}

u64 profiler_getCount() {
    return SDL_GetTicks();
    // return _rdtsc();
}


struct ProfileBlock {
    char *name;
    u64 start;
    
    ProfileBlock(char *title) {
        name = title;
        start = profiler_getCount();
        
    }

    ~ProfileBlock() {
        u64 end = profiler_getCount();
        assert(end >= start);
        float seconds = (end - start);

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