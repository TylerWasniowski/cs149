/*
 * Using first fit FSMP because it is easy.
 * Upon delete, make sure any adjacent free spaces are merged
 * Upon add to freed space, create new free space entry with leftover space
 * Upon delete to last entry, set free_start to the start of that entry
 * We ensure there will never be two or more contiguous free spaces.
 * Thus when adding, we search for the first empty space that fits our data
*/


#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include "memdb.h"


static const int GROWTH_SIZE = 5 * (1 << 10);

// MAIN API
    // Returns location of new data
    moffset_t add(fhdr, str);
    void list(fhdr);
    void delete(fhdr);

moffset_t search(fhdr, str);

// Returns size
off_t createDb(fhdr, fd);
// Returns new size
off_t growDb(fhdr, fd, currentSize);


int main(int argc, char **argv) {
    // <START Section copied from stringfile.c>
    if (argc <= 1 || argc >= 4) {
        printf("USAGE: %s strings_file [string to add] <-t (temporary flag)>\n", argv[0]);
        exit(1);
    }


    int fd = open(argv[1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror(argv[1]);
        exit(2);
    }

    struct fhdr_s *fhdr = mmap(NULL, INIT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ((void *)fhdr == (void *)-1) {
        perror("mmap");
        exit(3);
    }

    struct stat s;
    if (fstat(fd, &s) == -1) {
        perror("fstat");
        exit(4);
    }

    off_t size = s.st_size;
    // <END Section copied from stringfile.c>

    if (size == 0) {
        size = createDb(fhdr, fd);
    } else {
        // <START Section copied from stringfile.c>
        if (fhdr->magic != FILE_MAGIC) {
            fprintf(stderr, "bad magic: not a strings file!\n");
            exit(6);
        }
        // <END Section copied from stringfile.c>
    }

    // // get a byte pointer to the beginning of the file so that we can
    // // just add byte offsets
    // char *ptr = (char *)fhdr;

    // if (argc == 2) {
    //     // we dump the list if we aren't given a string to add
    //     int offset = sizeof(*fhdr);
    //     while (offset < fhdr->end_of_last_string) {
    //         printf("%s\n", ptr + offset);
    //         offset += strlen(ptr+offset) + 1; // +1 for the null at the end
    //     }
    // } else {
    //     int new_end = fhdr->end_of_last_string + strlen(argv[2]) + 1;
    //     while (new_end > size) {
    //         // <START Section copied from stringfile.c>
    //         size += GROWTH_SIZE;

    //         if (ftruncate(fd, size) == -1) {
    //             perror("ftruncate");
    //             exit(7);
    //         }
    //         // <END Section copied from stringfile.c>



    //     }
    //     strcpy(ptr + fhdr->end_of_last_string, argv[2]);
    //     fhdr->end_of_last_string += strlen(argv[2]) + 1;
    // }
    return 0;
}


moffset_t add(struct fhdr_s* fhdr, char str[]) {
    void placeEntry();

    // Stolen from stringfile.c
    char* ptr = (char*) fhdr;

    // Already exists
    if (search(fhdr, str) != -1) {
        printf("Error: string already exists, not added.\n");
        printf("Note: wasn't sure if you actually wanted me to perror bc you said \"print an error\" but I figured you didn't mean that.\n");
        return -1;
    }

    // Entry to add to list
    struct entry_s entryToAdd = {
        .magic = ENTRY_MAGIC_DATA,
        .str = str
    };
    entryToAdd.len = sizeof(entryToAdd);

    moffset_t offset = fhdr->data_start;

    // If no data
    if (!offset) {
        fhdr->data_start = fhdr->free_start;
        offset = fhdr->free_start;

        placeEntry();

        fhdr->free_start += entryToAdd.len;

        return offset;
    }

    // Find placement
        // Keeps track of current index in loop
        struct entry_s* entryIndex_p;
        do {
            entryIndex_p = (struct entry_s*) (ptr + offset);
            
            if (entryIndex_p->magic != ENTRY_MAGIC_DATA && entryIndex_p->magic != ENTRY_MAGIC_FREE) {
                // Stolen from stringfile.c
                fprintf(stderr, "bad magic: not an entry\n");
                exit(6);
            }

            if (entryIndex_p->magic == ENTRY_MAGIC_FREE && entryToAdd.len <= entryIndex_p->len) {
                // Add new empty space to the right if it fits
                    struct entry_s freeEntry = {
                        .magic = ENTRY_MAGIC_FREE,
                        .str = " "
                    };
                    .len = sizeof(freeEntry);
                    
                    int spaceLeft = entryIndex_p->len - entryToAdd.len;               
                    if (spaceLeft >= freeEntry.len)
                        memcpy(ptr + offset + entryToAdd.len, &freeEntry, freeEntry.len);                    
                

                setNext();
                placeEntry();

                return offset;
            } else if (entryIndex_p->magic == ENTRY_MAGIC_DATA) {
                printf("%s\n", entryIndex_p->str);
                offset += entryIndex_p->len;
            }


            offset += entryIndex_p->len;
        } while (offset < fhdr->free_start);

        // No deleted space found, add at end of list
        offset = fhdr->free_start;
        setNext();
        placeEntry();

        fhdr->free_start += entryToAdd.len;
        return offset;


    void placeEntry() {
        memcopy(ptr + offset, &entryToAdd, sizeof(entryToAdd));
    }

    void setNext() {
        offset = fhdr->data_start;

        do {
            entryIndex_p = (struct entry_s*) (ptr + offset);

            if (entry->magic != ENTRY_MAGIC_DATA && entry->magic != ENTRY_MAGIC_FREE) {
                // Stolen from stringfile.c
                fprintf(stderr, "bad magic: not an entry\n");
                exit(6);
            } else if (entry->magic == ENTRY_MAGIC_FREE) {
                fprintf(stderr, "bad magic: some entry points to an empty entry\n");
                exit(6);
            }

            if (strcmp(entryIndex_p->str, entryToAdd.str) > 0) {
                entryToAdd.next = offset;
                return;
            }

            offset = entryIndex_p->next;
        } while (entryIndex_p->next);

        // Defaults to 0 if we can't find a string that goes after
        entryToAdd.next = 0;
    }
}

void list(struct fhdr_s* fhdr) {
    // Stolen from stringfile.c
    char* ptr = (char*) fdhr;

    moffset_t offset = fhdr->data_start;

    // No data
    if (!offset) return;

    struct entry_s* entry_p;
    do {
        entry_p = (struct entry_s*) (ptr + offset);

        if (entry_p->magic != ENTRY_MAGIC_DATA && entry_p->magic != ENTRY_MAGIC_FREE) {
            // Stolen from stringfile.c
            fprintf(stderr, "bad magic: not an entry\n");
            exit(6);
        } else if (entry_p->magic == ENTRY_MAGIC_FREE) {
            fprintf(stderr, "bad magic: some entry points to an empty entry\n");
            exit(6);
        }

        printf("%s\n", entry_p->str);
        offset = entry_p->next;
    } while (entry_p->next);
}

void delete(struct fhdr_s* fhdr, char str[]) {
    moffset_t offset = search(fhdr, str);
}


moffset_t search(struct fhdr_s* fhdr, char target[]) {
    // Stolen from stringfile.c
    char* ptr = (char*) fdhr;

    moffset_t offset = fhdr->data_start;

    // No data
    if (!offset) return;

    struct entry_s* entry_p;
    while (offset < fhdr->free_start) {
        entry_p = (struct entry_s*) (ptr + offset);

        if (entry_p->magic == ENTRY_MAGIC_DATA || entry_p->magic == ENTRY_MAGIC_FREE) {
            if (entry_p->magic == ENTRY_MAGIC_DATA && strcmp(entry_p->str, target) == 0) return offset;
            offset += entry_p->len;
        } else {
            // Stolen from stringfile.c
            fprintf(stderr, "bad magic: not an entry\n");
            exit(6);
        }
    }
    
    return 0;
}


// Stolen from stringfile.c
off_t createDb(struct fhdr_s* fhdr, int fd) {
    char* ptr = (char*) fdhr;

    if (ftruncate(fd, INIT_SIZE) == -1) {
        perror("ftruncate");
        exit(5);
    }
    fhdr->magic = FILE_MAGIC;
    // no strings, so point to 0;
    fhdr->data_start = 0;
    // no strings, so point to the end of the header
    fhdr->free_start = sizeof(*fhdr);

    // nothing deleted yet, so set deleted items 0
    int* deletedItems_p = ptr + INIT_SIZE - 4;
    *deleteItems_p = 0;

    return INIT_SIZE;
}