/*
  Troy Frazier
  CS 4348.0U1
  Homework 4
  7/20/2018
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
           GLOBAL CONSTANTS
  TLB_ENTRIES = Total entries in the TLB cache
  TABLE_ENTRIES = Total page entries
  TOTAL_FRAMES = Total frames in memory
  FRAME_SIZE = Total bytes in a frame
  PAGE_SIZE = total bytes in a page
  BIT_SIZE = bit mask of 16
  OFFSET_SIZE = bit mask of 8

          GLOBAL VARAIBLES
  tablePages[] = [0] holds page number
  tableFrames[] = parrallel to tablePages and points to index in memory[][]
  tlbPage[] = Holds page numb in the TLB cache
  tlbFrame[] = parrallel to tlbPage and points to index in memory
  memory[][] = holds data in physical memory

  curPages = total pages in the page table. Starts at 0 to implement pure demand paging
  curTLBSIZE = total pages in the tlb table. Starts at 0
  pageFaults = total times a page is not in the page table or tlb and a disk lookup is enacted
  tlbHits = total times a page number was found in the TLB cache
  oldestFrame = the oldest frame current residing in physical memory. Used for a FIFO system
  
  extract() -> gets page number and offset value from a logical address
  pageFind() -> searches TLB, page table, and disk lookup for a signed char in physical memory
  tlbShift() -> shifts the TLB using a FIFO system
  readStorage() -> reads physical memory from BACKING_STORE and stores it into memory
  removeFromTLB() -> Removes a page from the TLB. Used in the case a page is no longer in memory
*/

#define TLB_ENTRIES 16 
#define TABLE_ENTRIES 256 
#define TOTAL_FRAMES 128
#define FRAME_SIZE 256
#define PAGE_SIZE 256 
#define BIT_SIZE 0xFFFF 
#define OFFSET_SIZE 0xFF

int tablePages[TABLE_ENTRIES]; 
int tableFrames[TABLE_ENTRIES]; 
int tlbPage[TLB_ENTRIES]; 
int tlbFrame[TLB_ENTRIES]; 
int memory[TOTAL_FRAMES][FRAME_SIZE]; 

int curPages = 0;
int curTLBSize = 0;
int pageFaults = 0;
int tlbHits = 0;
int oldestFrame = 0;

int extract(int,int*,int*);
int pageFind(int,int,int);
int tlbShift(int,int);
int readStorage(int,int);
/*
  Program explaination:
  Mimics a Virtual Memory Manager with pure demand paging from a
  file that contains logical addresses of 16-bit addresses (Anything bigger gets truncated via 16-bit mask).
  Afterwards, page number and offset is found and then data of that logical address is found from either the TLB, page table, or disk lookup.

  FILE *fp = pointer that opens file in argv[1]
  char buffer[] = hold for whatever is read in fp
  int pNum = the page number of a logical address
  int offset = offset of a logical address
  int logicalAddress = int data that is read from fp
  int count = counting variable for how many times a file is read
  double faultRate = rate of total pageFaults:  pagefaults/count
  double tlbRate = rate of tlb hits: tlbHits/count
 */

int main(int argc, char* argv[]){

  FILE *fp;
  char buffer[FRAME_SIZE];
  int pNum = 0; 
  int offset = 0; 
  int logicalAddress = 0; 
  int count = 0;
  double faultRate = 0;
  double tlbRate = 0;

  if(argc != 2){ //Usage error handling
   fprintf(stderr,"USAGE: ./VMM <file.txt>\n");
    exit(1);
  }

  fp = fopen(argv[1],"r");

  if(!fp){ //File error handling
    fprintf(stderr,"File unable to be opened.\n");
    exit(1);
  }

  /* sets Page table and TLB indexes to -1 before VMM begins */
  for(count = 0; count < TABLE_ENTRIES; count++){ 
    tablePages[count] = -1;
    tableFrames[count] = -1;
  }
  for(count = 0; count < TLB_ENTRIES; count++){
    tlbPage[count] = -1;
    tlbFrame[count] = -1;
  }

  /* reads entire file for logical addresses and mimics a memory handler*/
  for(count = 0; fgets(buffer,FRAME_SIZE,fp); count++){
    logicalAddress = atoi(buffer);
    extract(logicalAddress,&pNum,&offset); //get page number & offset
    pageFind(logicalAddress,pNum,offset); //find LA's physical memory value
  }
 
  faultRate = (double)pageFaults/count;
  tlbRate = (double)tlbHits/count;

  printf("Page faults: %d tlb hits: %d Total logical addresses: %d\n",pageFaults,tlbHits,count);
  printf("Page-fault rate: %lf \nTLB hit rate: %lf\n",faultRate,tlbRate);

  fclose(fp);
  return 1;
}

/*
  tlbShift() -> shifts tlb cache down and kicks off a page number at the bottom of the cache in a FIFO system
  
  int pageData = page data to be inserted into the top of the pageNumber tlb cache
  int frameData = frame data to be inserted into the top of the pageNumber tlb cache

  int pageHold = holds a page number to swap
  int frameHold = holds a frame number to swap
  int transferHold = holds a value before moving it to pageHold or frameHold
  int count = counting variable for loop
 */

int tlbShift(int pageData,int frameData){
  int pageHold = 0;
  int frameHold = 0;
  int transferHold = 0;
  int count = 0;

  if(curTLBSize < 1){ //Empty TLB
    tlbPage[0] = pageData;
    tlbFrame[0] = frameData;
    curTLBSize++;
  }
  else{
    /* Base Case*/
    pageHold = tlbPage[0];
    frameHold = tlbFrame[0];
    tlbPage[0] = pageData;
    tlbFrame[0] = frameData;

    for(count = 1; count < curTLBSize; count++){ //shift all values down the array
      transferHold = tlbPage[count];
      tlbPage[count] = pageHold;
      pageHold = transferHold;
      transferHold = tlbFrame[count];
      tlbFrame[count] = frameHold;
      frameHold = transferHold;
      }
    if(pageData != pageHold && curTLBSize < TLB_ENTRIES-1){ //If tlb table not full then move last content over. Otherwise dump it
      tlbPage[count+1] = pageHold;
      tlbFrame[count+1] = frameHold;
      curTLBSize++;
    }
  }
  return 1;
}

/*
  extract() -> from a value which should be a 16 bit int (truncate by a bit mask if not), page number and offset is found and returned
  
  int value = logical address
  int* pNum = page number of logical address
  int* offset = offset of logical address

  int pagePower = log base 2 power of a page size
 */

int extract(int value,int* pNum,int* offset){
  int pagePower = (int)log2(PAGE_SIZE);
  if(value > BIT_SIZE)
    value = value & BIT_SIZE;
  *pNum = value >> pagePower;
  *offset = value & OFFSET_SIZE;
  return 1;
}

/*
  readStorage() -> reads from BACKING_STORE and stores the data from a logical address into memory

  FILE *fp = file pointer to open files
  char buffer[] = contents of read file
  int count = counting variable for loops
 */
int readStorage(int pNum,int frameIndex){
  FILE *fp;
  char buffer[FRAME_SIZE];
  int count = 0;

  fp = fopen("BACKING_STORE.bin","rb");
  if(!fp){ //File open error
    fprintf(stderr,"Error opening BACKING_STORE.bin.\n");
    exit(1);
  }
  if(fseek(fp,pNum*FRAME_SIZE,SEEK_SET) != 0){ //Seek error
    fprintf(stderr,"Error seeking page in BACKING_STORE.bin.\n");
    exit(1);
  }

  fread(buffer,sizeof(signed char),FRAME_SIZE,fp);

  /*store physical memory */
  for(count = 0; count < FRAME_SIZE; count++)
    memory[frameIndex][count] = buffer[count];

  fclose(fp);
  return 1;
}

/*
  removeFromTLB() -> Removes a page from the TLB. Used for the case of a page losing its frame in physical memory

  int count = main counting variable used for main loops
  int subCount = secondary counting variable used for sub loops
 */
int removeFromTLB(int pageNumber){
  int count;
  int subCount;

  for(count = 0; count < curTLBSize; count++){ //Find the page Number
    if(tlbPage[count] == pageNumber){
      for(subCount = count; subCount < curTLBSize-1; subCount++){ //Shift all values up and dump the targeted page number
	tlbPage[subCount] = tlbPage[subCount+1];
	tlbFrame[subCount] = tlbFrame[subCount+1];
      }
      tlbPage[curTLBSize-1] = -1; //deallocate last tlb index and lower the size
      tlbFrame[curTLBSize-1] = -1;
      curTLBSize--;
    }
  }
  return 1;
}
/*
  pageFind() -> find the physical memory data of a page number and offset through the TLB, page table, or disk lookup

  int la = logical address to be looked up
  int pNum = page number of the offset
  int offset = offset value of the logical address

  int frameIndex = a counting variable that finds the index of a page number to a frame index
  int pageFlag = a psuedo-boolean variable that holds the index of a page number in the tlb or page table
  int value = data of a logical address is in physical memory
*/

int pageFind(int la,int pNum,int offset){
  int frameIndex;
  int pageFound = -1;
  int pageFlag = -1;
  signed char value;
  
  /*TLB cache lookup */
  for(frameIndex = 0; frameIndex < curTLBSize && pageFlag < 0; frameIndex++)
    if(pNum == tlbPage[frameIndex]){
      pageFlag = tlbFrame[frameIndex]; //get the postion in memory
      tlbHits++;
    }

  /*If not in the TLB, look in the page table */
  if(pageFlag < 0){
    for(frameIndex = 0; frameIndex < curPages; frameIndex++) 
      if(pNum == tablePages[frameIndex]){ //If page number is found and is a valid bit
	pageFlag = tableFrames[frameIndex];
	pageFound = frameIndex;
	if(tableFrames[frameIndex] > 0) //if page has a frame
	  tlbShift(tablePages[frameIndex],tableFrames[frameIndex]); //adds page into the tlb cache
	break;
      }
  }
  
  /* If not in the TLB cache or the page table then check disk*/
  if(pageFlag < 0){
    if(curPages < TOTAL_FRAMES){ //Page not in memory and there are free frames
      readStorage(pNum,curPages);
    
      tablePages[curPages] = pNum; //adds page to the page table
      tableFrames[curPages] = curPages; //adds the physical memory index to frames *Note this only works if Virtual memory = physical memory

      tlbShift(pNum,curPages);

      pageFlag = curPages;
      value = memory[curPages][offset];
      curPages++; //New page added to Page Table
    }
    else if(pageFound < 0){ //Page not in memory and no free free frames

      /*Find victim's page */
      for(frameIndex = 0; frameIndex < TABLE_ENTRIES; frameIndex++)
	if(tableFrames[frameIndex] == oldestFrame){
	  pageFound = frameIndex;
	  break;
	}
      pageFlag = tableFrames[pageFound]; //store victim's page number
      readStorage(pNum,tableFrames[pageFound]); //read new page's Physical memory
      removeFromTLB(tablePages[pageFound]); //Check and remove old page from tlb
      tablePages[curPages] = pNum; // new page in page table
      tableFrames[curPages] = tableFrames[pageFound]; //new frame points to victim's frame index
      tableFrames[pageFound] = -1; //deallocate frame from victim
      value = memory[tableFrames[pageFound]][offset];
      oldestFrame++;
      curPages++;
      if(oldestFrame >= TOTAL_FRAMES) oldestFrame = 0;
      tlbShift(pNum,tableFrames[curPages]);
    }
    else{ //Page in memory but frame is not in memory
      int victimFrame = 0; //Holds the page index that will have its frame replaced
      /*Find victim */
      for(frameIndex = 0; frameIndex < TABLE_ENTRIES; frameIndex++)
	if(tableFrames[frameIndex] == oldestFrame){
          victimFrame = frameIndex;
          break;
        }

      tableFrames[pageFound] = tableFrames[victimFrame]; //New page is set to the index of the victim
      tableFrames[victimFrame] = -1; //deallocate victim
      removeFromTLB(tablePages[victimFrame]);
      readStorage(pNum,tableFrames[pageFound]);
      value = memory[tableFrames[pageFound]][offset];
      oldestFrame++;
      if(oldestFrame >= TOTAL_FRAMES) oldestFrame = 0;
      pageFlag = pageFound;
      tlbShift(pNum,tableFrames[pageFound]);
      } 
    pageFaults++; //finishes page fault with an increament
 
  }
  else
    value = memory[pageFlag][offset]; //extract data from memory
  
  printf("Virtual address: %d Physical address: %d Value: %d \n",la,pageFlag*FRAME_SIZE+offset,value);
    
  return 1;
}
