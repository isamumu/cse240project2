//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"
#include <math.h> // ADDED

//
// TODO:Student Information
//
const char *studentName = "Isamu Arthur Poy";
const char *studentID   = "A59011424";
const char *email       = "ipoy@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;     // Number of sets in the I$
uint32_t icacheAssoc;    // Associativity of the I$
uint32_t icacheHitTime;  // Hit Time of the I$

uint32_t dcacheSets;     // Number of sets in the D$
uint32_t dcacheAssoc;    // Associativity of the D$
uint32_t dcacheHitTime;  // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$
uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize;      // Block/Line size
uint32_t memspeed;       // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

//
//TODO: Add your Cache data structures here
//

// !! take care of corner cases for null pointers !!
// structure: block 
typedef struct Block{
  uint32_t tag; 
  struct Block *prev;
  struct Block *next;
} Block;

// structure: set 
typedef struct Set{
  uint32_t count;
  Block *front;
  Block *back;
} Set; 

// functions: LRU replacement...
// ---> after a block is hit/accessed send it to the back
// ---> essentially, whatever is in the back is the most recently used
// ---> the Least Recently Used (LRU) block will be at the front
// ---> LRU replacement occurs when the cache is full
// ---> scenario 1: the cache is not full but you get a miss in an occupied block, so you replace
// ---> scenario 2: the cache is not full and you get a miss in an empty block, so you simply add
// ---> scenario 3: the cache is full, so you need to pop either end (whichever has the LRU block) and replace accordingly

// TODO --> make sure that each block has a valid bit
// without the valid bit, we have no way of telling whether a block can be overwritten or not with a new block's info

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

Set *icache;
Set *dcache;
Set *l2cache;

uint16_t iIndexFilter;
uint16_t dIndexFilter;
uint16_t l2IndexFilter;

uint16_t iIndexNum;
uint16_t dIndexNum;
uint16_t l2IndexNum;

uint16_t offsetBitsNum;

// add new elements to the front, add old ones to the back
int isEmpty(Set *set){
  return set->front == NULL;
}

// helper: create a block for a set (if set isn't full)
Block* createBlock(uint32_t tag){
  Block *newBlock = (Block*)malloc(sizeof(Block));
  newBlock->next = NULL;
  newBlock->prev = NULL;
  newBlock->tag = tag;
}

// helper: remove a back block from queue
void pop_block(Set *set){
  if(isEmpty(set))
    return;
  
  if(set->front == set->back)
    set->front = NULL;

  Block *temp = set->back;
  set->back = set->back->prev;

  if(set->back != NULL)
    set->back->next = NULL;
  
  free(temp);
  set->count -= 1;
}

// helper: insert a block to the front
void insert_block(Set *set, uint32_t tag){

  Block *temp = createBlock(tag);
  temp->next = set->front;

  if(isEmpty(set)){
    set->front = temp;
    set->back = set->front;
  }

  set->count += 1;
}

void evictTarget(Set *set, uint32_t tag){
  Block *temp = set->front;
  Block *prev = NULL;
  
  // if front
  if(set->front->tag == tag){
    set->front = temp->next;
    free(set->front);
    return;
  } 
  // if middle
  while((temp->tag != tag) && (temp != NULL)){
    prev = temp;
    temp = temp->next;
  }

  // means nothing was found to evict in L1
  if(temp == NULL)
    return;
  
  // at this point something was found
  prev = temp->next;
  free(temp);
}

// 1: dcache 0: icache
void searchAndEvict(uint32_t addr, int isData){
  uint32_t addr_tag;
  uint32_t addr_index;
  int num_blocks;
  Block *temp;

  if(isData){
    addr_tag = addr >> (dIndexNum + offsetBitsNum);
    addr_index = (addr >> offsetBitsNum) & dIndexFilter;
    num_blocks = dcache[addr_index].count;
    temp = dcache[addr_index].front;

    for(int i = 0; i < num_blocks; i++){
      if(temp->tag == addr_tag){
        // evict from dcache
        evictTarget(&(dcache[i]), 1);
      } else{
        temp = temp->next; // update for checking the next recent block
      }
    }
  } else{
    addr_tag = addr >> (iIndexNum + offsetBitsNum);
    addr_index = (addr >> offsetBitsNum) & iIndexFilter;
    num_blocks = icache[addr_index].count;
    temp = icache[addr_index].front;

    for(int i = 0; i < num_blocks; i++){
      if(temp->tag == addr_tag){
        // evict from icache
        evictTarget(&(icache[i]), 0);
      } else{
        temp = temp->next; // update for checking the next recent block
      }
    }
  }
}

// Initialize the Cache Hierarchy
//
void
init_cache()
{
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;
  
  //
  //TODO: Initialize Cache Simulator Data Structures
  //

  icache = (Set*)malloc(sizeof(Set)*icacheSets);
  dcache = (Set*)malloc(sizeof(Set)*dcacheSets);
  l2cache = (Set*)malloc(sizeof(Set)*l2cacheSets);

  offsetBitsNum = log2(blocksize);
  // handle icache bits
  iIndexNum = log2(icacheSets);
  iIndexFilter = ((1 << iIndexNum) - 1);

  // handle dcache bits
  dIndexNum = log2(dcacheSets);
  dIndexFilter = ((1 << dIndexNum) - 1);
 
  // handle l2cache bits
  l2IndexNum = log2(l2cacheSets);
  l2IndexFilter = ((1 << l2IndexNum) - 1);

  // init icache
  for(int i = 0; i < icacheSets; i++){
    icache[i].count = 0;
    icache[i].front = NULL;
    icache[i].back = NULL;
  }

  // init dcache
  for(int i = 0; i < dcacheSets; i++){
    dcache[i].count = 0;
    dcache[i].front = NULL;
    dcache[i].back = NULL;
  }

  // init l2cache
  for(int i = 0; i < l2cacheSets; i++){
    l2cache[i].count = 0;
    l2cache[i].front = NULL;
    l2cache[i].back = NULL;
  }
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  // isolate tag and index values
  uint32_t addr_tag = addr >> (iIndexNum + offsetBitsNum);
  uint32_t addr_index = (addr >> offsetBitsNum) & iIndexFilter;
  Block *temp = icache[addr_index].front;
  int num_blocks = icache[addr_index].count;

  // check if we go straight to l2
  if(icacheSets == 0)
    return l2cache_access(addr);
  
  // we know for sure icache is being ref now
  icacheRefs += 1;

  // loop through the cache related to the index AND look for a hit
  for(int i = 0; i < num_blocks; i++){
    if(temp->tag == addr_tag){
      return icacheHitTime;
    } else{
      temp = temp->next; // update for checking the next recent block
    }
  }

  // either way, it is a miss now
  uint32_t l2_time = l2cache_access(addr);

  icacheMisses += 1;
  icachePenalties += l2_time; // penalty is a subset of access time

  // insert the block to the set if it is not full
  if(num_blocks < icacheAssoc){
    insert_block(&(icache[addr_index]), addr_tag);
  } else{ // if full, pop back and insert to front
    pop_block(&(icache[addr_index]));
    insert_block(&(icache[addr_index]), addr_tag);
  }
  
  return (l2_time + icacheHitTime);
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  // isolate tag and index values
  uint32_t addr_tag = addr >> (dIndexNum + offsetBitsNum);
  uint32_t addr_index = (addr >> offsetBitsNum) & dIndexFilter;
  Block *temp = dcache[addr_index].front;
  int num_blocks = dcache[addr_index].count;

  // check if we go straight to l2
  if(dcacheSets == 0)
    return l2cache_access(addr);
  
  // we know for sure icache is being ref now
  dcacheRefs += 1;

  // loop through the cache related to the index AND look for a hit
  for(int i = 0; i < num_blocks; i++){
    if(temp->tag == addr_tag){
      return dcacheHitTime;
    } else{
      temp = temp->next; // update for checking the next recent block
    }
  }

  // either way, it is a miss now
  uint32_t l2_time = l2cache_access(addr);

  dcacheMisses += 1;
  dcachePenalties += l2_time; // penalty is a subset of access time

  // insert the block to the set if it is not full
  if(num_blocks < dcacheAssoc){
    insert_block(&(dcache[addr_index]), addr_tag);
  } else{ // if full, pop back and insert to front
    pop_block(&(dcache[addr_index]));
    insert_block(&(dcache[addr_index]), addr_tag);
  }
  
  return (l2_time + dcacheHitTime);
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  // isolate tag and index values
  uint32_t addr_tag = addr >> (l2IndexNum + offsetBitsNum);
  uint32_t addr_index = (addr >> offsetBitsNum) & l2IndexFilter;

  Block *temp = l2cache[addr_index].front;
  int num_blocks = l2cache[addr_index].count;

  // check if we go straight to l2
  if(l2cacheSets == 0)
    return memspeed;
  
  // we know for sure icache is being ref now
  l2cacheRefs += 1;

  // loop through the cache related to the index AND look for a hit
  for(int i = 0; i < num_blocks; i++){
    if(temp->tag == addr_tag){
      return l2cacheHitTime;
    } else{
      temp = temp->next; // update for checking the next recent block
    }
  }

  l2cacheMisses += 1;
  l2cachePenalties += memspeed; // penalty is a subset of access time

  // if inclusive (if we evict from L2 we must also evict from L1)
  if(l2cache[addr_index].count < l2cacheAssoc){
    insert_block(&(l2cache[addr_index]), addr_tag);
  } else{
    if(inclusive){
      // target to evict 
      Block *target = l2cache[addr_index].back;
      uint32_t tag = target->tag;
      uint32_t dittoAddr = ((tag << addr_index) + addr_index) << offsetBitsNum;

      // evict from dcache the LRU from l2
    
      // evict from icache the LRU from l2
      
      // evict from l2cache AND insert to l2cache
    } else{

    }
  }
  

  return (l2cacheHitTime + memspeed);
}
