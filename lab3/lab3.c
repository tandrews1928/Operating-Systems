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

//http://faculty.salina.k-state.edu/tim/CMST302/study_guide/topic2/enumerator.html


typedef enum {FREEHOLES, PARKING} MemoryQueue;


/******************************************************************************
*                             Global definitions                              *
******************************************************************************/
#define MAX_QUEUE_SIZE 10 
#define FCFS            1 
#define RR              3 
#define MAXMETRICS      5

#define OMAP            0
#define PAGING          1
#define BESTFIT         2
#define WORSTFIT        3


/******************************************************************************
*                            Global data structures                           *
******************************************************************************/

//I got all of this from the powerpoint

typedef struct FreeMemoryHoleTag {
  Memory AddressFirstElement; // Address of first element 
  Memory Size;                // Size of the hole
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
Quantity NumberofJobs[MAXMETRICS];
Average  SumMetrics[MAXMETRICS];


int AvailablePages;

int policy = PAGING;

int pageSize = 8192;

MemoryQueueParms queues[2];


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

//I created the following Functions
void                 EnqueueHole(MemoryQueue memQueue, FreeMemoryHole *whichProcess);
FreeMemoryHole       *DequeueHole(MemoryQueue memQueue);
Memory               getStartAddress(ProcessControlBlock *whichProcess);


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

      if (policy == PAGING) {

      int requestedPages =  processOnCPU->MemoryRequested / pageSize;

      AvailablePages += requestedPages;
    }
    else if (policy == OMAP) {

      AvailableMemory += processOnCPU->MemoryAllocated;

      processOnCPU->MemoryAllocated = 0;
    }
    else if (policy == BESTFIT || policy == WORSTFIT) {

        FreeMemoryHole *newMemoryHole;

        newMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));

        if (newMemoryHole){

            if (processOnCPU->MemoryAllocated > 0) {

                newMemoryHole->AddressFirstElement = processOnCPU->TopOfMemory;

                newMemoryHole->Size = processOnCPU->MemoryAllocated;

                EnqueueHole(FREEHOLES, newMemoryHole);
            }
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
    if (-1 != getStartAddress(currentProcess)) {
      currentProcess->TimeInJobQueue = Now() - currentProcess->JobArrivalTime; // Set TimeInJobQueue
      currentProcess->JobStartTime = Now(); // Set JobStartTime
      SumMetrics[WTJQ] += currentProcess->TimeInJobQueue;
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

  AvailablePages = AvailableMemory / pageSize;

  FreeMemoryHole *NewMemoryHole;
  int j;

  for (j = 0; j < 2; j++){

    queues[j].Tail = (FreeMemoryHole *) NULL;

    queues[j].Head = (FreeMemoryHole *) NULL;

    queues[j].NumberOfHoles = 0;
  }
 
  NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));

  if (NewMemoryHole){ 

    NewMemoryHole->AddressFirstElement = 0;

    NewMemoryHole->Size = MAXMEMORYSIZE;

    EnqueueHole(FREEHOLES,NewMemoryHole);

  }

  return TRUE;
}


/************************************************************************                                            
* Input : Pointer to process being allocated some memory                *                                            
* Output: Returns address of allocated memory block                     *                                            
* Function: Handles allocation of memory for a process                  *                                            
************************************************************************/
Memory getStartAddress(ProcessControlBlock *whichProcess) {
    if (policy == OMAP) {

      if (AvailableMemory >= whichProcess->MemoryRequested ) {       

       whichProcess->MemoryAllocated = whichProcess->MemoryRequested;
          
       AvailableMemory -= whichProcess->MemoryRequested;

    

       return 1;

      } else {

        return -1;
      }

    }

    else if (policy == BESTFIT) {

      Memory smallest = UINT_MAX;

      FreeMemoryHole *selectedHole = (FreeMemoryHole *) NULL;

      FreeMemoryHole *currentHole = (FreeMemoryHole *) NULL;


      int i;
      
      for (i = 0; i < queues[FREEHOLES].NumberOfHoles; i++) {

        currentHole = DequeueHole(FREEHOLES);

        if (currentHole->Size >= whichProcess->MemoryRequested && currentHole->Size <= smallest) {

          if (selectedHole) {

              EnqueueHole(FREEHOLES, selectedHole);
          }

          smallest = currentHole->Size;

          selectedHole = currentHole;


        } else {

          EnqueueHole(FREEHOLES, currentHole);

        }
      }

      if(selectedHole) {

          FreeMemoryHole *NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));

          NewMemoryHole->AddressFirstElement = selectedHole->AddressFirstElement + whichProcess->MemoryRequested;

          NewMemoryHole->Size = selectedHole->Size - whichProcess->MemoryRequested;

          if (NewMemoryHole->Size > 0) {

              EnqueueHole(FREEHOLES, NewMemoryHole);

          }

          whichProcess->TopOfMemory = selectedHole->AddressFirstElement;

          whichProcess->MemoryAllocated = whichProcess->MemoryRequested;

          return 1;

      } else {
        
          selectedHole = DequeueHole(FREEHOLES);

          if(selectedHole && selectedHole->Size >= whichProcess->MemoryRequested) {

              FreeMemoryHole *NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));

              NewMemoryHole->Size = selectedHole->Size - whichProcess->MemoryRequested;

              NewMemoryHole->AddressFirstElement = selectedHole->AddressFirstElement + whichProcess->MemoryRequested;

              if (NewMemoryHole->Size > 0) {

                  EnqueueHole(FREEHOLES, NewMemoryHole);
              }
          }

          whichProcess->TopOfMemory = selectedHole->AddressFirstElement;

          whichProcess->MemoryAllocated = whichProcess->MemoryRequested;

          return 1;
      }

      whichProcess->MemoryAllocated = 0;
      return -1;
    }

   else if (policy == WORSTFIT) {

      Memory sizeOfBiggestHole = 0;

      FreeMemoryHole *currentHole = (FreeMemoryHole *) NULL;

      FreeMemoryHole *selectedHole = (FreeMemoryHole *) NULL;

      int i;

      for (i = 0; i < queues[FREEHOLES].NumberOfHoles; i++) {

        currentHole = DequeueHole(FREEHOLES);

        if (currentHole->Size >= whichProcess->MemoryRequested && currentHole->Size >= sizeOfBiggestHole) {

          if (selectedHole) {

            EnqueueHole(FREEHOLES, selectedHole);
            
          }

          sizeOfBiggestHole = currentHole->Size;

          selectedHole = currentHole;

        } else {

          EnqueueHole(FREEHOLES, currentHole);

        }
      }
      if(selectedHole) {

          FreeMemoryHole *NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));

          NewMemoryHole->Size = selectedHole->Size - whichProcess->MemoryRequested;

          NewMemoryHole->AddressFirstElement = selectedHole->AddressFirstElement + whichProcess->MemoryRequested;

          if (NewMemoryHole->Size > 0) {

              EnqueueHole(FREEHOLES, NewMemoryHole);

          }

          whichProcess->TopOfMemory = selectedHole->AddressFirstElement;

          whichProcess->MemoryAllocated = whichProcess->MemoryRequested;

          return 1;

      } else {

          selectedHole = DequeueHole(FREEHOLES);

          if(selectedHole && selectedHole->Size >= whichProcess->MemoryRequested) {

              FreeMemoryHole *NewMemoryHole = (FreeMemoryHole *) malloc(sizeof(FreeMemoryHole));

              NewMemoryHole->Size = selectedHole->Size - whichProcess->MemoryRequested;

              NewMemoryHole->AddressFirstElement = selectedHole->AddressFirstElement + whichProcess->MemoryRequested;

              if (NewMemoryHole->Size > 0) {

                  EnqueueHole(FREEHOLES, NewMemoryHole);
              }
          }

          whichProcess->TopOfMemory = selectedHole->AddressFirstElement;

          whichProcess->MemoryAllocated = whichProcess->MemoryRequested;

          return 1;
      }
      whichProcess->MemoryAllocated = 0;

      return -1;
    
  

   }

   else if (policy == PAGING) { 

      int requestedPages = ceil(whichProcess->MemoryRequested / pageSize);

      if (AvailablePages >= requestedPages) {

         AvailablePages -= requestedPages;

         whichProcess->MemoryAllocated = requestedPages * pageSize;

         return 1;
      } 
      else {

         return -1;
      }

   }

   else {
      return 1;
   }

}    

                                         

void EnqueueHole(MemoryQueue memQueue, FreeMemoryHole *MemoryHole){

  if ((FreeMemoryHole *) NULL == MemoryHole) {

    return;
  }

  queues[memQueue].NumberOfHoles++;

  if (queues[memQueue].Head)

    queues[memQueue].Head->previous = MemoryHole;

  MemoryHole->next = queues[memQueue].Head;

  MemoryHole->previous = NULL;

  queues[memQueue].Head = MemoryHole;

  if (NULL == queues[memQueue].Tail)

    queues[memQueue].Tail = MemoryHole;
}


FreeMemoryHole *DequeueHole(MemoryQueue memQueue){

  FreeMemoryHole *HoleToRemove;

  HoleToRemove = queues[memQueue].Tail;

  if ((FreeMemoryHole *) NULL != HoleToRemove) {

    queues[memQueue].NumberOfHoles--;

    HoleToRemove->next = (FreeMemoryHole *) NULL;

    queues[memQueue].Tail = queues[memQueue].Tail->previous;


    HoleToRemove->previous =(FreeMemoryHole *) NULL;

    if ((FreeMemoryHole *) NULL == queues[memQueue].Tail) {

      queues[memQueue].Head = (FreeMemoryHole *) NULL;

    } else {
      queues[memQueue].Tail->next = (FreeMemoryHole *) NULL;
    }
  }

  return(HoleToRemove);
}
