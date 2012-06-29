/******************************************************
Functions to work with data_file_si structure
*******************************************************/
#include "main.h"
#define MAXBLOCKSIZE 50000000 // 50MB to work with many files at same time

int init_data_file_si(struct data_file_si *df,const char *file_name, int num_channels)
{
  /*function to initialise the data_file_si structure so that it can
   read a .dat file.
   The .dat file will remain open as long as clean_data_file_si is not called.
   This is done to speed up file operation. I will test what is the gain.
*/
  df->file_name=NULL;
  df->data_block=NULL;
  // allocate memory for df->file_name
  if((df->file_name=(char *)malloc(strlen(file_name)))==NULL)
    {
      fprintf(stderr,"init_data_file_si(): problem allocating memory for file_name\n");
      return 1;
    }
  strcpy(df->file_name,file_name);  // copy the file_name
  df->num_channels=num_channels; // get number of channels
  if (df->num_channels<=0)
    {
      fprintf(stderr,"init_data_file_si(): number of channels is 0 or less\n");
      return 1;
    }
  
  // give a warning if size of short is not 2 bytes on this system
  if (sizeof(short int)!=2)
    {
      fprintf(stderr,"init_data_file_si(): sizeof(short int) on this system is not 2\n");
      fprintf(stderr,"that might cause problem in reading/writing dat files\n");
    }
  
  // try to open the file
  if((df->file_descriptor=open(file_name,O_RDONLY,0))==-1)
    {
      fprintf(stderr,"init_data_file_si(): problem opening %s\n",file_name);
      return 1;
    }
  if((df->file_size=lseek(df->file_descriptor,0L,2))==-1)
    {
      fprintf(stderr,"init_data_file_si(): problem getting file size\n");
      return 1;
    }
  if(lseek(df->file_descriptor,0L,0)==-1)
    {
      fprintf(stderr,"init_data_file_si(): problem moving in the file\n");
      return 1;
    }
  if(df->file_size%(df->num_channels*sizeof(short))!=0)
    {
      fprintf(stderr,"init_data_file_si(): problem with the size of the file\n");
      fprintf(stderr,"the size should divide by num_channel*sizeof(short):%d, but is %ld\n",df->num_channels*(int)sizeof(short),(long int)df->file_size);
      return 1;
    }
  df->num_samples_in_file=df->file_size/(df->num_channels*sizeof(short));

  // calculate the size of a data_block
  df->num_samples_in_complete_block=MAXBLOCKSIZE/(df->num_channels*sizeof(short));
  df->block_size=df->num_samples_in_complete_block*(df->num_channels*sizeof(short));
  // allocate memory for data_block
  if((df->data_block=(short *)malloc(df->block_size))==NULL)
    {
      fprintf(stderr,"init_data_file_si(): problem allocating memory for data_block\n");
      return 1;
    }
  return 0;
}
int clean_data_file_si(struct data_file_si *df)
{
  /* function to free memory and close a .dat file after reading it.
   */
  //  fprintf(stderr,"clean_data_file_si()\n");
  // free memory
  if (df->file_name!=NULL)
    free(df->file_name);
  if (df->data_block!=NULL)
    free(df->data_block);
  
  // try to close the file
  if(close(df->file_descriptor)==-1)
    {
      fprintf(stderr,"clean_data_file_si(): problem closing file descriptor %d\n",df->file_descriptor);
      return 1;
    }
  return 0;
}
int data_file_si_load_block(struct data_file_si* df, long int start_index, long int size)
{
  /* Function to read a data_block.
     Store the data in data_block member of the data_file_si structure
     start_index is in bytes from beginning of file

     assumes that the file is already open
  */
  if (size>df->block_size)
    {
      fprintf(stderr,"data_file_si_load_block(): size is larger than block_size, %ld\n",size);
      return 1;
    }
  if ( start_index < 0)
    {
      fprintf(stderr,"data_file_si_load_block(): start index < 0, %ld\n",start_index);
      return 1;
    }
  if (start_index + size > df->file_size)
    {
      fprintf(stderr,"data_file_si_load_block(): start_index+size > file_size\n");
      return 1;
    }
  if(lseek(df->file_descriptor,start_index,SEEK_SET)==-1)
    {
      fprintf(stderr,"data_file_si_load_block(): problem with lseek\n");
      return 1;
    }
  if(read(df->file_descriptor,df->data_block,size)==-1)
    {
      fprintf(stderr,"data_file_si_load_block(): problem reading the file\n");
      return 1;
    }
  return 0;
}
int data_file_si_get_data_one_channel(struct data_file_si* df, int channel_no, short int* one_channel, long int start_index, long int end_index)
{
  /*function to read the data from one channel
    the index as parameters are in sample number
*/
  

  if(channel_no < 0)
    {
      fprintf(stderr,"data_file_si_get_data_one_channel(): channel_no < 0\n");
      return 1;
    }
  if(channel_no >= df->num_channels)
    {
      fprintf(stderr,"data_file_si_get_data_one_channel(): channel_no >= num_channels\n");
      return 1;
    }
  if (start_index<0)
    {
      fprintf(stderr,"data_file_si_get_data_one_channel(): start_index < 0\n");
      return 1;
    }
  if (start_index>df->num_samples_in_file)
    {
      fprintf(stderr,"data_file_si_get_data_one_channel(): start_index > num_samples\n");
      return 1;
    }
  if(end_index<=start_index)
    {
      fprintf(stderr,"data_file_si_get_data_one_channel(): start_index <= end_index\n");
      return 1;
    }
  if(end_index>df->num_samples_in_file)
    {
      fprintf(stderr,"data_file_si_get_data_one_channel(): end_index > num_samples\n");
      return 1;
    }
  int num_samples_to_read=end_index-start_index;
  int num_complete_blocks_to_read=num_samples_to_read/df->num_samples_in_complete_block;
  int num_blocks_to_read;
  int num_samples_incomplete_block=num_samples_to_read%df->num_samples_in_complete_block;
  long int i,j,index;
  long int start_index_bytes;
  index=0;
  if(num_samples_incomplete_block>0)
    num_blocks_to_read=num_complete_blocks_to_read+1;
  else
    num_blocks_to_read=num_complete_blocks_to_read;
  
  for (i = 0; i < num_blocks_to_read; i++)
    {
      start_index_bytes=(start_index*sizeof(short)*df->num_channels)+(df->block_size*i);
      if(i<num_complete_blocks_to_read) // complete block
	{
	  if(data_file_si_load_block(df,start_index_bytes,df->block_size)!=0)
	    {
	      fprintf(stderr,"data_file_si_get_data_one_channel(): problem loading block\n");
	      fprintf(stderr,"data_file_si_load_block(file,%ld,%d)\n",start_index_bytes,df->block_size);
	      return 1;
	    }
	  for (j = 0; j <  df->num_samples_in_complete_block; j++)
	    {
	      one_channel[index]=df->data_block[(j*df->num_channels)+(channel_no)];
	      index++;
	    }
	}
      if(i==num_complete_blocks_to_read) // smaller and last block
	{
	  if(data_file_si_load_block(df,start_index_bytes,num_samples_incomplete_block*sizeof(short)*df->num_channels))
	    {
	      fprintf(stderr,"data_file_si_get_data_one_channel(): problem loading last block\n");
	      return 1;
	    }
	  for (j = 0; j < num_samples_incomplete_block; j++)
	    {
	      one_channel[index]=df->data_block[(j*df->num_channels)+(channel_no)];
	      index++;
	    }
	}
    }
  return 0;
}
int data_file_si_get_data_all_channels(struct data_file_si* df, short int* data, long int start_index, long int end_index)
{
  /*function to read the data from one channel
    the index as parameters are in sample number
*/
  if (start_index<0)
    {
      fprintf(stderr,"data_file_si_get_data_all_channels(): start_index < 0\n");
      return 1;
    }
  if (start_index>df->num_samples_in_file)
    {
      fprintf(stderr,"data_file_si_get_data_all_channels(): start_index > num_samples\n");
      return 1;
    }
  if(end_index<=start_index)
    {
      fprintf(stderr,"data_file_si_get_data_all_channels(): start_index <= end_index\n");
      return 1;
    }
  if(end_index>df->num_samples_in_file)
    {
      fprintf(stderr,"data_file_si_get_data_all_channels(): end_index > num_samples\n");
      return 1;
    }
  int num_samples_to_read=end_index-start_index;
  int num_complete_blocks_to_read=num_samples_to_read/df->num_samples_in_complete_block;
  int num_blocks_to_read;
  int num_samples_incomplete_block=num_samples_to_read%df->num_samples_in_complete_block;
  int i,j,index;
  int start_index_bytes;
  index=0;
  if(num_samples_incomplete_block>0)
    num_blocks_to_read=num_complete_blocks_to_read+1;
  else
    num_blocks_to_read=num_complete_blocks_to_read;
  
  for (i = 0; i < num_blocks_to_read; i++)
    {
      start_index_bytes=(start_index*sizeof(short)*df->num_channels)+(df->block_size*i);
      if(i<num_complete_blocks_to_read) // complete block
	{
	  if(data_file_si_load_block(df,start_index_bytes,df->block_size)!=0)
	    {
	      fprintf(stderr,"data_file_si_get_data_all_channels(): problem loading block\n");
	      return 1;
	    }
	  for (j = 0; j < df->block_size/(int)sizeof(short); j++)
	    {
	      data[index]=df->data_block[j];
	      index++;
	    }
	}
      if(i==num_complete_blocks_to_read) // smaller and last block
	{
	  if(data_file_si_load_block(df,start_index_bytes,num_samples_incomplete_block*sizeof(short)*df->num_channels))
	    {
	      fprintf(stderr,"data_file_si_get_data_all_channels(): problem loading last block\n");
	      return 1;
	    }
	  for (j = 0; j < num_samples_incomplete_block*df->num_channels; j++)
	    {
	      data[index]=df->data_block[j];
	      index++;
	    }
	}
    }
  return 0;
}
int data_file_si_cut_data_file(struct data_file_si* df, char* new_file_name,long int start_index, long int end_index)
{
  // function to make a dat file shorter
  // same as data_file_si_get_data_all_channels but will save data in a file instead of reading
  if (start_index<0)
    {
      fprintf(stderr,"data_file_si_get_data_all_channels(): start_index < 0\n");
      return 1;
    }
  if (start_index>df->num_samples_in_file)
    {
      fprintf(stderr,"data_file_si_get_data_all_channels(): start_index > num_samples\n");
      return 1;
    }
  if(end_index<=start_index)
    {
      fprintf(stderr,"data_file_si_get_data_all_channels(): start_index <= end_index\n");
      return 1;
    }
  if(end_index>df->num_samples_in_file)
    {
      fprintf(stderr,"data_file_si_get_data_all_channels(): end_index > num_samples\n");
      return 1;
    }
  
  int file_descriptor; // to save the data
  int file_error;
  if ((file_descriptor = open(new_file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP| S_IWGRP | S_IROTH)) == -1)
    {
      fprintf(stderr,"init_data_file_si(): problem opening %s\n",new_file_name);
      return 1;
    }
  int num_samples_to_read=end_index-start_index;
  int num_complete_blocks_to_read=num_samples_to_read/df->num_samples_in_complete_block;
  int num_blocks_to_read;
  int num_samples_incomplete_block=num_samples_to_read%df->num_samples_in_complete_block;
  int i;
  int start_index_bytes;
  if(num_samples_incomplete_block>0)
    num_blocks_to_read=num_complete_blocks_to_read+1;
  else
    num_blocks_to_read=num_complete_blocks_to_read;
  for (i = 0; i < num_blocks_to_read; i++)
    {
      start_index_bytes=(start_index*sizeof(short)*df->num_channels)+(df->block_size*i);
      if(i<num_complete_blocks_to_read) // complete block
	{
	  if(data_file_si_load_block(df,start_index_bytes,df->block_size)!=0)
	    {
	      fprintf(stderr,"data_file_si_get_data_all_channels(): problem loading block\n");
	      return 1;
	    }
	  // save the data in a file
	  if ((file_error=write(file_descriptor,df->data_block,df->block_size))<0)
	    {
	      fprintf(stderr, "data_file_si_cut_data_file() : problem writing data to %s, %d\n",new_file_name,file_error);
	      return 1;
	    }
	}
      if(i==num_complete_blocks_to_read) // smaller and last block
	{
	  if(data_file_si_load_block(df,start_index_bytes,num_samples_incomplete_block*sizeof(short)*df->num_channels))
	    {
	      fprintf(stderr,"data_file_si_get_data_all_channels(): problem loading last block\n");
	      return 1;
	    }
	  // save the data in a file
	  if ((file_error=write(file_descriptor,df->data_block,num_samples_incomplete_block*sizeof(short)*df->num_channels))<0)
	    {
	      fprintf(stderr, "data_file_si_cut_data_file() : problem writing data to %s, %d\n",new_file_name,file_error);
	      return 1;
	    }
	}
    }
  return 0;
}
