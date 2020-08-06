#include <math.h>
/******************************************************************************
* Laboratory Exercises COMP 3500                                              *
* Author: Saad Biaz                                                           *
* Updated 6/5/2017 to distribute to students to redo Lab 1                    *
* Updated 5/9/2017 for COMP 3500 labs                                         *
* Date  : February 20, 2009                                                   *
******************************************************************************/


/******************************************************************************
*                             Global system headers                           *
******************************************************************************/
#include "common2.h"


/******************************************************************************
*                             Global data types                               *
******************************************************************************/
typedef enum {TAT,RT,CBT,THGT,WT,WTJQ} Metric;
typedef enum {FREEHOLES, PARKING} MemoryQueue;
typedef enum {INFINITE,OMAP,PAGING,BESTFIT,WORSTFIT} MemoryPolicy;


/******************************************************************************
*                             Global definitions                              *
******************************************************************************/
#define MAX_QUEUE_SIZE 10 
#define FCFS            1 
#define RR              3 
#define MAXMETRICS      6


/******************************************************************************
*                            Global data structures                           *
******************************************************************************/
typedef struct FreeMemoryHoleTag {
  Memory       AddressFirstElement; // Address of first element 
  Memory       Size;                // Size of the hole
  struct FreeMemoryHoleTag *previous; /* previous element in linked list */
  struct FreeMemoryHoleTag *next;     /* next element in linked list */
} FreeMemoryHole;

typedef struct MemoryQueueParmsTag {
  FreeMemoryHole *Head;
  FreeMemoryHole *Tail;
  Quantity       NumberOfHoles; // Number of Holes in the queue
} MemoryQueueParms;


/******************************************************************************
*                                  Global data                                *
******************************************************************************/
Quantity NumberofJobs[MAXMETRICS]; // Number of Jobs for which metric was collected
Average  SumMetrics[MAXMETRICS]; // Sum for each Metrics
MemoryQueueParms MemoryQueues[2]; // Free Holes and Parking
MemoryPolicy memoryPolicy = PAGING;
int pageSize;
int pagesAvailable;


/******************************************************************************
*                               Function prototypes                           *
******************************************************************************/
void                 ManageProcesses(void);
void                 NewJobIn(ProcessControlBlock whichProcess);
void                 BookKeeping(void);
Flag                 ManagementInitialization(void);
void                 LongtermScheduler(void);
void                 IO();
void                 CPUScheduler(Identifier whichPolicy);
ProcessControlBlock  *SRTF();
void                 Dispatcher();
void                 EnqueueMemoryHole(MemoryQueue whichQueue, FreeMemoryHole *whichProcess);
FreeMemoryHole       *DequeueMemoryHole(MemoryQueue whichQueue);
Memory               getStartAddress(ProcessControlBlock *whichProcess);
void                 compactMemory();


/******************************************************************************
* function: main()                                                            *
* usage:    Create an artificial environment operating systems. The parent    *
*           process is the "Operating Systems" managing the processes using   *
*           the resources (CPU and Memory) of the system                      *
*******************************************************************************
* Inputs: ANSI flat C command line parameters                                 *
* Output: None                                                                *
*                                                                             *
* INITIALIZE PROGRAM ENVIRONMENT                                              *
* START CONTROL ROUTINE                                                       *
******************************************************************************/
int main (int argc, char **argv) {
   if (Initialization(argc,argv)){
     ManageProcesses();
   }
} /* end of main function */


/************************************************************************
* Input : none                                                          *
* Output: None                                                          *
* Function: Monitor Sources and process events (written by students)    *
************************************************************************/
void ManageProcesses(void){
  ManagementInitialization();
  while (1) {
    IO();
    CPUScheduler(PolicyNumber);
    Dispatcher();
  }
}


/************************************************************************
* Input : none                                                          *          
* Output: None                                                          *        
* Function:                                                             *
*    1) if CPU Burst done, then move process on CPU to Waiting Queue    *
*         otherwise (RR) return to rReady Queue                         *                           
*    2) scan Waiting Queue to find processes with complete I/O          *
*           and move them to Ready Queue                                *         
************************************************************************/
void IO() {
  ProcessControlBlock *currentProcess = DequeueProcess(RUNNINGQUEUE); 
  if (currentProcess){
    if (currentProcess->RemainingCpuBurstTime <= 0) { // Finished current CPU Burst
      currentProcess->TimeEnterWaiting = Now(); // Record when entered the waiting queue
      EnqueueProcess(WAITINGQUEUE, currentProcess); // Move to Waiting Queue
      currentProcess->TimeIOBurstDone = Now() + currentProcess->IOBurstTime; // Record when IO completes
      currentProcess->state = WAITING;
    } else { // Must return to Ready Queue                
      currentProcess->JobStartTime = Now();                                               
      EnqueueProcess(READYQUEUE, currentProcess); // Mobe back to Ready Queue
      currentProcess->state = READY; // Update PCB state 
    }
  }

  /* Scan Waiting Queue to find processes that got IOs  complete*/
  ProcessControlBlock *ProcessToMove;
  /* Scan Waiting List to find processes that got complete IOs */
  ProcessToMove = DequeueProcess(WAITINGQUEUE);
  if (ProcessToMove){
    Identifier IDFirstProcess =ProcessToMove->ProcessID;
    EnqueueProcess(WAITINGQUEUE,ProcessToMove);
    ProcessToMove = DequeueProcess(WAITINGQUEUE);
    while (ProcessToMove){
      if (Now()>=ProcessToMove->TimeIOBurstDone){
	ProcessToMove->RemainingCpuBurstTime = ProcessToMove->CpuBurstTime;
	ProcessToMove->JobStartTime = Now();
	EnqueueProcess(READYQUEUE,ProcessToMove);
      } else {
	EnqueueProcess(WAITINGQUEUE,ProcessToMove);
      }
      if (ProcessToMove->ProcessID == IDFirstProcess){
	break;
      }
      ProcessToMove =DequeueProcess(WAITINGQUEUE);
    } // while (ProcessToMove)
  } // if (ProcessToMove)
}


/***********************************************************************
* Input : whichPolicy (1:FCFS, 2: SRTF, and 3:RR)                      *       
* Output: None                                                         *
* Function: Selects Process from Ready Queue and Puts it on Running Q. *
***********************************************************************/
void CPUScheduler(Identifier whichPolicy) {
  ProcessControlBlock *selectedProcess;
  if ((whichPolicy == FCFS) || (whichPolicy == RR)) {
    selectedProcess = DequeueProcess(READYQUEUE);
  } else{ // Shortest Remaining Time First 
    selectedProcess = SRTF();
  }
  if (selectedProcess) {
    selectedProcess->state = RUNNING; // Process state becomes Running                                     
    EnqueueProcess(RUNNINGQUEUE, selectedProcess); // Put process in Running Queue                         
  }
}


/************************************************************************                        
* Input : None                                                          *                                     
* Output: Pointer to the process with shortest remaining time (SRTF)    *                                     
* Function: Returns process control block with SRTF                     *                                     
************************************************************************/
ProcessControlBlock *SRTF() {
  /* Select Process with Shortest Remaining Time*/
  ProcessControlBlock *selectedProcess, *currentProcess = DequeueProcess(READYQUEUE);
  selectedProcess = (ProcessControlBlock *) NULL;
  if (currentProcess){
    TimePeriod shortestRemainingTime = currentProcess->TotalJobDuration - currentProcess->TimeInCpu;
    Identifier IDFirstProcess =currentProcess->ProcessID;
    EnqueueProcess(READYQUEUE,currentProcess);
    currentProcess = DequeueProcess(READYQUEUE);
    while (currentProcess){
      if (shortestRemainingTime >= (currentProcess->TotalJobDuration - currentProcess->TimeInCpu)){
	EnqueueProcess(READYQUEUE,selectedProcess);
	selectedProcess = currentProcess;
	shortestRemainingTime = currentProcess->TotalJobDuration - currentProcess->TimeInCpu;
      } else {
	EnqueueProcess(READYQUEUE,currentProcess);
      }
      if (currentProcess->ProcessID == IDFirstProcess){
	break;
      }
      currentProcess =DequeueProcess(READYQUEUE);
    } // while (ProcessToMove)
  } // if (currentProcess)
  return(selectedProcess);
}


/***********************************************************************\  
 * Input : None                                                         *   
 * Output: None                                                         *   
 * Function:                                                            *
 *  1)If process in Running Queue needs computation, put it on CPU      *
 *              else move process from running queue to Exit Queue      *     
\***********************************************************************/
void Dispatcher() {
  double start;
  ProcessControlBlock *processOnCPU = Queues[RUNNINGQUEUE].Tail; // Pick Process on CPU
  if (!processOnCPU) { // No Process in Running Queue, i.e., on CPU
    return;
  }
  if(processOnCPU->TimeInCpu == 0.0) { // First time this process gets the CPU
    SumMetrics[RT] += Now()- processOnCPU->JobArrivalTime;
    NumberofJobs[RT]++;
    processOnCPU->StartCpuTime = Now(); // Set StartCpuTime
  }
  
  if (processOnCPU->TimeInCpu >= processOnCPU-> TotalJobDuration) { // Process Complete
    if (memoryPolicy == OMAP) {
      AvailableMemory += processOnCPU->MemoryAllocated;
      printf(" >>>>>Deallocated %u bytes from process # %d, %u bytes available\n", 
        processOnCPU->MemoryAllocated, processOnCPU->ProcessID, AvailableMemory);
      processOnCPU->MemoryAllocated = 0;
    }
    else if (memoryPolicy == PAGING) {
      int pages = processOnCPU->MemoryAllocated / pageSize;
      printf(" >> Deallocated %d pages from process # %d, %d pages available\n", 
        pages, processOnCPU->ProcessID, pagesAvailable);
      pagesAvailable += pages;
    }
    else if (memoryPolicy == BESTFIT || memoryPolicy == WORSTFIT) {
        FreeMemoryHole *memoryHole;
        memoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));
        if (memoryHole){ // malloc successful
          memoryHole->AddressFirstElement = processOnCPU->TopOfMemory;
          memoryHole->Size = processOnCPU->MemoryAllocated;
          printf("freed hole of size %d\n", memoryHole->Size);
          EnqueueMemoryHole(FREEHOLES, memoryHole);
        }
    }

    printf(" >>>>>Process # %d complete, %d Processes Completed So Far <<<<<<\n",
	   processOnCPU->ProcessID,NumberofJobs[THGT]);   
    processOnCPU=DequeueProcess(RUNNINGQUEUE);
    EnqueueProcess(EXITQUEUE,processOnCPU);

    NumberofJobs[THGT]++;
    NumberofJobs[TAT]++;
    NumberofJobs[WT]++;
    NumberofJobs[CBT]++;
    SumMetrics[TAT] += Now() - processOnCPU->JobArrivalTime;
    SumMetrics[WT] += processOnCPU->TimeInReadyQueue;

    // processOnCPU = DequeueProcess(EXITQUEUE);
    // XXX free(processOnCPU);

  } else { // Process still needs computing, out it on CPU
    TimePeriod CpuBurstTime = processOnCPU->CpuBurstTime;
    processOnCPU->TimeInReadyQueue += Now() - processOnCPU->JobStartTime;
    if (PolicyNumber == RR){
      CpuBurstTime = Quantum;
      if (processOnCPU->RemainingCpuBurstTime < Quantum)
	      CpuBurstTime = processOnCPU->RemainingCpuBurstTime;
    }
    processOnCPU->RemainingCpuBurstTime -= CpuBurstTime;
    // SB_ 6/4 End Fixes RR 
    TimePeriod StartExecution = Now();
    OnCPU(processOnCPU, CpuBurstTime); // SB_ 6/4 use CpuBurstTime instead of PCB-> CpuBurstTime
    processOnCPU->TimeInCpu += CpuBurstTime; // SB_ 6/4 use CpuBurstTime instead of PCB-> CpuBurstTimeu
    SumMetrics[CBT] += CpuBurstTime;
  }
}

/***********************************************************************\
* Input : None                                                          *
* Output: None                                                          *
* Function: This routine is run when a job is added to the Job Queue    *
\***********************************************************************/
void NewJobIn(ProcessControlBlock whichProcess){
  ProcessControlBlock *NewProcess;
  /* Add Job to the Job Queue */
  NewProcess = (ProcessControlBlock *) malloc(sizeof(ProcessControlBlock));
  memcpy(NewProcess,&whichProcess,sizeof(whichProcess));
  NewProcess->TimeInCpu = 0; // Fixes TUX error
  NewProcess->RemainingCpuBurstTime = NewProcess->CpuBurstTime; // SB_ 6/4 Fixes RR
  EnqueueProcess(JOBQUEUE,NewProcess);
  DisplayQueue("Job Queue in NewJobIn",JOBQUEUE);
  LongtermScheduler(); /* Job Admission  */
}


/***********************************************************************\                                                   
* Input : None                                                         *                                                    
* Output: None                                                         *                                                    
* Function:                                                            *
* 1) BookKeeping is called automatically when 250 arrived              *
* 2) Computes and display metrics: average turnaround  time, throughput*
*     average response time, average waiting time in ready queue,      *
*     and CPU Utilization                                              *                                                     
\***********************************************************************/
void BookKeeping(void){
  double end = Now(); // Total time for all processes to arrive
  Metric m;

  // Compute averages and final results
  if (NumberofJobs[TAT] > 0){
    SumMetrics[TAT] = SumMetrics[TAT]/ (Average) NumberofJobs[TAT];
  }
  if (NumberofJobs[RT] > 0){
    SumMetrics[RT] = SumMetrics[RT]/ (Average) NumberofJobs[RT];
  }
  SumMetrics[CBT] = SumMetrics[CBT]/ Now();

  if (NumberofJobs[WT] > 0){
    SumMetrics[WT] = SumMetrics[WT]/ (Average) NumberofJobs[WT];
  }

  if (NumberofJobs[WTJQ] > 0) {
    SumMetrics[WTJQ] = SumMetrics[WTJQ] / (Average) NumberofJobs[WTJQ];
  }

  printf("\n********* Processes Managemenent Numbers ******************************\n");
  printf("Policy Number = %d, Quantum = %.6f   Show = %d\n", PolicyNumber, Quantum, Show);
  printf("Number of Completed Processes = %d\n", NumberofJobs[THGT]);
  printf("ATAT=%f   ART=%f  CBT = %f  T=%f AWT=%f\n AWTJQ=%f\n", 
	 SumMetrics[TAT], SumMetrics[RT], SumMetrics[CBT], 
	 NumberofJobs[THGT]/Now(), SumMetrics[WT], SumMetrics[WTJQ]);

  exit(0);
}

/***********************************************************************\
* Input : None                                                          *
* Output: None                                                          *
* Function: Decides which processes should be admitted in Ready Queue   *
*           If enough memory and within multiprogramming limit,         *
*           then move Process from Job Queue to Ready Queue             *
\***********************************************************************/
void LongtermScheduler(void){
  ProcessControlBlock *currentProcess = DequeueProcess(JOBQUEUE);
  while (currentProcess) {
    if (getStartAddress(currentProcess) != -1) {
      currentProcess->TimeInJobQueue = Now() - currentProcess->JobArrivalTime; // Set TimeInJobQueue
      currentProcess->JobStartTime = Now(); // Set JobStartTime
      SumMetrics[WTJQ] += currentProcess->TimeInJobQueue; // Record time in job queue
      NumberofJobs[WTJQ]++;
      EnqueueProcess(READYQUEUE,currentProcess); // Place process in Ready Queue
      currentProcess->state = READY; // Update process state
      currentProcess = DequeueProcess(JOBQUEUE);
    }
    else {
      EnqueueProcess(JOBQUEUE, currentProcess);
      break;
    }
  }
}


/***********************************************************************\
* Input : None                                                          *
* Output: TRUE if Intialization successful                              *
\***********************************************************************/
Flag ManagementInitialization(void){
  Metric m;
  for (m = TAT; m < MAXMETRICS; m++){
     NumberofJobs[m] = 0;
     SumMetrics[m]   = 0.0;
  }

  pageSize = 8192;
  pagesAvailable = AvailableMemory / pageSize;
  printf("%uavmem, %upagesize, %upages\n", AvailableMemory, pageSize, pagesAvailable);

  FreeMemoryHole *NewMemoryHole;
  int i;
  //Initialize the queues
  for (i = 0; i < 2; i++){
    MemoryQueues[i].Tail = (FreeMemoryHole *) NULL;
    MemoryQueues[i].Head = (FreeMemoryHole *) NULL;
    MemoryQueues[i].NumberOfHoles = 0;
  }
 
  // Create initial big memory holes containing the full memory
  NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));
  if (NewMemoryHole){ // malloc successful
    NewMemoryHole->AddressFirstElement = 0;
    NewMemoryHole->Size = MAXMEMORYSIZE;

    // Move what was in the parking into the Queue of free holes
    EnqueueMemoryHole(FREEHOLES,NewMemoryHole);

    // Testing:
    printf("Number of holes in Parking = %d\n",MemoryQueues[PARKING].NumberOfHoles);
    if (MemoryQueues[PARKING].NumberOfHoles)
      printf("Parking should be empty");

                                                                                  
    printf("Number of holes in Free Holes = %d\n",MemoryQueues[FREEHOLES].NumberOfHoles);
    if (MemoryQueues[FREEHOLES].NumberOfHoles){
      printf("Starting Address %d\n",MemoryQueues[FREEHOLES].Tail->AddressFirstElement);
      printf("Size in hexadecimal 0x%x\n",MemoryQueues[FREEHOLES].Tail->Size);
    }
  }

  return TRUE;
}


/************************************************************************                                            
* Input : Pointer to process being allocated some memory                *                                            
* Output: Returns address of allocated memory block                     *                                            
* Function: Handles allocation of memory for a process                  *                                            
************************************************************************/
Memory getStartAddress(ProcessControlBlock *whichProcess) {
  switch(memoryPolicy) {
    case OMAP: 
    {
      if (AvailableMemory >= whichProcess->MemoryRequested ) {
       AvailableMemory -= whichProcess->MemoryRequested;
       whichProcess->MemoryAllocated = whichProcess->MemoryRequested;
       printf(" >>>>>Allocated %u bytes to %d, %u bytes available\n", 
        whichProcess->MemoryAllocated, whichProcess->ProcessID, AvailableMemory);
       return 1;
      } else { // not enough memory, put process back in job queue 
        printf(" >>>>>Denied %u bytes to %d, %u bytes available\n", 
        whichProcess->MemoryAllocated, whichProcess->ProcessID, AvailableMemory);
        return -1;
      }
      break;
    }

    case PAGING: 
    { 
      int pagesRequested = (int) ceil((double) whichProcess->MemoryRequested / (double) pageSize);
      if (pagesAvailable >= pagesRequested) {
        pagesAvailable -= pagesRequested;
        printf(" >>>>>Allocated %u pages to %d, %u pages available\n", 
        pagesRequested, whichProcess->ProcessID, pagesAvailable);
        whichProcess->MemoryAllocated = pagesRequested * pageSize;
        return 1;
      } 
      else {
        printf(" >>>>>Denied %u pages to %d, %u pages available\n", 
        pagesRequested, whichProcess->ProcessID, pagesAvailable);
        return -1;
      }
      break;
    }

    case BESTFIT: 
    {
      FreeMemoryHole *currentMemoryHole = (FreeMemoryHole *) NULL;
      FreeMemoryHole *selectedMemoryHole = (FreeMemoryHole *) NULL;
      Memory sizeOfSmallestHole = UINT_MAX;
      int i;
      
      for (i = 0; i < MemoryQueues[FREEHOLES].NumberOfHoles; i++) {
        currentMemoryHole = DequeueMemoryHole(FREEHOLES);
        if (currentMemoryHole->Size >= whichProcess->MemoryRequested && currentMemoryHole->Size <= sizeOfSmallestHole) {
          if (selectedMemoryHole) {
              EnqueueMemoryHole(FREEHOLES, selectedMemoryHole);
          }
          selectedMemoryHole = currentMemoryHole;
          sizeOfSmallestHole = currentMemoryHole->Size;
        } else {
          EnqueueMemoryHole(FREEHOLES, currentMemoryHole);
        }
      }
      if(selectedMemoryHole) {
          printf(" >> Allocating hole at %u to process %d\n", selectedMemoryHole->AddressFirstElement, whichProcess->ProcessID);
          FreeMemoryHole *NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));
          NewMemoryHole->AddressFirstElement = selectedMemoryHole->AddressFirstElement + whichProcess->MemoryRequested;
          NewMemoryHole->Size = selectedMemoryHole->Size - whichProcess->MemoryRequested;
          if (NewMemoryHole->Size > 0) {
              EnqueueMemoryHole(FREEHOLES, NewMemoryHole);
          }
          whichProcess->MemoryAllocated = whichProcess->MemoryRequested;
          whichProcess->TopOfMemory = selectedMemoryHole->AddressFirstElement;
          return 1;
      } else {
          compactMemory();
          selectedMemoryHole = DequeueMemoryHole(FREEHOLES);
          if(selectedMemoryHole && selectedMemoryHole->Size >= whichProcess->MemoryRequested) {
            printf(" >> Allocating hole at %u to process %d\n", selectedMemoryHole->AddressFirstElement, whichProcess->ProcessID);
            FreeMemoryHole *NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));
            NewMemoryHole->AddressFirstElement = selectedMemoryHole->AddressFirstElement + whichProcess->MemoryRequested;
            NewMemoryHole->Size = selectedMemoryHole->Size - whichProcess->MemoryRequested;
            if (NewMemoryHole->Size > 0) {
                EnqueueMemoryHole(FREEHOLES, NewMemoryHole);
            }
            whichProcess->MemoryAllocated = whichProcess->MemoryRequested;
            whichProcess->TopOfMemory = selectedMemoryHole->AddressFirstElement;
            return 1;
          }  else {
            EnqueueMemoryHole(FREEHOLES, selectedMemoryHole);
          }
      }
      printf(" >>>>>Denied %u memory to %d\n", 
      whichProcess->MemoryRequested, whichProcess->ProcessID);
      return -1;
      break;
    }

    case WORSTFIT: 
    {
      FreeMemoryHole *currentMemoryHole = (FreeMemoryHole *) NULL;
      FreeMemoryHole *selectedMemoryHole = (FreeMemoryHole *) NULL;
      Memory sizeOfBiggestHole = 0;
      int i;
      for (i = 0; i < MemoryQueues[FREEHOLES].NumberOfHoles; i++) {
        currentMemoryHole = DequeueMemoryHole(FREEHOLES);
        if (currentMemoryHole->Size >= whichProcess->MemoryRequested && currentMemoryHole->Size >= sizeOfBiggestHole) {
          if (selectedMemoryHole) {
            EnqueueMemoryHole(FREEHOLES, selectedMemoryHole);
          }
          selectedMemoryHole = currentMemoryHole;
          sizeOfBiggestHole = currentMemoryHole->Size;
        } else {
          EnqueueMemoryHole(FREEHOLES, currentMemoryHole);
        }
      }
      if(selectedMemoryHole) {
          printf(" >> Allocating hole at %u to process %d\n", selectedMemoryHole->AddressFirstElement, whichProcess->ProcessID);
          FreeMemoryHole *NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));
          NewMemoryHole->AddressFirstElement = selectedMemoryHole->AddressFirstElement + whichProcess->MemoryRequested;
          NewMemoryHole->Size = selectedMemoryHole->Size - whichProcess->MemoryRequested;
          if (NewMemoryHole->Size > 0) {
              EnqueueMemoryHole(FREEHOLES, NewMemoryHole);
          }
          whichProcess->MemoryAllocated = whichProcess->MemoryRequested;
          whichProcess->TopOfMemory = selectedMemoryHole->AddressFirstElement;
          return 1;
      } else {
          compactMemory();
          selectedMemoryHole = DequeueMemoryHole(FREEHOLES);
          if(selectedMemoryHole && selectedMemoryHole->Size >= whichProcess->MemoryRequested) {
            printf(" >> Allocating hole at %u to process %d\n", selectedMemoryHole->AddressFirstElement, whichProcess->ProcessID);
            FreeMemoryHole *NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));
            NewMemoryHole->AddressFirstElement = selectedMemoryHole->AddressFirstElement + whichProcess->MemoryRequested;
            NewMemoryHole->Size = selectedMemoryHole->Size - whichProcess->MemoryRequested;
            if (NewMemoryHole->Size > 0) {
                EnqueueMemoryHole(FREEHOLES, NewMemoryHole);
            }
            whichProcess->MemoryAllocated = whichProcess->MemoryRequested;
            whichProcess->TopOfMemory = selectedMemoryHole->AddressFirstElement;
            return 1;
          } else {
            EnqueueMemoryHole(FREEHOLES, selectedMemoryHole);
          }
      }
      printf(" >>>>>Denied %u memory to %d\n", 
      whichProcess->MemoryRequested, whichProcess->ProcessID);
      return -1;
      break;
    break;
    }

    case INFINITE:
    {
      return 1;
      break;
    }

   }
}


/***********************************************************************\                                            
 * Input : Queue where to enqueue and Element to enqueue                 *                                            
 * Output: Updates Head and Tail as needed                               *                                            
 * Function: Enqueues FIFO element in queue and updates tail and head    *                                            
\***********************************************************************/
void EnqueueMemoryHole(MemoryQueue whichQueue, FreeMemoryHole *whichMemoryHole){
  if (whichMemoryHole == (FreeMemoryHole *) NULL) {
    return;
  }

  MemoryQueues[whichQueue].NumberOfHoles++;

  /* Enqueue the process in the queue */
  if (MemoryQueues[whichQueue].Head)
    MemoryQueues[whichQueue].Head->previous = whichMemoryHole;

  whichMemoryHole->next = MemoryQueues[whichQueue].Head;
  whichMemoryHole->previous = NULL;
  MemoryQueues[whichQueue].Head = whichMemoryHole;

  if (MemoryQueues[whichQueue].Tail == NULL)
    MemoryQueues[whichQueue].Tail = whichMemoryHole;
}


/***********************************************************************\                                            
 * Input : Queue where to enqueue and Element to enqueue                *                                            
 * Output: Returns tail of queue                                        *                                            
 * Function: Removes tail elelemnt and updates tail and head as needed  *                                            
\***********************************************************************/
FreeMemoryHole *DequeueMemoryHole(MemoryQueue whichQueue){
  FreeMemoryHole *HoleToRemove;

  HoleToRemove = MemoryQueues[whichQueue].Tail;
  if (HoleToRemove != (FreeMemoryHole *) NULL) {
    MemoryQueues[whichQueue].NumberOfHoles--;

    HoleToRemove->next = (FreeMemoryHole *) NULL;
    MemoryQueues[whichQueue].Tail = MemoryQueues[whichQueue].Tail->previous;

    HoleToRemove->previous =(FreeMemoryHole *) NULL;
    if (MemoryQueues[whichQueue].Tail == (FreeMemoryHole *) NULL){
      MemoryQueues[whichQueue].Head = (FreeMemoryHole *) NULL;
    } else {
      MemoryQueues[whichQueue].Tail->next = (FreeMemoryHole *) NULL;
    }
  }

  return(HoleToRemove);
}

void compactMemory() {
    printf("!! PERFORMING COMPACTION !!\n");
    FreeMemoryHole *newMemoryHole = DequeueMemoryHole(FREEHOLES);
    FreeMemoryHole *currentMemoryHole = (FreeMemoryHole *) NULL;;
    int i = 0;
    for (i = 0; i < MemoryQueues[FREEHOLES].NumberOfHoles; i++) {
        currentMemoryHole = DequeueMemoryHole(FREEHOLES);
        newMemoryHole->Size += currentMemoryHole->Size;
    }
    if (newMemoryHole) {
        newMemoryHole->AddressFirstElement = 0;
        EnqueueMemoryHole(FREEHOLES, newMemoryHole);
        printf("!! COMPACTED %d MEMORY HOLES FOR %d BYTES !!\n", MemoryQueues[FREEHOLES].NumberOfHoles, newMemoryHole->Size);
    } else {
      printf("!! NO HOLES TO COMPACT !!\n");
    }
}