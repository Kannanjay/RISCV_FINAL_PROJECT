#include "dogfault.h"
#include "cache.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "config.h"

// HELPER FUNCTIONS USEFUL FOR IMPLEMENTING THE CACHE

unsigned long long address_to_block(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
    /*
    int byte_offset = cache->blockBits;
    return ( (address >> byte_offset) << byte_offset);
    */
    return ( (address >> CACHE_BLOCK_BITS) << CACHE_BLOCK_BITS);
}

unsigned long long cache_tag(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
    /*
    // Cache tag is in the MSBs of the address
    return address >> (cache->setBits + cache->blockBits);
    */
    return address >> (CACHE_SET_BITS + CACHE_BLOCK_BITS);
}

unsigned long long cache_set(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
    /*
    // Set index is in between the tag and offset bits
    return (address >> cache->blockBits) & ((1U << cache->setBits) - 1); 
    */
    return (address >> CACHE_BLOCK_BITS) & ((1U << CACHE_SET_BITS) - 1); 

}

bool probe_cache(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
    unsigned long long set_index = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);

    // Pointers like this make it easier to read all these helper functions, instead
    // of dealing with multiple nested dot/arrow operators which can get messy
    Set *set = &cache->sets[set_index]; 

    //for (int i = 0; i < cache->linesPerSet; ++i) {
    for (int i = 0; i < CACHE_LINES_PER_SET; ++i) {

        Line *line = &set->lines[i];

        // Along with checking tag value, need to check the valid bit
        if ( (line->valid == true) && (line->tag == tag) ) {
            return true;
        }
    }
    return false;   
}

void hit_cacheline(const unsigned long long address, Cache *cache) {
    /*YOUR CODE HERE*/

    unsigned long long set_index = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);
    
    Set *set = &cache->sets[set_index];

    int replacement_policy = CACHE_LFU;

    //for (int i = 0; i < cache->linesPerSet; ++i) {
    for (int i = 0; i < CACHE_LINES_PER_SET; ++i) {
        Line *line = &set->lines[i];

        if ( (line->valid == true) && (line->tag == tag) ) {
            //if (cache->lfu == 0) { // LRU Case
            if (replacement_policy == 0) { // LRU Case
                line->lru_clock = ++set->lru_clock;
            } else { // LFU Case
                line->access_counter++;
            }
            break;
        }
    }
    
    // Did not find line, not supposed to be here
    //assert(0);
}

bool insert_cacheline(const unsigned long long address, Cache *cache) {
    /*YOUR CODE HERE*/
    unsigned long long set_index = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);
    unsigned long long block_addr = address_to_block(address, cache);

    Set *set = &cache->sets[set_index];

    //for (int i = 0; i < cache->linesPerSet; ++i) {
    for (int i = 0; i < CACHE_LINES_PER_SET; ++i) {

        Line *line = &set->lines[i];

        if (line->valid == false) {
            line->valid = true;
            line->block_addr = block_addr;
            line->tag = tag;
            line->lru_clock = ++set->lru_clock;
            line->access_counter = 1; // initiate (not increment) access_counter
            return true;
        }
    }
    return false; 
}

unsigned long long victim_cacheline(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
    unsigned long long set_index = cache_set(address, cache);
    Set *set = &cache->sets[set_index];
    int victim_index = 0;

    int min_accesses = set->lines[0].access_counter;
    unsigned long long min_lru_clock = set->lines[0].lru_clock;

    int replacement_policy = CACHE_LFU;

    //for (int i = 1; i < cache->linesPerSet; ++i) {
    for (int i = 1; i < CACHE_LINES_PER_SET; ++i) {

        Line *line = &set->lines[i];

        //if (cache->lfu) {
        if (replacement_policy == 1) { //LFU
            if (line->access_counter < min_accesses ||
                (line->access_counter == min_accesses && line->lru_clock < min_lru_clock)) { 
                    min_accesses = line->access_counter;
                    min_lru_clock = line->lru_clock;
                    victim_index = i;
            }
        } else { // LRU
            if (line->lru_clock < min_lru_clock) {
                min_lru_clock = line->lru_clock;
                victim_index = i;
            }
        }
    }
    return set->lines[victim_index].block_addr;
}

void replace_cacheline(const unsigned long long victim_block_addr, const unsigned long long insert_addr, Cache *cache) {
    /*YOUR CODE HERE*/
 
    unsigned long long set_index = cache_set(insert_addr, cache);
    unsigned long long tag = cache_tag(insert_addr, cache);
    unsigned long long block_addr = address_to_block(insert_addr, cache);

    Set *set = &cache->sets[set_index];

    //for (int i = 0; i < cache->linesPerSet; ++i) {
    for (int i = 0; i < CACHE_LINES_PER_SET; ++i) {

        Line *line = &set->lines[i];

        if (line->block_addr == victim_block_addr) {
            line->block_addr = block_addr;
            line->tag = tag;
            line->lru_clock = ++set->lru_clock;
            line->access_counter = 1; // initiate (not increment) access_counter
            break;
        }
    }
    
    // Not supposed to be here
    //assert(0);
}

void cacheSetUp(Cache *cache, char *name) {
    cache->hit_count = 0;
    /*YOUR CODE HERE*/

    //int num_sets = 1 << cache->setBits;
    int num_sets = 1 << CACHE_SET_BITS;
    cache->sets = (Set *)malloc(num_sets * sizeof(Set));

    for (int i = 0; i < num_sets; ++i) {
        //cache->sets[i].lines = (Line *)malloc(cache->linesPerSet * sizeof(Line));
        cache->sets[i].lines = (Line *)malloc(CACHE_LINES_PER_SET * sizeof(Line));
        cache->sets[i].lru_clock = 0;
        //for (int j = 0; j < cache->linesPerSet; ++j) {
        for (int j = 0; j < CACHE_LINES_PER_SET; ++j) {
            cache->sets[i].lines[j].valid = false;
            cache->sets[i].lines[j].lru_clock = 0;
            cache->sets[i].lines[j].access_counter = 1;
        }
    }

    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->eviction_count = 0;
    cache->name = name;
}

void deallocate(Cache *cache) {
    /*YOUR CODE HERE*/
    //int num_sets = 1 << cache->setBits;
    int num_sets = 1 << CACHE_SET_BITS;
    for (int i = 0; i < num_sets; ++i) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
}

result operateCache(const unsigned long long address, Cache *cache) {
    result r;
    /*YOUR CODE HERE*/
    //printf(CACHE_HIT_FORMAT, address); 

    ///////////////////////FROM LAB4/////////////////////////////
    // increment the global lru_clock in the corresponding cache set for the address
    unsigned long long set_index = cache_set(address, cache);
    Set *set = &cache->sets[set_index];
    set->lru_clock++;
    
    // check if the address is already in the cache
    if (probe_cache(address, cache) == true) { // Cache hit
        // update the counters inside the hit cache line
        hit_cacheline(address, cache);
        cache->hit_count++;
        // record hit status
        r.status = CACHE_HIT;
        
        #ifdef PRINT_CACHE_TRACES
        printf(CACHE_HIT_FORMAT, address);
        #endif

    } else { // Cache miss

        // find an empty cache line in the cache set
        if (insert_cacheline(address, cache) == true) {
            cache->miss_count++;
            r.status = CACHE_MISS;
            r.insert_block_addr = address_to_block(address, cache);

            #ifdef PRINT_CACHE_TRACES 
            printf(CACHE_MISS_FORMAT, address);
            #endif

        } else {
            unsigned long long victim_block_addr = victim_cacheline(address, cache);
            replace_cacheline(victim_block_addr, address, cache);
            cache->miss_count++;
            cache->eviction_count++;
            // Eviction
            r.status = CACHE_EVICT;
            r.victim_block_addr = victim_block_addr;
            r.insert_block_addr = address_to_block(address, cache);

            #ifdef PRINT_CACHE_TRACES 
            printf(CACHE_EVICTION_FORMAT, address);
            #endif
        }
    }
    //////////////////////FROM LAB4/////////////////////////////
    
    return r;
    /*YOUR CODE HERE*/
}

int processCacheOperation(unsigned long address, Cache *cache) {
    result r;
    /*YOUR CODE HERE*/
    r = operateCache(address, cache);

    if (r.status == CACHE_HIT) {
        return CACHE_HIT_LATENCY;
    } else if (r.status == CACHE_MISS)
    {
        return CACHE_MISS_LATENCY;
    }
    else
    {
        return CACHE_OTHER_LATENCY;
    }
}
