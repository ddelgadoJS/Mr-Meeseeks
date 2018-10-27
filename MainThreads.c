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

void *threadFunction(void *dummyPtr);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
float p = 0.0;

char job[100], difficultyStr[100];
float requestDifficulty, localWaitTime;
int maxNumChildren, N = 0, i = 0, numChildren;
pthread_t thread_id;

int fd[2]; //File descriptor for creating a pipe

/*** Time variables initialization ***/
time_t startTime, currentTime, localTime1, localTime2;
struct tm *startStruct;
struct tm currentStruct;
struct tm localTime1Struct;
struct tm localTime2Struct;
char buffer[160];
/*** End of time initialization ***/

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

void append(char* s, char c)
{
        int len = strlen(s);
        s[len] = c;
        s[len+1] = '\0';
}

void *reader()
{
    char ch = ' ', stCh[100] = "Child Process - Reading from pipe - Job is ";

    while(ch != '\n'){
        read (fd[0], &ch, 1);
        append(stCh, ch);
    }
    
    printf("%s\n", stCh);
    threadFunction(stCh);
}

void *writer(int N, int i, char job[100])
{
   int result;
   char *ch=job;

   printf("Parent Process - Writing to pipe - Job is %s\n", job);

   while(1){
        // printf("Parent Process - Writing to pipe - Char is %c\n", *ch);
        if (write (fd[1], &*ch, 1) != 1){
            perror ("write");
            exit (2);
        }

        ch++;

        if (*ch == '\n') {
            // printf("Parent Process - Writing to pipe - Char is %c\n", *ch);
            if (write (fd[1], &*ch, 1) != 1){
                perror ("write");
                exit (2);
            }
            break;
        }
    }
}

void *threadFunction(void *dummyPtr) {
    /** Defines current time for all processess **/
    currentTime = time(NULL);
    currentStruct = *((struct tm*)localtime(&currentTime));

    /*** Solving and creating children***/
    while(p < MAXCOMPLETENESS && difftime(currentTime, startTime) < MAXTIME){
        localWaitTime = getNormalProbNum(MINTIMEREQ, MAXTIMEREQ);
        
        localTime1 = time(NULL);
        localTime1Struct = *((struct tm*)localtime(&localTime1));

        while (p < MAXCOMPLETENESS && difftime(localTime2, localTime1) < localWaitTime) {
            pthread_mutex_lock( &mutex1 );
            //printf ("Mr. Meeseeks(%d) is trying to solve the task.\n", getpid());
            sleep (0.5); // This sleeps seems to be necessary for the synchronization
            p += requestDifficulty * normalProbabilty(MIU, SIGMA, 1);
            printf ("Mr. Meeseeks(%lu) newvalue of *p=%f.\n", pthread_self(), p);

            // HERE MUST GO THE MEESEEKS THAT FINISHED THE TASK
            pthread_mutex_unlock( &mutex1 );

            currentTime = time(NULL);
            currentStruct = *((struct tm*)localtime(&currentTime));

            localTime2 = time(NULL);
            localTime2Struct = *((struct tm*)localtime(&localTime2));
        }

        if(p < MAXCOMPLETENESS){
            //if(!pidChild){ // If the fork doesn't have a child
                numChildren = getNumberOfChildren(&N, &i, requestDifficulty); // Get number of children to create
                N++;
                for(i = 0; i < numChildren; i++){ // Create children
                    if (pipe(fd) < 0){
                        perror("pipe ");
                        exit(1);
                    }

                    pthread_create(&thread_id, NULL, reader, NULL);
                    writer(N, i, job);
                }
            //}
        } else break;
    }
}

int main ()
{
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

    /** Defines the start time **/
    time(&startTime); 
    startStruct = localtime(&startTime); 

    /** Defining difficulty **/
    if (difficultyStr[0] != '\n') requestDifficulty = atof(difficultyStr);
    else requestDifficulty = getNormalProbNum(0, 100);

    printf ("Task's difficulty is: %f.\n", requestDifficulty);
    printf("-----------------------------------------------------------------------\n");

    pthread_create(&thread_id, NULL, threadFunction, NULL);

    pthread_join( thread_id, NULL);
    
    printf("%s\n", "The task was completed :)");
    printf("Execution time: %f\n", difftime(currentTime, startTime));

    /*** Problem solved or limit time reached ***/
    
    /*if (pidChild != 0) {
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
    }*/

    return 0;
}