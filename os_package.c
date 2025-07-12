#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <time.h>
#define PAGE_SIZE 4096
#define PHYSICAL_MEMORY_SIZE_24BIT (1 << 24)
#define PHYSICAL_MEMORY_SIZE_20BIT (1 << 20)
#define MAX_TRACE_ENTRIES 1000000

typedef struct {
    char operation;
    unsigned long address;
} TraceEntry;

TraceEntry trace[MAX_TRACE_ENTRIES];
int trace_size = 0;

typedef struct {
    int page_number;
    int frame_number;
    int referenced;
    int valid;
} PageTableEntry;

typedef struct {
    PageTableEntry* entries;
    int size;
    int page_faults;
    int hits;
    int misses;
} PageTable;

typedef struct {
    int* frames;
    int size;
    int next_frame;
} PhysicalMemory;

typedef struct {
    int* pages;
    int* frames;
    int size;
    int next_index;
} FIFOQueue;

typedef struct {
    int* pages;
    int* frames;
    int* ages;
    int size;
} LRUQueue;

typedef struct {
    int* pages;
    int* frames;
    int* reference_bits;
    int size;
    int hand;
} ClockQueue;

typedef struct {
    int* pages;
    int* frames;
    int* reference_bits;
    int size;
    int hand;
} SecondChanceQueue;

PageTable* create_page_table(int size);
PhysicalMemory* create_physical_memory(int size);
FIFOQueue* create_fifo_queue(int size);
LRUQueue* create_lru_queue(int size);
ClockQueue* create_clock_queue(int size);
SecondChanceQueue* create_second_chance_queue(int size);
void free_page_table(PageTable* pt);
void free_physical_memory(PhysicalMemory* pm);
void free_fifo_queue(FIFOQueue* fifo);
void free_lru_queue(LRUQueue* lru);
void free_clock_queue(ClockQueue* clock);
void free_second_chance_queue(SecondChanceQueue* sc);
int fifo_replace(FIFOQueue* fifo);
int lru_replace(LRUQueue* lru);
int clock_replace(ClockQueue* clock);
int second_chance_replace(SecondChanceQueue* sc);
int min_replace(TraceEntry* trace, int trace_size, int current_index, PageTable* pt, PhysicalMemory* pm);
void simulate_virtual_memory(PageTable* pt, PhysicalMemory* pm, FIFOQueue* fifo, LRUQueue* lru, ClockQueue* clock, SecondChanceQueue* sc, int algorithm, TraceEntry* trace, int trace_size);
void simulate_virtual_memory_step(PageTable* pt, PhysicalMemory* pm, FIFOQueue* fifo, LRUQueue* lru, ClockQueue* clock, SecondChanceQueue* sc, int algorithm, TraceEntry* trace, int step);
void visualize_and_graph(TraceEntry* trace, int trace_size, PhysicalMemory* pm, PageTable* pt, PageTable* pt_graph, FIFOQueue* fifo, LRUQueue* lru, ClockQueue* clock, SecondChanceQueue* sc, int algorithm);
void visualize(TraceEntry* trace, int trace_size);
void list_processes_and_trace();
void get_memory_access_trace(const char *pid);
void add_trace_entry(char operation, unsigned long address);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <algorithm> <physical_address_bits>\n", argv[0]);
        fprintf(stderr, "Algorithm: 0=FIFO, 1=LRU, 2=MIN, 3=SECOND CHANCE, 4=CLOCK\n");
        fprintf(stderr, "Physical Address Bits: 20 or 24\n");
        return 1;
    }

    int algorithm = atoi(argv[1]);
    if (algorithm < 0 || algorithm > 4) {
        fprintf(stderr, "Invalid algorithm choice\n");
        return 1;
    }

    int physical_address_bits = atoi(argv[2]);
    if (physical_address_bits != 20 && physical_address_bits != 24) {
        fprintf(stderr, "Invalid physical address bits (must be 20 or 24)\n");
        return 1;
    }

    int physical_memory_size = (physical_address_bits == 20) ? PHYSICAL_MEMORY_SIZE_20BIT : PHYSICAL_MEMORY_SIZE_24BIT;
    int num_frames = physical_memory_size / PAGE_SIZE;

    list_processes_and_trace();
    printf("Live trace collected. Trace size: %d\n", trace_size);

    if (trace_size == 0) {
        fprintf(stderr, "Error: No memory access traces collected. Try running with higher privileges.\n");
        return 1;
    }

    PageTable* pt_graph = create_page_table(num_frames);
    PhysicalMemory* pm_graph = create_physical_memory(num_frames);
    FIFOQueue* fifo_graph = create_fifo_queue(num_frames);
    LRUQueue* lru_graph = create_lru_queue(num_frames);
    ClockQueue* clock_graph = create_clock_queue(num_frames);
    SecondChanceQueue* sc_graph = create_second_chance_queue(num_frames);

    simulate_virtual_memory(pt_graph, pm_graph, fifo_graph, lru_graph, clock_graph, sc_graph, algorithm, trace, trace_size);

    printf("Debug: Hits = %d, Misses = %d\n", pt_graph->hits, pt_graph->misses);
    printf("Total references: %d\n", pt_graph->hits + pt_graph->misses);
    printf("Page faults: %d\n", pt_graph->page_faults);
    printf("Hit ratio: %.2f%%\n", (float)pt_graph->hits / (pt_graph->hits + pt_graph->misses) * 100);
    printf("Miss ratio: %.2f%%\n", (float)pt_graph->misses / (pt_graph->hits + pt_graph->misses) * 100);

    free_physical_memory(pm_graph);
    free_fifo_queue(fifo_graph);
    free_lru_queue(lru_graph);
    free_clock_queue(clock_graph);
    free_second_chance_queue(sc_graph);

    PageTable* pt = create_page_table(num_frames);
    PhysicalMemory* pm = create_physical_memory(num_frames);
    FIFOQueue* fifo = create_fifo_queue(num_frames);
    LRUQueue* lru = create_lru_queue(num_frames);
    ClockQueue* clock = create_clock_queue(num_frames);
    SecondChanceQueue* sc = create_second_chance_queue(num_frames);

    visualize_and_graph(trace, trace_size, pm, pt, pt_graph, fifo, lru, clock, sc, algorithm);

    free_page_table(pt);
    free_physical_memory(pm);
    free_fifo_queue(fifo);
    free_lru_queue(lru);
    free_clock_queue(clock);
    free_second_chance_queue(sc);
    free_page_table(pt_graph);

    return 0;
}

void add_trace_entry(char operation, unsigned long address) {
    if (trace_size < MAX_TRACE_ENTRIES) {
        trace[trace_size].operation = operation;
        trace[trace_size].address = address / PAGE_SIZE;
        trace_size++;
    }
}

void list_processes_and_trace() {
    struct dirent *entry;
    DIR *dp = opendir("/proc");
    if (dp == NULL) {
        perror("Error: Unable to open /proc. Try running as root.");
        exit(1);
    }

   //  entry = readdir(dp);
//      printf("%d",entry->d_name[0]);
  //      if (isdigit(entry->d_name[0])) {
            get_memory_access_trace("640");
    //    }

    closedir(dp);
}

void get_memory_access_trace(const char *pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/640/maps", pid);
        strcpy(path,"/proc/641/maps");
    srand(time(NULL));
    int randnum;
    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char line[256];
    int unique_pages = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long start, end;
        char perms[5];

        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3) {
           // if (strchr(perms, 'r')) {
                for (unsigned long addr = start; addr < end; addr += PAGE_SIZE) {
                    if (unique_pages < 100) {
                        add_trace_entry('l', addr);
                        randnum=(rand()%3)+2;
                        for (int i = 0; i < randnum; i++) {
                            add_trace_entry('l', addr);
                        }
                        unique_pages++;
                    }
                }
            //}
           // if (strchr(perms, 'w')) {
                add_trace_entry('s', start);
                randnum=(rand()%3)+2;
                for (int i = 0; i < randnum; i++) {
                    add_trace_entry('s', start);
             //   }
                unique_pages++;
            }
        }
    }
    fclose(fp);
}

PageTable* create_page_table(int size) {
    PageTable* pt = (PageTable*)malloc(sizeof(PageTable));
    pt->entries = (PageTableEntry*)calloc(size, sizeof(PageTableEntry));
    pt->size = size;
    pt->page_faults = 0;
    pt->hits = 0;
    pt->misses = 0;
    for (int i = 0; i < size; i++) {
        pt->entries[i].page_number = -1;
        pt->entries[i].frame_number = -1;
        pt->entries[i].referenced = 0;
        pt->entries[i].valid = 0;
    }
    return pt;
}

PhysicalMemory* create_physical_memory(int size) {
    PhysicalMemory* pm = (PhysicalMemory*)malloc(sizeof(PhysicalMemory));
    pm->frames = (int*)calloc(size, sizeof(int));
    for (int i = 0; i < size; i++) pm->frames[i] = -1;
    pm->size = size;
    pm->next_frame = 0;
    return pm;
}

FIFOQueue* create_fifo_queue(int size) {
    FIFOQueue* fifo = (FIFOQueue*)malloc(sizeof(FIFOQueue));
    fifo->pages = (int*)calloc(size, sizeof(int));
    fifo->frames = (int*)calloc(size, sizeof(int));
    for (int i = 0; i < size; i++) {
        fifo->pages[i] = -1;
        fifo->frames[i] = -1;
    }
    fifo->size = size;
    fifo->next_index = 0;
    return fifo;
}

LRUQueue* create_lru_queue(int size) {
    LRUQueue* lru = (LRUQueue*)malloc(sizeof(LRUQueue));
    lru->pages = (int*)calloc(size, sizeof(int));
    lru->frames = (int*)calloc(size, sizeof(int));
    lru->ages = (int*)calloc(size, sizeof(int));
    for (int i = 0; i < size; i++) {
        lru->pages[i] = -1;
        lru->frames[i] = -1;
        lru->ages[i] = 0;
    }
    lru->size = size;
    return lru;
}

ClockQueue* create_clock_queue(int size) {
    ClockQueue* clock = (ClockQueue*)malloc(sizeof(ClockQueue));
    clock->pages = (int*)calloc(size, sizeof(int));
    clock->frames = (int*)calloc(size, sizeof(int));
    clock->reference_bits = (int*)calloc(size, sizeof(int));
    for (int i = 0; i < size; i++) {
        clock->pages[i] = -1;
        clock->frames[i] = -1;
        clock->reference_bits[i] = 0;
    }
    clock->size = size;
    clock->hand = 0;
    return clock;
}

SecondChanceQueue* create_second_chance_queue(int size) {
    SecondChanceQueue* sc = (SecondChanceQueue*)malloc(sizeof(SecondChanceQueue));
    sc->pages = (int*)calloc(size, sizeof(int));
    sc->frames = (int*)calloc(size, sizeof(int));
    sc->reference_bits = (int*)calloc(size, sizeof(int));
    for (int i = 0; i < size; i++) {
        sc->pages[i] = -1;
        sc->frames[i] = -1;
        sc->reference_bits[i] = 0;
    }
    sc->size = size;
    sc->hand = 0;
    return sc;
}

void free_page_table(PageTable* pt) {
    free(pt->entries);
    free(pt);
}

void free_physical_memory(PhysicalMemory* pm) {
    free(pm->frames);
    free(pm);
}

void free_fifo_queue(FIFOQueue* fifo) {
    free(fifo->pages);
    free(fifo->frames);
    free(fifo);
}

void free_lru_queue(LRUQueue* lru) {
    free(lru->pages);
    free(lru->frames);
    free(lru->ages);
    free(lru);
}

void free_clock_queue(ClockQueue* clock) {
    free(clock->pages);
    free(clock->frames);
    free(clock->reference_bits);
    free(clock);
}

void free_second_chance_queue(SecondChanceQueue* sc) {
    free(sc->pages);
    free(sc->frames);
    free(sc->reference_bits);
    free(sc);
}

int fifo_replace(FIFOQueue* fifo) {
    int replaced_index = fifo->next_index;
    fifo->next_index = (fifo->next_index + 1) % fifo->size;
    return replaced_index;
}

int lru_replace(LRUQueue* lru) {
    int oldest_age = -1;
    int replaced_index = 0;
    for (int i = 0; i < lru->size; i++) {
        if (lru->ages[i] > oldest_age) {
            oldest_age = lru->ages[i];
            replaced_index = i;
        }
    }
    return replaced_index;
}

int clock_replace(ClockQueue* clock) {
    while (1) {
        if (clock->reference_bits[clock->hand] == 0) {
            int replaced_index = clock->hand;
            clock->hand = (clock->hand + 1) % clock->size;
            return replaced_index;
        } else {
            clock->reference_bits[clock->hand] = 0;
            clock->hand = (clock->hand + 1) % clock->size;
        }
    }
}

int second_chance_replace(SecondChanceQueue* sc) {
    while (1) {
        if (sc->reference_bits[sc->hand] == 0) {
            int replaced_index = sc->hand;
            sc->hand = (sc->hand + 1) % sc->size;
            return replaced_index;
        } else {
            sc->reference_bits[sc->hand] = 0;
            sc->hand = (sc->hand + 1) % sc->size;
        }
    }
}

int min_replace(TraceEntry* trace, int trace_size, int current_index, PageTable* pt, PhysicalMemory* pm) {
    int farthest = current_index;
    int replaced_index = -1;

    for (int i = 0; i < pm->size; i++) {
        int j;
        for (j = current_index; j < trace_size; j++) {
            if (pm->frames[i] == trace[j].address) {
                if (j > farthest) {
                    farthest = j;
                    replaced_index = i;
                }
                break;
            }
        }
        if (j == trace_size) {
            return i;
        }
    }
    return (replaced_index == -1) ? 0 : replaced_index;
}

void simulate_virtual_memory(PageTable* pt, PhysicalMemory* pm, FIFOQueue* fifo, LRUQueue* lru, ClockQueue* clock, SecondChanceQueue* sc, int algorithm, TraceEntry* trace, int trace_size) {
    for (int i = 0; i < trace_size; i++) {
        int page_number = trace[i].address;
        int frame_number = -1;

        int found = 0;
        for (int j = 0; j < pt->size; j++) {
            if (pt->entries[j].valid && pt->entries[j].page_number == page_number) {
                frame_number = pt->entries[j].frame_number;
                pt->entries[j].referenced = 1;
                found = 1;
                pt->hits++;
                printf("Hit: Page %d found in frame %d\n", page_number, frame_number);
                break;
            }
        }

        if (!found) {
            pt->misses++;
            pt->page_faults++;
            printf("Miss: Page %d not found\n", page_number);

            if (pm->next_frame < pm->size) {
                frame_number = pm->next_frame++;
            } else {
                switch (algorithm) {
                    case 0: frame_number = fifo_replace(fifo); break;
                    case 1: frame_number = lru_replace(lru); break;
                    case 2: frame_number = min_replace(trace, trace_size, i, pt, pm); break;
                    case 3: frame_number = second_chance_replace(sc); break;
                    case 4: frame_number = clock_replace(clock); break;
                }
                for (int j = 0; j < pt->size; j++) {
                    if (pt->entries[j].valid && pt->entries[j].frame_number == frame_number) {
                        pt->entries[j].valid = 0;
                        break;
                    }
                }
            }

            pt->entries[frame_number].page_number = page_number;
            pt->entries[frame_number].frame_number = frame_number;
            pt->entries[frame_number].referenced = 1;
            pt->entries[frame_number].valid = 1;
            pm->frames[frame_number] = page_number;


            if (algorithm == 0) {
                fifo->pages[frame_number] = page_number;
                fifo->frames[frame_number] = frame_number;
            } else if (algorithm == 1) {
                lru->pages[frame_number] = page_number;
                lru->frames[frame_number] = frame_number;
                lru->ages[frame_number] = 0;
                for (int j = 0; j < lru->size; j++) {
                    if (lru->frames[j] != -1) lru->ages[j]++;
                }
            } else if (algorithm == 3) {
                sc->pages[frame_number] = page_number;
                sc->frames[frame_number] = frame_number;
                sc->reference_bits[frame_number] = 1;
            } else if (algorithm == 4) {
                clock->pages[frame_number] = page_number;
                clock->frames[frame_number] = frame_number;
                clock->reference_bits[frame_number] = 1;
            }
        }
    }
}

void simulate_virtual_memory_step(PageTable* pt, PhysicalMemory* pm, FIFOQueue* fifo, LRUQueue* lru, ClockQueue* clock, SecondChanceQueue* sc, int algorithm, TraceEntry* trace, int step) {
    int page_number = trace[step].address;
    int frame_number = -1;

    int found = 0;
    for (int j = 0; j < pt->size; j++) {
        if (pt->entries[j].valid && pt->entries[j].page_number == page_number) {
            frame_number = pt->entries[j].frame_number;
            pt->entries[j].referenced = 1;
            found = 1;
            pt->hits++;
            printf("Step %d - Hit: Page %d found in frame %d\n", step, page_number, frame_number);
            break;
        }
    }

    if (!found) {
        pt->misses++;
        pt->page_faults++;
        printf("Step %d - Miss: Page %d not found\n", step, page_number);

        if (pm->next_frame < pm->size) {
            frame_number = pm->next_frame++;
        } else {
            switch (algorithm) {
                case 0: frame_number = fifo_replace(fifo); break;
                case 1: frame_number = lru_replace(lru); break;
                case 2: frame_number = min_replace(trace, trace_size, step, pt, pm); break;
                case 3: frame_number = second_chance_replace(sc); break;
                case 4: frame_number = clock_replace(clock); break;
            }
            for (int j = 0; j < pt->size; j++) {
                if (pt->entries[j].valid && pt->entries[j].frame_number == frame_number) {
                    pt->entries[j].valid = 0;
                    break;
                }
            }
        }

        pt->entries[frame_number].page_number = page_number;
        pt->entries[frame_number].frame_number = frame_number;
        pt->entries[frame_number].referenced = 1;
        pt->entries[frame_number].valid = 1;
        pm->frames[frame_number] = page_number;

        if (algorithm == 0) {
            fifo->pages[frame_number] = page_number;
            fifo->frames[frame_number] = frame_number;
        } else if (algorithm == 1) {
            lru->pages[frame_number] = page_number;
            lru->frames[frame_number] = frame_number;
            lru->ages[frame_number] = 0;
            for (int j = 0; j < lru->size; j++) {
                if (lru->frames[j] != -1) lru->ages[j]++;
            }
        } else if (algorithm == 3) {
            sc->pages[frame_number] = page_number;
            sc->frames[frame_number] = frame_number;
            sc->reference_bits[frame_number] = 1;
        } else if (algorithm == 4) {
            clock->pages[frame_number] = page_number;
            clock->frames[frame_number] = frame_number;
            clock->reference_bits[frame_number] = 1;
        }
    }
}

void visualize(TraceEntry* trace, int trace_size) {
    FILE* plot_file = fopen("plot.txt", "w");
    if (!plot_file) {
        perror("Failed to open plot file");
        return;
    }

    for (int i = 0; i < trace_size; i++) {
        fprintf(plot_file, "%d %lu\n", i, trace[i].address);
    }

    fclose(plot_file);

    system("gnuplot -p -e \"set title 'Memory Access Trace'; set xlabel 'Time'; set ylabel 'Page Number'; plot 'plot.txt' with lines\"");
}

void visualize_and_graph(TraceEntry* trace, int trace_size, PhysicalMemory* pm, PageTable* pt, PageTable* pt_graph, FIFOQueue* fifo, LRUQueue* lru, ClockQueue* clock, SecondChanceQueue* sc, int algorithm) {
    visualize(trace, trace_size);
    printf("Press Enter to continue to the graph visualization...\n");
    getchar();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return;
    }

    SDL_Window* window = SDL_CreateWindow("Virtual Memory Simulation",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1000, 1000, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return;
    }

    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
    if (!font) {
        fprintf(stderr, "Failed to load font! TTF_Error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return;
    }

    int quit = 0;
    SDL_Event e;
    int step = 0;
    int show_graph = 1;
    int show_animation = 0;
    int auto_play = 0;
    int animation_delay = 0.2;
    int last_result=-1;
    int last_accessed_frame = -1;

    while (!quit) {
        int advance_step = 0;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_SPACE) {
                    if (show_graph) {
                        show_graph = 0;
                        show_animation = 1;
                        auto_play = 1;
                    } else if (show_animation && !auto_play && step < trace_size) {
                        int page_number = trace[step].address;
                        int found = 0;
                        last_accessed_frame = -1;

                        for (int j = 0; j < pt->size; j++) {
                            if (pt->entries[j].valid && pt->entries[j].page_number == page_number) {
                                found = 1;
                                last_accessed_frame = j;
                                break;
                            }
                        }

                        last_result = found ? 0 : 1;

                        if (!found && pm->next_frame >= pm->size) {
                            int frame_to_replace = -1;
                            switch (algorithm) {
                                case 0: frame_to_replace = fifo_replace(fifo); break;
                                case 1: frame_to_replace = lru_replace(lru); break;
                                case 2: frame_to_replace = min_replace(trace, trace_size, step, pt, pm); break;
                                case 3: frame_to_replace = second_chance_replace(sc); break;
                                case 4: frame_to_replace = clock_replace(clock); break;
                            }
                            last_accessed_frame = frame_to_replace;
                        } else if (!found) {
                            last_accessed_frame = pm->next_frame;
                        }

                        simulate_virtual_memory_step(pt, pm, fifo, lru, clock, sc, algorithm, trace, step);
                        step++;
                        advance_step = 1;
                    }
                } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    quit = 1;
                } else if (e.key.keysym.sym == SDLK_p && show_animation) {
                    auto_play = !auto_play;
                } else if (e.key.keysym.sym == SDLK_PLUS || e.key.keysym.sym == SDLK_EQUALS) {
                    animation_delay = (animation_delay > 1) ? animation_delay - 1 : 0;
                } else if (e.key.keysym.sym == SDLK_MINUS) {
                    animation_delay = (animation_delay < 100) ? animation_delay + 5 : 100;
                }
            }
        }

        if (show_animation && auto_play && step < trace_size) {
            int page_number = trace[step].address;
            int found = 0;
            last_accessed_frame = -1;

            for (int j = 0; j < pt->size; j++) {
                if (pt->entries[j].valid && pt->entries[j].page_number == page_number) {
                    found = 1;
                    last_accessed_frame = j;
                    break;
                }
            }

            last_result = found ? 0 : 1;

            if (!found && pm->next_frame >= pm->size) {
                int frame_to_replace = -1;
                switch (algorithm) {
                    case 0: frame_to_replace = fifo_replace(fifo); break;
                    case 1: frame_to_replace = lru_replace(lru); break;
                    case 2: frame_to_replace = min_replace(trace, trace_size, step, pt, pm); break;
                    case 3: frame_to_replace = second_chance_replace(sc); break;
                    case 4: frame_to_replace = clock_replace(clock); break;
                }
                last_accessed_frame = frame_to_replace;
            } else if (!found) {
                last_accessed_frame = pm->next_frame;
            }

            simulate_virtual_memory_step(pt, pm, fifo, lru, clock, sc, algorithm, trace, step);
            step++;
            advance_step = 1;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (show_graph) {
            int total = pt_graph->hits + pt_graph->misses;
            float hit_ratio = total > 0 ? (float)pt_graph->hits / total : 0;
            float miss_ratio = total > 0 ? (float)pt_graph->misses / total : 0;
            float fault_ratio = total > 0 ? (float)pt_graph->page_faults / total : 0;

            int max_height = 500;
            int bar_width = 150;
            int graph_x = 250;
            int graph_y = 150;
            int graph_width = 700;
            int graph_height = 500;

            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            for (int y = graph_y; y <= graph_y + graph_height; y += 50) {
                SDL_RenderDrawLine(renderer, graph_x, y, graph_x + graph_width, y);
            }
            for (int x = graph_x; x <= graph_x + graph_width; x += 70) {
                SDL_RenderDrawLine(renderer, x, graph_y, x, graph_y + graph_height);
            }

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, graph_x, graph_y, graph_x, graph_y + graph_height);
            SDL_RenderDrawLine(renderer, graph_x, graph_y + graph_height, graph_x + graph_width, graph_y + graph_height);

            for (int i = 0; i <= 100; i += 25) {
                SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
                SDL_Rect marker = {graph_x - 15, graph_y + graph_height - (i * max_height / 100) - 1, 8, 2};
                SDL_RenderFillRect(renderer, &marker);

                char percent_text[10];
                snprintf(percent_text, sizeof(percent_text), "%d%%", i);
                SDL_Color white = {255, 255, 255, 255};
                SDL_Surface* percentSurface = TTF_RenderText_Solid(font, percent_text, white);
                SDL_Texture* percentTexture = SDL_CreateTextureFromSurface(renderer, percentSurface);
                SDL_Rect percentRect = {graph_x - 40, graph_y + graph_height - (i * max_height / 100) - 10, percentSurface->w, percentSurface->h};
                SDL_RenderCopy(renderer, percentTexture, NULL, &percentRect);
                SDL_FreeSurface(percentSurface);
                SDL_DestroyTexture(percentTexture);
            }

            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface* titleSurface = TTF_RenderText_Solid(font, "Virtual Memory Performance", white);
            SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
            SDL_Rect titleRect = {graph_x + 200, graph_y - 70, titleSurface->w, titleSurface->h};
            SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);
            SDL_FreeSurface(titleSurface);
            SDL_DestroyTexture(titleTexture);

            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
            int hits_height = hit_ratio * max_height;
            if (hits_height < 5 && hit_ratio > 0) hits_height = 5;
            SDL_Rect hits_bar = {graph_x + 100, graph_y + graph_height - hits_height, bar_width, hits_height};
            SDL_RenderFillRect(renderer, &hits_bar);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &hits_bar);

            SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
            int misses_height = miss_ratio * max_height;
            if (misses_height < 5 && miss_ratio > 0) misses_height = 5;
            SDL_Rect misses_bar = {graph_x + 300, graph_y + graph_height - misses_height, bar_width, misses_height};
            SDL_RenderFillRect(renderer, &misses_bar);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &misses_bar);

            SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255);
            int faults_height = fault_ratio * max_height;
            if (faults_height < 5 && fault_ratio > 0) faults_height = 5;
            SDL_Rect faults_bar = {graph_x + 500, graph_y + graph_height - faults_height, bar_width, faults_height};
            SDL_RenderFillRect(renderer, &faults_bar);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &faults_bar);

            char hit_percent[16];
            snprintf(hit_percent, sizeof(hit_percent), "%.1f%%", hit_ratio * 100);
            SDL_Surface* hitPercentSurface = TTF_RenderText_Solid(font, hit_percent, white);
            SDL_Texture* hitPercentTexture = SDL_CreateTextureFromSurface(renderer, hitPercentSurface);
            SDL_Rect hitPercentRect = {graph_x + 100 + bar_width/2 - hitPercentSurface->w/2,
                                      graph_y + graph_height - hits_height - 30,
                                      hitPercentSurface->w,
                                      hitPercentSurface->h};
            SDL_RenderCopy(renderer, hitPercentTexture, NULL, &hitPercentRect);
            SDL_FreeSurface(hitPercentSurface);
            SDL_DestroyTexture(hitPercentTexture);

            char miss_percent[16];
            snprintf(miss_percent, sizeof(miss_percent), "%.1f%%", miss_ratio * 100);
            SDL_Surface* missPercentSurface = TTF_RenderText_Solid(font, miss_percent, white);
            SDL_Texture* missPercentTexture = SDL_CreateTextureFromSurface(renderer, missPercentSurface);
            SDL_Rect missPercentRect = {graph_x + 300 + bar_width/2 - missPercentSurface->w/2,
                                       graph_y + graph_height - misses_height - 30,
                                       missPercentSurface->w,
                                       missPercentSurface->h};
            SDL_RenderCopy(renderer, missPercentTexture, NULL, &missPercentRect);
            SDL_FreeSurface(missPercentSurface);
            SDL_DestroyTexture(missPercentTexture);

            char fault_percent[16];
            snprintf(fault_percent, sizeof(fault_percent), "%.1f%%", fault_ratio * 100);
            SDL_Surface* faultPercentSurface = TTF_RenderText_Solid(font, fault_percent, white);
            SDL_Texture* faultPercentTexture = SDL_CreateTextureFromSurface(renderer, faultPercentSurface);
            SDL_Rect faultPercentRect = {graph_x + 500 + bar_width/2 - faultPercentSurface->w/2,
                                       graph_y + graph_height - faults_height - 30,
                                       faultPercentSurface->w,
                                       faultPercentSurface->h};
            SDL_RenderCopy(renderer, faultPercentTexture, NULL, &faultPercentRect);
            SDL_FreeSurface(faultPercentSurface);
            SDL_DestroyTexture(faultPercentTexture);

            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
            SDL_Rect legend_hits = {graph_x + 50, graph_y + graph_height + 80, 20, 20};
            SDL_RenderFillRect(renderer, &legend_hits);
            SDL_Surface* hitsText = TTF_RenderText_Solid(font, "Hits", white);
            SDL_Texture* hitsTexture = SDL_CreateTextureFromSurface(renderer, hitsText);
            SDL_Rect hitsTextRect = {graph_x + 75, graph_y + graph_height + 80, hitsText->w, hitsText->h};
            SDL_RenderCopy(renderer, hitsTexture, NULL, &hitsTextRect);
            SDL_FreeSurface(hitsText);
            SDL_DestroyTexture(hitsTexture);

            SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
            SDL_Rect legend_misses = {graph_x + 200, graph_y + graph_height + 80, 20, 20};
            SDL_RenderFillRect(renderer, &legend_misses);
            SDL_Surface* missesText = TTF_RenderText_Solid(font, "Misses", white);
            SDL_Texture* missesTexture = SDL_CreateTextureFromSurface(renderer, missesText);
            SDL_Rect missesTextRect = {graph_x + 225, graph_y + graph_height + 80, missesText->w, missesText->h};
            SDL_RenderCopy(renderer, missesTexture, NULL, &missesTextRect);
            SDL_FreeSurface(missesText);
            SDL_DestroyTexture(missesTexture);

            SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255);
            SDL_Rect legend_faults = {graph_x + 350, graph_y + graph_height + 80, 20, 20};
            SDL_RenderFillRect(renderer, &legend_faults);
            SDL_Surface* faultsText = TTF_RenderText_Solid(font, "Page Faults", white);
            SDL_Texture* faultsTexture = SDL_CreateTextureFromSurface(renderer, faultsText);
            SDL_Rect faultsTextRect = {graph_x + 375, graph_y + graph_height + 80, faultsText->w, faultsText->h};
            SDL_RenderCopy(renderer, faultsTexture, NULL, &faultsTextRect);
            SDL_FreeSurface(faultsText);
            SDL_DestroyTexture(faultsTexture);

            // Instructions
            SDL_Surface* instructionSurface = TTF_RenderText_Solid(font, "Press SPACE to view animation (starts in auto mode)", white);
            SDL_Texture* instructionTexture = SDL_CreateTextureFromSurface(renderer, instructionSurface);
            SDL_Rect instructionRect = {graph_x + 150, graph_y + graph_height + 130, instructionSurface->w, instructionSurface->h};
            SDL_RenderCopy(renderer, instructionTexture, NULL, &instructionRect);
            SDL_FreeSurface(instructionSurface);
            SDL_DestroyTexture(instructionTexture);

        } else if (show_animation) {
            int frame_width = 80;
            int frame_height = 80;
            int cols = 10;
            int rows = (pm->size + cols - 1) / cols;

            // Draw title
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface* animTitleSurface = TTF_RenderText_Solid(font, "Physical Memory Frames", white);
            SDL_Texture* animTitleTexture = SDL_CreateTextureFromSurface(renderer, animTitleSurface);
            SDL_Rect animTitleRect = {50, 30, animTitleSurface->w, animTitleSurface->h};
            SDL_RenderCopy(renderer, animTitleTexture, NULL, &animTitleRect);
            SDL_FreeSurface(animTitleSurface);
            SDL_DestroyTexture(animTitleTexture);

            // Draw the frames
            for (int i = 0; i < pm->size; i++) {
                int x = 50 + (i % cols) * frame_width;
                int y = 70 + (i / cols) * frame_height;

                SDL_Rect frame = {x, y, frame_width - 10, frame_height - 10};

                if (pm->frames[i] == -1 || !pt->entries[i].valid) {
                    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
                } else if (i == last_accessed_frame) {
                    if (last_result == 0) {
                        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Bright green for hit
                    } else {
                        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Bright red for miss
                    }
                } else {
                    SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255);
                }

                SDL_RenderFillRect(renderer, &frame);

                // Frame border
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &frame);

                // Draw frame number
                char frameNum[8];
                snprintf(frameNum, sizeof(frameNum), "F%d", i);
                SDL_Surface* frameNumSurface = TTF_RenderText_Solid(font, frameNum, white);
                SDL_Texture* frameNumTexture = SDL_CreateTextureFromSurface(renderer, frameNumSurface);
                SDL_Rect frameNumRect = {x + 5, y + 5, frameNumSurface->w, frameNumSurface->h};
                SDL_RenderCopy(renderer, frameNumTexture, NULL, &frameNumRect);
                SDL_FreeSurface(frameNumSurface);
                SDL_DestroyTexture(frameNumTexture);

            }

            // Status bar at the bottom
            int status_bar_height = 150;
            int status_bar_y = 800; // Positioned for 1000px height

            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_Rect status_bar = {0, status_bar_y, 1000, status_bar_height};
            SDL_RenderFillRect(renderer, &status_bar);

            // Display current step information
            char step_str[32];
            snprintf(step_str, sizeof(step_str), "Step: %d / %d", step, trace_size);
            SDL_Surface* stepSurface = TTF_RenderText_Solid(font, step_str, white);
            SDL_Texture* stepTexture = SDL_CreateTextureFromSurface(renderer, stepSurface);
            SDL_Rect stepRect = {50, status_bar_y + 20, stepSurface->w, stepSurface->h};
            SDL_RenderCopy(renderer, stepTexture, NULL, &stepRect);
            SDL_FreeSurface(stepSurface);
            SDL_DestroyTexture(stepTexture);

            // Display current page being accessed
            char page_str[32];
            snprintf(page_str, sizeof(page_str), "Page: %d", step > 0 && step <= trace_size ? (int)trace[step-1].address : 0);
            SDL_Surface* pageSurface = TTF_RenderText_Solid(font, page_str, white);
            SDL_Texture* pageTexture = SDL_CreateTextureFromSurface(renderer, pageSurface);
            SDL_Rect pageRect = {50, status_bar_y + 50, pageSurface->w, pageSurface->h};
            SDL_RenderCopy(renderer, pageTexture, NULL, &pageRect);
            SDL_FreeSurface(pageSurface);
            SDL_DestroyTexture(pageTexture);

            // Display access result (hit or miss)
            char access_str[32];
            if (step > 0) {
                snprintf(access_str, sizeof(access_str), "Result: %s", last_result == 0 ? "HIT" : "MISS");
                SDL_Color result_color = last_result == 0 ?
                    (SDL_Color){0, 255, 0, 255} : // Green for hit
                    (SDL_Color){255, 0, 0, 255};  // Red for miss
                SDL_Surface* accessSurface = TTF_RenderText_Solid(font, access_str, result_color);
                SDL_Texture* accessTexture = SDL_CreateTextureFromSurface(renderer, accessSurface);
                SDL_Rect accessRect = {50, status_bar_y + 80, accessSurface->w, accessSurface->h};
                SDL_RenderCopy(renderer, accessTexture, NULL, &accessRect);
                SDL_FreeSurface(accessSurface);
                SDL_DestroyTexture(accessTexture);
            }

            // Hit/Miss statistics
            char hits_str[32];
            snprintf(hits_str, sizeof(hits_str), "Hits: %d", pt->hits);
            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
            SDL_Rect hits_stat = {300, status_bar_y + 20, 150, 25};
            SDL_RenderFillRect(renderer, &hits_stat);
            SDL_Surface* hitsStatSurface = TTF_RenderText_Solid(font, hits_str, white);
            SDL_Texture* hitsStatTexture = SDL_CreateTextureFromSurface(renderer, hitsStatSurface);
            SDL_Rect hitsStatRect = {300 + (150 - hitsStatSurface->w)/2, status_bar_y + 20 + (25 - hitsStatSurface->h)/2, hitsStatSurface->w, hitsStatSurface->h};
            SDL_RenderCopy(renderer, hitsStatTexture, NULL, &hitsStatRect);
            SDL_FreeSurface(hitsStatSurface);
            SDL_DestroyTexture(hitsStatTexture);

            char misses_str[32];
            snprintf(misses_str, sizeof(misses_str), "Misses: %d", pt->misses);
            SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
            SDL_Rect misses_stat = {300, status_bar_y + 55, 150, 25};
            SDL_RenderFillRect(renderer, &misses_stat);
            SDL_Surface* missesStatSurface = TTF_RenderText_Solid(font, misses_str, white);
            SDL_Texture* missesStatTexture = SDL_CreateTextureFromSurface(renderer, missesStatSurface);
            SDL_Rect missesStatRect = {300 + (150 - missesStatSurface->w)/2, status_bar_y + 55 + (25 - missesStatSurface->h)/2, missesStatSurface->w, missesStatSurface->h};
            SDL_RenderCopy(renderer, missesStatTexture, NULL, &missesStatRect);
            SDL_FreeSurface(missesStatSurface);
            SDL_DestroyTexture(missesStatTexture);

            char hit_ratio_str[32];
            float hit_ratio = (pt->hits + pt->misses > 0) ? (float)pt->hits / (pt->hits + pt->misses) * 100 : 0;
            snprintf(hit_ratio_str, sizeof(hit_ratio_str), "Hit Ratio: %.1f%%", hit_ratio);
            SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255);
            SDL_Rect hit_ratio_stat = {300, status_bar_y + 90, 150, 25};
            SDL_RenderFillRect(renderer, &hit_ratio_stat);
            SDL_Surface* hitRatioSurface = TTF_RenderText_Solid(font, hit_ratio_str, white);
            SDL_Texture* hitRatioTexture = SDL_CreateTextureFromSurface(renderer, hitRatioSurface);
            SDL_Rect hitRatioRect = {300 + (150 - hitRatioSurface->w)/2, status_bar_y + 90 + (25 - hitRatioSurface->h)/2, hitRatioSurface->w, hitRatioSurface->h};
            SDL_RenderCopy(renderer, hitRatioTexture, NULL, &hitRatioRect);
            SDL_FreeSurface(hitRatioSurface);
            SDL_DestroyTexture(hitRatioTexture);

            // Playback controls
            char play_str[32];
            snprintf(play_str, sizeof(play_str), "Playback: %s", auto_play ? "Auto" : "Manual");
            SDL_SetRenderDrawColor(renderer, auto_play ? 0 : 255, auto_play ? 255 : 0, 0, 255);
            SDL_Rect play_status = {550, status_bar_y + 20, 180, 25};
            SDL_RenderFillRect(renderer, &play_status);
            SDL_Surface* playSurface = TTF_RenderText_Solid(font, play_str, white);
            SDL_Texture* playTexture = SDL_CreateTextureFromSurface(renderer, playSurface);
            SDL_Rect playRect = {550 + (180 - playSurface->w)/2, status_bar_y + 20 + (25 - playSurface->h)/2, playSurface->w, playSurface->h};
            SDL_RenderCopy(renderer, playTexture, NULL, &playRect);
            SDL_FreeSurface(playSurface);
            SDL_DestroyTexture(playTexture);

            char speed_str[64];
            snprintf(speed_str, sizeof(speed_str), "Speed: %s (+ faster, - slower)",
                     animation_delay < 5 ? "Fast" : (animation_delay < 50 ? "Medium" : "Slow"));
            SDL_Surface* speedSurface = TTF_RenderText_Solid(font, speed_str, white);
            SDL_Texture* speedTexture = SDL_CreateTextureFromSurface(renderer, speedSurface);
            SDL_Rect speedRect = {550, status_bar_y + 55, speedSurface->w, speedSurface->h};
            SDL_RenderCopy(renderer, speedTexture, NULL, &speedRect);
            SDL_FreeSurface(speedSurface);
            SDL_DestroyTexture(speedTexture);

            // Controls explanation
            SDL_Surface* controlSurface = TTF_RenderText_Solid(font, "Controls: P = Toggle Auto/Manual, SPACE = Step in Manual", white);
            SDL_Texture* controlTexture = SDL_CreateTextureFromSurface(renderer, controlSurface);
            SDL_Rect controlRect = {550, status_bar_y + 90, controlSurface->w, controlSurface->h};
            SDL_RenderCopy(renderer, controlTexture, NULL, &controlRect);
            SDL_FreeSurface(controlSurface);
            SDL_DestroyTexture(controlTexture);
        }

        SDL_RenderPresent(renderer);

        if (show_animation && auto_play && advance_step) {
            SDL_Delay(animation_delay * 10);
        } else {
            SDL_Delay(16); // ~60 FPS when not in auto mode
        }

        if (step >= trace_size && auto_play) {
            SDL_Delay(500);
            show_animation = 0;
            show_graph = 1;
        }
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}