/****************************************************************
Copyright (C) 2011 Kevin Allen

This file is part of laser_stimulation

laser_stimulation is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

laser_stimulation is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with laser_stimulation.  If not, see <http://www.gnu.org/licenses/>.

date 15.02.2010
************************************************************************/
#include "main.h"

void print_options();
int main(int argc, char *argv[])
{
  char * prog_name=PACKAGE_NAME; // defined in config.h
  char * version=PACKAGE_VERSION;
  char * copyright_notice=PACKAGE_COPYRIGHT;
  char * package_bugreport=PACKAGE_BUGREPORT;
  int non_opt_arguments; // given by user
  int num_arg_needed=3; // required by program
  // flag for each option
  int with_h_opt=0;
  int with_v_opt=0;
  int with_t_opt=0;
  int with_T_opt=0;
  int with_R_opt=0;
  int with_s_opt=0;
  int with_o_opt=0;
  int with_c_opt=0;
  int with_x_opt=0;
  int with_y_opt=0;
  int with_C_opt=0;
  int with_d_opt=0;
  int with_D_opt=0;
  // variables to store values passed with options
  double stimulation_theta_phase=90;
  double train_frequency_hz=6;
  double minimum_interval_ms=100;
  double maximum_interval_ms=200;
  double swr_power_threshold; // as a z score
  double swr_convolution_peak_threshold=0.5; // as a z score
  char * dat_file_name;
  int channels_in_dat_file=32;
  int offline_channel=0;
  int swr_offline_reference=1;
  int device_index_for_stimulation=DEVICE_INDEX_FOR_STIMULATION;
  // to get the options
  int opt; 
  int i;
  
  // get the options
  while (1)
    {
      static struct option long_options[] =
	{
	  {"help", no_argument,0,'h'},
	  {"version", no_argument,0,'v'},
	  {"theta", required_argument,0,'t'},
	  {"swr",required_argument,0,'s'},
	  {"train", required_argument,0,'T'},
	  {"random", no_argument,0,'R'},
	  {"minimum_interval_ms", required_argument,0,'m'},
	  {"maximum_interval_ms", required_argument,0,'M'},
	  {"offline", required_argument,0,'o'},
	  {"channels_in_dat_file", required_argument,0,'c'},
	  {"offline_channel", required_argument,0,'x'},
	  {"swr_offline_reference", required_argument,0,'y'},
	  {"swr_convolution_peak_threshold", required_argument,0,'C'},
	  {"output_device_for_stimulation", required_argument,0,'d'},
	  {"delay_swr", no_argument,0,'D'},
	  {0, 0, 0, 0}
	};
      int option_index = 0;
      opt = getopt_long (argc, argv, "x:y:c:o:s:hvt:T:Rm:M:C:d:D",
			 long_options, &option_index);
      
      /* Detect the end of the options. */
      if (opt == -1)
	break;
      
      switch (opt)
	{
	case 0:
	  /* If this option set a flag, do nothing else now. */
	  if (long_options[option_index].flag != 0)
	    break;
	  printf ("option %s", long_options[option_index].name);
	  if (optarg)
	    printf (" with arg %s", optarg);
	  printf ("\n");
	  break;
	case 'h':
	  {
	    with_h_opt=1;
	    break;
	  }
	case 'v':
	  {
	    with_v_opt=1;
	    break;
	  }
	case  't':
	  {
	    with_t_opt=1;
	    stimulation_theta_phase=atof(optarg);
	    break;
	  }
	case  'T':
	  {
	    with_T_opt=1;
	    train_frequency_hz=atof(optarg);
	    break;
	  }	  
	case 'R':
	  {
	    with_R_opt=1;
	    break;
	  }
	case  'm':
	  {
	    minimum_interval_ms=atof(optarg);
	    break;
	  }	  
	case  'M':
	  {
	    maximum_interval_ms=atof(optarg);
	    break;
	  }	  
	case  's':
	  {
	    with_s_opt=1;
	    swr_power_threshold=atof(optarg);
	    break;
	  }	  
	case  'o':
	  {
	    with_o_opt=1;
	    dat_file_name=optarg;
	    break;
	  }	  
	case  'c':
	  {
	    with_c_opt=1;
	    channels_in_dat_file=atoi(optarg);
	    break;
	  }	  
	case  'x':
	  {
	    with_x_opt=1;
	    offline_channel=atoi(optarg);
	    break;
	  }	  
	case  'y':
	  {
	    with_y_opt=1;
	    swr_offline_reference=atoi(optarg);
	    break;
	  }	  
	case  'C':
	  {
	    with_C_opt=1;
	    swr_convolution_peak_threshold=atof(optarg);
	    break;
	  }	  
	case  'd':
	  {
	    with_d_opt=1;
	    device_index_for_stimulation=atoi(optarg);
	    break;
	  }	  
	case 'D':
	  {
	    with_D_opt=1;
	    break;
	  }
	case '?':
	  /* getopt_long already printed an error message. */
	  //	  break;
	  
	default:
	  return 1;
	}
    }
  
  if (with_v_opt)
    {
      printf("%s %s\n",prog_name, version);
      printf("%s\n",copyright_notice);
      printf("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.\n");
      return 0;
    }
  if (with_h_opt)
    {
      printf("%s is a program to do laser brain stimulation\n",prog_name);
      printf("Usage for %s is \n", prog_name);
      printf("%s trial_duration_sec pulse_duration_ms laser_intensity_volts\n",prog_name);
      print_options();
      printf("report bugs at %s\n",package_bugreport);
      return 0;
    }

  // no -v or -h, can continue
  non_opt_arguments=argc-optind; // number of non-option argument required
  if ((non_opt_arguments) !=  num_arg_needed)
    {
      fprintf(stderr,"Usage for %s is \n", prog_name);
      fprintf(stderr,"%s trial_duration_sec pulse_duration_ms laser_intensity_volts\n",argv[0]);
      print_options();
      fprintf(stderr,"You need %d arguments but gave %d arguments: \n",num_arg_needed,non_opt_arguments);
      for (i = 1; i < argc; i++)
	{
	  fprintf(stderr,"%s\n", argv[i]);
	}
      return (1);
    }  
  

  // variables read from arguments
  double laser_intensity_volt; // for laser power
  double baseline_volt=0; // for ttl pulse
  double pulse_volt=3;// for ttl pulse

  // same as above variables, but in comedi value instead of volts
  lsampl_t comedi_intensity =0;
  lsampl_t comedi_baseline =0;
  lsampl_t comedi_pulse=0;

  double theta_frequency=(MIN_FREQUENCY_THETA+MAX_FREQUENCY_THETA)/2;
  double theta_degree_duration_ms=(1000/theta_frequency)/360;
  double theta_delta_ratio;


  // structure with variables about the comedi device, see main.h for details
  struct comedi_interface comedi_inter;

  // structure with variables to do the filtering of signal
  struct fftw_interface_theta fftw_inter;
  struct fftw_interface_swr fftw_inter_swr;

  // structure with variables to do time keeping
  struct time_keeper tk;

  struct data_file_si data_file; // data_file_short_integer

  // list of channels to scan, bluff, we will sample the same channel many times
  int channel_list[NUMBER_SAMPLED_CHANNEL_DEVICE_0];
  long int last_sample_no=0;
  double current_phase=0;
  double phase_diff;
  double max_phase_diff=MAX_PHASE_DIFFERENCE;
  double swr_power=0;
  double swr_convolution_peak=0;

  // parce the arguments from the command line
  tk.trial_duration_sec=atof(argv[optind]);
  tk.pulse_duration_ms=atof(argv[optind+1]);
  laser_intensity_volt=atof(argv[optind+2]);
 
  
  tk.duration_sleep_when_no_new_data=set_timespec_from_ms(SLEEP_WHEN_NO_NEW_DATA_MS);
  tk.duration_pulse=set_timespec_from_ms(tk.pulse_duration_ms);
  tk.interval_duration_between_swr_processing=set_timespec_from_ms(INTERVAL_DURATION_BETWEEN_SWR_PROCESSING_MS);


  // variables to work offline from a dat file
  int new_samples_per_read_operation=60; //  3 ms of data
  short int* data_from_file;
  short int* ref_from_file;


  /*************************************************************
   block to check if the options and arguments make sense
  *************************************************************/
  if(with_t_opt==1&&with_T_opt==1)
    {
      fprintf(stderr,"%s: can't run with both -t and -T options\n",prog_name);
      return 1;
    }
  if(with_R_opt==1&&with_t_opt==1)
    {
      fprintf(stderr,"%s: can't run with both -R and -t options\n",prog_name);
      return 1;
    }
  if(with_s_opt==1&&with_t_opt==1)
    {
      fprintf(stderr,"%s: can't run with both -t and -s options\n",prog_name);
      return 1;
    }
  if(with_R_opt==1&&with_T_opt==1)
    {
      fprintf(stderr,"%s: can't run with both -R and -T options\n",prog_name);
      return 1;
    }
  if(with_R_opt==1&&with_s_opt==1)
    {
      fprintf(stderr,"%s: can't run with both -R and -s options\n",prog_name);
      return 1;
    }
  if(with_s_opt==1&&with_T_opt==1)
    {
      fprintf(stderr,"%s: can't run with both -T and -s options\n",prog_name);
      return 1;
    }

  if (tk.trial_duration_sec<=0 || tk.trial_duration_sec > 10000)
    {
      fprintf(stderr,"%s: trial duration should be between 0 and 10000 sec\nYou gave %lf\n",prog_name,tk.trial_duration_sec);
      return 1;
    }
  if(tk.pulse_duration_ms <0 || tk.pulse_duration_ms >1000)
    {
      fprintf(stderr,"%s: pulse_duration_ms should be between 0 and 1000 ms\nYou gave %lf\n",prog_name,tk.pulse_duration_ms);
      return 1;
    }
  if(laser_intensity_volt<=0 || laser_intensity_volt>4)
    {
      fprintf(stderr,"%s: voltage to control laser power should be between 0 and 4 volt\nYou gave %lf\n",prog_name,laser_intensity_volt);
      return 1;
    }
  if (stimulation_theta_phase<0 || stimulation_theta_phase>=360)
    {
      fprintf(stderr,"%s: stimulation phase should be from 0 to 360\nYou gave %lf\n",prog_name,stimulation_theta_phase);
      return 1;
    }
  if(with_R_opt==1 || with_D_opt==1)
    {
      if(minimum_interval_ms >= maximum_interval_ms)
	{
	  fprintf(stderr,"%s: minimum_interval_ms needs to be smaller than maximum_intervals_ms\nYou gave %lf and %lf\n",prog_name,minimum_interval_ms,maximum_interval_ms);
	  return 1;
	}
      if(minimum_interval_ms<0)
	{
	  fprintf(stderr,"%s: minimum_interval_ms should be larger than 0\nYou gave %lf\n",prog_name,minimum_interval_ms);
	  return 1;
	}
    }
  if(with_s_opt==1)
    {
      if(swr_power_threshold<0|| swr_power_threshold>20)
	{
	  fprintf(stderr,"%s: swr_power_threashold should be between 0 and 20\n. You gave %lf\n", prog_name, swr_power_threshold);
	  return 1;
	}
    }
  if(with_o_opt==1)
    {
      // we then need -c
      if (with_c_opt==0)
	{
	  fprintf(stderr,"%s: option -o requires the use of -c.\n", prog_name);
	  return 1;
	}
      // we need either -s or -t
      if (with_s_opt==0&& with_t_opt==0)
	{
	  fprintf(stderr,"%s: option -o requires the use of either -s or -t.\n", prog_name);
	   return 1;
	}
      if (with_s_opt==1)
	{
	  if(with_x_opt==0|| with_y_opt==0)
	    {
	      fprintf(stderr,"%s: option -o used with -s requires the use of both -x and -y.\n", prog_name);
	      return 1;
	    }
	}
      if(with_t_opt==1)
	{
	  if(with_x_opt==0)
	    {
	      fprintf(stderr,"%s: option -o used with -t requires the use of -x.\n", prog_name);
	      return 1;
	    }
	}
    }
  if(with_c_opt)
    {
      if(channels_in_dat_file < 0)
	{
	  fprintf(stderr,"%s: channels_in_dat_file should be larger than 0 but is %d.\n", prog_name,channels_in_dat_file);
	  return 1;
	}
    }
  if(with_x_opt)
    {
      if(offline_channel<0 || offline_channel>=channels_in_dat_file)
	{
	  fprintf(stderr,"%s: offline_channel should be from 0 to %d but is %d.\n", prog_name,channels_in_dat_file-1,offline_channel);
	  return 1;
	}
    }
  if(with_y_opt)
    {
      if(swr_offline_reference<0 || swr_offline_reference >= channels_in_dat_file)
	{
	  fprintf(stderr,"%s: swr_offline_reference should be from 0 to %d but is %d.\n", prog_name,channels_in_dat_file-1,swr_offline_reference);
	  return 1;
	}
    }
  if(with_C_opt)
    {
      if(swr_convolution_peak_threshold>20||swr_convolution_peak_threshold<0)
	{
	  fprintf(stderr,"%s: swr_convolution_peak_threshold should be between 0 and 20 but is %lf.\n", prog_name,swr_convolution_peak_threshold);
	  return 1;
	}
    }
  if(with_d_opt)
    {
      if(device_index_for_stimulation<0)
	{
	  fprintf(stderr,"%s: device_index_for_stimulation should not be negative but is %d.\n", prog_name,device_index_for_stimulation);
	  return 1;
	}
    }
  
  /***********************************************
      declare the program as a reat time program 
      the user will need to be root to do this
  *************************************************/
  if (with_o_opt==0) // don't do it if you work offline
    {
      struct sched_param param;
      param.sched_priority = MY_PRIORITY;
      if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) 
	{
	  fprintf(stderr,"%s : sched_setscheduler failed\n",prog_name);
	  fprintf(stderr,"Do you have permission to run real-time applications? You might need to be root or use sudo\n");
	  return 1;
	}
      // Lock memory 
      if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) 
	{
	  fprintf(stderr,"%s: mlockall failed",prog_name);
	  return 1;
	}
      // Pre-fault our stack 
      unsigned char dummy[MAX_SAFE_STACK];
      memset(&dummy, 0, MAX_SAFE_STACK);
    }
      
  /*********************************************************
     code to initialize the AD card drivers and our interface
  **********************************************************/
  if (with_o_opt==0)
    { // only done when working online
      if (comedi_interface_init(&comedi_inter)==-1)
	{
	  fprintf(stderr,"%s could not initialize comedi_interface\n",prog_name);
	  return 1;
	}

      // set the comedi_interface so it tries to get data every 2 ms
      comedi_interface_set_inter_acquisition_sleeping_time(&comedi_inter,COMEDI_INTERFACE_ACQUISITION_SLEEP_TIME_MS);
      // set the first comedi device so that is sample NUMBER_SAMPLED_CHANNEL_DEVICE_0 times channel BRAIN_CHANNEL_1
      // useful during stimulation based on brain activity
      // buffer on the card will fill up more quickly and we get access to it more frequently
      for(i=0;i<NUMBER_SAMPLED_CHANNEL_DEVICE_0;i++)
	{
	  channel_list[i]=BRAIN_CHANNEL_1;
	  // for swr detection we need both BRAIN_CHANNEL_1 and BRAIN_CHANNEL_2
	  // they will be available in channel 0 and 1 of the comedi_interface, respectively
	  if(i==1&&with_s_opt==1)
	    {
	      channel_list[i]=BRAIN_CHANNEL_2;
	    }
	}
      comedi_interface_set_sampled_channels(&comedi_inter,0, NUMBER_SAMPLED_CHANNEL_DEVICE_0,channel_list);
      
      // make sure there is a valid analog output subdevice on the acquisition card
      if(comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output==-1)
	{
	  fprintf(stderr,"%s: there is no valid analog output subdevice on device %d\n",prog_name,device_index_for_stimulation);
	  return 1;
	}
      // make sure we have at least 2 channels for analog output: intensity and pulse of laser
      if (comedi_inter.dev[device_index_for_stimulation].number_channels_analog_output<2)
	{
	  fprintf(stderr, "%s: subdevice_analog_output has less than 2 channels\n",prog_name);
	  return 1;
	}
      // check if laser intensity is outside of range for the analog output subdevice
      if (comedi_inter.dev[device_index_for_stimulation].voltage_max_output<laser_intensity_volt)
	{
	  fprintf(stderr, "%s: subdevice_analog_output max voltage is smaller than laser intensity\n",prog_name);
	  return 1;
	}
      // check if laser pulse is outside of range for the analog output subdevice
      if (comedi_inter.dev[device_index_for_stimulation].voltage_max_output<pulse_volt)
	{
	  fprintf(stderr, "%s: subdevice_analog_output max voltage is smaller than laser pulse\n",prog_name);
	  return 1;
	}
      if(with_s_opt==1)
	{
	  if(comedi_inter.dev[device_index_for_stimulation].number_channels_analog_input<2)
	    {
	      fprintf(stderr, "%s: number of analog input channels should be at least 2 to do swr stimulation\n",prog_name);
	      return 1;
	    }
	}
      
      // get the value for the laser intensity and pulse
      comedi_baseline=comedi_from_phys(baseline_volt,
				       comedi_inter.dev[device_index_for_stimulation].range_output_array[comedi_inter.dev[device_index_for_stimulation].range_output],
				       comedi_inter.dev[device_index_for_stimulation].maxdata_output);
      comedi_intensity=comedi_from_phys(laser_intensity_volt,
					comedi_inter.dev[device_index_for_stimulation].range_output_array[comedi_inter.dev[device_index_for_stimulation].range_output],
					comedi_inter.dev[device_index_for_stimulation].maxdata_output);
      comedi_pulse=comedi_from_phys(pulse_volt,
				    comedi_inter.dev[device_index_for_stimulation].range_output_array[comedi_inter.dev[device_index_for_stimulation].range_output],
				    comedi_inter.dev[device_index_for_stimulation].maxdata_output);
      
      // set the intensity channel
      comedi_data_write(comedi_inter.dev[device_index_for_stimulation].comedi_dev,
			comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output, 
			CHANNEL_FOR_LASER_INTENSITY,
			comedi_inter.dev[device_index_for_stimulation].range_output,
			comedi_inter.dev[device_index_for_stimulation].aref,
			comedi_intensity);

      // set the pulse channel to baseline
      comedi_data_write(comedi_inter.dev[device_index_for_stimulation].comedi_dev,
			comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output,
			CHANNEL_FOR_PULSE,
			comedi_inter.dev[device_index_for_stimulation].range_output,
			comedi_inter.dev[device_index_for_stimulation].aref,
			comedi_baseline);

    }
  
  
  /*****************************************
     code to initialize the dat file if needed
  ********************************************/
  if(with_o_opt==1)
    {
      if((init_data_file_si(&data_file,dat_file_name,channels_in_dat_file))!=0)
	{
	  fprintf(stderr, "%s: problem in initialisation of dat file\n",prog_name);
	  return 1;
	}
    }
  
  /*****************************************************************************
   section to do train a stimulation at a given frequency or at random intervals
  ******************************************************************************/
  if(with_T_opt==1|| with_R_opt==1)
    {
      if (with_T_opt==1)
	{ // set the time variables for train stimulation at a fixed frequency
	  tk.inter_pulse_duration_ms=1000/train_frequency_hz;
	  tk.inter_pulse_duration=set_timespec_from_ms(tk.inter_pulse_duration_ms);
	  tk.end_to_start_pulse_ms=tk.inter_pulse_duration_ms-tk.pulse_duration_ms;
	  if (tk.end_to_start_pulse_ms<=0)
	    {
	      fprintf(stderr,"%s: tk.end_to_start_pulse_ms <= 0, %lf\n",prog_name,tk.end_to_start_pulse_ms);
	      fprintf(stderr,"Is the length of the pulse longer than the inter pulse duration?\n");
	      return 1;
	    }
	}
      if(with_R_opt==1)
	{
	  { // check that the time variables make sense for the smallest possible interval
	    tk.inter_pulse_duration_ms=minimum_interval_ms;
	    tk.inter_pulse_duration=set_timespec_from_ms(tk.inter_pulse_duration_ms);
	    tk.end_to_start_pulse_ms=tk.inter_pulse_duration_ms-tk.pulse_duration_ms;
	    if (tk.end_to_start_pulse_ms<=0)
	      {
		fprintf(stderr,"%s: tk.end_to_start_pulse_ms <= 0, %lf\n",prog_name,tk.end_to_start_pulse_ms);
		fprintf(stderr,"The length of the pulse longer than the minimum_interval_ms\n");
		return 1;
	      }
	  }
	}

      // get time at beginning of trial
      clock_gettime(CLOCK_REALTIME, &tk.time_beginning_trial);
      clock_gettime(CLOCK_REALTIME, &tk.time_now);
      clock_gettime(CLOCK_REALTIME, &tk.time_last_stimulation);
      tk.elapsed_beginning_trial=diff(&tk.time_beginning_trial,&tk.time_now);
      while(tk.elapsed_beginning_trial.tv_sec < tk.trial_duration_sec) // loop until the trial is over
	{
	  clock_gettime(CLOCK_REALTIME,&tk.time_last_stimulation); 
	  // start the pulse
	  comedi_data_write(comedi_inter.dev[device_index_for_stimulation].comedi_dev,
			    comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output,
			    CHANNEL_FOR_PULSE,
			    0,
			    comedi_inter.dev[device_index_for_stimulation].aref,
			    comedi_pulse);
	  // wait
	  nanosleep(&tk.duration_pulse,&tk.req);
	  
	  // end of the pulse
	  comedi_data_write(comedi_inter.dev[device_index_for_stimulation].comedi_dev,
			    comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output,
			    CHANNEL_FOR_PULSE,
			    0,
			    comedi_inter.dev[device_index_for_stimulation].aref,
			    comedi_baseline);
	  
	 // get time now and the actual pulse duration
	  clock_gettime(CLOCK_REALTIME, &tk.time_end_last_stimulation);
	  tk.actual_pulse_duration=diff(&tk.time_last_stimulation,&tk.time_end_last_stimulation); 
	  if(with_R_opt==1)
	    { // if doing random stimulation, get the next interval
	      tk.inter_pulse_duration_ms=minimum_interval_ms+(rand() % (int)(maximum_interval_ms-minimum_interval_ms));
	      tk.inter_pulse_duration=set_timespec_from_ms(tk.inter_pulse_duration_ms);
	    }
	  fprintf(stderr,"interval: %lf ms\n",tk.inter_pulse_duration_ms);
	  // calculate how long we need to sleep until the next pulse
	  tk.end_to_start_pulse_duration=diff(&tk.actual_pulse_duration,&tk.inter_pulse_duration);
	  
	  // sleep until the next pulse, note that this will overshoot by the time of 4 lines of code! (almost nothing)
	  nanosleep(&tk.end_to_start_pulse_duration,&tk.req); 
	  
	  // get the time since the beginning of the trial
	  clock_gettime(CLOCK_REALTIME, &tk.time_now);
	  tk.elapsed_beginning_trial=diff(&tk.time_beginning_trial,&tk.time_now);
	}
    }
  
 
  /*************************************************
   section to do the theta stimulation
  *************************************************/
  if(with_t_opt==1)
    {
      if(fftw_interface_theta_init(&fftw_inter)==-1)
	{
	  fprintf(stderr,"%s could not initialize fftw_interface_theta\n",prog_name);
	  return 1;
	}
      if(with_o_opt==1)
	{ // if get data from dat file, allocate memory to store short integer from dat file
	  if((data_from_file=(short *) malloc(sizeof(short)*fftw_inter.real_data_to_fft_size))==NULL)
	    {
	      fprintf(stderr,"%s: problem allocating memory for data_from_file\n",prog_name);
	      return 1;
	    }
	}
      tk.duration_refractory_period=set_timespec_from_ms(STIMULATION_REFRACTORY_PERIOD_THETA_MS);
      
      // start the acquisition thread, which will run in the background until comedi_inter.is_acquiring is set to 0
#ifdef DEBUG_THETA
      fprintf(stderr,"starting acquisition\n");
#endif

      
      if(comedi_interface_start_acquisition(&comedi_inter)==-1)
	{
	  fprintf(stderr,"%s could not start comedi acquisition\n",prog_name);
	  return 1;
	}
 #ifdef DEBUG_THETA
      // to check the intervals before getting new data.
      clock_gettime(CLOCK_REALTIME, &tk.time_previous_new_data);
      long int counter=0;
#endif

      // get time at beginning of trial
      clock_gettime(CLOCK_REALTIME, &tk.time_beginning_trial);
      clock_gettime(CLOCK_REALTIME, &tk.time_now);
      clock_gettime(CLOCK_REALTIME, &tk.time_last_stimulation);
      tk.elapsed_beginning_trial=diff(&tk.time_beginning_trial,&tk.time_now);
      
#ifdef DEBUG_THETA
      fprintf(stderr,"start trial loop\n");
#endif
      while(tk.elapsed_beginning_trial.tv_sec < tk.trial_duration_sec) // loop until the trial is over
	{
	  // check if acquisition loop is still running, could have stop because of buffer overflow
	  if(comedi_inter.is_acquiring==0)
	    {
	      fprintf(stderr,"comedi acquisition was stopped, theta stimulation not possible\n");
	      return 1;
	    }
	  
	  if (with_o_opt==0) // get data from comedi acquisition device
	    {
	      // if no new data is available or not enough data to do a fftw
	      while((comedi_inter.number_samples_read<=last_sample_no) || (comedi_inter.number_samples_read < fftw_inter.real_data_to_fft_size) )
		{
		  // sleep a bit and check for new data
		  nanosleep(&tk.duration_sleep_when_no_new_data,&tk.req);
		}
	      // copy the last data from the comedi_interface into the fftw_inter.signal_data
	      last_sample_no=comedi_interface_get_last_data_one_channel(&comedi_inter,BRAIN_CHANNEL_1,fftw_inter.real_data_to_fft_size,fftw_inter.signal_data,&tk.time_last_acquired_data);
	      if(last_sample_no==-1)
		{
		  fprintf(stderr,"error returned by comedi_interface_get_data_one_channe()\n");
		  return 1;
		}
	    }
	  else // get data from a dat file
	    {
	      if(last_sample_no==0)
		{
		  // fill the buffer with the beginning of the file
		  if((data_file_si_get_data_one_channel(&data_file, offline_channel, data_from_file,0,fftw_inter.real_data_to_fft_size))!=0)
		    {
		      fprintf(stderr,"%s, problem with data_file_si_get_data_one_channel, first index: %d, last index: %d\n",prog_name,0,fftw_inter.real_data_to_fft_size);
		      return 1;
		    }
		  last_sample_no=fftw_inter.real_data_to_fft_size;
		}
	      else
		{
		  // get data further on in the dat file
		  if((data_file_si_get_data_one_channel(&data_file, offline_channel, data_from_file,last_sample_no+new_samples_per_read_operation-fftw_inter.real_data_to_fft_size,last_sample_no+new_samples_per_read_operation))!=0)
		    {
		      fprintf(stderr,"%s, problem with data_file_si_get_data_one_channel, first index: %ld, last index: %ld\n",prog_name,last_sample_no+new_samples_per_read_operation-fftw_inter.real_data_to_fft_size,last_sample_no+new_samples_per_read_operation);
		      return 1;
		    }
		  last_sample_no= last_sample_no+new_samples_per_read_operation;
		}
	      // copy the short int array to double array
	      for (i=0; i < fftw_inter.real_data_to_fft_size;i++)
		{
		  fftw_inter.signal_data[i]=data_from_file[i];
		}
	    }
#ifdef DEBUG_THETA
	  clock_gettime(CLOCK_REALTIME, &tk.time_current_new_data);
	  tk.duration_previous_current_new_data=diff(&tk.time_previous_new_data,&tk.time_current_new_data);
	  fprintf(stderr,"%ld untill sample %ld, last_sample_no: %ld  with interval %lf(us)\n",counter++,comedi_inter.number_samples_read, last_sample_no, tk.duration_previous_current_new_data.tv_nsec/1000.0);
	  tk.time_previous_new_data=tk.time_current_new_data;
#endif
	  if(last_sample_no>=fftw_inter.real_data_to_fft_size)
	    { 
	      // filter for theta and delta
	      fftw_interface_theta_apply_filter_theta_delta(&fftw_inter);
	      
	      /* to see real and filtered signal
	      for(i=0;i < fftw_inter.real_data_to_fft_size;i++)
		{
		  printf("aa %lf, %lf\n",fftw_inter.filtered_signal_theta[i], fftw_inter.signal_data[i]);
		}
	      return 1;
	      */
	      // get the theta/delta ratio
	                                   
	      theta_delta_ratio=fftw_interface_theta_delta_ratio(&fftw_inter);
#ifdef DEBUG_THETA
	      fprintf(stderr,"theta_delta_ratio: %lf\n",theta_delta_ratio);
#endif
	      if (theta_delta_ratio>THETA_DELTA_RATIO)
		{
		  clock_gettime(CLOCK_REALTIME, &tk.time_now);
		  tk.elapsed_last_acquired_data=diff(&tk.time_last_acquired_data,&tk.time_now);
		  // get the phase
		  current_phase=fftw_interface_theta_get_phase(&fftw_inter, &tk.elapsed_last_acquired_data,theta_frequency); 
		  // phase difference between wanted and what it is now, from -180 to 180
		  phase_diff=phase_difference(current_phase,stimulation_theta_phase);
#ifdef DEBUG_THETA
		  fprintf(stderr,"stimulation_theta_phase: %lf current_phase: %lf phase_difference: %lf\n",stimulation_theta_phase,current_phase,phase_difference);
#endif
		  // if the absolute phase difference is smaller than the max_phase_difference
		  if(sqrt(phase_diff*phase_diff)<max_phase_diff)
		    { 
		      // if we are just before the stimulation phase, we nanosleep to be bang on the correct phase
		      if(phase_diff<0)
			{
			  tk.duration_sleep_to_right_phase=set_timespec_from_ms((0-phase_diff)*theta_degree_duration_ms);
			  nanosleep(&tk.duration_sleep_to_right_phase,&tk.req);
			}
		      clock_gettime(CLOCK_REALTIME,&tk.time_now);
		      tk.elapsed_last_stimulation=diff(&tk.time_last_stimulation,&tk.time_now);
		      
		      // if the laser refractory period is over
		      if(tk.elapsed_last_stimulation.tv_nsec>tk.duration_refractory_period.tv_nsec || 
			 tk.elapsed_last_stimulation.tv_sec>tk.duration_refractory_period.tv_sec )
			{
			  // stimulation time!!
			  clock_gettime(CLOCK_REALTIME,&tk.time_last_stimulation); 
						  
			  // start the pulse
			  comedi_data_write(comedi_inter.dev[device_index_for_stimulation].comedi_dev,
					    comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output,
					    CHANNEL_FOR_PULSE,
					    0,
					    comedi_inter.dev[device_index_for_stimulation].aref,
					    comedi_pulse);
			  // wait
			  nanosleep(&tk.duration_pulse,&tk.req);
			  
			  // end of the pulse
			  comedi_data_write(comedi_inter.dev[device_index_for_stimulation].comedi_dev,
					    comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output,
					    CHANNEL_FOR_PULSE,
					    0,
					    comedi_inter.dev[device_index_for_stimulation].aref,
					    comedi_baseline);
#ifdef DEBUG_THETA
			  fprintf(stderr,"interval from last stimulation: %ld (us)\n",tk.elapsed_last_stimulation.tv_nsec/1000);
#endif
			}
		    }
		}
	      
	      clock_gettime(CLOCK_REALTIME, &tk.time_now);
	      tk.elapsed_last_acquired_data=diff(&tk.time_last_acquired_data,&tk.time_now);
	      
	      // will stop the trial
	      //	      tk.elapsed_beginning_trial.tv_sec = tk.trial_duration_sec;
	    }
	  clock_gettime(CLOCK_REALTIME, &tk.time_now);
	  tk.elapsed_beginning_trial=diff(&tk.time_beginning_trial,&tk.time_now);
	}
      
      // this will stop the acquisition thread
       if(comedi_interface_stop_acquisition(&comedi_inter)==-1)
	{
	  fprintf(stderr,"%s could not stop comedi acquisition\n",prog_name);
	  return 1;
	}
       
       // free the memory used by fftw_inter    
      fftw_interface_theta_free(&fftw_inter);
    }
     





  /****************************************
   ****************************************
        section to do swr stimulation
   ****************************************
   **************************************/
  if(with_s_opt==1)
    {
      if(fftw_interface_swr_init(&fftw_inter_swr)==-1) // initialize the fftw interface
	{
	  fprintf(stderr,"%s could not initialize fftw_interface_swr\n",prog_name);
	  return 1;
	}
      if(with_o_opt==1) // the program is running from a data file. allocate memory for 2 arrays.
	{ 
	  if((data_from_file=(short *) malloc(sizeof(short)*fftw_inter_swr.real_data_to_fft_size))==NULL)
	    {
	      fprintf(stderr,"%s: problem allocating memory for data_from_file\n",prog_name);
	      return 1;
	    }
	  if((ref_from_file=(short *) malloc(sizeof(short)*fftw_inter_swr.real_data_to_fft_size))==NULL)
	    {
	      fprintf(stderr,"%s: problem allocating memory for ref_from_file\n",prog_name);
	      return 1;
	    }
	}
      if(with_o_opt==0)
	{
	  if(comedi_interface_start_acquisition(&comedi_inter)==-1) // initialize the comedi interface
	    {
	      fprintf(stderr,"%s could not start comedi acquisition\n",prog_name);
	      return 1;
	    }
	}



#ifdef DEBUG_SWR
      // to check the intervals before getting new data.
      clock_gettime(CLOCK_REALTIME, &tk.time_previous_new_data);
      long int counter=0;
#endif
      // get time at beginning of trial
      clock_gettime(CLOCK_REALTIME, &tk.time_beginning_trial);
      clock_gettime(CLOCK_REALTIME, &tk.time_now);
      clock_gettime(CLOCK_REALTIME, &tk.time_last_stimulation);
      tk.elapsed_beginning_trial=diff(&tk.time_beginning_trial,&tk.time_now);
      
      
      // loop until the time is over
#ifdef DEBUG_SWR
      // to check the intervals before getting new data.
      fprintf(stderr,"starting trial loop for swr\n");
#endif
      while(tk.elapsed_beginning_trial.tv_sec<tk.trial_duration_sec) // while (the trial is not over yet)
	{
	  if(with_o_opt==0) // get data from comedi acquisition interface
	    {
	      // check if acquisition loop is still running, could have stop because of buffer overflow
	      if(comedi_inter.is_acquiring==0)
		{
		  fprintf(stderr,"comedi acquisition was stopped, swr stimulation not possible\n");
		  return 1;
		}
	      // if no new data is available or not enough data to do a fftw
	      while((comedi_inter.number_samples_read<=last_sample_no) || (comedi_inter.number_samples_read < fftw_inter_swr.real_data_to_fft_size) )
		{
		  // sleep a bit and check for new data
		  nanosleep(&tk.duration_sleep_when_no_new_data,&tk.req);
		}
	      // copy the last data from the comedi_interface into the fftw_inter_swr.signal_data and fftw_inter_swr.ref_signal_data
	      last_sample_no=comedi_interface_get_last_data_two_channels(&comedi_inter,BRAIN_CHANNEL_1,BRAIN_CHANNEL_2,fftw_inter_swr.real_data_to_fft_size,fftw_inter_swr.signal_data,fftw_inter_swr.ref_signal_data,&tk.time_last_acquired_data);
	    }
	  else // get data from a dat file
	    {
	      if(last_sample_no==0)
		{
		  // fill the buffer with the beginning of the file
		  if((data_file_si_get_data_one_channel(&data_file, offline_channel, data_from_file, 0,fftw_inter_swr.real_data_to_fft_size-1))!=0)
		    {
		      fprintf(stderr,"%s, problem with data_file_si_get_data_one_channel, first index: %d, last index: %d\n",prog_name,0,fftw_inter_swr.real_data_to_fft_size-1);
		      return 1;
		    }
		  if((data_file_si_get_data_one_channel(&data_file, swr_offline_reference, ref_from_file,0,fftw_inter_swr.real_data_to_fft_size-1))!=0)
		    {
		      fprintf(stderr,"%s, problem with data_file_si_get_data_one_channel, first index: %d, last index: %d\n",prog_name,0,fftw_inter_swr.real_data_to_fft_size-1);
		      return 1;
		    }
		  last_sample_no=fftw_inter_swr.real_data_to_fft_size-1;
		}
	      else
		{
		  // check if we still have data in the file
		  if(data_file.num_samples_in_file<last_sample_no+new_samples_per_read_operation-1)
		    {
		      // no more data in file, exit the trial loop
		      break;
		    }
		  // update the buffer by adding new data in it
		  if((data_file_si_get_data_one_channel(&data_file, offline_channel, 
							data_from_file,
							last_sample_no+new_samples_per_read_operation-fftw_inter_swr.real_data_to_fft_size,
							last_sample_no+new_samples_per_read_operation))!=0)
		    {
		      fprintf(stderr,"%s, problem with data_file_si_get_data_one_channel, first index: %ld, last index: %ld\n",
			      prog_name,
			      last_sample_no+new_samples_per_read_operation-fftw_inter_swr.real_data_to_fft_size,
			      last_sample_no+new_samples_per_read_operation-1);
		      return 1;
		    }
		  if((data_file_si_get_data_one_channel(&data_file, swr_offline_reference, 
							ref_from_file,
							last_sample_no+new_samples_per_read_operation-fftw_inter_swr.real_data_to_fft_size,
							last_sample_no+new_samples_per_read_operation))!=0)
		    {
		      fprintf(stderr,"%s, problem with data_file_si_get_data_one_channel, first index: %ld, last index: %ld\n",
			      prog_name,
			      last_sample_no+new_samples_per_read_operation-fftw_inter_swr.real_data_to_fft_size,
			      last_sample_no+new_samples_per_read_operation-1);
		      return 1;
		    }
		  last_sample_no= last_sample_no+new_samples_per_read_operation-1;
		}
	      
	      // copy the short int array to double array
	      for (i=0; i < fftw_inter_swr.real_data_to_fft_size;i++)
		{
		  fftw_inter_swr.signal_data[i]=data_from_file[i];
		  fftw_inter_swr.ref_signal_data[i]=ref_from_file[i];
		}
	    }
	  if(last_sample_no==-1)
	    {
	      fprintf(stderr,"error returned by comedi_interface_get_last_data_two_channels()\n");
	      return 1;
	    }
	  if(last_sample_no>=fftw_inter_swr.real_data_to_fft_size)
	    { 
	      
	      // do differential, filtering, and convolution
	      fftw_interface_swr_differential_and_filter(&fftw_inter_swr);
	      
	      // get the power
	      swr_power=fftw_interface_swr_get_power(&fftw_inter_swr);
	      
	      // get the peak in the convoluted signal
	      swr_convolution_peak=fftw_interface_swr_get_convolution_peak(&fftw_inter_swr);
	      
	      if(swr_power>swr_power_threshold && swr_convolution_peak > swr_convolution_peak_threshold)
		{
#ifdef DEBUG_SWR
		  printf("%ld\n",last_sample_no);
		  for(i=0;i < fftw_inter_swr.real_data_to_fft_size;i++)
		    {
		      printf("%lf %lf %lf\n",fftw_inter_swr.signal_data[i],fftw_inter_swr.filtered_signal_swr[i], fftw_inter_swr.convoluted_signal[i]);
		    }
		  fprintf(stderr,"check no: %ld, last_sample_no: %ld  sleep time: %.2lf (us), processing time: %.2lf, diff_between_two_get_data: %.2lf power: %.2lf threshold: %.2lf convolution_peak: %.2lf\n",counter++,last_sample_no, tk.duration_sleep_between_swr_processing.tv_nsec/1000.0, tk.elapsed_from_acquisition.tv_nsec/1000.0,tk.duration_previous_current_new_data.tv_nsec/1000.0,swr_power,swr_power_threshold,swr_convolution_peak);  		  
		  return 1;
#endif
		  // stimulation time!!
		  clock_gettime(CLOCK_REALTIME,&tk.time_last_stimulation); 
		  if(with_o_opt==0) // not working with a data file
		    {

		      // do we need a delay before swr stimulation
		      if(with_D_opt)
			{
			  tk.swr_delay_ms=minimum_interval_ms+(rand() % (int)(maximum_interval_ms-minimum_interval_ms));
			  tk.swr_delay=set_timespec_from_ms(tk.swr_delay_ms);
			  fprintf(stderr,"swr delay: %lf ms\n",tk.swr_delay_ms);
			  // sleep until the next pulse, note that this will overshoot by the time of 1 lines of code! (almost nothing)
			  nanosleep(&tk.swr_delay,&tk.req);
			}
		    
		      // start the pulse
		      comedi_data_write(comedi_inter.dev[device_index_for_stimulation].comedi_dev,
					comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output,
					CHANNEL_FOR_PULSE,
					0,
					comedi_inter.dev[device_index_for_stimulation].aref,
					comedi_pulse);
		      // wait
		      nanosleep(&tk.duration_pulse,&tk.req);
		      
		      // end of the pulse
		      comedi_data_write(comedi_inter.dev[device_index_for_stimulation].comedi_dev,
					comedi_inter.dev[device_index_for_stimulation].subdevice_analog_output,
					CHANNEL_FOR_PULSE,
					0,
					comedi_inter.dev[device_index_for_stimulation].aref,
					comedi_baseline);
		    }
		  if(with_o_opt==1) // working with data file
		    {
		      //printf("%ld %lf %lf %lf %ld\n",last_sample_no,swr_power,fftw_inter_swr.mean_power,fftw_inter_swr.std_power,fftw_inter_swr.number_segments_analysed);
		      // print the res value of the stimulation time
		      printf("%ld\n",last_sample_no);
		      // move forward in file by the duration of the pulse
		      last_sample_no=last_sample_no+(tk.pulse_duration_ms*DEFAULT_SAMPLING_RATE/1000);
		    }
		}  
	    }
	  // sleep so that the interval between two calculation of power is approximately tk.interval_duration_between_swr_processing
	  clock_gettime(CLOCK_REALTIME, &tk.time_now);
	  tk.elapsed_from_acquisition=diff(&tk.time_last_acquired_data,&tk.time_now);
	  tk.duration_sleep_between_swr_processing=diff(&tk.elapsed_from_acquisition,&tk.interval_duration_between_swr_processing);
	  nanosleep(&tk.duration_sleep_between_swr_processing,&tk.req);
	  tk.elapsed_beginning_trial=diff(&tk.time_beginning_trial,&tk.time_now);
	  
#ifdef DEBUG_SWR
	  tk.duration_previous_current_new_data=diff(&tk.time_previous_new_data,&tk.time_last_acquired_data);
	  tk.time_previous_new_data=tk.time_last_acquired_data;
	  fprintf(stderr,"1check no: %ld, last_sample_no: %ld  sleep time: %.2lf (us), processing time: %.2lf, diff_between_two_get_data: %.2lf power: %.2lf threshold: %.2lf convolution_peak: %.2lf\n",counter++,last_sample_no, tk.duration_sleep_between_swr_processing.tv_nsec/1000.0, tk.elapsed_from_acquisition.tv_nsec/1000.0,tk.duration_previous_current_new_data.tv_nsec/1000.0,swr_power,swr_power_threshold,swr_convolution_peak);
#endif
	} // stimulation trial is over
      
      fftw_interface_swr_free(&fftw_inter_swr);
      if(with_o_opt==0)
	{
	  if(comedi_interface_stop_acquisition(&comedi_inter)==-1)
	    {
	      fprintf(stderr,"%s could not stop comedi acquisition\n",prog_name);
	      return 1;
	    }
	}
    }
  
  
  if (with_o_opt==0)
    {
      // free the memory used by comedi_inter
      comedi_interface_free(&comedi_inter);
    }
  // free the memory for dat file data, if running with offline data
  if(with_o_opt==1)
    {
      if((clean_data_file_si(&data_file))!=0)
	{
	  fprintf(stderr, "%s: problem with clean_data_file_si\n",prog_name);
	  return 1;
	}
      free(data_from_file);
      if (with_s_opt)
	{
	  free(ref_from_file);
	}
    }
  return 0;
} 

void print_options()
{
  printf("possible options:\n");
  printf("--theta <theta_phase> or -t\t\t: stimulation at the given theta phases\n"); 
  printf("--swr <power_threashold> or -s\t\t: stimulation during swr, give the power detection threashold (z score) as option argument\n");
  printf("--swr_convolution_peak_threshold <convolution_threshold> of -C\t\t: give the detection threshold (z score) as option argument\n");
  printf("--train <frequency_hz> or -T\t\t:  train of stimulations at a fix frequency\n");
  printf("--random or -R \t\t\t\t: train of stimulations with random intervals, use with -m and -M\n");
  printf("--minimum_interval_ms <min_ms> or -m\t: set minimum interval for stimulation at random intervals (-R) or swr delay (-s -D)\n");
  printf("--maximum_interval_ms <max_ms> or -M\t: set maximum interval for stimulation at random intervals (-R) or swr delay (-s -D)\n");
  printf("--offline <dat_file_name> or -o\t\t: use a .dat file as input data. You also need option -c, together with either -s or -t, to work with -o\n");
  printf("--channels_in_dat_file <number> or -c\t: give the number of channels in the dat file. Use only when working offline from a dat file (-o or --offline)\n");
  printf("--offline_channel <number> or -x\t: give the channel on which swr detection is done when working offline from a dat file (-o and -s)\n");
  printf("--swr_offline_reference <number> or -y\t: give the reference channel for swr detection when working offline from a dat file (-o and -s)\n");
  printf("--swr_convolution_peak_threshold <number> or -C\t: give an additional treshold for swr detection\n");
  printf("--output_device_for_stimulation <number> or -d\t: give the comedi device to use\n");
  printf("--delay_swr or -D\t: add a delay between swr detection and beginning of stimulation. Use -m and -M to set minimum and maximum delay\n");
  printf("--version or -v\t\t\t\t: print the program version\n");
  printf("--help or -h\t\t\t\t: will print this text\n");
  return;
}
