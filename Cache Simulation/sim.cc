#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"
#include "cache.h"


   // Example:
   // ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt;
   //  argc = 9
   //  argv[0] = "./sim"
   //  argv[1] = "32"
   //  argv[2] = "8192"
   //  argv[3] = "4"
   //  argv[4] = "262144"
   // argv[5] = "8"
   // argv[6] = "3"
   // argv[7] = "10"
   // argv[8] = "gcc_trace.txt"   

   int argc = 9; 
   char *argv[9];

int main (int argc, char *argv[]) {
   FILE *fp;			// File pointer.
   char *trace_file;		// This variable holds the trace file name.
   cache_params_t params;	// Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;			// This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;		// This variable holds the request's address obtained from the trace.
				// The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
    
   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];

   //instantiate caches here
   cache* L2 = nullptr;
   cache* L1 = nullptr;
   if(params.L2_SIZE < 1){
      L1 = new cache(params.L1_SIZE, params.L1_ASSOC, params.BLOCKSIZE, nullptr, 1, params.PREF_N, params.PREF_M);
      //printf("no L2 cache\n");
   } 
   else {
      L2 = new cache(params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE, nullptr, 2, params.PREF_N, params.PREF_M);
      L1 = new cache(params.L1_SIZE, params.L1_ASSOC, params.BLOCKSIZE, L2, 1);
      //printf("L2 cache instantiated\n");
   }


      
   //instantiate caches here

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }
    
   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");

   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
   //    if (rw == 'r')
   //       printf("r %x\n", addr);
   //    else if (rw == 'w')
   //       printf("w %x\n", addr);
   //    else {
   //       printf("Error: Unknown request type %c.\n", rw);
	//  exit(EXIT_FAILURE);
   //    }

      ///////////////////////////////////////////////////////
      // Issue the request to the L1 cache instance here.
      
      L1->request(rw, addr);
      ///////////////////////////////////////////////////////
   }
      L1->print_contents();
      if(L1->next_level != nullptr){
         printf("\n");
         L2->print_contents();
      }
      if((L1->next_level == nullptr) && (params.PREF_M > 0)){
         printf("\n=====Stream Buffer(s) Contents=====\n");
         L1->print_streams();
      }
      else if(params.PREF_M > 0){
         printf("\n=====Stream Buffer(s) Contents=====\n");
         L2->print_streams();
      }

      printf("\n=====Measurements=====\n");
      double mr;
      printf("a. L%d reads:                    %d\n", L1->level, L1->reads);                       //print number of reads
      printf("b. L%d read misses:              %d\n", L1->level, L1->read_misses);           //print number of read misses
      printf("c. L%d writes:                   %d\n", L1->level, L1->writes);                     //print number of writes
      printf("d. L%d write misses:             %d\n", L1->level, L1->write_misses);         //print number of write misses
      if(L1->next_level == nullptr){
            mr = ((double)L1->read_misses + (double)L1->write_misses) / ((double)L1->reads + (double)L1->writes);
      }
      else{
            mr = ((double)L1->read_misses + (double)L1->write_misses) / ((double)L1->reads + (double)L1->writes);
      }
      printf("e. L%d miss rate:                %.4f\n", L1->level, mr);
      printf("f. L%d writebacks:              %d\n",L1->level, L1->write_backs);           //print number of write backs
      printf("g. L%d prefetches:               %d\n", L1->level, L1->prefetches);

      if(L1->next_level != nullptr){
         printf("h. L2 reads (demand):         %d\n",  L2->reads);                       //print number of reads
         printf("i. L2 read misses (demand):   %d\n",  L2->read_misses);           //print number of read misses
         printf("j. L2 reads (prefetch):          0\n");
         printf("k. L2 read misses (prefetch):    0\n");
         printf("l. L2 writes:                 %d\n", L2->writes);                     //print number of writes
         printf("m. L2 write misses:           %d\n", L2->write_misses);         //print number of write misses
         if(L1->next_level == nullptr){
               mr = ((double)L2->read_misses + (double)L2->write_misses) / ((double)L2->reads + (double)L2->writes);
         }
         else{
               mr = ((double)L2->read_misses) / ((double)L2->reads);
         }
         printf("n. L2 miss rate:              %.4f\n",  mr);
         printf("o. L2 writebacks:             %d\n", L2->write_backs);           //print number of write backs
         printf("p. L2 prefetches:              %d\n",L2->prefetches);
      }
      else{
         printf("h. L2 reads (demand):          %d\n",  0);                       //print number of reads
         printf("i. L2 read misses (demand):    %d\n", 0);           //print number of read misses
         printf("j. L2 reads (prefetch):        %d\n",  0);                       //print number of reads
         printf("k. L2 read misses (prefetch):  %d\n", 0);  
         printf("l. L2 writes:                  %d\n", 0);                     //print number of writes
         printf("m. L2 write misses:            %d\n", 0);         //print number of write misses
         if(L1->next_level == nullptr){
               mr = ((double)L2->read_misses + (double)L2->write_misses) / ((double)L2->reads + (double)L2->writes);
         }
         else{
               mr = ((double)L2->read_misses) / ((double)L2->reads);
         }
         printf("n. L2 miss rate:               %.4f\n", 0.0000);
         printf("o. L2 write backs:             %d\n", 0);           //print number of write backs
         printf("p. L2 prefetches:              %d\n", 0);
      }

        if(L1->next_level == nullptr){
            printf("q. Memory traffic:              %d\n", L1->read_misses + L1->write_misses + L1->write_backs); //print total memory traffic
         }
         else{
            printf("q. Memory traffic:              %d\n", L2->read_misses + L2->write_misses + L2->write_backs); //print total memory traffic
         }
         return(0);
    }