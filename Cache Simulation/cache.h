#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <vector>

using namespace std;


class cache{
    int size, assoc, blocksize, sets, BO, index, tag;
    int reads, read_misses, writes, write_misses, write_backs; //counters for statistics


    

    public:
        //Block class to be used in vectors
        class block{
                public:
                    uint64_t tag, recency;                        //tag of the block
                    bool valid = false, dirty = false;
            };
        //end of block class

        class stream{
            public:
                uint64_t tag;
                bool valid = false;
        };

        //vector of vectors to be used as sets in cache
        vector<vector<block>> set_list;

        //vector of vector for stream buffers
        vector<vector<stream>> stream_buffer;


        //constructor for cache class, should have following configurable parameters(size, associability, blocksize))
        cache(int bytes = 0, int associability = 0 , int block = 0, int stream_count = 0, int stream_size = 0){
            size = bytes;                           //total size of cache
            assoc = associability;                  //number of blocks per set
            blocksize = block;                      //block size in bytes
            sets = (size/(blocksize*assoc));        //number of sets in cache

            //adjust set_list according to number of sets
            set_list.resize(sets);
            for(int i = 0; i < sets; i++){
                set_list[i].resize(assoc);          //resize each set to number of blocks per set
            }

            //initialize stream buffer
            stream_buffer.resize(stream_count);
            for(int i = 0; i < stream_count; i++){
                stream_buffer[i].resize(stream_size); //resize each stream buffer to stream size
            }
        }
        //end of constructor code

        //function for handling cache writes
        void write(uint64_t address){
            int lru_index = 0;                                      //variable to track LRU block
            uint64_t lru_recency = set_list[index][0].recency;      //variable to track LRU recency

            writes++;                                             //increment write counter
            uint64_t index_mask = sets - 1;                       //mask to get index bits
            index = (address / blocksize) & index_mask;           //calculate index
            tag = address / (blocksize * sets);                   //calculate tag
            bool cache_hit = false;                               //flag to check if hit or miss in cache
            bool stream_hit = false;                              //flag to check if hit or miss in stream buffer

            //check for hit in the cache
            for(int i = 0; i < assoc; i++){
                if(set_list[index][i].tag == tag){
                    cache_hit = true;                                 //set hit flag to true
                    lru_index = i;                             //store index of the hit block
                    //adjust recency of all blocks in the set
                    for(int j = 0; j < assoc; j++){
                        if((set_list[index][j].recency  < set_list[index][lru_index].recency) && set_list[index][j].valid){ //if block is more recent than the hit block
                            set_list[index][j].recency++;       //increment recency of all valid blocks
                            
                        }
                    }
                    set_list[index][i].dirty = 1;               //set dirty bit to 1
                    set_list[index][i].recency = 0;             //update recency
                    break;
                }
            }
            //check for hit in the stream buffer
            if(!cache_hit){                                        //only check stream buffer if miss in cache
                for(int i = 0; i < stream_buffer.size(); i++){      //for each stream buffer
                    for(int j = 0; j < stream_buffer[i].size(); j++){ //for each block in the stream buffer
                        if(stream_buffer[i][j].tag == tag && stream_buffer[i][j].valid){ //if hit in stream buffer
                            stream_hit = true;                          //set hit flag to true
                            //move the block to the cache
                            //empty block check***************************************************
                            for(int k = 0; k < assoc; k++){
                                if(!set_list[index][k].valid){                 //if empty block found
                                    set_list[index][k].valid = 1;              //set valid bit to 1
                                    set_list[index][k].tag = tag;              //set tag
                                    set_list[index][k].dirty = 1;              //set dirty bit to 1
                                    set_list[index][k].recency = 0;            //update recency
                                    lru_index = k;                             //store index of the empty block
                                    
                                    //invalidate the block in the stream buffer
                                    stream_buffer[i][j].valid = 0;
                                    return;
                                }
                            }
                            //empty block check***************************************************

                            //if no empty block, need to evict a block using LRU policy           
                            
                            //code finds lRU block
                            for(int k = 0; k < assoc; k++){
                                if(set_list[index][k].recency > lru_recency){
                                    lru_recency = set_list[index][k].recency;
                                    lru_index = k;
                                }
                            }

                            //if the block to be evicted is dirty, increment write back counter
                            if(set_list[index][lru_index].dirty){
                                write_backs++;
                            }

                            //adjust recency of all blocks in the set
                            for(int k = 0; k < assoc; k++){
                                if(set_list[index][k].valid && k != lru_index){ //if block is valid and not the one being evicted
                                    set_list[index][k].recency++;
                                }
                            }

                            //move the block from stream buffer to cache
                            set_list[index][lru_index].valid = 1;              //set valid bit

            //handles write misses and updates recency
            if(!cache_hit && !stream_hit){                                               //if miss
                write_misses++;//be sure this does not occur if stream buffer hits                                    //increment write miss counter

                //check for empty block in the set
                for(int i = 0; i < assoc; i++){
                    if(!set_list[index][i].valid){                 //if empty block found
                        set_list[index][i].valid = 1;              //set valid bit to 1
                        set_list[index][i].tag = tag;              //set tag
                        set_list[index][i].dirty = 1;              //set dirty bit to 1
                        set_list[index][i].recency = 0;            //update recency
                        lru_index = i;                             //store index of the empty block
                        return;
                    }
                }

                //if no empty block, need to evict a block using LRU polic           
                
                //code finds lRU block
                for(int i = 0; i < assoc; i++){
                    if(set_list[index][i].recency > lru_recency){
                        lru_recency = set_list[index][i].recency;
                        lru_index = i;
                    }
                }

                //if the block to be evicted is dirty, increment write back counter
                if(set_list[index][lru_index].dirty){
                    write_backs++;
                }

                //adjust recency of all blocks in the set
                for(int i = 0; i < assoc; i++){
                    if(set_list[index][i].valid && i != lru_index){ //if block is valid and not the one being evicted
                        set_list[index][i].recency++;
                    }
                }
            }
        }
        //end of write function

        //function for handling cache reads
        

};


