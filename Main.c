#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <math.h>


// Normal random numbers generator - Marsaglia algorithm.
float getRandomNumber(float minNum, float maxNum)
{
    float value, x, y, rsq, f;

    do {
        x = 2.0 * rand() / (float)RAND_MAX - 1.0;
        y = 2.0 * rand() / (float)RAND_MAX - 1.0;
        rsq = x * x + y * y;
    } while( rsq >= 1. || rsq == 0. );
    
    f = sqrt( -2.0 * log(rsq) / rsq );
    // (x * f) is a number between [-3, 2.9]
    // (x * f) + 3 to get a number between [0, 5.9]
    // ((x * f) + 3) * 100 / 5.9 to get a number between [0, 100]
    value = ((x * f) + 3) * maxNum / 5.9; // maxNum = highest number
    if (value > maxNum) value = maxNum; // In case value is > 100
    else if (value < minNum) value = minNum; // In case value is < 0

    return value;
}

pid_t createFork(int N, int i, char job[100]) {
    int pipefds[2], returnstatus;
    int pid = 0;
    char *readmessage;
    returnstatus = pipe(pipefds);
    if (returnstatus == -1) {
      printf("Unable to create pipe\n");
      return pid;
    }

    pid = fork();

    if (pid < 0) { // Error
        fprintf(stderr, "Fork Failed\n");
        return pid;
    }
    else if (pid == 0) { // Child process
        printf("\nHi I'm Mr Meeseeks! Look at Meeeee. (%d, %d, %d, %d)",
            getpid(), getppid(), N, i);

        sleep(1);

        read(pipefds[0], &readmessage, sizeof(readmessage));
        printf("\nChild Process - Reading from pipe - Job is %s", readmessage);
        return pid;
    }
    else { //Parent process
        printf("\nParent Process - Writing to pipe - Job is %s", job);
        write(pipefds[1], &job, sizeof(&job));
        return pid;
    }
}

int main ()
{
    char job[100], difficultyStr[100];
    float difficulty, time;
    int N = 0, i = 0;
    pid_t pidChild;

    printf("%s\n%s\n",
           "** Welcome to the Mr. Meeseeks Box **",
           "What do you want your Mr. Meeseeks for?");
    fgets(job, sizeof(job), stdin);

    printf("\n%s%s\n",
           "What's the difficulty of the task?",
           " (Press enter for random)");
    fgets(difficultyStr, sizeof(difficultyStr), stdin);

    // Defining difficulty
    if (difficultyStr[0] != '\n')
        difficulty = atof(difficultyStr);
    else
        difficulty = getRandomNumber(0, 100);

    time = getRandomNumber(0.5, 5);
    pidChild = createFork(N, i, job);

    //printf("Child %d, Father %d\n", getpid(), getppid());

    if (pidChild != 0) waitpid(pidChild, NULL, 0);

    return 0;
}
