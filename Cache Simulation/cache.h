#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <vector>
#include <list>

using namespace std;


class cache{
    public:
    int test = 0;
    uint32_t size, assoc, blocksize, sets, BO, tag, index, ss;
    int reads = 0, read_misses = 0, writes = 0, write_misses = 0, write_backs = 0, prefetches = 0;              //counters for statistics
    cache *next_level; //pointer to next level of cache
    int level;
    //important variables
        bool cache_hit;                          //flag to check if hit or miss
        bool stream_hit;                   //flag to check if hit in stream buffer
        int head;
    //important variables done





    //Block class to be used in vectors
    class block{
            public:
                uint32_t tag;                        //tag of the block
                bool valid = false, dirty = false;
        };
    //end of block class

    class stream{
        public:
            vector<uint32_t> addys;      //vector of tags in stream buffer
            //uint64_t recency;           //recency of stream buffer
            bool valid = false;
    };

    //vector of vectors to be used as sets in cache
    vector<list<block>> set_list;

    //vector of vector for stream buffers
    list<stream> stream_buffers;


    //constructor for cache class, should have following configurable parameters(size, associability, blocksize))
    cache(int bytes = 0, int associativity = 0 , int block = 0, cache *next = nullptr, int l = 0, int stream_count = 0, int stream_size = 0){
        size = bytes;                           //total size of cache
        assoc = associativity;                  //number of blocks per set
        blocksize = block;                      //block size in bytes
        BO = log2(blocksize);                   //number of block offset bits
        sets = (size/(blocksize*assoc));        //number of sets in cache
        next_level = next;                      //pointer to next level of cache
        ss = stream_size;
        


        level = l;
        //adjust set_list according to number of sets
        set_list.resize(sets);
        for(uint32_t i = 0; i < sets; i++){              //for each set create a list
            set_list[i].resize(assoc);              //resize list to associativity               
        }

        //initialize stream buffer
        stream_buffers.resize(stream_count);            //resize stream buffer to number of stream buffers
        for(auto &s : stream_buffers){                //for each stream buffer
            s.addys.resize(stream_size);               //resize tags vector to stream size
            //printf("%d",s.addys.size());
            
        }
    }
    //end of constructor code


    void request(char rw, uint32_t addy){   //function that handles read/write requests
        //printf("cuh");

        //calculate index and tag
        int BO_bits = log2(blocksize);                    //number of block offset bits
        int index_bits = log2(sets);             //index bits
        uint32_t mask = sets - 1;                    //mask to get index bits
        index = ((addy >> BO_bits) & mask);       //get index by shifting address right by block offset bits and masking
        tag = addy >> (BO_bits + index_bits);    //get tag by shifting address right by block offset bits and index bits
        //calculate tag and index done

        cache_hit = false;
        stream_hit = false;

        //variables for iterating through lists and vectors
        auto it = set_list[index].begin();    //iterator to beginning of set list
        auto it_s = stream_buffers.begin(); //iterator to beginning of stream buffer list
        uint32_t stream_tag_index = 0;               //index of tag in stream buffer

        //check for hit in cache
        for(it = set_list[index].begin(); it != set_list[index].end(); ++it){          //for each block in the set
            if(it->valid && it->tag == tag){         //if block is valid and tags match
                cache_hit = true;                       //set hit flag to true
                break;
            }
        }
        //check for hit in cache done

        //check for hit in stream buffer 
        for(it_s = stream_buffers.begin(); it_s != stream_buffers.end(); it_s++){ //for each stream buffer
            //printf("checking stream\n");
            //printf("%d\n", it_s->addys.size());
            if(it_s->valid){
                for(stream_tag_index = 0; stream_tag_index < ss; stream_tag_index++){               //for each tag in stream buffer
                    //printf("checking stream");
                    if(it_s->addys[stream_tag_index] == (addy >> BO)){                        //if tag matches
                        //printf("we hittington");
                        stream_hit = true;               //set hit flag to true
                        break;                          //break out of loop
                    }
                }
            }
            if(stream_hit){                         //if hit in stream buffer
                //printf("cuzz");
                break;                              //break out of loop
            }
        }
        if(!stream_hit){
            it_s--;             
        }

        //check for hit in stream buffer done

        
        //Need to make sure that I am considering the following scenarios:
        //Scenario 1: Misses in cache and stream buffer (handle miss as usual updating both the cache and the stream buffer)
        if(!cache_hit && !stream_hit){                 
            cache_store(rw, addy, index, index_bits, BO_bits); //call function to handle cache store
                
            //printf("jittle %d\n",it_s->addys.size());
            stream_store(addy,it_s,ss,stream_tag_index);
            update_stream(it_s);

            if(next_level != nullptr){          //if there is a next level of cache
                next_level->request('r', addy); //send read request to next level of cache
            }

            if(rw == 'r'){                    //if read request
                reads++;                      //increment read counter
                read_misses++;                 //increment read miss counter
            }
            else{                            //if write request
                writes++;                     //increment write counter
                write_misses++;                //increment write miss counter
            }
            return;                          //break out of loop   

        }

        //Scenario 2: misses in cache and hits in buffer (allocate block in cache using addy from stream buffer then update stream buffer)
        if(!cache_hit && stream_hit){
            cache_store(rw,addy, index, index_bits, BO_bits);

            stream_store(addy,it_s, ss, stream_tag_index);
            update_stream(it_s);

            if(rw == 'r'){                    //if read request
                reads++;                      //increment read counter
            }
            else{                            //if write request
                writes++;                     //increment write counter
            }
                
            return;
        }
        //Scenario 3: hits in cache and misses in stream buffer(nothing to do here except adjust necessary counters)
        if(cache_hit && !stream_hit){
            if(rw == 'r'){                    //if read request
                reads++;                      //increment read counter
            }
            else{                            //if write request
                writes++;                     //increment write counter
                it->dirty = true;               //set dirty bit to true
            }
            //update LRU
            update_order(index, it);                  //update LRU by moving block to front of list
            return;
        }
        //Scenario 4: hits in cache and hits in stream buffer (only update stream buffer and counters)
        if(cache_hit && stream_hit){
            stream_store(addy,it_s,ss, stream_tag_index);
            //printf("both hit");
            update_order(index,it);
            update_stream(it_s);

            if(rw == 'r'){                    //if read request
                reads++;                      //increment read counter
            }
            else{                            //if write request
                set_list[index].begin()->dirty = true;
                writes++;                     //increment write counter
            }

            return;
        }
    }
    

    void update_order(int set, list<block>::iterator &b){               //function to set most recently used block to front of list
        if(b != set_list[set].begin())  {            //if block is not already at front of list
            set_list[set].splice(set_list[set].begin(), set_list[set], b);  //move block to front of list(handles removal and insertion)                  
        }
    }

    void update_stream(list<stream>::iterator &b){
        if(stream_buffers.size() == 0) return;
        if(b != stream_buffers.begin()){
            stream_buffers.splice(stream_buffers.begin(), stream_buffers, b);
        }
    }

    void cache_store(char rw, uint32_t addy, uint32_t in, uint32_t ib, uint32_t bo){   //function that handles read/write requests
        auto it = set_list[in].begin();    //iterator to beginning of set list

        it = set_list[in].end();    //iterator to end of set list
            it--;                               //move iterator to last block in set(list is not empty since miss occurred)

                //check if evicted block is dirty
            if(it->valid && it->dirty){        //if block is valid and dirty
                write_backs++;                 //increment write back counter
                if(next_level != nullptr){     //if there is a next level of cache
                    next_level->request('w', (it->tag << (ib + bo)) | (in << (bo))); //send write request to next level of cache
                }
                it->dirty = false;            //set dirty bit to false
            }


            // if(next_level != nullptr){          //if there is a next level of cache
            //     next_level->request('r', addy); //send read request to next level of cache
            // }
            //replace evicted block with new block
            it->tag = tag;                   //set tag of block to new tag
            it->valid = true;                //set valid bit to true
            if(rw == 'r'){                    //if read request
                it->dirty = false;              //set dirty bit to false
            }
            else{                            //if write request
                it->dirty = true;               //set dirty bit to true
            }
            update_order(index, it);              //update LRU by moving block to front of list   
    }

    void stream_store(uint32_t addy, list<stream>::iterator &x, uint32_t size, uint32_t stop){//need address to be stored, which stream to store it in, which index in stream to update until
        if(stream_buffers.size() == 0) return;
        x->valid = true;
        //in stream buffer we will update from the head(previous hit index) to the stop(hit index)
        //The new head will be the index one after the hit index
        //The value that will take the new heads place will be the (address stored in (head-1)) + 1

        //three main cases to watch
        //case 0: head < stop
        if(head < stop){
            for(int i = 0; i < stop; i++){                  //increment through vector starting at the head and moving to the hit index
                prefetches++;
                if(head == 0){
                    x->addys[(head + i) % ss] = x->addys[ss - 1] + 1 + i;             //replace vector value
                }
                else{
                    x->addys[(head + i) % ss] = x->addys[head - 1] + 1 + i;             //replace vector value
                }
            }
            head = (stop + 1) % ss;                                 //set new head to the index after hit index
            return;
        }

        //case 1: head > stop
        

        //case 2: head == stop


        if(!stream_hit){
            prefetches += ss;
            head = 0;
            test++;
            for(uint32_t i = 0; i < ss; i++){
                x->addys[i] = (addy >> BO) + 1 + i;
            }
            if(test < 100){
                printf("stream miss");

                printf("%x \n",addy >> BO);

                for(uint32_t i = 0; i < ss; i++){
                    printf("%x  ", x->addys[(head + i) % ss]);
                }
                printf("\n");
            }
            return;
        }
        head = stop + 1;
        prefetches += head;
        //use (size - head index) to find value that we need to increment from (i.e. head at index 2, size 4, then when we start storing at beginning then we need to store addresses starting from (hit_address +  stop))
        for(int i = 0; i < head; i++){
            x->addys[i] = (addy >> BO) + (size - stop) + i;
        }
        if(test < 100){
            printf("stream hit");
            printf("%x \n",addy >> BO);
            for(uint32_t i = 0; i < ss; i++){
                printf("%x  ", x->addys[(head + i) % ss]);
            }
            printf("\n");
        }

        return;
    }         //need address to be stored, which stream to store it in, which index in stream to update until

    void print_stats(){    //function to print cache statistics    
        double mr;     
        if(level == 1){
            printf("a. L%d reads:                    %d\n", level, reads);                       //print number of reads
            printf("b. L%d read misses:              %d\n", level, read_misses);           //print number of read misses
            printf("c. L%d writes:                   %d\n", level, writes);                     //print number of writes
            printf("d. L%d write misses:             %d\n", level, write_misses);         //print number of write misses
            if(level == 1){
                mr = ((double)read_misses + (double)write_misses) / ((double)reads + (double)writes);
            }
            else{
                mr = ((double)read_misses) / ((double)reads);
            }
            printf("e. L%d miss rate:               %4f\n", level, mr);
            printf("f. L%d write backs:              %d\n",level, write_backs);           //print number of write backs
            printf("g. L%d prefetches:               %d\n", level, prefetches);
        }
        else{
            printf("h. L%d reads (demand):                    %d\n", level, reads);                       //print number of reads
            printf("i. L%d read misses (demand):              %d\n", level, read_misses);           //print number of read misses
            printf("j. L%d reads (prefetch):                   %d\n", level, 0);                     //print number of writes
            printf("k. L%d read misses (prefetch):             %d\n", level, 0);         //print number of write misses
            printf("j. L%d writes (prefetch):                   %d\n", level, writes);                     //print number of writes
            printf("k. L%d write misses (prefetch):             %d\n", level, write_misses);         //print number of write misses
            if(level == 1){
                mr = ((double)read_misses + (double)write_misses) / ((double)reads + (double)writes);
            }
            else{
                mr = ((double)read_misses) / ((double)reads);
            }
            printf("l. L%d miss rate:                %f\n", level, mr);
            printf("m. L%d write backs:              %d\n",level, write_backs);           //print number of write backs
            printf("n. L%d prefetches:               %d\n", level, prefetches);
        }



        if(next_level == nullptr){
                printf("q. Memory traffic:              %d\n", read_misses + write_misses + write_backs); //print total memory traffic
            }
        return;
    }

    void print_contents(){
        printf("===== L%d contents =====\n",level);
        for(uint32_t i = 0; i < sets; i++){              //for each set
            printf("Set %d:  ", i);                  //print set number
            for(auto &b : set_list[i]){             //for each block in set
                if(b.valid){                         //if block is valid
                    printf(" %x", b.tag);           //print tag of block
                    if(b.dirty){                     //if block is dirty
                        printf(" D");                 //print D for dirty
                    }
                    else{                           //if block is not dirty
                        printf("  ");                  //print space for clean block
                    }
                }
                else{                               //if block is not valid
                    printf(" *");                   //print * for invalid block
                }
            }
            printf("\n");                          //new line after each set
        }
        return;
    }

    void print_streams(){
        for(auto it_s = stream_buffers.begin(); it_s != stream_buffers.end(); it_s++){
            if(it_s->valid){
                for(uint32_t i = 0; i < ss; i++){
                    printf("%x  ", it_s->addys[(head + i) % ss]);
                }
            }
            printf("\n");
        }
        return;
    }
};