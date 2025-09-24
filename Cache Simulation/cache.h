#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <vector>

using namespace std;


class cache{
    int size, assoc, blocksize, sets, BO, index, tag;
    int reads, read_misses, writes, write_misses, write_backs, prefetches;              //counters for statistics
    cache *next_level; //pointer to next level of cache
    int LRU_index = 0;                         //index of least recently used block in set



    

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
                vector<uint64_t> tags;      //vector of tags in stream buffer
                uint64_t recency;           //recency of stream buffer
                bool valid = false;
        };

        //vector of vectors to be used as sets in cache
        vector<vector<block>> set_list;

        //vector of vector for stream buffers
        vector<stream> stream_buffers;


        //constructor for cache class, should have following configurable parameters(size, associability, blocksize))
        cache(int bytes = 0, int associability = 0 , int block = 0, cache *next = nullptr, int stream_count = 0, int stream_size = 0){
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
            stream_buffers.resize(stream_count);            //resize stream buffer to number of stream buffers
            for(int i = 0; i < stream_count; i++){
                stream_buffers[i].tags.resize(stream_size); //resize each stream buffer to stream size
            }
        }
        //end of constructor code


        void request(char rw, uint64_t addy){   //function that handles read/write requests
            //find tag and index bits
            int BO_bits = log2(blocksize);                   //number of block offset bits
            int index_bits = log2(sets);                     //number of index bits
            int mask = sets - 1;                             //mask to get index bits
            index = (addy >> BO_bits) & mask;                    //index bits
            tag = addy >> (BO_bits + index_bits);           //tag bits
            //find tag and index bits

            bool cache_hit = false;                     //flag to check if cache hit or miss
            bool stream_hit = false;                    //flag to check if stream buffer hit or miss
            int LRU_index = 0;                         //index of least recently used block in set

            //check for cache hit/
            for(int i = 0; i < assoc; i++){             //loop through all blocks in the set
                if(set_list[index][i].tag == tag){              //if block tags match
                    cache_hit = true;    
                    //check for stream hit                                       //cache hit
                    for(int j = 0; j < stream_buffers.size(); j++){             //check streams for hit
                        for(int k = 0; k < stream_buffers[j].tags.size(); k++){
                            if(stream_buffers[j].tags[k] == tag){   //if tags match
                                stream_hit = true;                  //stream buffer hit
                                stream_store(stream_buffers, tag, j);   //update stream buffer
                            }
                        }
                    }                                 

                    if(rw == 'w'){                       //if write request
                        set_list[index][i].dirty = true; //set dirty bit
                        writes++;                        //increment write counter
                    }
                    else{
                        reads++;                         //increment read counter
                    }
                    block_adjust(set_list[index], i);    //adjust recency of blocks in set
                    return;                              //exit function
                }
            }
            //check for cache hit

            //if cache miss, check for stream buffer hit
            if(!cache_hit){
                for(int i = 0; i < stream_buffers.size(); i++){     //loop through all stream buffers
                    if(stream_buffers[i].valid){                    //if stream buffer is valid
                        for(int j = 0; j < stream_buffers[i].tags.size(); j++){ //loop through all tags in stream buffer
                            if(stream_buffers[i].tags[j] == tag){   //if tags match
                                stream_hit = true;                  //stream buffer hit
                                //code here to move stream buffer block to cache
                                stream_store(stream_buffers, tag, i);      //store new tags in stream buffer
                                cache_store(set_list[index], tag, addy);          //store new block in cache
                                if(rw == 'w'){                       //if write request
                                    writes++;                        //increment write counter
                                    set_list[index][LRU_index].dirty = true; //set dirty bit                                   
                                } else {
                                    reads++;                         //increment read counter
                                }
                                return;                              //exit function
                            }
                        }
                    }
                }
            }
            //if stream buffer miss, handle cache miss

            //handle miss 
            if(!cache_hit && !stream_hit){
                if(next_level != nullptr){   //if there is a next level of cache
                    next_level->request('r', addy); //send read request to next level of cache
                }
                cache_store(set_list[index], tag, addy);          //store new block in cache
                for(int j = 0; j < stream_buffers.size(); j++){             //find stream to store new tags
                    if(stream_buffers[j].recency == stream_buffers.size()){                          //if stream buffer is most recently used
                        stream_store(stream_buffers, tag, j);                       //replace most recently used stream buffer
                        break;                                            
                    }
                    if(!stream_buffers[j].valid){                               //if stream buffer is invalid
                        stream_store(stream_buffers, tag, j);                    //store new tags in stream buffer
                        break;                                                  //exit loop
                    }
                }                                 
                if(rw == 'w'){                       //if write request
                    writes++;                        //increment write counter
                    write_misses++;                  //increment write miss counter
                    set_list[index][LRU_index].dirty = true; //set dirty bit                                   
                } else {
                    reads++;                         //increment read counter
                    read_misses++;                   //increment read miss counter
                }
            }
            return;
        }           
        
        //function used to adjust recency of blocks in a vector
        void block_adjust(vector<block> &v, int index){
        for(int i = 0; i < v.size(); i++){
            if(v[i].valid){
                v[i].recency++;                 //increase the recency of all blocks older than the accessed block
            }
        }
        v[index].recency = 0;               //set recency of accessed block to 0
        }
        //function used to adjust recency of blocks in a vector

        //function used to adjust recency of stream buffers in a vector
        void stream_adjust(vector<stream> &v, int index){
        for(int i = 0; i < v.size(); i++){
            if(v[i].valid){
                v[i].recency++;                 //increase the recency of all blocks older than the accessed block
            }
        }
        v[index].recency = 0;               //set recency of accessed block to 0
        }
        //function used to adjust recency of stream buffers in a vector

        //function to store new items in stream 
        void stream_store(vector<stream> &v, uint64_t tag, int stream){
        for(int i = 0; i < v[stream].tags.size(); i++){
            v[stream].tags[i] = tag+i+1;    //store new tags in stream buffer
        }
        v[stream].valid = true;            //set stream buffer to valid
        stream_adjust(v, stream);          //adjust recency of stream buffers
        }
        //function to store new items in stream 

        //function to store new block in cache
        void cache_store(vector<block> &v, uint64_t tag, uint64_t address){          //set(not set_list) is passed as argument
            for(int i = 0; i < v.size(); i++){          //loop through blocks to find invalid block
                if(!v[i].valid){                //if block is invalid
                    v[i].tag = tag;             //store new tag in block
                    v[i].valid = true;          //set block to valid
                    v[i].dirty = false;         //set dirty bit to false
                    block_adjust(v, i);         //adjust recency of blocks in set
                    return;                     //exit function
                }
            }
            //if all blocks are valid, replace least recently used block
            LRU_index = 0;
            for(int i = 0; i < v.size(); i++){
                if(v[i].recency == v.size()){   //find index of least recently used block
                    LRU_index = i;
                }
            }
            //if dirty bit is set, write back to next level of cache
            if(v[LRU_index].dirty){
                //write back to next level of cache
                write_backs++;
                if(next_level != nullptr){
                    next_level->request('w', address); //send write request to next level of cache
                }
            }
            v[LRU_index].tag = tag;             //store new tag in block
            v[LRU_index].valid = true;          //set block to valid
            v[LRU_index].dirty = false;         //set dirty bit to false
            block_adjust(v, LRU_index);         //adjust recency of blocks in set
        }

        void print_stats(){
            printf("=======Cache Contents=======\n");
            for(int i = 0; i < sets; i++){
                for(int j = 0; j < assoc; j++){
                    if(set_list[i][j].valid){
                        printf("Set %d, Block %d: Tag: %lx, Recency: %d, Dirty: %d\n", i, j, set_list[i][j].tag, set_list[i][j].recency, set_list[i][j].dirty);
                    } else {
                        printf("Set %d, Block %d: Invalid\n", i, j);
                    }
                }
            }

            printf("=======measurements=======\n");
            printf("Reads: %d\n", reads);
            printf("Read Misses: %d\n", read_misses);
            printf("Writes: %d\n", writes);
            printf("Write Misses: %d\n", write_misses);
            printf("Write Backs: %d\n", write_backs);
            if(stream_buffers.size() > 0){
                printf("Prefetches: %d\n", prefetches);
            }
            float miss_rate = (float)(read_misses + write_misses) / (float)(reads + writes);
            printf("Miss Rate: %.4f\n", miss_rate);
        }
};



