#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/wait.h>
#include "ui.h"
#include "compression.h"

int makeMask(int leftoverBits) {
   int i =0;
   int mask = 1;
   for (i = 0; i < leftoverBits; i++) 
      mask = mask*2+1;
   return mask;
}

void outputByte(Data *data) {
   int output = 0;
   int numLOB = data->curEncodeSize-(8-data->leftoverBits);
   int mask = makeMask(numLOB);
   output = ((data->leftoverByte) << (8-data->leftoverBits)) 
      | (data->curNode >> numLOB);
   fputc(output, data->out);
   data->leftoverByte = mask & data->curNode;
   data->leftoverBits = numLOB;

   if (data->leftoverBits >= 8) {
      numLOB = data->leftoverBits-8;
      mask = makeMask(data->curEncodeSize-(data->leftoverBits-8));
      output = ((data->leftoverByte)>>numLOB);
      fputc(output, data->out);
      data->leftoverByte = mask & data->leftoverByte;
      data->leftoverBits = numLOB;
   }
}

void resizeTrie(Data *data) {
   
   (data->curEncodeSize)++;
   data->curEncodeMax = 1 << data->curEncodeSize;
   if (data->memSize < data->r) {
      data->dictionary = 
         realloc(data->dictionary, sizeof(TrieNode)*(data->curEncodeMax));

      memset(data->dictionary+(data->curEncodeMax / 2), 
         0, (data->curEncodeMax / 2) * sizeof(TrieNode));
      data->memSize = data->curEncodeSize;
   }
   if (data->curByte != EOF) 
      data->dictionary[data->curNode].child[data->curByte] = (data->dictSize)++;
}

TrieNode* addToTrie(Data *data) {

   if (data->dictSize < data->curEncodeMax) {
      if (data->curByte != EOF)
         data->dictionary[data->curNode].child[data->curByte] 
            = (data->dictSize)++;
   } 
   else if (data->curEncodeSize < data->maxEncodeSize) 
      resizeTrie(data);
   else {
      memset(data->dictionary, 0, data->curEncodeMax * sizeof(TrieNode));
      data->curEncodeSize = 9;
      data->curEncodeMax = 1 << data->curEncodeSize;
      data->dictSize = 257;
   }
   return data->dictionary;
}

int checkEOF(Data *data) {
   if (data->curByte == EOF) {
      outputByte(data);
      
      data->dictionary = addToTrie(data);
      data->curNode = data->EOD;
      outputByte(data);
      data->curNode = 0;
      if (data->leftoverBits > 0) 
         fputc(((data->leftoverByte) << (8-data->leftoverBits)), data->out);
      
      return 1;
   }
   return 0;
}

int addNode(Data * data) {
   data->nextNode = data->dictionary[data->curNode].child[data->curByte];
   if (data->nextNode != 0) {
      data->curNode = data->nextNode;
      return 1;
   }
   return 0;
}

void initData(Data *data, FILE *in, FILE *out, int r) {
   data->in = in;
   data->out = out;
   data->r = r;
   data->dictionary = NULL;
   data->dictSize = 257;
   data->nextByte = fgetc(data->in);
   data->curNode  = data->nextByte;
   data->leftoverBits = 0;
   data->leftoverByte = 0;

   data->nextNode = 0;
   data->curByte = 0;

   data->EOD = 256;

   data->maxEncodeSize = r;
   data->curEncodeSize = 9;
   data->curEncodeMax = 1 << data->curEncodeSize;

   data->memSize = data->curEncodeSize;

}

void compress(FILE *in, FILE *out, int r) {
   Data data;

   initData(&data, in, out, r);

   if (data.nextByte == EOF)
      return;

   data.dictionary = calloc(data.curEncodeMax, sizeof(TrieNode));

   fwrite(&(data.r), 1, 1, data.out);
   do {
      data.curByte = fgetc(data.in);

      if (checkEOF(&data) == 1)
         break;

      if (addNode(&data) == 1)
         continue;

      outputByte(&data);

      data.dictionary = addToTrie(&data);

      data.curNode = data.curByte;
   } while (1);

   free(data.dictionary);
}