/*
ElevatorSim.c

Andrew Samost

Description: 	Simulates an elvator scheduler
				gets the number of floors, time interval for when call requests will come in, and number of seconds from commandline args
				runs the simulation until the time is up and outputs statistical data
			 	prints out all significant events as they happen (picking up, dropping off, calls being made) as well as
				what floor they occured on and how many passengers there were at each event
				
Input: 			gotten from command line arguments
				1 - int -> number of floors
				2 - int -> time interval when call requests come in
				3 - int -> length (seconds) of the simulation

				Example: ./a.out 8 5 180

Output:			printed to console
				prints out each event with the time it occured, the number of passengers involed, and which floor.
				IE: Time  12: The elevator picked up 2 passengers at floor 3
				IE: Time   0: Call Recieved at F5 with destination to F16
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

/* Function Declarations */


void Init(int[][5]);
void Run(int[][5]);
void *Stop();
void *IssueRequest();
void WriteToCallsWaiting(int, int, int, int, int);
void IssueRequestPre();
int FindClosestCall(int);
int PassengersExiting(int[][5], int, int);
int UpdateNextJobs(int, int);
int StillInProgress(int[][5]);
int ChangeState(int);
int ChangeStateHelper(int);
int PickingUp(int[][5], int, int, int);
int PutOnElevator(int[6], int[][5], int);


/* End Function Declarations */

/* globals */


const int MAXPASSENGERS = 10;
const double CALLREQCHANCE = .15; /* the chance a call will generate on each floor */
int CurTime; /* holds the current time at any given moment */
int FloorThread = 0; /* keeps track of the floor that the current thread is working on */
int NumFloors; /* holds the number of floors */
int NumPassengers = 0; /* keeps track of the number of passengers in the elevator */
int AverageWaitTime[2] = {0, 0}; /* adds the wait times to [0] and increments [1] to find average at end */
int AverageTurnaround[2] = {0, 0}; /* adds the turnaround times to [0] and increments [1] to find average at end */
sem_t mutex; /* mutex lock for when each thread wants to write to the callsWaiting array */
int **callsWaiting;
int TimeInterval;
int AllThreads;
int TotalTimeSim; /* when I was passing this variable to the stop function, even though they had the same memory addresses, the value was overwritten? */
time_t SaveTime;
time_t TimeNow;


/* end globals */



int main(int argc, char **argv)
{
	/* checks for correct num of args */
	if (argc != 4){
		printf("%s\n", "Wrong # of Arguments\nExpected # floors, time interval for calls, total time for simulation(seconds)\nExample: ./a.out 8 5 180");
		exit(1);
	}
	CurTime = 0;
	NumFloors = atoi(argv[1]);		/* number of elevator floors */
	TimeInterval = atoi(argv[2]);	/* time interval between elevator calls */
	TotalTimeSim = atoi(argv[3]);	/* total time the simulation will run in seconds */
	int callsWaiting[NumFloors * 2][6];		/* holds the calls waiting */
	int elevator[NumFloors][5];			/* holds current jobs currently in the elevator */
	Init(elevator); /* initializes data in the 2 arrays */
	Run(elevator);
}

void Init(int elevator[][5])
{
	pthread_t finish;
	sem_init(&mutex, 0, 1);
	pthread_t threads[NumFloors];
	int locations[NumFloors];
	int ret;

	/* allocate callsWaiting */
	callsWaiting = malloc(NumFloors * 2 * sizeof(int*));
	for (int i = 0; i < NumFloors * 2; i++)
		callsWaiting[i] = malloc(6 * sizeof(int*));
	for (int i = 0; i < NumFloors; i++)
	{
		callsWaiting[i * 2    ][0] = -1; /* this means this element of the array can be overwritten */
		callsWaiting[i * 2 + 1][0] = -1;
		callsWaiting[i * 2    ][1] = -1; /* holds # of passengers if the floor had to be skipped or -1 */
		callsWaiting[i * 2 + 1][1] = -1;
		elevator[i][0] = -1; /* -1 here means this can be overwritten when a party enters the elevator */
	}
	/* bottom floor can't go down and top floor can't go up so setting to -99 */
	callsWaiting[0][0] = -99;
	callsWaiting[NumFloors * 2 - 1][0] = -99;
	AllThreads = NumFloors;
	for (int i = 0; i < NumFloors; i++)
	{
		locations[i] = i;
    	if (ret = pthread_create(&threads[i], NULL, IssueRequest, (void*)&locations[i]) != 0)
   			printf("thread creation failed\n");
  	}
  	srand(time(0));
  	if (ret = pthread_create(&finish, NULL, Stop, NULL) != 0)
   			printf("thread creation failed\n");
}


/*
Run
simulates the elevator
prints out the drop offs and pickups
*/
void Run(int elevator[][5])
{
	CurTime = 0;	/* holds the current time at any given moment */
	int curFloor = 1; 	/* holds the current floor at any given moment */
	int totalPassengers = 0; /* holds the current # passengers at any given moment */
	int state = -1;	/* 0 if down, 1 if up, -1 if idle */
	int retVal; /* gets output from function calls */
	int nextFloor; /* holds next floor */
	bool stateChange = false; /* when a state is changed there is code we don't want to execute */
	
	while(true)
	{	
		if (state == -1)
		{
			retVal = FindClosestCall(curFloor); /* when idle we go to the closest call */
			if (retVal >= 0)
			{
				if (callsWaiting[retVal][5] == 0)
					CurTime = 0; /*  */
				nextFloor = callsWaiting[retVal][2];
				state = callsWaiting[retVal][4]; /* 0 for down 1 for up */

				if (curFloor - nextFloor > 0)
					printf("Going down %d floors!\n", curFloor - nextFloor);
				else if (curFloor - nextFloor < 0)
					printf("Going up %d floors!\n", (curFloor - nextFloor) * -1);
				sleep(abs(nextFloor - curFloor) * 5);
				CurTime += abs(nextFloor - curFloor) * 5;
				curFloor = nextFloor;
			}
			else {
				sleep(1);
				CurTime += 1;
			}
		}
		if (state == 0 || state == 1)
		{
			retVal = PassengersExiting(elevator, state, curFloor); /* whenever we get to a new floor we check if any passengers are leaving*/
			if (retVal > 0) /* retval = 0 when no passengers exited, retval = # of passengers leaving when there are passengers exiting*/
			{
				totalPassengers -= retVal;
				printf("Time %3d: The elevator dropped off %d passengers at floor %d\n", CurTime, retVal, curFloor);
				NumPassengers += retVal;
				sleep(2);
				CurTime += 2;
			}
			retVal = UpdateNextJobs(state, curFloor); /* check if there are any new jobs we can pickup on our current run */
			if (retVal == -1)
			{
				retVal = StillInProgress(elevator); /* if there aren't new jobs check if we have any jobs still in progress */
				if (retVal == -1)
				{
					stateChange = true; /* if we do not we have to change states */
					retVal = ChangeState(state);
					if (retVal > 0)
					{
						callsWaiting[retVal][0] = 1;
						state = callsWaiting[retVal][4];
						nextFloor = callsWaiting[retVal][2];

						if (curFloor - nextFloor > 0)
							printf("Going down %d floors!\n", curFloor - nextFloor);
						else if (curFloor - nextFloor < 0)
							printf("Going up %d floors!\n", (curFloor - nextFloor) * -1);
						sleep(abs(nextFloor - curFloor) * 5);
						CurTime += abs(nextFloor - curFloor) * 5;
						curFloor = nextFloor;
					}
					else 
					{
						state = -1;
					}
				}
			}
			if (!stateChange)
			{
				retVal = PickingUp(elevator, curFloor, totalPassengers, state); /* if no state change see if we are picking up */
				if (retVal > 0)
				{
					totalPassengers += retVal;
					printf("Time %3d: The elevator picked up %d passengers at floor %d\n", CurTime, retVal, curFloor);
					sleep(2);
					CurTime += 2;
				}
				if (state == 0) /* increment or decrement floors */
				{
					curFloor--;
					printf("Going down 1 floor\n");
				}
				else
				{
					curFloor++;
					printf("Going up 1 floor\n");
				}
				sleep(5);
				CurTime += 5;
			}
			stateChange = false;
		}
	}
}








/*
PutOnElevator
takes information about the current call and populates an open element of the elevator array
returns the # of passengers
*/
int PutOnElevator(int curCall[6], int elevator[][5], int passengersRand)
{
	for (int i = 0; i < NumFloors; i++)
	{
		if (elevator[i][0] == -1)
		{
			elevator[i][0] = 1;
			elevator[i][1] = curCall[3];
			elevator[i][2] = passengersRand;
			elevator[i][3] = CurTime - curCall[5]; /* wait time */
			elevator[i][4] = curCall[5]; /* start time to use for calculating turnaround time */
			return passengersRand;
		}
	}
}

/*
PickingUp
checks if we are picking up any passengers
returns # of new passengers
*/

int PickingUp(int elevator[][5], int curFloor, int totalPassengers, int state)
{
	int passengersEntering = 0;
	int passengersRand = rand() % 5 + 1;
	if (state == 0) /* down */
	{
		if (callsWaiting[(curFloor - 1) * 2][0] == 1) /* if a call is in progress */
		{
			if (callsWaiting[(curFloor - 1) * 2][1] != -1) /* if we had to skip them last time the # passengers overwrites the -1 */ 
			{
				passengersRand = callsWaiting[(curFloor - 1) * 2][1];
			}
			if (totalPassengers + passengersRand <= MAXPASSENGERS) /* if there is enough room on the elevator */
			{
				passengersEntering = PutOnElevator(callsWaiting[(curFloor - 1) * 2], elevator, passengersRand); /* put it on the elevator */
				callsWaiting[(curFloor - 1) * 2][0] = -1; /* call can now be overwritten */
				callsWaiting[(curFloor - 1) * 2][1] = -1;
			}
			else 
			{
				callsWaiting[(curFloor - 1) * 2][0] = 0;
				callsWaiting[(curFloor - 1) * 2][1] = passengersRand; /* reset the progress to waiting */
			}
		}
	}
	else /* up */
	{
		if (callsWaiting[((curFloor - 1) * 2) + 1][0] == 1) /* if a call is in progress */
		{
			if (callsWaiting[((curFloor - 1) * 2) + 1][1] != -1) /* if we had to skip them last time the # passengers overwrites the -1 */ 
			{
				passengersRand = callsWaiting[((curFloor - 1) * 2) + 1][1];
			}
			if (totalPassengers + passengersRand <= MAXPASSENGERS) /* if there is enough room on the elevator */
			{
				passengersEntering = PutOnElevator(callsWaiting[((curFloor - 1) * 2) + 1], elevator, passengersRand); /* put it on the elevator */
				callsWaiting[((curFloor - 1) * 2) + 1][0] = -1;
				callsWaiting[((curFloor - 1) * 2) + 1][1] = -1; /* call can now be overwritten */
			}
			else 
			{
				callsWaiting[((curFloor - 1) * 2) + 1][0] = 0;
				callsWaiting[((curFloor - 1) * 2) + 1][1] = passengersRand; /* reset the progress to waiting */
			}
		}
	}
	return passengersEntering;
}

/*
ChangeState
swaps states and looks for the best floor
if there is no best floor swaps states again
if both of these fail returns -1
if not returns the location in the array for the floor we will be going to
*/
int ChangeState(int state)
{
	int tempState = (state + 1) % 2; /* swaps whatever the state is */
	int retVal = ChangeStateHelper(tempState);
	if (retVal > 0)
	{
		return retVal;
	}
	retVal = ChangeStateHelper(state); /* see if any calls are available from the current state that we missed */
	if (retVal > 0)
	{
		return retVal;
	}
	return -1;
}

/*
ChangeStateHelper
takes the state given by ChangeState as input
finds best floor to start for the new state
returns that floor
if no floors ofund reutnrs -1
*/
int ChangeStateHelper(int state)
{
	int bestFloor = -1;
	if (state == 0) /* down */
	{
		for (int i = (NumFloors - 1) * 2; i >= 0; i -= 2)
		{
			if (callsWaiting[i][0] == 0)
			{
				bestFloor = i;
				break;
			}
		}
	}
	else /* up */ 
	{
		for (int i = 1; i < NumFloors * 2; i += 2)
		{
			if (callsWaiting[i][0] == 0)
			{
				bestFloor = i;
				break;
			}
		}
	}
	return bestFloor;
}

/* 
StillInProgress
checks if there are any jobs that are still in progress 
checks both the elevator and calls waiting
if there are returns 1 if not -1
*/
int StillInProgress(int elevator[][5])
{
	for (int i = 0; i < NumFloors * 2; i++)
	{
		if (callsWaiting[i][0] == 1)
			return 1;
	}
	for (int i = 0; i < NumFloors; i++)
	{
		if (elevator[i][0] == 1)
			return 1;
	}
	return -1;
}

/* 
UpdateNextJobs
check if their are jobs with the same state that are in the direction we are going
if there are set them to in progress and return 1, if not return -1
*/
int UpdateNextJobs(int state, int curFloor)
{
	int retVal = -1;
	if (state == 0) /* down */
	{
		for (int i = (curFloor - 1) * 2; i >= 0; i-= 2) /* only good down jobs will show up with this loop */
		{
			if (callsWaiting[i][0] == 0) /* if the good jobs are waiting */
			{
				retVal = 1;
				callsWaiting[i][0] = 1; /* set them in progress */
			}
		}
	}
	else /* up */
	{
		for (int i = ((curFloor - 1) * 2) + 1; i < NumFloors * 2; i += 2) /* only good up jobs will show up with this loop */
		{
			if (callsWaiting[i][0] == 0) /* if the good jobs are waiting */
			{
				retVal = 1;
				callsWaiting[i][0] = 1; /* set them in progress */
			}
		}
	}
	return retVal;
}


/* 
PassengersExiting
checks if there are groups getting off at the current floor and returns how many are getting off
then sets their in progress field to -1 so we know that call is no longer in use
*/
int PassengersExiting(int elevator[][5], int state, int curFloor)
{
	int passengersExiting = 0;
	for (int i = 0; i < NumFloors; i++)
	{
		if (elevator[i][0] == 1 && elevator[i][1] == curFloor) /* if in the elevator and are getting off at the current floor */
		{
			passengersExiting += elevator[i][2];
			elevator[i][0] = -1; /* this slot can now be overwritten */
			AverageWaitTime[0] += elevator[i][3];
			AverageWaitTime[1] ++;
			AverageTurnaround[0] += CurTime - elevator[i][4];
			AverageTurnaround[1]++;
		}
	}
	return passengersExiting;
}

/*
FindClosestCall
returns index of the closest call to the current floor or -1
*/
int FindClosestCall(int curFloor)
{
	int bestIndex = -1;
	int bestFloor = NumFloors * 2;
	for (int i = 0; i < NumFloors * 2; i++)
	{
		if (callsWaiting[i][0] == 0)
		{
			if (abs(callsWaiting[i][2] - curFloor) < bestFloor)
			{
				bestFloor = callsWaiting[i][2];
				bestIndex = i;
			}
		}
	}
	return bestIndex;
}

/*
IssueRequest
Input:
	threadID: an int corresponding to the floor each thread will work on 
each thread will try and generate a call for their corresponding threadID floor
if chanceCall is > than CALLREQCHANCE, it tries to generate a duplicate request,
or the location can't hold certain calls (ie floor 1 going down) no call will be made for that floor
Good calls will be written into the callsWaiting array
*/
void *IssueRequest(void* arg)
{
	int threadID = *(int *) arg;
	static int time = 0;
	double chanceCall;
	int endFloor;
	while(true)
	{
		chanceCall = (double)rand() / (double)RAND_MAX; /* random variable from 0 to 1 */
		endFloor = rand() % NumFloors + 1; /* random variable from 1 to the # of floors */
		if (chanceCall <= CALLREQCHANCE)
		{
			if (threadID + 1 > endFloor) /* going down */
			{
				if (callsWaiting[threadID * 2][0] == -1)
				{
					printf("Time %3d: Call Recieved at F%d with destination to F%d\n", time, threadID + 1, endFloor);
					WriteToCallsWaiting(threadID * 2, threadID + 1, endFloor, 0, time);
				}
			}
			else if (threadID + 1 < endFloor)
			{
				if (callsWaiting[threadID * 2 + 1][0] == -1)
				{
					printf("Time %3d: Call Recieved at F%d with destination to F%d\n", time, threadID + 1, endFloor);
					WriteToCallsWaiting(threadID * 2 + 1, threadID + 1, endFloor, 1, time);
				}
			}
		}
		AllThreads--;
		if (AllThreads == 0)
		{
			time += TimeInterval;
			AllThreads = NumFloors;
		}
		sleep(5);
	}
	
}

/*
WriteToCallsWaiting
takes data for call requests generated by threads
writes it to callsWaiting array using a mutex lock for synchronization 
*/
void WriteToCallsWaiting(int index, int startFloor, int endFloor, int state, int time)
{
	sem_wait(&mutex);
	callsWaiting[index][0] = 0;
	callsWaiting[index][2] = startFloor;
	callsWaiting[index][3] = endFloor;
	callsWaiting[index][4] = state;
	callsWaiting[index][5] = time;
	sem_post(&mutex);
}
/*
Stop
sleeps for the time of the simulation, 
then prints out counters and kills the program
*/
void *Stop(void* arg)
{
	sleep(TotalTimeSim);
	printf("\nNumber of passengers serviced: %d\n", NumPassengers);
	if (AverageWaitTime[1] != 0)
		printf("Average wait time for all passengers serviced: %d\n", AverageWaitTime[0] / AverageWaitTime[1]);
	else
		printf("Average wait time for all passengers serviced: 0\n");
	if (AverageTurnaround[1] != 0)
		printf("Average turnaround time for all passengers serviced: %d\n", AverageTurnaround[0] / AverageTurnaround[1]);
	else
		printf("Average turnaround time for all passengers serviced: 0\n");
	exit(1);
}