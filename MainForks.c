// Compile with: gcc -o MainForks MainForks.c -lm -pthread

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <fcntl.h>

#define CK 1000 //Number of iteration to get a number from a normal distribution
#define MAXCOMPLETENESS 100
#define MAXTIME 90 //Max time until all Mr. Meeseeks get into a Planetary Chaos
#define MAXTIMEREQ 5    //Maximum time that has the request to give a respond
#define MINTIMEREQ 0.5  //Minimum time that has the request to give a respond
#define MIU 0           //Miu Parameter for the normal distribution
#define REQDIFLEVEL1 85.0   //Minimum number for the level 1 of difficulty: [85.0 to 100.0]
#define REQDIFLEVEL2 45.0   ////Minimum number for the level 2 of difficulty [45.0 to 85.0]
#define SIGMA 1 //Sigma Parameter for the normal distribution


// Normal random numbers generator - Marsaglia algorithm.
float getNormalProbNum(float minNum, float maxNum){
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

//Base on: http://cypascal.blogspot.com/2016/02/crear-una-distribucion-normal-en-c.html
float normalProbabilty(float miu, float sigma, int range){
    srand(time(NULL));  //Reset the random seed
    int i = 1; float aux;
    for(i; i <= CK; i++){
        aux += (float)rand()/RAND_MAX;
    }
    return fabs(sigma * sqrt((float)12/CK) * (aux - (float)CK/2) + miu) * range;
}

int getNumberOfChildren(int *level, int *I, float difficulty){
    int numberOfChildren = 0, maxNumChildren;
    if (difficulty < REQDIFLEVEL2){
        maxNumChildren = 2;
        numberOfChildren = getNormalProbNum(0, maxNumChildren) * normalProbabilty(MIU, SIGMA, 1);
        *level = *level+1;
        *I = 2;
    }else if (difficulty < REQDIFLEVEL1){
        maxNumChildren = 1;
        numberOfChildren = getNormalProbNum(0, maxNumChildren) * normalProbabilty(MIU, SIGMA, 1);
        *level = *level+1;
        *I = 1;
    }
    return numberOfChildren;
} 

pid_t createFork(int N, int i, char job[100]){
    int pipefds[2], returnstatus, pid = 0;
    char *readmessage;
    char *testmessage;
    returnstatus = pipe(pipefds);
    if (returnstatus == -1) {
      printf("Unable to create pipe\n");
      return pid;
    }

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork Failed\n");
    }
    else if (pid == 0) { // Child process
        printf("\nHi I'm Mr Meeseeks! Look at Meeeee. (%d, %d, %d, %d)",
            getpid(), getppid(), N, i);

        //sleep(1);

        read(pipefds[0], &readmessage, sizeof(readmessage));
        printf("\nChild Process - Reading from pipe - Job is %s", readmessage);
    }
    else { //Parent process
        printf("\nParent Process - Writing to pipe - Job is %s", job);
        write(pipefds[1], &job, sizeof(&job));
    }

    return pid;
}

void taskCompleted(pid_t MeeseeksWhoFinished, pid_t originalMeeseeks) {

}

int main ()
{
    char job[100], difficultyStr[100];
    float requestDifficulty, localWaitTime;
    int maxNumChildren, N = 0, i = 0, numChildren;
    pid_t originalProcess = getpid(), originalFather = getppid();

    /*** Time variables initialization ***/
    time_t startTime, currentTime, localTime1, localTime2;
    struct tm *startStruct;
    struct tm currentStruct;
    struct tm localTime1Struct;
    struct tm localTime2Struct;
    char buffer[160];
    /*** End of time initialization ***/

    /*** Allocating shared memory ***/
    key_t shmkey; // Shared memory key
    int shmid; // Shared memory id
    sem_t *sem; // Synch semaphore (shared)
    pid_t pidChild = 0; // Fork pid
    float *p; // Shared variable (shared)
    unsigned int value = 1; // Semaphore value

    /* Initialize shared variables in shared memory */
    shmkey = ftok ("/dev/null", 5);
    shmid = shmget (shmkey, sizeof (int), 0644 | IPC_CREAT);
    if (shmid < 0) { // Shared memory error check
        perror ("shmget\n");
        exit (1);
    }

    // Initialize semaphores for shared processes
    sem = sem_open ("pSem", O_CREAT | O_EXCL, 0644, value); 
    // Name of semaphore is "pSem", semaphore is reached using this name
    sem_unlink ("pSem");      
    // Unlink prevents the semaphore existing forever
    // if a crash occurs during the execution
    //printf ("semaphores initialized.\n\n");

    p = (float *) shmat (shmid, NULL, 0); // Attach p to shared memory
    *p = 0;
    /*** Finished allocating shared memory ***/


    /*** Receiving input ***/
    printf("%s\n%s\n",
           "** Welcome to the Mr. Meeseeks Box **",
           "What do you want your Mr. Meeseeks for?");
    fgets(job, sizeof(job), stdin);

    printf("\n%s%s\n",
           "What's the difficulty of the task?",
           " (Press enter for random)");
    fgets(difficultyStr, sizeof(difficultyStr), stdin);
    /*** Finished receiving input ***/

    /** Defining difficulty **/
    if (difficultyStr[0] != '\n') requestDifficulty = atof(difficultyStr);
    else requestDifficulty = getNormalProbNum(0, 100);
    
    printf ("Task's difficulty is: %f.\n", requestDifficulty);
    printf("-----------------------------------------------------------------------\n");

    /** Defines the start time **/
    time(&startTime); 
    startStruct = localtime(&startTime); 

    pidChild = createFork(N, i, job); // Create main fork

    /** Defines current time for all processess **/
    currentTime = time(NULL);
    currentStruct = *((struct tm*)localtime(&currentTime));

    /*** Solving and creating children***/
    while(*p < MAXCOMPLETENESS && difftime(currentTime, startTime) < MAXTIME){
        localWaitTime = getNormalProbNum(MINTIMEREQ, MAXTIMEREQ);
        
        localTime1 = time(NULL);
        localTime1Struct = *((struct tm*)localtime(&localTime1));

        while (*p < MAXCOMPLETENESS && difftime(localTime2, localTime1) < localWaitTime) {
            sem_wait (sem);
            //printf ("Mr. Meeseeks(%d) is trying to solve the task.\n", getpid());
            sleep (0.5); // This sleeps seems to be necessary for the synchronization
            *p += requestDifficulty * normalProbabilty(MIU, SIGMA, 1);
            printf ("Mr. Meeseeks(%d) newvalue of *p=%f.\n", getpid(), *p);

            // HERE MUST GO THE MEESEEKS THAT FINISHED THE TASK
            sem_post (sem);

            currentTime = time(NULL);
            currentStruct = *((struct tm*)localtime(&currentTime));

            localTime2 = time(NULL);
            localTime2Struct = *((struct tm*)localtime(&localTime2));
        }

        if(*p < MAXCOMPLETENESS){
            if(!pidChild){ // If the fork doesn't have a child
                numChildren = getNumberOfChildren(&N, &i, requestDifficulty); // Get number of children to create
                N++;
                for(i = 0; i < numChildren; i++){ // Create children
                    pidChild = createFork(N, i, job);
                }
            }
        } else break;
    }
    /*** Problem solved or limit time reached ***/
    
    if (pidChild != 0) {
        // Wait for all children to exit
        while (pidChild = waitpid (-1, NULL, 0)){
            if (errno == ECHILD)
                break;
        }
        printf ("\nAll Mr. Meeseeks created by %d have died.\n", getpid());

        if (originalProcess == getpid()) {
            // TODO COMPLETION MESSAGE
            currentTime = time(NULL);
            currentStruct = *((struct tm*)localtime(&currentTime));

            printf("%s\n", "The task was completed :)");
            printf("Execution time: %f\n", difftime(currentTime, startTime));
        }

        // Shared memory detach
        shmdt(p);
        shmctl(shmid, IPC_RMID, 0);
        // Cleanup semaphores
        sem_destroy(sem);

        // Say goodbye and destroy Meeseeks (parent)
        printf("\nMr. Meeseeks %d died :)\n", getpid());
        kill(getpid(), SIGKILL);
    } 
    else { // Meeseeks without children
        // Shared memory detach
        shmdt(p);
        shmctl(shmid, IPC_RMID, 0);
        // Cleanup semaphores
        sem_destroy(sem);

        // Say goodbye and destroy Meeseeks
        printf("\nMr. Meeseeks %d died :)\n", getpid());
        kill(getpid(), SIGKILL);
    }

    return 0;
}
