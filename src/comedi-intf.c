/*
 * Copyright (C) 2010 Kevin Allen
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "comedi-intf.h"

#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "timespec-utils.h"

int comedi_interface_init(struct comedi_interface* com)
{
  // returns -1 if nothing is working
  // returns 0 if all ok
  // returns 1 if minor error
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_init\n");
#endif
  int i;
  
#ifdef DEBUG_ACQ
  fprintf(stderr,"default sampling rate=%d\n",com->sampling_rate);
#endif

  com->buffer_size= MAX_BUFFER_LENGTH*COMEDI_INTERFACE_MAX_DEVICES*COMEDI_INTERFACE_TO_DEVICE_BUFFER_SIZE_RATIO;
#ifdef DEBUG_ACQ
  fprintf(stderr,"com->buffer_size=%d\n",com->buffer_size);
#endif
  
  if((com->buffer_data=malloc(sizeof(short int)*com->buffer_size))==NULL)
    {
      fprintf(stderr,"problem allocating memory for com->buffer_data\n");
      return -1;
    }

  com->number_devices=0;
  com->is_acquiring=0;
  com->command_running=0;
  com->data_offset_to_dat=-32768;
  com->number_samples_read=0;
  com->sample_no_to_add=0;
  com->inter_acquisition_sleep_ms=COMEDI_INTERFACE_ACQUISITION_SLEEP_TIME_MS;
  com->inter_acquisition_sleep_timespec=set_timespec_from_ms(com->inter_acquisition_sleep_ms);
  com->pause_restart_acquisition_thread_ms=100;
  com->timespec_pause_restat_acquisition_thread=set_timespec_from_ms(com->pause_restart_acquisition_thread_ms);
  
  if(comedi_dev_init(&com->dev[0],"/dev/comedi0")!=0)
    {
      fprintf(stderr,"do not have a valid device to work with\n");
      return -1;
    }
  // device 0 is working so add one to num_device
  com->number_devices++;

  if(COMEDI_INTERFACE_MAX_DEVICES>1)
    {
      // try a second device.. this could be a loop to accomodate more than 2 devices.
      if ((comedi_dev_init(&com->dev[1],"/dev/comedi1"))==0)
	{
	  
	  // check if the two devices have the same drivers  
	  if((strcmp(com->dev[0].driver,com->dev[1].driver) !=0))
	    {
	      fprintf(stderr,"The two devices on the computer have different name or use different drivers\n");
	      fprintf(stderr,"device names: %s and %s\n driver names %s and %s",com->dev[0].name,com->dev[1].name,com->dev[0].driver,com->dev[1].driver);
	    }
	  else
	    { 
	      // we got all the information from device 2, all seems to work so add 1 to device counter
	      com->number_devices++;
	    }
	}
    }
  // get the total number of channels available
  com->number_channels=0;
  for (i=0;i<com->number_devices;i++)
    {
      com->number_channels+=com->dev[i].number_sampled_channels;
    }
  
  comedi_interface_set_channel_list(com);
  com->max_number_samples_in_buffer=com->buffer_size/com->number_channels;
  
  if((comedi_interface_set_sampling_rate(com,DEFAULT_SAMPLING_RATE))!=0)
    {
      fprintf(stderr,"Problem setting sampling rate in comedi_interface_init()\n");
      return -1;
    }

  comedi_interface_build_command(com);
    
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_init, number_channels: %d\n",com->number_channels);
  fprintf(stderr,"comedi_interface_init, com->buffer_size: %d  com->max_number_samples_in_buffer:%d\n",com->buffer_size,com->max_number_samples_in_buffer);
#endif
  return 0;
}

int comedi_interface_free(struct comedi_interface* com)
{
  // this function is called when the kacq is close
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_free\n");
#endif
  int i;
  free(com->buffer_data);
  for (i=0;i<com->number_devices;i++)
    {
      comedi_dev_free(&com->dev[i]);
    }
  return 0;
}

int comedi_interface_set_inter_acquisition_sleeping_time(struct comedi_interface* com, double ms)
{
  com->inter_acquisition_sleep_ms=ms;
  com->inter_acquisition_sleep_timespec=set_timespec_from_ms(com->inter_acquisition_sleep_ms);
  return 0;
}

int comedi_dev_init(struct comedi_dev *dev, char* file_name)
{
  // return 0 if all ok, -1 if problem
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_dev_inti for %s\n",file_name);
#endif

  int i;
  
  dev->file_name=file_name; // file to interface device 
  // try to open file
  if((dev->comedi_dev=comedi_open(dev->file_name))==NULL)
    {
      // that is serious problem because we need at least one working device
      fprintf(stderr, "error opening %s \n",dev->file_name);
      fprintf(stderr, "check that %s exists and that you have rw permission\n",dev->file_name);
      return -1;
    }
  // this device is valid so get the information about it structure
  dev->is_acquiring=0;
  dev->cumulative_samples_read=0;
  dev->samples_read=0;
  dev->aref=AREF_GROUND; // to use ground as reference???
  dev->buffer_size=MAX_BUFFER_LENGTH;
  if((dev->name=comedi_get_board_name(dev->comedi_dev))==NULL)
    {
      fprintf(stderr, "problem getting the board name for %s\n", dev->file_name);
      return -1;
    }
  if((dev->driver=comedi_get_driver_name(dev->comedi_dev))==NULL)
    {
      fprintf(stderr, "problem getting the driver name for %s\n", dev->file_name);
      return -1;
    }
  if((dev->number_of_subdevices=comedi_get_n_subdevices(dev->comedi_dev))==-1)
    {
      fprintf(stderr, "problem getting the number of subdevices for %s\n", dev->file_name);
      return -1;
    }
  
  ////////////////////////////////////////////////
  //  get info on analog input    //
  ///////////////////////////////////////////////

  if((dev->subdevice_analog_input=comedi_find_subdevice_by_type(dev->comedi_dev,COMEDI_SUBD_AI,0))==-1)
    {
      fprintf(stderr, "problem finding a COMEDI_SUBD_AI subdevice on %s\n", dev->file_name);
      return -1;
    }
  if((dev->number_channels_analog_input=comedi_get_n_channels(dev->comedi_dev,dev->subdevice_analog_input))==-1)
    {
      fprintf(stderr, "problem getting the number of analog input channels on %s\n", dev->file_name);
      return -1;
    }
  if((dev->maxdata_input=comedi_get_maxdata(dev->comedi_dev,dev->subdevice_analog_input,0))==-1)
    {
      fprintf(stderr, "problem getting the maxdata from subdevice on %s\n", dev->file_name);
      return -1;
    }
  if((dev->number_ranges=comedi_get_n_ranges(dev->comedi_dev,dev->subdevice_analog_input,0))==-1)
    {
      fprintf(stderr, "problem getting the number of ranges on %s\n", dev->file_name);
      return -1;
    }
  if((dev->range_input=malloc(sizeof(comedi_range*)*dev->number_ranges))==NULL)
    {
      fprintf(stderr, "problem allocating memory for dev->range_input\n");
      return -1;
    }
  for (i=0; i < dev->number_ranges;i++)
    {
      dev->range_input[i]=comedi_get_range(dev->comedi_dev,dev->subdevice_analog_input,0,i);
      if(dev->range_input[i]==NULL)
	{
	  fprintf(stderr, "problem with comedi_get_range for range %d on %s\n",i, dev->file_name);
	  return -1;
	}
    }
  if(dev->number_ranges>1)
    {
      dev->range=1;
    }
  else
    {
      dev->range=0;
    }
#ifdef DEBUG_ACQ
  fprintf(stderr, "range %d, min: %lf, max: %lf, unit: %u\n",dev->range, dev->range_input[dev->range]->min,dev->range_input[dev->range]->max,dev->range_input[dev->range]->unit);
#endif
  
  // check if number of analog channels is not more than the number of channels supported
  if (dev->number_channels_analog_input>COMEDI_DEVICE_MAX_CHANNELS)
    {
      fprintf(stderr,"dev->number_channels_analog_input (%d) is larger than COMEDI_DEVICE_MAX_CHANNELS(%d)\n",dev->number_channels_analog_input,COMEDI_DEVICE_MAX_CHANNELS);
      return -1;
    }
  // by default, sample each channel once per samping event
  dev->number_sampled_channels=dev->number_channels_analog_input;
  // by default, read all channels once
  for (i=0; i < dev->number_sampled_channels; i++)
    {
      dev->channel_list[i]=i;
    }
  dev->data_point_out_of_samples=0;
  
  ////////////////////////////////////////////////
  //  get info on analog output //
  ///////////////////////////////////////////////
  if((dev->subdevice_analog_output=comedi_find_subdevice_by_type(dev->comedi_dev,COMEDI_SUBD_AO,0))==-1)
    {
      fprintf(stderr, "problem finding a COMEDI_SUBD_AO subdevice on %s\n", dev->file_name);
      fprintf(stderr, "this is not a fatal error\n");
      dev->number_channels_analog_output=-1;
      dev->maxdata_output=-1;
    }
  else
    {
      if((dev->number_channels_analog_output=comedi_get_n_channels(dev->comedi_dev,dev->subdevice_analog_output))==-1)
	{
	  fprintf(stderr, "problem getting the number of channels on subdevice analog_output\n");
	  return 1;
	}
      
      if((dev->number_ranges_output=comedi_get_n_ranges(dev->comedi_dev,dev->subdevice_analog_output,0))==-1)
	{
	  fprintf(stderr, "problem getting the number of ranges of analog output on %s\n", dev->file_name);
	  return -1;
	}

      if((dev->range_output_array=malloc(sizeof(comedi_range*)*dev->number_ranges_output))==NULL)
	{
	  fprintf(stderr, "problem allocating memory for dev->range_output\n");
	  return -1;
	}
      for (i=0; i < dev->number_ranges_output;i++)
	{
	  dev->range_output_array[i]=comedi_get_range(dev->comedi_dev,dev->subdevice_analog_output,0,i);
	  if(dev->range_output_array[i]==NULL)
	    {
	      fprintf(stderr, "problem with comedi_get_range for range for output %d on %s\n",i, dev->file_name);
	      return -1;
	    }
	}
      if(dev->number_ranges_output>1)
	{
	  dev->range_output=0;
	}
      else
	{
	  dev->range_output=0;
	}
      
      if((dev->maxdata_output=comedi_get_maxdata(dev->comedi_dev,dev->subdevice_analog_output,dev->range_output))==-1)
	{
	  fprintf(stderr, "problem with comedi_get_maxdata(dev->comedi_dev,dev->subdevice_analog_output,0\n");
	  return 1;
	}
      
      if(isnan(dev->voltage_max_output=comedi_to_phys(dev->maxdata_output-1,
						      dev->range_output_array[dev->range_output], 
						      dev->maxdata_output))!=0) 
	{
	  fprintf(stderr, "problem with comedi_to_phys(dev->maxdata_output-1,dev->range_output_array[dev->range_output],dev->maxdata_output)\n");
	  return 1;
	}
    }
  return 0;
}
int comedi_dev_free(struct comedi_dev *dev)
{
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_dev_free\n");
#endif
  // close the device
  if((comedi_close(dev->comedi_dev))==-1)
    {
      fprintf(stderr, "error closing device %s \n",dev->file_name);
      return -1;
    }
  // free memory
  free(dev->range_input);
  free(dev->range_output_array);
  return 0;
}
int comedi_interface_set_channel_list(struct comedi_interface* com)
{
  // set the channel number on the user side
  // for first device, names are the same
  // for second device, names are value+dev[0]->number_channels_analog_input;
  // etc.
  
  int cumul=0;
  int i,j;
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_set_channel_list()\n");
#endif
  com->number_channels=0;
  for (i=0; i < com->number_devices;i++)
    {
      for (j=0; j < com->dev[i].number_sampled_channels; j++)
	{
	  com->channel_list[cumul+j]=com->dev[i].channel_list[j];
	}
      cumul=com->dev[i].number_channels_analog_input;
      com->number_channels+=com->dev[i].number_sampled_channels;
    }
  return 0;
}

int comedi_interface_build_command(struct comedi_interface* com)
{
  int i,j,r;
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_build_command()\n");
#endif

  // if only one device, configure to get all channels and start when command is run
  if (com->number_devices==1)
    {
#ifdef DEBUG_ACQ
      fprintf(stderr,"building command for single device\n");
#endif
      
      for (i=0; i < com->number_devices;i++)
	{
	  r = comedi_get_cmd_generic_timed(com->dev[i].comedi_dev,
					   com->dev[i].subdevice_analog_input,
					   &com->dev[i].command,
					   com->dev[i].number_sampled_channels,
					   (int)(1e9/com->sampling_rate));
	  if(r<0)
	    {
	      printf("comedi_get_cmd_generic_timed failed in comedi_interface_build_command()\n");
	      printf("%s, %d Hz\n",com->dev[i].file_name,com->sampling_rate);
	      return 1;
	    }
	}
 
      i=0;
      /* Modify parts of the command */
      // cmd[devNo]->chanlist           = chanlist[devNo];
      //cmd[devNo]->chanlist_len       = channels_in_use;
      //cmd[devNo]->scan_end_arg = channels_in_use;
      //cmd[devNo]->stop_src=TRIG_NONE;
      //cmd[devNo]->stop_arg=0;
      //      com->dev[i].command.subdev         = com->dev[i].subdevice_analog_input;
      //      com->dev[i].command.flags          = 0;
      com->dev[i].command.start_src      = TRIG_NOW;//  event to make the acquisition start
      com->dev[i].command.start_arg      = 0;
      com->dev[i].command.scan_begin_src = TRIG_TIMER;
      // com->dev[i].command.scan_begin_arg = 1e9/com->sampling_rate;
      com->dev[i].command.convert_src    = TRIG_TIMER;
      // com->dev[i].command.convert_arg    = 0;
      com->dev[i].command.scan_end_src   = TRIG_COUNT;
      // com->dev[i].command.scan_end_arg   = com->dev[i].number_sampled_channels;
      com->dev[i].command.stop_src       = TRIG_NONE;
      com->dev[i].command.stop_arg       = 0;
      com->dev[i].command.chanlist       = com->dev[i].channel_list;
      com->dev[i].command.chanlist_len   = com->dev[i].number_sampled_channels;
      for (j = 0 ; j < com->dev[i].number_sampled_channels; j++)
	{ 	  
	  com->dev[i].channel_list[j]=CR_PACK(com->dev[i].channel_list[j],
					      com->dev[i].range,
					      com->dev[i].aref);
	}
      // check if the command can be done with the current device
      if (comedi_command_test(com->dev[i].comedi_dev, &com->dev[i].command)<0)
	{
	  fprintf(stderr, "error during comedi_command_test on %s\n", com->dev[i].file_name);
	  return 1;
	}
#ifdef DEBUG_ACQ
      fprintf(stderr,"comedi_command_test successful on single device\n");
#endif
    }
  if(com->number_devices>1)
    {
#ifdef DEBUG_ACQ
      fprintf(stderr,"building command for master and slaves\n");
#endif
      
      // set the card to work as master and slaves
      for (i=0; i <com->number_devices;i++)
	{
	  if (i==0)
	    {      // this is the master card
	      comedi_t_enable_master(com->dev[i].comedi_dev);
	      // com->dev[i].command.subdev         = com->dev[i].subdevice_analog_input;
	      //com->dev[i].command.flags          = 0;
	      com->dev[i].command.start_src      = TRIG_NOW;//  event to make the acquisition start
	      com->dev[i].command.start_arg      = 0;
	      com->dev[i].command.scan_begin_src = TRIG_TIMER;
	      //com->dev[i].command.scan_begin_arg = 1e9/com->sampling_rate;
	      com->dev[i].command.convert_src    = TRIG_TIMER;
	      com->dev[i].command.convert_arg    = 0;
	      com->dev[i].command.scan_end_src   = TRIG_COUNT;
	      // com->dev[i].command.scan_end_arg   = com->dev[i].number_sampled_channels;
	      com->dev[i].command.stop_src       = TRIG_NONE;
	      com->dev[i].command.stop_arg       = 0;
	      com->dev[i].command.chanlist       = com->dev[i].channel_list;
	      com->dev[i].command.chanlist_len   = com->dev[i].number_sampled_channels;
	      for (j = 0 ; j < com->dev[i].number_sampled_channels; j++)
		{ // read x times the same channels so that the read functions returns quickly
		  
		  com->dev[i].channel_list[j]=CR_PACK(com->dev[i].channel_list[j],
							      com->dev[i].range,
							      com->dev[i].aref);
		}
	      // check if the command can be done with the current device
	      if (comedi_command_test(com->dev[i].comedi_dev, &com->dev[i].command)<0)
		{
		  fprintf(stderr, "error during comedi_command_test on %s\n", com->dev[i].file_name);
		  return 1;
		}
#ifdef DEBUG_ACQ
	      fprintf(stderr,"comedi_command_test successful on %s\n",com->dev[i].file_name);
#endif
	    }
	  else
	    {
	      // set the slave cards
	      comedi_t_enable_slave(com->dev[i].comedi_dev);
	      //  com->dev[i].command.subdev         = com->dev[i].subdevice_analog_input;
	      //com->dev[i].command.flags          = 0;
	      com->dev[i].command.start_src      = TRIG_EXT;//  start from master
	      com->dev[i].command.start_arg      = CR_EDGE | NI_EXT_RTSI(0);
	      com->dev[i].command.scan_begin_src = TRIG_TIMER;
	      // com->dev[i].command.scan_begin_arg = 1e9/com->sampling_rate;
	      com->dev[i].command.convert_src    = TRIG_EXT;
	      com->dev[i].command.convert_arg    = CR_INVERT | CR_EDGE | NI_EXT_RTSI(2);
	      com->dev[i].command.scan_end_src   = TRIG_COUNT;
	      //com->dev[i].command.scan_end_arg   = com->dev[i].number_sampled_channels;
	      com->dev[i].command.stop_src       = TRIG_NONE;
	      com->dev[i].command.stop_arg       = 0;
	      com->dev[i].command.chanlist       = com->dev[i].channel_list;
	      com->dev[i].command.chanlist_len   = com->dev[i].number_sampled_channels;
	      for (j = 0 ; j < com->dev[i].number_sampled_channels; j++)
		{ // read x times the same channels so that the read functions returns quickly
		  
		  com->dev[i].channel_list[j]=CR_PACK(com->dev[i].channel_list[j],
							      com->dev[i].range,
							      com->dev[i].aref);
		}
	      // check if the command can be done with the current device
	      if (comedi_command_test(com->dev[i].comedi_dev, &com->dev[i].command)<0)
		{
		  fprintf(stderr, "error during comedi_command_test on %s\n", com->dev[i].file_name);
		  return 1;
		}
#ifdef DEBUG_ACQ
	      fprintf(stderr,"comedi_command_test successful on %s\n",com->dev[i].file_name);
#endif
	      
	    }
	}
    }

  // do a test run with that command
  if(comedi_interface_run_command(com)!=0)
    {
      fprintf(stderr,"error returned by comedi_interface_run_command(), from comedi_interface_build_command\n");
      return -1 ;
    }
  // function to stop the comedi command from getting new data
  if(comedi_interface_stop_command(com)!=0)
    {
      fprintf(stderr,"error returned by comedi_interface_stop_command(), from comedi_interface_build_command\n");
      return -1 ;
    }


  return 0;
}

int comedi_t_enable_master(comedi_t *dev)
{
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_t_enable_master\n");
#endif

  comedi_insn   configCmd;
  lsampl_t      configData[2];
  int           ret;
  static const unsigned rtsi_subdev = 10;
  static const unsigned rtsi_clock_line = 7;

  /* Route RTSI clock to line 7 (not needed on pre-m-series boards since their
     clock is always on line 7). */
  memset(&configCmd, 0, sizeof(configCmd));
  memset(&configData, 0, sizeof(configData));
  configCmd.insn = INSN_CONFIG;
  configCmd.subdev = rtsi_subdev;
  configCmd.chanspec = rtsi_clock_line;
  configCmd.n = 2;
  configCmd.data = configData;
  configCmd.data[0] = INSN_CONFIG_SET_ROUTING;
  configCmd.data[1] = NI_RTSI_OUTPUT_RTSI_OSC;
  ret = comedi_do_insn(dev, &configCmd);
  if(ret < 0){
    comedi_perror("comedi_do_insn: INSN_CONFIG");
    return(1);
  }
  // Set clock RTSI line as output
  ret = comedi_dio_config(dev, rtsi_subdev, rtsi_clock_line, INSN_CONFIG_DIO_OUTPUT);
  if(ret < 0){
    comedi_perror("comedi_dio_config");
    return(1);
  }
  
  /* Set routing of the 3 main AI RTSI signals and their direction to output.
     We're reusing the already initialized configCmd instruction here since
     it's mostly the same. */
  configCmd.chanspec = 0;
  configCmd.data[1] =  NI_RTSI_OUTPUT_ADR_START1;
  ret = comedi_do_insn(dev, &configCmd);
  if(ret < 0){
    comedi_perror("comedi_do_insn: INSN_CONFIG");
    return(1);
  }
  ret = comedi_dio_config(dev, rtsi_subdev, 0, INSN_CONFIG_DIO_OUTPUT);
  if(ret < 0){
    comedi_perror("comedi_dio_config");
    return(1);
  }
  
  configCmd.chanspec = 1;
  configCmd.data[1] =  NI_RTSI_OUTPUT_ADR_START2;
  ret = comedi_do_insn(dev, &configCmd);
  if(ret < 0){
    comedi_perror("comedi_do_insn: INSN_CONFIG");
    return(1);
  }
  ret = comedi_dio_config(dev, rtsi_subdev, 1, INSN_CONFIG_DIO_OUTPUT);
  if(ret < 0){
    comedi_perror("comedi_dio_config");
    return(1);
  }
  
  configCmd.chanspec = 2;
  configCmd.data[1] =  NI_RTSI_OUTPUT_SCLKG;
  ret = comedi_do_insn(dev, &configCmd);
  if(ret < 0){
    comedi_perror("comedi_do_insn: INSN_CONFIG");
    return(1);
  }
  ret = comedi_dio_config(dev, rtsi_subdev, 2, INSN_CONFIG_DIO_OUTPUT);
  if(ret < 0){
    comedi_perror("comedi_dio_config");
    return(1);
  }
  return 0;
}
int comedi_t_enable_slave(comedi_t *dev)
{
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_t_enable_slaver\n");
#endif

  comedi_insn   configCmd;
  lsampl_t      configData[3];
  int           ret;
  static const unsigned rtsi_subdev = 10;
  static const unsigned rtsi_clock_line = 7;
  
  memset(&configCmd, 0, sizeof(configCmd));
  memset(&configData, 0, sizeof(configData));
  configCmd.insn = INSN_CONFIG;
  configCmd.subdev = rtsi_subdev;
  configCmd.chanspec = 0;
  configCmd.n = 3;
  configCmd.data = configData;
  configCmd.data[0] = INSN_CONFIG_SET_CLOCK_SRC;
  configCmd.data[1] = NI_MIO_PLL_RTSI_CLOCK(rtsi_clock_line);
  configCmd.data[2] = 100;	/* need to give it correct external clock period */
  ret = comedi_do_insn(dev, &configCmd);
  if(ret < 0){
    comedi_perror("comedi_do_insn: INSN_CONFIG");
    return(1);
  }
  /* configure RTSI clock line as input */
  ret = comedi_dio_config(dev, rtsi_subdev, rtsi_clock_line, INSN_CONFIG_DIO_INPUT);
  if(ret < 0){
    comedi_perror("comedi_dio_config");
    return(1);
  }
  /* Configure RTSI lines we are using for AI signals as inputs. */
  ret = comedi_dio_config(dev, rtsi_subdev, 0, INSN_CONFIG_DIO_INPUT);
  if(ret < 0){
    comedi_perror("comedi_dio_config");
    return(1);
  }
  ret = comedi_dio_config(dev, rtsi_subdev, 1, INSN_CONFIG_DIO_INPUT);
  if(ret < 0){
    comedi_perror("comedi_dio_config");
    return(1);
  }
  ret = comedi_dio_config(dev, rtsi_subdev, 2, INSN_CONFIG_DIO_INPUT);
  if(ret < 0){
    comedi_perror("comedi_dio_config");
    return(1);
  }
  return 0;
}

int comedi_interface_run_command(struct comedi_interface* com)
{
  // because of the master/slave arrangment and because card 0 is the master
  // we need to start device 0 last
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_run_command\n");
#endif
  int i;
  for (i=com->number_devices-1;i>=0;i--)
    {
      if ((comedi_command(com->dev[i].comedi_dev, &com->dev[i].command)) < 0) 
	{
	  fprintf(stderr, "comedi_command failed to start for %s in comedi_interface_run_command\n",com->dev[i].file_name);
	  return (1);
	}
    }
  com->command_running=1;
  return 0;
}
int comedi_interface_stop_command(struct comedi_interface* com)
{
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_stop_command\n");
#endif
  int i;
  for (i=0; i <com->number_devices;i++)
    {
      if ((comedi_cancel(com->dev[i].comedi_dev, com->dev[i].subdevice_analog_input)) < 0) 
	{
	  fprintf(stderr, "comedi_cancel failed to stop for %s in comedi_interface_stop_command\n",com->dev[i].file_name);
	  return (1);
	}
    }
  com->command_running=0;
  return 0;
}

int comedi_interface_get_data_from_devices(struct comedi_interface* com)
{
  // function to get new data into the comedi_interface buffer.
  // that is periodically run by the acquisition function (acquisition thread)
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_get_data_from_devices\n");
#endif
  int i,j,k;
  int channel_device_offset;
  // call function to get the data from device
  for (i=0; i <com->number_devices;i++)
    {
      if (comedi_dev_read_data(&com->dev[i])==-1)
	{
	  fprintf(stderr, "error reading from comedi device %d %s\n",i,com->dev[i].file_name);
	  com->command_running=0;
	  return 1;
	}
    }
  
  // get the time, which is immediately after the data were collected
  clock_gettime(CLOCK_REALTIME, &com->time_last_sample_acquired_timespec);

  // get the number of complete samples that can be put in interface buffer
  // which is the smallest number of samples loaded by one of the devices
  for (i=0; i <com->number_devices;i++)
    {
      if (i==0)
	{
	  com->min_samples_read_from_devices=com->dev[i].samples_read;
	}  
      else
	{
	  if(com->min_samples_read_from_devices>com->dev[i].samples_read)
	    {
	      com->min_samples_read_from_devices=com->dev[i].samples_read;
	    }
	}
    }
 
  // adjust the number of unused data for each device, they will not be overwritten on next call to comedi_dev_read_data
  // and will be available on the next call to this function
  for (i=0; i <com->number_devices;i++)
    {
      comedi_dev_adjust_data_point_out_of_samples(&com->dev[i], com->min_samples_read_from_devices);
    }
 
 
  // use to prevent access to com->buffer_data while modifying it
  pthread_mutex_lock( &mutex_comedi_interface_buffer );
  
  if(com->sample_no_to_add+com->min_samples_read_from_devices<com->max_number_samples_in_buffer)
    {
      // add the new data after the old data in the buffer
      channel_device_offset=0;
      for (i=0; i<com->number_devices;i++)
	{
	  if(i>0)
	    {
	      channel_device_offset+=com->dev[i].number_sampled_channels;
	    }
	  for(j=0;j<com->min_samples_read_from_devices;j++) // for all samples
	    {
	      for(k=0;k<com->dev[i].number_sampled_channels;k++) // for all channels
		{// [number samples already in buffer+added samples * total number of channels       +   chan_no   +   offset of the card]
		  com->buffer_data[(com->sample_no_to_add+j)*com->number_channels+k+channel_device_offset]=
		    //[added_samples *   number_channels on that device + chan_no]       com->data_offset_to_dat
		    com->dev[i].buffer_data[j*com->dev[i].number_sampled_channels+k]+com->data_offset_to_dat;
		}
	    }
	}
    }
  else
    {
      // add part of the new data after the old data in the buffer
      channel_device_offset=0;
      for (i=0; i < com->number_devices;i++)
	{
	  if(i>0)
	    {
	      channel_device_offset+=com->dev[i].number_sampled_channels;
	    }
	  // number of samples to add at following the old data
	  for(j=0;j<com->max_number_samples_in_buffer-com->sample_no_to_add;j++)
	    {
	      for(k=0;k<com->dev[i].number_sampled_channels;k++)
		{
		  com->buffer_data[(com->sample_no_to_add+j)*com->number_channels+k+channel_device_offset]=
		    com->dev[i].buffer_data[j*com->dev[i].number_sampled_channels+k]+com->data_offset_to_dat;
		}
	    }
	}
      
      // add part of the new data after samples to add at the beginning of buffer
      com->samples_copied_at_end_of_buffer=com->max_number_samples_in_buffer-com->sample_no_to_add;
      if(com->samples_copied_at_end_of_buffer<com->min_samples_read_from_devices)
	{
	  channel_device_offset=0;
	  for (i=0; i < com->number_devices;i++)
	    {
	      if(i>0)
		{
		  channel_device_offset+=com->dev[i].number_sampled_channels;
		}
	      
	      // number of samples to add at following the old data
      	      for(j=0; j<com->min_samples_read_from_devices-com->samples_copied_at_end_of_buffer;j++)
		{
		  for(k=0;k<com->dev[i].number_sampled_channels;k++)
		    {
		      com->buffer_data[j*com->number_channels+k+channel_device_offset]=
			com->dev[i].buffer_data[(com->samples_copied_at_end_of_buffer+j)*com->dev[i].number_sampled_channels+k]+com->data_offset_to_dat;
		    }
		}
	    }
	}
    }
  com->number_samples_read+=com->min_samples_read_from_devices;
  com->sample_no_to_add=com->number_samples_read%com->max_number_samples_in_buffer; // index of last sample added to buffer (more recent one)
  pthread_mutex_unlock(&mutex_comedi_interface_buffer );
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_get_data_from_devices, com->number_samples_read: %ld\n",com->number_samples_read);
#endif
  return 0;
} 
 
int comedi_dev_adjust_data_point_out_of_samples(struct comedi_dev *dev,int number_samples_transfered_to_inter_buffer)
{
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_dev_adjust_data_point_out_of_sample\n");
#endif

  //  fprintf(stderr,"comedi_dev_adjust_data_point_out_of_samples: number_samples_transfered_to_inter_buffer: %d\n", number_samples_transfered_to_inter_buffer);
  int unused_samples;
  unused_samples=dev->samples_read-number_samples_transfered_to_inter_buffer; 
  dev->data_point_out_of_samples+=unused_samples*dev->number_sampled_channels;
  dev->samples_read=number_samples_transfered_to_inter_buffer;
  dev->cumulative_samples_read-=unused_samples;
  //fprintf(stderr,"comedi_dev_adjust_data_point_out_of_samples: unused_samples:%d samples_read: %d\n",unused_samples,dev->samples_read);
  return 0;
}
int comedi_dev_read_data(struct comedi_dev *dev)
{
  // function move data from driver's buffer into the device buffer.
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_dev_read_data\n");
#endif

  int i;
  // move back incomplete sample data and complete samples not used to the beginning of the buffer
  for (i=0;i<dev->data_point_out_of_samples;i++)
    {
      dev->buffer_data[i]=dev->buffer_data[dev->samples_read * dev->number_sampled_channels +i];
    }
  
  // move the pointer to avoid overwritting the incomplete sample data
  dev->pointer_buffer_data=dev->buffer_data+dev->data_point_out_of_samples;

  // read new data from device
  dev->read_bytes=read(comedi_fileno(dev->comedi_dev),dev->pointer_buffer_data,sizeof(dev->buffer_data)-(sizeof(sampl_t)*dev->data_point_out_of_samples));
  
  // check if read was successful
  if (dev->read_bytes<0)// reading failed
    {
      fprintf(stderr, "error reading from comedi device %s in comdedi_dev_read_data()\n",dev->file_name);
      fprintf(stderr, "check for buffer overflow\n");
      return 1;
    }
  else // read was ok
    {
      // get number of samples read
      dev->samples_read=(dev->read_bytes + (dev->data_point_out_of_samples*sizeof(sampl_t))) / (sizeof(sampl_t)*dev->number_sampled_channels);
      dev->cumulative_samples_read+=dev->samples_read;
      dev->data_point_out_of_samples=(dev->read_bytes/sizeof(sampl_t) + dev->data_point_out_of_samples) % dev->number_sampled_channels;
    }
  return 0;
}

int comedi_interface_clear_current_acquisition_variables(struct comedi_interface* com)
{
  // function called when a comedi acquisition is over
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_clear_current_acquisition_variables\n");
#endif

  int i=0;
  com->number_samples_read=0;
  com->sample_no_to_add=0;
  for(i=0;i<com->number_devices;i++)
    {
      comedi_dev_clear_current_acquisition_variables(&com->dev[i]);
    }
  return 0;
}
int comedi_dev_clear_current_acquisition_variables(struct comedi_dev *dev)
{
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_dev_clear_current_acquisition_variables\n");
#endif

  dev->read_bytes=0;
  dev->samples_read=0;
  dev->data_point_out_of_samples=0;
  dev->cumulative_samples_read=0;
  return 0;
}


void * acquisition(void* comedi_inter) // the comedi interface needs to be passed to this function
{
  // function that is run by the acquisition_thread
#ifdef DEBUG_ACQ
  fprintf(stderr,"acquisition(void* comedi_inter)\n");
#endif

  struct comedi_interface *com;
  com= (struct comedi_interface *) comedi_inter;
  void *pt=NULL;
  com->acquisition_thread_running=1;
 
  com->is_acquiring=1;
  while(com->is_acquiring==1)
    {
      if((comedi_interface_get_data_from_devices(com))==-1)
	{
	  fprintf(stderr,"error returned by comedi_interface_get_data_from_devises(), acquisition stoped\n");
	  comedi_interface_clear_current_acquisition_variables(com);
	  com->is_acquiring=0;
	  com->acquisition_thread_running=0;
	  return pt ;
	}
      nanosleep(&com->inter_acquisition_sleep_timespec,&com->req);
    }
  
 
  // exiting from this function will kill the recording_thread

#ifdef DEBUG_ACQ
  fprintf(stderr,"leaving acquisition(void* comedi_inter)\n");
#endif
  com->acquisition_thread_running=0;
  pthread_exit(NULL);
}


int comedi_interface_set_sampled_channels(struct comedi_interface* com,int dev_no,int number_channels_to_sample,int* list)
{

  // function to change the number or list of channels being sampled from a device in each swept
  // by default, the device samples once each of the analog channel of the device. this is changed by this function
  // it might have implications for functions reading the comedi_interface buffer.

  int i;
  // check the arguments
  if (dev_no < 0 || dev_no > com->number_devices)
    {
      fprintf(stderr,"dev_no is < 0 or larger than com->number_devices in comedi_interface_set_sampled_channels: %d\n",dev_no);
      return -1 ;
    }
  if(number_channels_to_sample>COMEDI_DEVICE_MAX_CHANNELS)
    {
      fprintf(stderr,"number_channels_to_sample is < 0 or larger than COMEDI_DEVICE_MAX_CHANNELS(%d): %d, in comedi_interface_set_sampled_channels\n",COMEDI_DEVICE_MAX_CHANNELS,number_channels_to_sample);
      return -1 ;
    }

  for(i=0; i < number_channels_to_sample;i++)
    {
      if(list[i]<0 || list[i]>=com->dev[dev_no].number_channels_analog_input)
	{
	  fprintf(stderr,"a channel from the list given is < 0 or larger than com->dev[dev_no].number_channels_analog_input(%d): %d\n",com->dev[dev_no].number_channels_analog_input,list[i]);	  
	  return -1;
	}
    }
  // do not allow change this change if the acquisition is already running
  if(com->is_acquiring==1)
    {
      fprintf(stderr,"cannot change sampled channels when the acquisition process is running\n");
      return -1 ;
    }
  
  // set the new list of sampled channel for the device
  com->dev[dev_no].number_sampled_channels=number_channels_to_sample;
  for (i=0; i < com->dev[dev_no].number_sampled_channels; i++)
    {
      com->dev[dev_no].channel_list[i]=list[i];
    }
  
  // rebuild the list of channels
  comedi_interface_set_channel_list(com);
  // recalculate the number of samples in buffer
  com->max_number_samples_in_buffer=com->buffer_size/com->number_channels;
  // recreate the command
  comedi_interface_build_command(com);
  return 0;
}
unsigned long int comedi_interface_get_data_one_channel(struct comedi_interface* com,int channel_no,int number_samples,double* data,struct timespec* time_acquisition)
{
  // function that puts the last "number_samples" samples into the array data for channel_no
  // returns -1 when error
  // returns 0 if no data copied
  // returns last_sample_no when valid data

#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_get_data_one_channel, channel no: %d, number_samples:%d\n",channel_no,number_samples);
#endif
 
  int i;
  int channel_index=-1;

  if (com->max_number_samples_in_buffer<number_samples)
    {
      fprintf(stderr,"number of samples needed is larger than number of samples available in comedi_interface buffer\n");
      fprintf(stderr,"in comedi_interface_get_data_one_channel(), number samples needed: %d, com->max_number_samples_in_buffer: %d\n",number_samples,com->max_number_samples_in_buffer);
      return -1;
    }
  
  if (com->number_samples_read<number_samples)
    {
      // the acquisition process just started, don't have enough data to return
       fprintf(stderr,"in comedi_interface_get_data_one_channel(), number samples needed: %d, com->num_samples_read: %ld\n",number_samples,com->number_samples_read);
      return 0;
    }
  
  // find the index of the channel needed
  for (i=0;i<com->number_channels;i++)
    {
      if(com->channel_list[i]==channel_no)
	{
	  channel_index=i;
	  i=com->number_channels; // end loop
	}
    }
  if(channel_index==-1)
    {
      fprintf(stderr,"in comedi_interface_get_data_one_channel(), channel required is not in the channel_list: %d\n",channel_no);
      return -1;
    }
  
  int index_acq;
  int num_samples_to_transfer_end;
  int num_samples_to_transfer_start;
   // if we are here that means we have enough data as requested, transfer the data from comdedi_inter buffer to data array
  pthread_mutex_lock( &mutex_comedi_interface_buffer );
  
  // if the comedi buffer has the data in chronological order and starts to sample 0
  if (com->number_samples_read<com->max_number_samples_in_buffer)
    {
      index_acq=(com->number_samples_read-number_samples)*com->number_channels+channel_index;
      // transfer the data
      for(i=0;i<number_samples;i++)
	{
	  data[i]=com->buffer_data[index_acq];
	  index_acq+=com->number_channels;
	}
    }
  else // the comedi buffer has the data in a non-chronological order and first sample is not 0
    {
      index_acq=(com->sample_no_to_add-number_samples)*com->number_channels+channel_index;
      // if larger or equal to 0, can get the data without without having to wrap the buffer
      if(index_acq>=0)
	{
	  // transfer the data
	  for(i=0;i<number_samples;i++)
	    {
	      data[i]=com->buffer_data[index_acq];
	      index_acq+=com->number_channels;
	    }
	}
      else // need to wrap the buffer
	{
	  // get the data at the end of the buffer
	  num_samples_to_transfer_end=(0-index_acq)/com->number_channels;
	  index_acq=com->max_number_samples_in_buffer*com->number_channels+index_acq;
	  // transfer the data from the end of buffer
	  for(i=0;i<num_samples_to_transfer_end;i++)
	    {
	      data[i]=com->buffer_data[index_acq];
	      index_acq+=com->number_channels;
	    }
	  // get the data from the beginning of buffer
	  num_samples_to_transfer_start=number_samples-num_samples_to_transfer_end;
	  index_acq=0+channel_index;
	  for(i=0;i<num_samples_to_transfer_start;i++)
	    {
	      data[num_samples_to_transfer_end+i]=com->buffer_data[index_acq];
	      index_acq+=com->number_channels;
	    }
	}
    }
 

 // get the time of last acquisition
  time_acquisition->tv_sec=com->time_last_sample_acquired_timespec.tv_sec;
  time_acquisition->tv_nsec=com->time_last_sample_acquired_timespec.tv_nsec;
  pthread_mutex_unlock( &mutex_comedi_interface_buffer );
  return com->number_samples_read;
}

int comedi_interface_set_sampling_rate(struct comedi_interface* com, int req_sampling_rate)
{

#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_set_sampling_rate, attempt to set to %d\n",req_sampling_rate);
#endif
  int i,r;
  if(com->is_acquiring==1)
    {
      fprintf(stderr,"attempt to change the sampling rate while the acquisition is running\n");
      return -1;
    }
  if(com->is_acquiring==1)
    {
      fprintf(stderr,"attempt to change the sampling rate while the acquisition is running\n");
      return -1;
    }
  if(com->command_running==1)
    {
      fprintf(stderr,"attempt to change the sampling rate while a comedi command is running\n");
      return -1;
    }
  if (req_sampling_rate<=0 || req_sampling_rate>MAX_SAMPLING_RATE)
    {
      fprintf(stderr,"sampling rate should be larger than 0 and not larger than %i but was %i in comedi_interface_set_sampling_rate\n",MAX_SAMPLING_RATE,req_sampling_rate);
      return -1;
    }
  for (i=0; i < com->number_devices;i++)
    {
      r = comedi_get_cmd_generic_timed(com->dev[i].comedi_dev,
				       com->dev[i].subdevice_analog_input,
				       &com->dev[i].command,
				       com->dev[i].number_sampled_channels,
				       (int)(1e9/req_sampling_rate));
      if(r<0)
	{
	  printf("comedi_get_cmd_generic_timed failed in comedi_interface_set_sampling_rate\n");
	  return 1;
        }
    }
  
  // the next two if statements assume that if there are several boards, they all have the same properties
  // the timing is done channel by channel
  if ((com->dev[0].command.convert_src    == TRIG_TIMER)&&(com->dev[0].command.convert_arg)) 
    {
      com->sampling_rate=((1E9/com->dev[0].command.convert_arg)/com->dev[0].number_sampled_channels);
    }
  // the timing is done scan by scan (all channels at once)
  if ((com->dev[0].command.scan_begin_src == TRIG_TIMER)&&(com->dev[0].command.scan_begin_arg)) 
    {
      com->sampling_rate=1E9/com->dev[0].command.scan_begin_arg;
    }

  if(com->sampling_rate!=req_sampling_rate)
    {
      fprintf(stderr,"requested sampling rate was adjusted from %d Hz to %d Hz in comedi_interface_set_sampling_rate()\n",req_sampling_rate,com->sampling_rate);
    }

  
  if(comedi_interface_build_command(com)==-1)
    {
      fprintf(stderr,"unable to build the command after changing sampling rate in comedi_interface_set_sampling_rate\n");
      return -1;
    }
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_set_sampling_rate, sampling rate set to %d\n",req_sampling_rate);
#endif
  return 0;
}

int comedi_interface_print_info(struct comedi_interface* com)
{
  int i;
  printf("**** information regarding comedi interface ****\n");
  printf("is_acquiring: %d\n",com->is_acquiring);
  printf("command_running: %d\n",com->command_running);
  printf("number_devices: %d\n",com->number_devices);
  printf("sampling_rate: %d\n",com->sampling_rate);
  printf("number_channels: %d\n",com->number_channels);
  printf("max_number_samples_in_buffer: %d\n", com->max_number_samples_in_buffer);
  printf("number_samples_read: %ld\n",com->number_samples_read);
  printf("buffer_size:%d\n",com->buffer_size);
  printf("inter_acquisition_sleep_ms:%lf\n",com->inter_acquisition_sleep_ms);
  for(i=0;i < com->number_devices;i++)
    {
      comedi_device_print_info(&com->dev[i]);
    }
  return 0;
}
int comedi_device_print_info(struct comedi_dev* dev)
{
  int i;
  printf("**** information regarding device %s ****\n",dev->file_name);
  printf("name: %s\n",dev->name);
  printf("driver: %s\n",dev->driver);
  printf("number_of_subdevices: %d\n",dev->number_of_subdevices);
  printf("subdevice_analog_inputs: %d\n",dev->subdevice_analog_input);
  printf("number_channels_analog_input: %d\n", dev->number_channels_analog_input);
  printf("number_ranges: %d\n", dev->number_ranges);
  printf("range: %d, min: %lf, max: %lf, unit: %u\n",dev->range, dev->range_input[dev->range]->min,dev->range_input[dev->range]->max,dev->range_input[dev->range]->unit);
  printf("buffer_size:%d\n",dev->buffer_size);
  printf("samples_read:%d\n",dev->samples_read);
  printf("cumulative_samples_read:%ld\n",dev->cumulative_samples_read);
  printf("number_sampled_channels:%d\n",dev->number_sampled_channels);
  printf("is_acquiring:%d\n",dev->is_acquiring);
  printf("channel list: \n");
  for (i =0; i < dev->number_sampled_channels;i++)
    {
      printf("%d:%hu ",i,dev->channel_list[i]);
    }
  printf("\n");
  return 0;
}


int comedi_interface_start_acquisition(struct comedi_interface* com)
{
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_start_acquisition\n");
#endif

  if (com->is_acquiring==1)
    {
      fprintf(stderr,"acquiring is already running in comedi_interface_start_acquisition()\n");
      return -1;
    }
  if (com->acquisition_thread_running==1)
    {
      fprintf(stderr,"acquiring thread is already running in comedi_interface_start_acquisition()\n");
      return -1;
    }
  
  // start the acquisition on the comedi devices
  if (com->command_running==0)
    {
      if(comedi_interface_run_command(com)!=0)
	{
	  fprintf(stderr,"error returned by comedi_interface_run_command(), acquisition not possible\n");
	  return -1 ;
	}
      com->command_running=1;
    }
  
  if((com->acquisition_thread_id=pthread_create(&com->acquisition_thread, NULL, acquisition, (void*)com))==1)
    {
      fprintf(stderr,"error creating the acquisition thread, error_no:%d\n",
	      com->acquisition_thread_id);
      return -1;
    }
  nanosleep(&com->inter_acquisition_sleep_timespec,&com->req); // wait to give time to thread to start
  return 0;
}
int comedi_interface_stop_acquisition(struct comedi_interface* com)
{
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_stop_acquisition\n");
#endif
  if(com->is_acquiring==0)
    {
      fprintf(stderr,"com->is_acquiring is already set to 0 in comedi_interface_stop_acquisition\n");
    }
  if(com->acquisition_thread_running==0)
    {
      fprintf(stderr,"com->acquisistion_thread_running is already set to 0 in comedi_interface_stop_acquisition\n");
    }
  com->is_acquiring=0;
  while(com->acquisition_thread_running==1) // wait untill recording thread is over
    {
      nanosleep(&com->inter_acquisition_sleep_timespec,&com->req);
    }
  
  
  // function to stop the comedi command from getting new data
  if(comedi_interface_stop_command(com)!=0)
    {
      fprintf(stderr,"error returned by comedi_interface_stop_command()\n");
      return -1 ;
    }
  // function to reset the counter and variables of interface and devices
  comedi_interface_clear_current_acquisition_variables(com);
  return 0;
}
long int comedi_interface_get_last_data_one_channel(struct comedi_interface* com,int channel_no,int number_samples,double* data,struct timespec* time_acquisition)
{
  // function that puts the last "number_samples" samples into the array data for channel_no
  // returns -1 when error
  // returns 0 if no data copied
  // returns last_sample_no when valid data
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_get_last_data_one_channel, channel no: %d, number_samples:%d\n",channel_no,number_samples);
#endif
  int i;
  int channel_index=-1;

  if (com->max_number_samples_in_buffer<number_samples)
    {
      fprintf(stderr,"number of samples needed is larger than number of samples available in comedi_interface buffer\n");
      fprintf(stderr,"in comedi_interface_get_last_data_one_channel(), number samples needed: %d, com->max_number_samples_in_buffer: %d\n",number_samples,com->max_number_samples_in_buffer);
      return -1;
    }
  
  if (com->number_samples_read<number_samples)
    {
      // the acquisition process just started, don't have enough data to return
       fprintf(stderr,"in comedi_interface_get_last_data_one_channel(), number samples needed: %d, com->num_samples_read: %ld\n",number_samples,com->number_samples_read);
      return 0;
    }
  
  // find the index of the channel needed
  for (i=0;i<com->number_channels;i++)
    {
      if(com->channel_list[i]==channel_no)
	{
	  channel_index=i;
	  i=com->number_channels; // end loop
	}
    }
  if(channel_index==-1)
    {
      fprintf(stderr,"in comedi_interface_get_last_data_one_channel(), channel required is not in the channel_list: %d\n",channel_no);
      return -1;
    }
  
  int index_acq;
  int num_samples_to_transfer_end;
  int num_samples_to_transfer_start;
   // if we are here that means we have enough data as requested, transfer the data from comdedi_inter buffer to data array
  pthread_mutex_lock( &mutex_comedi_interface_buffer );
  
  // if the comedi buffer has the data in chronological order and starts to sample 0
  if (com->number_samples_read<com->max_number_samples_in_buffer)
    {
      index_acq=(com->number_samples_read-number_samples)*com->number_channels+channel_index;
      // transfer the data
      for(i=0;i<number_samples;i++)
	{
	  data[i]=com->buffer_data[index_acq];
	  index_acq+=com->number_channels;
	}
    }
  else // the comedi buffer has the data in a non-chronological order and first sample is not 0
    {
      index_acq=(com->sample_no_to_add-number_samples)*com->number_channels+channel_index;
      // if larger or equal to 0, can get the data without without having to wrap the buffer
      if(index_acq>=0)
	{
	  // transfer the data
	  for(i=0;i<number_samples;i++)
	    {
	      data[i]=com->buffer_data[index_acq];
	      index_acq+=com->number_channels;
	    }
	}
      else // need to wrap the buffer
	{
	  // get the data at the end of the buffer
	  num_samples_to_transfer_end=(0-index_acq)/com->number_channels;
	  index_acq=com->max_number_samples_in_buffer*com->number_channels+index_acq;
	  // transfer the data from the end of buffer
	  for(i=0;i<num_samples_to_transfer_end;i++)
	    {
	      data[i]=com->buffer_data[index_acq];
	      index_acq+=com->number_channels;
	    }
	  // get the data from the beginning of buffer
	  num_samples_to_transfer_start=number_samples-num_samples_to_transfer_end;
	  index_acq=0+channel_index;
	  for(i=0;i<num_samples_to_transfer_start;i++)
	    {
	      data[num_samples_to_transfer_end+i]=com->buffer_data[index_acq];
	      index_acq+=com->number_channels;
	    }
	}
    }
 
 // get the time of last acquisition
  time_acquisition->tv_sec=com->time_last_sample_acquired_timespec.tv_sec;
  time_acquisition->tv_nsec=com->time_last_sample_acquired_timespec.tv_nsec;
  pthread_mutex_unlock( &mutex_comedi_interface_buffer );
  return com->number_samples_read;
}

long int comedi_interface_get_last_data_two_channels(struct comedi_interface* com,int channel_no_1, int channel_no_2,int number_samples,double* data_1, double* data_2, struct timespec* time_acquisition)
{
   // function that puts the last "number_samples" samples into the array data_1 for channel_no_1
  // returns -1 when error
  // returns 0 if no data copied
  // returns last_sample_no when valid data
#ifdef DEBUG_ACQ
  fprintf(stderr,"comedi_interface_get_last_data_two_channels, channel no: %d, number_samples:%d\n",channel_no_1,number_samples);
#endif
  int i;
  int channel_index_1=-1;
  int channel_index_2=-1;

  if (com->max_number_samples_in_buffer<number_samples)
    {
      fprintf(stderr,"number of samples needed is larger than number of samples available in comedi_interface buffer\n");
      fprintf(stderr,"in comedi_interface_get_last_data_two_channels(), number samples needed: %d, com->max_number_samples_in_buffer: %d\n",number_samples,com->max_number_samples_in_buffer);
      return -1;
    }
  
  if (com->number_samples_read<number_samples)
    {
      // the acquisition process just started, don't have enough data to return
       fprintf(stderr,"in comedi_interface_get_last_data_two_channels(), number samples needed: %d, com->num_samples_read: %ld\n",number_samples,com->number_samples_read);
      return 0;
    }
  
  // find the index of the channel needed
  for (i=0;i<com->number_channels;i++)
    {
      if(com->channel_list[i]==channel_no_1)
	{
	  channel_index_1=i;
	}
      if(com->channel_list[i]==channel_no_2)
	{
	  channel_index_2=i;
	}
    }
  if(channel_index_1==-1||channel_index_2==-1)
    {
      fprintf(stderr,"in comedi_interface_get_last_data_two_channels(), channels required are not in the channel_list: %d and %d\n",channel_no_1, channel_no_2);
      return -1;
    }
  
  int index_acq_1,index_acq_2;
  int num_samples_to_transfer_end;
  int num_samples_to_transfer_start;
  // if we are here that means we have enough data as requested, transfer the data from comdedi_inter buffer to data array
  pthread_mutex_lock( &mutex_comedi_interface_buffer );
  
  // if the comedi buffer has the data in chronological order and starts to sample 0
  if (com->number_samples_read<com->max_number_samples_in_buffer)
    {
      index_acq_1=(com->number_samples_read-number_samples)*com->number_channels+channel_index_1;
      index_acq_2=(com->number_samples_read-number_samples)*com->number_channels+channel_index_2;
      
      // copyr the data
      for(i=0;i<number_samples;i++)
	{
	  data_1[i]=com->buffer_data[index_acq_1];
	  data_2[i]=com->buffer_data[index_acq_2];
	  index_acq_1+=com->number_channels;
	  index_acq_2+=com->number_channels;
	}
    }
  else // the comedi buffer has the data in a non-chronological order and first sample is not 0
    {
      index_acq_1=(com->sample_no_to_add-number_samples)*com->number_channels+channel_index_1;
      index_acq_2=(com->sample_no_to_add-number_samples)*com->number_channels+channel_index_2;
      // if larger or equal to 0, can get the data without without having to wrap the buffer
      if(index_acq_1>=0)
	{
	  // transfer the data
	  for(i=0;i<number_samples;i++)
	    {
	      data_1[i]=com->buffer_data[index_acq_1];
	      data_2[i]=com->buffer_data[index_acq_2];
	      index_acq_1+=com->number_channels;
	      index_acq_2+=com->number_channels;
	    }
	}
      else // need to wrap the buffer
	{
	  // get the data at the end of the buffer
	  num_samples_to_transfer_end=(0-index_acq_1)/com->number_channels;
	  index_acq_1=com->max_number_samples_in_buffer*com->number_channels+index_acq_1;
	  index_acq_2=com->max_number_samples_in_buffer*com->number_channels+index_acq_2;

	  // transfer the data from the end of buffer
	  for(i=0;i<num_samples_to_transfer_end;i++)
	    {
	      data_1[i]=com->buffer_data[index_acq_1];
	      data_2[i]=com->buffer_data[index_acq_2];
	      index_acq_1+=com->number_channels;
	      index_acq_2+=com->number_channels;
	    }
	  // get the data from the beginning of buffer
	  num_samples_to_transfer_start=number_samples-num_samples_to_transfer_end;
	  index_acq_1=0+channel_index_1;
	  index_acq_2=0+channel_index_2;
	  for(i=0;i<num_samples_to_transfer_start;i++)
	    {
	      data_1[num_samples_to_transfer_end+i]=com->buffer_data[index_acq_1];
	      data_2[num_samples_to_transfer_end+i]=com->buffer_data[index_acq_2];
	      index_acq_1+=com->number_channels;
	      index_acq_2+=com->number_channels;
	    }
	}
    }
  // get the time of last acquisition
  time_acquisition->tv_sec=com->time_last_sample_acquired_timespec.tv_sec;
  time_acquisition->tv_nsec=com->time_last_sample_acquired_timespec.tv_nsec;
  pthread_mutex_unlock( &mutex_comedi_interface_buffer );
  return com->number_samples_read;
  
}
