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

char* command;
char* arg;
pid_t currentChild;

void error(int exitCode, char* errorText) {
    perror(errorText);
    fflush(stderr);
    exit(exitCode);
}

void endProcess(pid_t processId) {
    printf("Signalling %d\n", processId);
    fflush(stdout);
    kill(processId, SIGINT);
}

void setSignalHandler(int sig, void* fun) {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = fun;
    sigaction(sig, &sa, NULL);
}

// Output messages to console
void restoreConsole() {
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    dup2(stdoutCopy, STDOUT_FILENO);
    dup2(stderrCopy, STDERR_FILENO);
}

void signalInterruptHandler(int sig, siginfo_t* sigInfo, void* context) {
    printf("Stopped executing %s %s signal = %d\n", command, arg, sig);
    fflush(stdout);
    
    restoreConsole();

    endProcess(currentChild);
}

void signalQuitHandler(int sig, siginfo_t* sigInfo, void* context) {
    restoreConsole();

    endProcess(currentChild);

    printf("Exiting due to quit signal\n");
    fflush(stdout);

    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        printf("No command provided. Exiting...");
        exit(-1);
    }

    stdoutCopy = dup(STDOUT_FILENO);
    stderrCopy = dup(STDERR_FILENO);

    currentChild = -1;

    setSignalHandler(SIGINT, signalInterruptHandler);
    setSignalHandler(SIGQUIT, signalQuitHandler);

    command = argv[1];
    int fileNumber = 1;

    for (int i = 2; i < argc; i++) {
        arg = argv[i];

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

        printf("Executing %s %s\n", command, arg);
        fflush(stdout);

        currentChild = fork();
        if (currentChild == 0) {
            execlp(command, command, arg, NULL);

            printf("Error executing \"%s %s\". Exiting...\n", command, arg);
            fflush(stdout);
            error(errno, "exec");

            return EXIT_FAILURE;
         } else if (currentChild < 0) {
            printf("Error forking on input \"%s %s\". Exiting...\n", command, arg);
            fflush(stdout);
            error(errno, "main fork");

            return EXIT_FAILURE;
        }

        int exitCode = 0;
        pid_t pid;
        do {
            pid = wait(&exitCode);
        } while (errno == EINTR);
        
        if (pid >= 0) {
            printf("Finished executing %s %s exit code = %d\n", command, arg, exitCode);
            fflush(stdout);
        }
    }

    return EXIT_SUCCESS;
}