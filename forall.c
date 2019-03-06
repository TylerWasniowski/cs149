#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/wait.h>

#include <errno.h>

#include <math.h>
#include <string.h>

#include <sys/types.h>


char* fileExtension = ".out";

int stdoutCopy;
int stderrCopy;

void error(int exitCode, char errorText[]) {
    perror(errorText);
    fflush(stderr);
    exit(exitCode);
}

// Output messages to console
void restoreConsole() {
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    dup2(stdoutCopy, STDOUT_FILENO);
    dup2(stderrCopy, STDERR_FILENO);
}

void signalHandler(int sig, siginfo_t* sigInfo, void* context) {
    restoreConsole();

    printf("Signalling %d\n", sig);
    // TODO: signal to child
    fflush(stdout);
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("No command provided. Exiting...");
        exit(-1);
    }

    stdoutCopy = dup(STDOUT_FILENO);
    stderrCopy = dup(STDERR_FILENO);

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = signalHandler;
    sigaction(SIGINT, &sa, NULL);

    char* command = argv[1];
    int fileNumber = 1;

    for (int i = 2; i < argc; i++) {
        int filenameLength = (log(fileNumber) / log(10) + 1) + strlen(fileExtension);
        char filename[filenameLength];

        // Learned to use sprintf from here: https://stackoverflow.com/a/5172154
        sprintf(filename, "%d.out", fileNumber);

        // Will route stdout to the next open file, and stderr to the one after that
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        int outFd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
        int errFd = dup(outFd);

        fileNumber++;


        switch (fork()) {
            case 0:
                sa.sa_sigaction = SIG_IGN;
                sigaction(SIGINT, &sa, NULL);

                printf("Executing %s %s\n", command, argv[i]);
                fflush(stdout);
                execlp(command, command, argv[i], NULL);

                printf("Error executing \"%s %s\". Exiting...\n", command, argv[i]);
                fflush(stdout);
                error(errno, "exec");
                break;
            case -1:
                printf("Error forking on input \"%s %s\". Exiting...\n", command, argv[i]);
                fflush(stdout);
                error(errno, "main fork");
                break;
        }

        int exitCode;
        pid_t pid;
        do {
            pid = wait(&exitCode);
        } while (errno == EINTR);

        printf("Finished executing %s %s exit code = %d\n", command, argv[i], exitCode);
        fflush(stdout);
    }
}