#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CK 1000 //Number of iteration to get a number from a normal distribution
#define MAXCOMPLETENESS 100
#define MAXTIME 300 //Max time until all Mr. Meeseeks get into a Planetary Chaos
#define MAXTIMEREQ 5    //Maximum time that has the request to give a respond
#define MINTIMEREQ 0.5  //Minimum time that has the request to give a respond
#define MIU 0           //Miu Parameter for the normal distribution
#define REQDIFLEVEL1 85.0   //Minimum number for the level 1 of difficulty: [85.0 to 100.0]
#define REQDIFLEVEL2 45.0   ////Minimum number for the level 2 of difficulty [45.0 to 85.0]
#define SIGMA 1 //Sigma Parameter for the normal distribution


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

int getNumberOfChildren(int &level, int &I, float difficulty){
    int numberOfChildren;
    switch(difficulty) {
        case difficulty < REQDIFLEVEL2:
            maxNumChildren = 2;
            numberOfChildren = getRandomNumber(0, maxNumChildren) * normalProbabilty(MIU, SIGMA, 1);
            level = level+1;
            I = 2;
        case REQDIFLEVEL2 > difficulty > REQDIFLEVEL1:
            maxNumChildren = 1;
            numberOfChildren = getRandomNumber(0, maxNumChildren) * normalProbabilty(MIU, SIGMA, 1);
            level = level+1;
            I = 1;
        default:
            maxNumChildren = 0;
            numberOfChildren = 0;
    }
    return numberOfChildren;
} 

//Base on: http://cypascal.blogspot.com/2016/02/crear-una-distribucion-normal-en-c.html
float normalProbabilty(float miu, float sigma, int range){
    srand(time(NULL));  //Reset the random seed
    int i = 1; float aux;
    for(i; i <= CK; i++){
        aux += (float)rand()/RAND_MAX;
    }
    return fabs(sigma * sqrt((float)12/CK) * (aux - (float)CK/2) + miu) * range;
}

pid_t createFork(int N, int i, char * job) {
    int pipefds[2], returnstatus;
    int pid = 0;
    char * readmessage;
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
    char *job, *difficultyStr;
    float difficulty, time, requestCompleteness = 0.0;
    int maxNumChildren, N = 1, i = 1, numChildren; // Is this 1 or 2 for the first fork?
    pid_t pidChild;

    job = malloc(sizeof(500));
    difficultyStr = malloc(sizeof(500));

    printf("%s\n%s\n",
           "** Welcome to the Mr. Meeseeks Box **",
           "What do you want your Mr. Meeseeks for?");
    fgets(job, sizeof(job), stdin);

    printf("\n%s%s\n",
           "What's the difficulty of the task?",
           " (Press enter for random)");
    fgets(difficultyStr, sizeof(difficultyStr), stdin);

    // Defining difficulty
    if (difficultyStr[0] != '\n'){
        requestDifficulty = atof(difficultyStr);
    }
    else
        requestDifficulty = normalProbabilty(MIU, SIGMA, 100);

    printf("Request's Diffculty= %f\n", requestDifficulty);
    time = getRandomNumber(MINTIMEREQ, MAXTIMEREQ); //Get the wait's time for the main fork
    pidChild = createFork(N, i, job);   //Create the main fork

    while(requestCompleteness < maxCompleteness || time < MAXTIME) {
        if(!pidChild) { //If the fork hasn't child
            numChildren = getNumberOfChildren(N, i, requestDifficulty); //Get the number of children
            int i = 0;
            for(i; i < numChildren; i++){   //Create the number of children
                pidChild = createFork(N, i, job);

            }
        } 
    }
    

    //printf("Child %d, Father %d\n", getpid(), getppid());

    if (pidChild != 0) waitpid(pidChild, NULL, 0);

    return 0;
}
