/*
 * Using first-fit FSMP because it is easy.
 * Upon add to freed space, create new free space entry with leftover space
 * Upon delete to last entry, set free_start to the start of that entry
 * Upon delete, make sure any adjacent free spaces are merged
 * We ensure there will never be two or more contiguous free spaces.
 * Thus when adding, we can easily search for the first empty space that fits our data
*/


#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#include "memdb.h"


static const int GROWTH_SIZE = 4 * (1 << 10);


// MAIN API
    // Returns location of new data
    moffset_t add(int, char*);
    void list();
    void delete(char*);

moffset_t search(char*);

// Helpers for managing map
    // Returns size
    void createDb(int);
    // Returns new size
    void growDb(int);

// Some helpers for entries
    void merge(struct entry_s*, struct entry_s*);
    void setNext(struct entry_s*, moffset_t);


struct fhdr_s *fhdr;
off_t size;
int temp;

int main(int argc, char **argv) {
    // <START Section copied from stringfile.c>
    if (argc <= 1 || argc >= 4 || (argc == 3 && (argv[2][0] != '-' || argv[2][1] != 't'))) {
        printf("USAGE: %s strings_file [-t (temporary flag)]\n", argv[0]);
        exit(1);
    }

    if (argc == 3) {
        temp = MAP_PRIVATE;
    } else {
        temp = MAP_SHARED;
    }


    int fd = open(argv[1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror(argv[1]);
        exit(2);
    }

    fhdr = mmap(NULL, INIT_SIZE, PROT_READ | PROT_WRITE, temp, fd, 0);
    if ((void *)fhdr == (void *)-1) {
        perror("mmap");
        exit(3);
    }

    struct stat s;
    if (fstat(fd, &s) == -1) {
        perror("fstat");
        exit(4);
    }

    size = s.st_size;
    // <END Section copied from stringfile.c>

    if (size == 0) {
        createDb(fd);
    } else {
        // <START Section copied from stringfile.c>
        if (fhdr->magic != FILE_MAGIC) {
            fprintf(stderr, "bad magic: not a strings file!\n");
            exit(6);
        }
        // <END Section copied from stringfile.c>
    }

    while (1) {
        char* input = NULL;
        size_t inputSize;
        if (getline(&input, &inputSize, stdin) == -1) {
            fprintf(stderr, "getline failed in input loop.\n");
            exit(8);
        }

        inputSize = 0;
        for (int i = 0; input[i] != '\n'; i++)
            inputSize++;

        char* str;
        if (input[0] == 'a' || input[0] == 'A' || input[0] == 'd' || input[0] == 'D') {
            if (inputSize < 2 || input[1] != ' ') {
                printf("Bad input. Expecting a space then a string after %c\n", input[0]);
                continue;
            }

            str = malloc(inputSize + 1);
            for (int i = 2; i < inputSize; i++)
                str[i - 2] = input[i];
            str[inputSize] = '\0';
        } else if (input[0] != 'l' && input[0] != 'L') {
            printf("Bad input. Please use any of: l, a string, d string\n");
            continue;
        }

        switch (input[0]) {
            case 'l':
            case 'L':
                list();
                break;
            case 'a':
            case 'A':
                add(fd, str);
                break;
            case 'd':
            case 'D':
                delete(str);
                break;
        }        
    }

    return 0;
}


moffset_t add(int fd, char str[]) {
    // Stolen from stringfile.c
    char* ptr = (char*) fhdr;

    // Already exists
    if (search(str)) {
        printf("Error: string already exists, not added.\n");
        printf("Note: wasn't sure if you actually wanted me to perror bc you said \"print an error\" but I figured you didn't mean that.\n");
        return 0;
    }

    // Entry to add to list
    struct entry_s entryToAdd = {
        .magic = ENTRY_MAGIC_DATA,
        .len = strlen(str) + 1
    };
    struct entry_s* entryToAdd_p = malloc(sizeof(entryToAdd) + entryToAdd.len);
    memcpy(entryToAdd_p, &entryToAdd, sizeof(entryToAdd));
    memcpy(sizeof(entryToAdd) + (char*) entryToAdd_p, str, entryToAdd.len);

    moffset_t offset = sizeof(*fhdr);

    // If no data
    if (!fhdr->data_start) {
        fhdr->data_start = fhdr->free_start;

        memcpy(ptr + offset, entryToAdd_p, sizeof(entryToAdd) + entryToAdd_p->len);

        fhdr->free_start += sizeof(entryToAdd) + entryToAdd_p->len;

        return offset;
    }

    // Keeps track of current index in loop
    struct entry_s* entryIndex_p;
    while (offset < fhdr->free_start) {
        entryIndex_p = (struct entry_s*) (ptr + offset);
        
        if (entryIndex_p->magic != ENTRY_MAGIC_DATA && entryIndex_p->magic != ENTRY_MAGIC_FREE) {
            // Stolen from stringfile.c
            fprintf(stderr, "bad magic: not an entry\n");
            exit(6);
        }

        if (entryIndex_p->magic == ENTRY_MAGIC_FREE && entryToAdd_p->len <= entryIndex_p->len) {
            int spaceLeft = entryIndex_p->len - entryToAdd_p->len;  
            moffset_t freeLocationOffset = offset + sizeof(*entryToAdd_p) + entryToAdd_p->len;
            // Add new empty space to the right if it fits
            struct entry_s freeEntry = {
                .magic = ENTRY_MAGIC_FREE,
                .len = spaceLeft - sizeof(*entryIndex_p)
            };
            if (freeLocationOffset + spaceLeft >= fhdr->free_start) {
                fhdr->free_start = freeLocationOffset;
            } else if (spaceLeft >= sizeof(freeEntry) + 1) {
                memcpy(ptr + freeLocationOffset, &freeEntry, sizeof(freeEntry));         
            } else {
                // Must add back space so we don't leave random floating data
                entryToAdd_p->len += spaceLeft;
            }


            setNext(entryToAdd_p, offset);
            memcpy(ptr + offset, entryToAdd_p, sizeof(entryToAdd) + entryToAdd_p->len);

            return offset;
        }

        offset += sizeof(*entryIndex_p) + entryIndex_p->len;
    }


    // No deleted space found, add at end of list
    offset = fhdr->free_start;
    setNext(entryToAdd_p, offset);

    while (offset + sizeof(entryToAdd) + entryToAdd_p->len > size) {
        growDb(fd);

        // Have to reassign ptr because fhdr changed in growDb
        ptr = (char*) fhdr;
    }
    memcpy(ptr + offset, entryToAdd_p, sizeof(entryToAdd) + entryToAdd_p->len);

    fhdr->free_start += sizeof(entryToAdd) + entryToAdd_p->len;
    return offset;
}

void list() {
    // Stolen from stringfile.c
    char* ptr = (char*) fhdr;

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

void delete(char str[]) {
    moffset_t offset = search(str);

    if (!offset) {
        printf("String not found.\n");
        return;
    }

    //Stolen from stringfile.c
    char* ptr = (char*) fhdr;

    struct entry_s* entryTarget_p = (struct entry_s*) (ptr + offset);

    if (entryTarget_p->magic != ENTRY_MAGIC_DATA && entryTarget_p->magic != ENTRY_MAGIC_FREE) {
        // Stolen from stringfile.c
        fprintf(stderr, "bad magic: not an entry\n");
        exit(6);
    } else if (entryTarget_p->magic == ENTRY_MAGIC_FREE) {
        fprintf(stderr, "bad magic: found a deleted entry when deleting\n");
        exit(6);
    }

    // If deleting head
    if (offset == fhdr->data_start) {
        fhdr->data_start = entryTarget_p->next;
    } else {
        // We must find the previous entry and set its next to the deleted entry's next
        struct entry_s* entryIndex_p;
        for (
            entryIndex_p = (struct entry_s*) (ptr + fhdr->data_start);
            entryIndex_p->next != offset;
            entryIndex_p = (struct entry_s*) (ptr + entryIndex_p->next)
        ) {
            if (entryIndex_p->magic != ENTRY_MAGIC_DATA && entryIndex_p->magic != ENTRY_MAGIC_FREE) {
                // Stolen from stringfile.c
                fprintf(stderr, "bad magic: not an entry\n");
                exit(6);
            } else if (entryIndex_p->magic == ENTRY_MAGIC_FREE) {
                fprintf(stderr, "bad magic: found a deleted entry when deleting\n");
                exit(6);
            }
        }
        entryIndex_p->next = entryTarget_p->next;
    }

    // We must merge left entry and/or right entry if necessary
    struct entry_s* entryLeft_p = entryTarget_p;
    // If not first entry
    if (offset != sizeof(*fhdr)) {
        for (
            entryLeft_p = (struct entry_s*) (ptr + sizeof(*fhdr));
            ((char*) entryLeft_p) + sizeof(*entryLeft_p) + entryLeft_p->len != ptr + offset;
            entryLeft_p = (struct entry_s*) (((char*) entryLeft_p) + sizeof(*entryLeft_p) + entryLeft_p->len)
        ) {
            if (entryLeft_p->magic != ENTRY_MAGIC_DATA && entryLeft_p->magic != ENTRY_MAGIC_FREE) {
                // Stolen from stringfile.c
                fprintf(stderr, "bad magic: not an entry\n");
                exit(6);
            }
        }
        if (entryLeft_p->magic == ENTRY_MAGIC_FREE) {
            merge(entryLeft_p, entryTarget_p);
        } else {
            entryLeft_p = entryTarget_p;
        }
    }

    struct entry_s* entryRight_p = (struct entry_s*) (((char*) entryLeft_p) + sizeof(*entryLeft_p) + entryLeft_p->len);
    if (
        entryRight_p < (struct entry_s*) (ptr + fhdr->free_start)
        && entryRight_p->magic == ENTRY_MAGIC_FREE
    ) {
        merge(entryLeft_p, entryRight_p);
    }

    entryTarget_p->magic = ENTRY_MAGIC_FREE;

    // If deleting last entry
    if (offset + sizeof(*entryTarget_p) + entryTarget_p->len == fhdr->free_start) {
        fhdr->free_start = offset;
    }

}


moffset_t search(char target[]) {
    moffset_t offset = sizeof(*fhdr);

    // If no data
    if (!offset) return 0;


    // Stolen from stringfile.c
    char* ptr = (char*) fhdr;

    struct entry_s* entry_p;
    while (offset < fhdr->free_start) {
        entry_p = (struct entry_s*) (ptr + offset);

        if (entry_p->magic == ENTRY_MAGIC_DATA || entry_p->magic == ENTRY_MAGIC_FREE) {
            if (entry_p->magic == ENTRY_MAGIC_DATA && strcmp(entry_p->str, target) == 0) {
                return offset;
            }
        } else {
            // Stolen from stringfile.c
            fprintf(stderr, "bad magic: not an entry\n");
            exit(6);
        }


        offset += sizeof(*entry_p) + entry_p->len;
    }
    
    return 0;
}


// Stolen from stringfile.c
void createDb(int fd) {
    char* ptr = (char*) fhdr;

    if (ftruncate(fd, INIT_SIZE) == -1) {
        perror("ftruncate on create");
        exit(5);
    }
    fhdr->magic = FILE_MAGIC;
    // no strings, so point to 0;
    fhdr->data_start = 0;
    // no strings, so point to the end of the header
    fhdr->free_start = sizeof(*fhdr);


    // return INIT_SIZE;
    size = INIT_SIZE;
}

void growDb(int fd) {
    munmap(fhdr, size);

    size += GROWTH_SIZE;
    if (size > MAX_SIZE) {
        fprintf(stderr, "Error: Tried to grow over max size.");
        exit(9);
    }

    // <START Section copied from stringfile.c>
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate on growth");
        exit(7);
    }
    // <END Section copied from stringfile.c>

    fhdr = mmap(NULL, size, PROT_READ | PROT_WRITE, temp, fd, 0);
}


void merge(struct entry_s* entryLeft_p, struct entry_s* entryRight_p) {
    struct entry_s entryFree = {
        .magic = ENTRY_MAGIC_FREE,
        .len = entryLeft_p->len + sizeof(*entryRight_p) + entryRight_p->len
    };
    memcpy(entryLeft_p, &entryFree, sizeof(entryFree));
}

void setNext(struct entry_s* entry_p, moffset_t entryLocation) {
    // Stolen from stringfile.c
    char* ptr = (char*) fhdr;


    moffset_t offset = fhdr->data_start;
    struct entry_s* entryIndex_p;

    do {
        entryIndex_p = (struct entry_s*) (ptr + offset);

        if (entryIndex_p->magic != ENTRY_MAGIC_DATA && entryIndex_p->magic != ENTRY_MAGIC_FREE) {
            // Stolen from stringfile.c
            fprintf(stderr, "bad magic: not an entry\n");
            exit(6);
        } else if (entryIndex_p->magic == ENTRY_MAGIC_FREE) {
            fprintf(stderr, "bad magic: some entry points to an empty entry\n");
            exit(6);
        }

        if (offset == fhdr->data_start && strcmp(entryIndex_p->str, entry_p->str) > 0) {
            // Entry should be new start
            entry_p->next = offset;
            fhdr->data_start = entryLocation;
            return;
        } else if (entryIndex_p->next) {
            struct entry_s* nextEntry_p = (struct entry_s*) (ptr + entryIndex_p->next);
            if (
                nextEntry_p->magic == ENTRY_MAGIC_DATA
                && strcmp(nextEntry_p->str, entry_p->str) > 0
            ) {
                // Entry should be somewhere in the middle
                entry_p->next = entryIndex_p->next;
                entryIndex_p->next = entryLocation;
                return;
            }
        } else {
            // Entry should be at end
            entryIndex_p->next = entryLocation;
            return;
        }

        
        offset = entryIndex_p->next;
    } while (offset);

    // Defaults to 0 if we can't find a string that goes after
    entry_p->next = 0;
}