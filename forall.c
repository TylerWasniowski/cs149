#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/types.h>


int main(int argc, char** argv)
{
    if (argc <= 1) {
        printf("No command provided. Exiting...");
        exit(-1);
    }

    char* command = argv[1];
    int fileNumber = 1;

    // Will route stdout to the next open file, and stderr to the one after that
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    for (int i = 2; i < argc; i++) {
        int nameLength = log(fileNumber) / log(10) + 1;
        char filename[nameLength + 4];

        // Learned to do this from here: https://stackoverflow.com/a/5172154
        sprintf(filename, "%d.out", fileNumber);

        int outFd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
        int errFd = dup(outFd);

        fileNumber++;

        switch (fork()) {
            case 0:
                printf("Executing %s %s\n", command, argv[i]);
                fflush(stdout);
                execlp(command, command, argv[i], NULL);
                exit(0);
                break;
            case -1:
                printf("Error executing input \"%s\". Exiting...\n", argv[i]);
                exit(-1);
                break;
        }

        int exitCode;
        wait(&exitCode);
        printf("Finished executing %s %s exit code = %d\n", command, argv[i], exitCode);
        fflush(stdout);
        close(outFd);
        close(errFd);
    }
}