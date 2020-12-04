// The MIT License (MIT)
//
// Copyright (c) 2020 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_NUM_ARGUMENTS 3

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

struct __attribute__((__packed__)) DirectoryEntry {
  char      DIR_NAME[11];
  uint8_t   DIR_Attr;
  uint8_t   Unused1[8];
  uint16_t  DIR_FirstClusterHigh;
  uint8_t   Unused2[4];
  uint16_t  DIR_FirstClusterLow;
  uint32_t  DIR_FileSize;
};

struct DirectoryEntry dir[16];

//afdsfasddf

//char   BS_OEMName[8];
int16_t  BPB_BytsPerSec;
int8_t   BPB_SecPerClus;
int16_t  BPB_RsvdSecCnt;
int8_t   BPB_NumFATS;
//int16_t BPB_RootEntCnt;
//char BS_VolLab[11];
int32_t  BPB_FATSz32;
//int32_t BPB_RootClus;
int32_t  Dir;

//int32_t RootDirSector = 0;
//int32_t FirstDataSector = 0;
//int32_t FirstSectorofCluster = 0;

int main()
{
  FILE *fp = NULL;
  FILE *filep = NULL;

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str  = strdup( cmd_str );

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your FAT32 functionality

    int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ )
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );
    }

    if(strcmp(token[0],"close") == 0)
    {
      if(filep == NULL)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        fclose(filep);
        filep = NULL;
      }
    }

    else if(strcmp(token[0],"cd")==0)
    {
      if(filep==NULL)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        int i;
        //int old_Dir = Dir;
        fseek(filep,Dir,SEEK_SET);
        fread(dir,16,sizeof(struct DirectoryEntry),filep);
        int a = strlen(token[1]);
        //if(strcmp(token[1],"..")==0)
        //{
            //Dir=old_Dir;
        //}
        //printf("token : %s", token[1]);
        token[1] = TO_UPPER(token[1]);
        //printf("token : %s", token[1]);

        for(i=0;i<16;i++)
        {
          char * name = dir[i].DIR_NAME;
          name[a] = "\0";
          printf("name: %s -- token[1]: %s\n",name,token[1]);
          if(strcmp(token[1],name)==0)
          {
            if(dir[i].DIR_FirstClusterLow==0)
            {
              dir[i].DIR_FirstClusterLow=2;
            }
            Dir = LBAToOffset(dir[i].DIR_FirstClusterLow);
            printf("here\n");
          }
        }
      }
    }

    else if(strcmp(token[0],"ls") == 0)
    {
      if(filep==NULL)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        int i = 0;
        //int DIR = (BPB_NumFATS * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec)

        fseek(filep,Dir,SEEK_SET);
        fread(dir,16,sizeof(struct DirectoryEntry),filep);

        for(i=0;i<16;i++)
        {
          if(dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
          {
            dir[i].DIR_NAME[11] = '\0';
            printf("Filename: %s\n", dir[i].DIR_NAME);
          }
        }
      }
    }

    else if(strcmp(token[0],"stat") == 0)
    {
      if(filep == NULL)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        int i = 0;
        int flag = 0;

        fseek(filep,Dir,SEEK_SET);
        fread(dir,16,sizeof(struct DirectoryEntry),filep);

      }
    }

    free( working_root );

  }
  return 0;
}
