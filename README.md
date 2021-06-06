# Elevator-Sim-Pthreads
Elevator Scheduling Algorithm Simulator in C for practice with threading<br>
Threads represent floors of the elevator and are used to create call requests

## How to compile and run:
Compile: ```gcc ElevatorSim.c -pthread```<br>
Args: [# of floors] [time interval when call requests come in] [Length (seconds) of simulation]<br>
Example: ```./a.out 8 5 180``` to run the simulation with 8 floors, 5 seconds between call requests generated, and for 3 minutes

## Assumptions
- The elevator takes 5 seconds to go from one floor to the next
- The elevator takes 2 seconds after picking up and dropping off passengers
- Every x seconds, there is a 15% chance that each floor can generate a call request, where x is the time interval inputted by the user
- The max # of passengers an elevator can hold is 10
