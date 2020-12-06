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

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

//struct for storing directory entry
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

int16_t  BPB_BytsPerSec;
int8_t   BPB_SecPerClus;
int16_t  BPB_RsvdSecCnt;
int8_t   BPB_NumFATS;
int32_t  BPB_FATSz32;
int32_t  Dir;

FILE *fp;
int file_open=0;

int32_t LBAToOffset(int32_t sector)
{
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);
}

int16_t NextLB(int32_t sector)
{
  uint32_t FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) * (sector * 4);
  int16_t val;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&val, 2, 1, fp);
  return val;
}

int compare(char *userString, char *directoryString)
{
  char *dotdot = "..";

  if(strncmp(dotdot,userString,2) == 0)
  {
      if(strncmp(userString, directoryString,2) == 0)
      {
          return 1;
      }
      return 0;
  }

  char IMG_Name[12];

  strncpy(IMG_Name,directoryString,11);
  IMG_Name[11] = '\0';

  char input[11];
  memset(input,0,11);
  strncpy(input,userString, strlen(userString));

  char expanded_name[12];
  memset(expanded_name,' ',12);

  char *token = strtok(input,".");

  strncpy(expanded_name,token,strlen(token));

  token = strtok(NULL,".");
  if(token)
  {
    strncpy((char*)(expanded_name+8),token,strlen(token));
  }

  expanded_name[11] = '\0';

  int i;
  for(i = 0;i<11;i++)
  {
    expanded_name[i] = toupper(expanded_name[i]);
  }

  if(strncmp(expanded_name,IMG_Name,11)==0)
  {
    return 1;
  }

  return 0;
}

#define MAX_NUM_ARGUMENTS 4
#define NUM_ENTRIES 16

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

int bpb()
{

  printf("BPB_BytsPerSec: %d 0x%x\n",BPB_BytsPerSec,BPB_BytsPerSec);
  printf("BPB_SecPerClus: %d 0x%x\n",BPB_SecPerClus,BPB_SecPerClus);
  printf("BPB_RsvdSecCnt: %d 0x%x\n",BPB_RsvdSecCnt,BPB_RsvdSecCnt);
  printf("BPB_NumFATS: %d 0x%x\n",BPB_NumFATS,BPB_NumFATS);
  printf("BPB_FATSz32: %d 0x%x\n",BPB_FATSz32,BPB_FATSz32);

  return 0;
}

int ls( )
{
  int i;

  for(i=0;i<NUM_ENTRIES;i++)
  {
    char filename[12];
    strncpy(filename,dir[i].DIR_NAME,11);
    filename[11]='\0';

    if((dir[i].DIR_Attr == ATTR_READ_ONLY || dir[i].DIR_Attr == ATTR_DIRECTORY || dir[i].DIR_Attr == ATTR_ARCHIVE)
    && filename[0]!=0xffffffe5)
    {
      printf("%s\n",filename);
    }
  }
  return 0;
}

int cd( char *directoryName)
{
  int i;
  int found = 0;

  for(i=0;i<NUM_ENTRIES;i++)
  {
    if(compare(directoryName,dir[i].DIR_NAME))
    {
      int cluster = dir[i].DIR_FirstClusterLow;

      if(cluster==0)
      {
        cluster=2;
      }

      int offset = LBAToOffset(cluster);
      fseek(fp,offset,SEEK_SET);

      fread(dir,sizeof(struct DirectoryEntry),NUM_ENTRIES,fp);
      found = 1;
      break;
    }

    if(!found)
    {
      printf("Error: Directory not found\n");
      return -1;
    }

    return 0;
  }
}

int statFile(char * fileName)
{
  int i = 0;
  int found = 0;
  for (i=0;i<NUM_ENTRIES;i++)
  {
    if(compare(fileName,dir[i].DIR_NAME))
    {
      printf("%s Attr: %d Size: %d Cluster: %d\n", fileName, dir[i].DIR_Attr,dir[1].DIR_FileSize, dir[i].DIR_FirstClusterLow);
      found = 1;
    }
  }
  if(!found)
  {
    printf("Error: File Not Found\n");
  }
  return 0;
}

int getFile(char *originalFilename, char *newFilename)
{
  FILE * ofp;

  if(newFilename == NULL)
  {
    ofp = fopen(originalFilename,"w");
    if(ofp == NULL)
    {
      perror("Error: ");
    }
  }
  else
  {
    ofp = fopen(newFilename,"w");
    if(ofp == NULL)
    {
      perror("Error: ");
    }
  }

  int i;
  int found = 0;


  for(i=0;i<NUM_ENTRIES;i++)
  {
    if(compare(originalFilename,dir[i].DIR_NAME))
    {
      int cluster = dir[i].DIR_FirstClusterLow;

      found = 1;
      int bytesRemainingToRead = dir[i].DIR_FileSize;
      int offset=0;
      unsigned char buffer[512];

      //Middle section of file
      while(bytesRemainingToRead >= BPB_BytsPerSec)
      {
        offset = LBAToOffset(cluster);
        fseek(fp,offset,SEEK_SET);
        fread(buffer,1,BPB_BytsPerSec,fp);
        fwrite(buffer,1,512,ofp);

        cluster = NextLB(cluster);

        bytesRemainingToRead = bytesRemainingToRead - BPB_BytsPerSec;
      }

      //last block
      if(bytesRemainingToRead)
      {
        cluster = NextLB(cluster);
        offset = LBAToOffset(cluster);
        fseek(fp,offset,SEEK_SET);
        fread(buffer,1,bytesRemainingToRead,fp);
        fwrite(buffer,1,bytesRemainingToRead,ofp);
      }

      fclose(ofp);
    }
  }
}

int readFile(char *filename, int requestedOffset, int requestedBytes)
{
  int i;
  int found = 0;
  int bytesRemainingToRead = requestedBytes;

  if(requestedOffset<0)
  {
    printf("Error: Offset can not be less than zero\n");
  }

  for(i=0;i<NUM_ENTRIES;i++)
  {
    if(compare(filename,dir[i].DIR_NAME))
    {
      int cluster = dir[i].DIR_FirstClusterLow;

      found = 1;

      int searchSize = requestedOffset;

      //search for the beginning cluster of the file
      while(searchSize>=BPB_BytsPerSec)
      {
        cluster = NextLB(cluster);
        searchSize = searchSize - BPB_BytsPerSec;
      }

      //Read first block
      int offset = LBAToOffset(cluster);
      int byteOffset = (requestedOffset % BPB_BytsPerSec);
      fseek(fp, offset+byteOffset, SEEK_SET);

      int loopOffset = requestedOffset;

      unsigned char buffer[BPB_BytsPerSec];

      //First part
      //figure out how many bytes in the first block we need to read
      int firstBlockBytes = BPB_BytsPerSec-requestedOffset;
      fread(buffer,1,firstBlockBytes,fp);

      for(i=0;i<firstBlockBytes;i++)
      {
        printf("%x ",buffer[i]);
      }

      bytesRemainingToRead = bytesRemainingToRead - firstBlockBytes;

      //Middle section of file
      while(bytesRemainingToRead >= 512)
      {
        cluster = NextLB(cluster);
        offset = LBAToOffset(cluster);
        fseek(fp,offset,SEEK_SET);
        fread(buffer,1,BPB_BytsPerSec,fp);

        for(i=0;i<BPB_BytsPerSec;i++)
        {
          printf("%x ",buffer[i]);
        }

        bytesRemainingToRead = bytesRemainingToRead - BPB_BytsPerSec;
      }

      //last block
      if(bytesRemainingToRead)
      {
        cluster = NextLB(cluster);
        offset = LBAToOffset(cluster);
        fseek(fp,offset,SEEK_SET);
        fread(buffer,1,bytesRemainingToRead,fp);

        for(i=0;i<bytesRemainingToRead;i++)
        {
          printf("%x ",buffer[i]);
        }
      }

      printf("\n");
    }
  }

  if(!found)
  {
    printf("Error: File not found\n");
    return -1;
  }
  return 0;
}




int main()
{

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

    int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ )
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );
    }

    if(strcmp(token[0],"open")==0)
    {
      fp=fopen(token[1],"r");
      if(fp ==NULL)
      {
        perror("Could not open file. ");
      }
      else
      {
        file_open = 1;
      }

      //read the BPB section
      fseek(fp,11,SEEK_SET);
      fread(&BPB_BytsPerSec,1,2,fp);

      fseek(fp,13,SEEK_SET);
      fread(&BPB_SecPerClus,1,1,fp);

      fseek(fp,14,SEEK_SET);
      fread(&BPB_RsvdSecCnt,1,2,fp);

      fseek(fp,16,SEEK_SET);
      fread(&BPB_NumFATS,1,2,fp);

      fseek(fp,36,SEEK_SET);
      fread(&BPB_FATSz32,1,4,fp);

      // THe root directory address is located past the reseverd sector are and both FATS
      int rootAddress = (BPB_RsvdSecCnt * BPB_BytsPerSec) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytsPerSec);

      fseek(fp,rootAddress,SEEK_SET);
      fread(dir,sizeof(struct DirectoryEntry),16,fp);
    }

    else if(strcmp(token[0],"close") == 0)
    {
      if(file_open)
      {
        fclose(fp);

        //reset file state
        fp=NULL;
        file_open=0;
      }
      else
      {
        printf("Error: File could not open.\n");
      }
    }

    else if(strcmp(token[0],"bpb")==0)
    {
      if(file_open)
      {
        bpb();
      }
      else
      {
        printf("Error: File Image Not Opened\n");
      }
    }

    else if(strcmp(token[0],"ls") == 0)
    {
      if(file_open)
      {
        ls();
      }
      else
      {
        printf("Error: File Image Not Opened\n");
      }
    }

    else if(strcmp(token[0],"cd")==0)
    {
      if(file_open)
      {
        cd(token[1]);
      }
      else
      {
        printf("Error: File Image Not Opened\n");
      }
    }

    else if(strcmp(token[0],"read")==0)
    {
      if(file_open)
      {
        readFile(token[1],atoi(token[2]),atoi(token[3]));
      }
      else
      {
        printf("Error: File Image Not Opened\n");
      }
    }

    else if(strcmp(token[0],"get")==0)
    {
      if(file_open)
      {
        getFile(token[1], token[2]);
      }
      else
      {
        printf("Error: File Image Not Opened\n");
      }
    }

    else if(strcmp(token[0],"stat") == 0)
    {
      if(file_open)
      {
        statFile(token[1]);
      }
      else
      {
        printf("Error: File Image Not Opened\n");
      }
    }

    free( working_root );

  }
  return 0;
}
