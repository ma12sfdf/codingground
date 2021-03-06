// AMDemod.cpp: implementation of the CAMDemod class.
//
//////////////////////////////////////////////////////////////////////


#include "AMDemod.h"

#ifndef _PC_VER
#include <tmlib\dprintf.h>
#include <ops\custom_defs.h>
#else
#include "../pc_types.h"
#endif

#include "../message_types.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

//#include "../myprintf.h"

#define DC_REMOVAL_CONSTANT	0.01f
#define DC_AGC_CONSTANT 0.0002f
#define AC_POWER_CONSTANT 0.0002f
#define THRESHOLD	0.0001f
#define AM_OUTPUT_CARRIER_LEVEL 0.5f
#define AM_OUTPUT_AC_LEVEL 0.1f
#define AM_KNEE 0.001f


#ifdef _CAB
	#define AUDIO_STEP 0
#elif _CAB_II
	#define AUDIO_STEP 0
#elif _PC_VER
	#define AUDIO_STEP 0
#elif _SIM
	#define AUDIO_STEP 0
#elif _VIDEO
	#define AUDIO_STEP 6
#endif

//#define CLIP_VALUE 32766

float CAMDemod::m_a_3[FILTER_COEFFICIENTS_3K]={

   1.00000000000000f,
  -6.38739651854714f,
  17.74783512043479f,
 -27.78821300031277f,
  26.46577424253861f,
 -15.32791548845794f,
   4.99759643943539f,
  -0.70762283563159f

};

float CAMDemod::m_b_3[FILTER_COEFFICIENTS_3K]={

   0.00049888571507f,
  -0.00183652551336f,
   0.00276621257220f,
  -0.00139959304424f,
  -0.00139959304424f,
   0.00276621257220f,
  -0.00183652551336f,
   0.00049888571507f

};



float CAMDemod::m_a_5[FILTER_COEFFICIENTS_5K]={
 1.00000000000000f,
-5.70305795952220f,
14.59963647102280f,
-21.63339109715538f,
19.98527554681267f,
-11.49504751461015f,
3.81051623608914f,
-0.56204030967292f


};

 

float CAMDemod::m_b_5[FILTER_COEFFICIENTS_5K]={

0.00116418724087f,
-0.00227288576475f, 
0.00310977293623f,
-0.00105538793037f,
-0.00105538793037f,
0.00310977293623f,
-0.00227288576475f,
0.00116418724087f

};




//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAMDemod::CAMDemod()
{

	m_data_in = new float[CAPTURE_BUFFER_POINTS_U];

	m_free_symbolnum=0;

	m_spdif_frame=0;

	m_audio_index = 0;

	int i;

/*	for(i=0; i< FILTER_COEFFICIENTS_3K-1 ;i++){
		m_x_buffer[i]=0.0f;
		m_y_buffer[i]=0.0f;
	}*/

	m_dc_agc_filter=1.0f;
	m_dc_removal_filter=1.0f;
	m_ac_power_filter=1.0f;


	m_count=0;

	m_offset=1.0f;
//	m_filt_out = new float[CAPTURE_BUFFER_POINTS_AM];
	m_pAudio=new float[AUDIO_POINTS_SUB];

	memset(m_pAudio, 0, AUDIO_POINTS_SUB * sizeof(float) );
//	memset(m_filt_out, 0, CAPTURE_BUFFER_POINTS_AM*sizeof(float) );

	m_local_signal_level=new float[SYMS_PER_FRAME_D];
	m_signal_level=new float[SYMS_PER_FRAME_D];
	m_peak_level=new float[SYMS_PER_FRAME_D];

	for(i=0; i< SYMS_PER_FRAME_D ;i++)
	{
		m_local_signal_level[i]=-200.0f;
		m_signal_level[i]=-200.0f;
		m_peak_level[i]=-200.0f;
	}

	m_signal_level_av=-200.0f;
	m_peak_level_av=-200.0f;

	m_left_volume=1.0f;
	m_right_volume=1.0f;

	m_leftPeak=0;
	m_rightPeak=0;

//	m_channel_filter.SetSpecOcc(5);
	m_channel_filter.SetSpecOcc(1);

}

CAMDemod::~CAMDemod()
{
	

//	delete [] m_pResamp;
//	delete [] m_filt_out;
	delete [] m_pAudio;
	delete [] m_local_signal_level;
	delete [] m_peak_level;
	delete [] m_signal_level;
	delete [] m_data_in;
}


TM_BOOL CAMDemod::envelope_det(float *in_buffer, float *out_buffer, Int16* OutMemAudio,
							Int16* OutMemData, Int16* OutMemSpdo, Int32 length)  
  
{
	
//	static float offset;//offset of the complex signal: real part (input_buffer[2*i]) / imaginary part (input_buffer[2*i+1])
	int i, j, audio_index=0, data_index1=0, data_index2=0;



	char string[100];

//	m_count++;

	float* restrict input_buffer = in_buffer;
	float* restrict output_buffer = out_buffer;

	//work out where to place the monitoring data in the output array
//	Int16* restrict trigger	= &OutMemData[NUMBER_OF_AUDIO_CHANNELS*m_free_symbolnum*TOTAL_SYMBOL_COMPLEX_D+2];
	Int16* restrict monit_pos1=&OutMemData[NUMBER_OF_AUDIO_OUT_CHANNELS*m_free_symbolnum*TOTAL_SYMBOL_COMPLEX_D+4];
	Int16* restrict monit_pos2=&OutMemData[NUMBER_OF_AUDIO_OUT_CHANNELS*m_free_symbolnum*TOTAL_SYMBOL_COMPLEX_D+6];
	Int16* restrict audio_pos=&OutMemAudio[NUMBER_OF_AUDIO_OUT_CHANNELS*m_free_symbolnum*TOTAL_SYMBOL_COMPLEX_D];
	float input1=0.0f,input2=0.0f;
//	float peak1=0.0f, peak2=0.0f;
//	int peak_pos1=0, peak_pos2=1;

	//float scale=1.0f;
	float dc_scale = 16000.0f;
	float inv_threshold = 1/ THRESHOLD;

	float out1=0.0f,out2=0.0f;

	static float ph = 0.0f;

	if(m_count > 50000)
	{
//		tNativeAcc out =fixed_sqrt(10000);
		int temp =0;
	}

	for (i=0, j=0; i <	2*length; i+=2, j++)
	//for (i=0, j=0;i<CAPTURE_BUFFER_POINTS_AM;i+=2, j++)
	{
		input1=input_buffer[i];
		input2=input_buffer[i+1];

		//calculation of the absolute value of the complex signal
		//output_buffer[i] = (float) (( FSQRT(input1*input1+input2*input2) ) ); 
		float val = (float) (( FSQRT(input1*input1+input2*input2) ) ); 


		 //calculation of the average of the offset
		//m_offset =(float) ((DC_CONSTANT*output_buffer[i]) + (1.0f-DC_CONSTANT)*m_offset);
		m_dc_agc_filter =(float) ((DC_AGC_CONSTANT*val) + (1.0f-DC_AGC_CONSTANT)*m_dc_agc_filter);
		m_dc_removal_filter =(float) ((DC_REMOVAL_CONSTANT*val) + (1.0f-DC_REMOVAL_CONSTANT)*m_dc_removal_filter);


		m_count++; 


		/*if(m_offset > THRESHOLD)
		{
			scale =30000.0f/(m_offset);
			scale*=m_left_volume;
		}
		else
			scale =10000.0f * inv_threshold;*/
		dc_scale = AM_OUTPUT_CARRIER_LEVEL / (m_dc_agc_filter + AM_KNEE);

		//subtraction of the offset from the absolute value of the signal
		//output_buffer[i] -= m_offset; 
		//float val = sin(1000.0f*2.0f*PI* ph/24000.0f); 
		//output_buffer[i] =  val*dc_scale;
		val -= m_dc_removal_filter;

		// filter the AC (sideband) power
		m_ac_power_filter = ((AC_POWER_CONSTANT*val*val) + (1.0f-AC_POWER_CONSTANT) * m_ac_power_filter);
		float ac_scale = AM_OUTPUT_AC_LEVEL / (FSQRT(m_ac_power_filter) + AM_KNEE);

		// Use the lesser of the two scaling factors to avoid applying too much gain when the carrier is notched out
		float scale = (ac_scale < dc_scale ? ac_scale : dc_scale);
		//float scale = dc_scale;
		val *= scale;
		//sin(2.0f*PI* (float)i/1920.0f); 
		//output_buffer[i] =  val;
		//output_buffer[i+1]=(i<length ? dc_scale : ac_scale)*0.1f;

		ph += 1.0f;

//		float val_i = sin(2.0f*PI* ph/1920.0f); 

//		ph += 1.0f;

	//	output_buffer[i] = input1;
	//	output_buffer[i+1] = input2;


		m_upsamp.upsample2_1(val, &out1, &out2);

		//if(i > 1900)
		//	printf("dummy");

		//m_pAudio[i]=(out1*scale);
		//m_pAudio[i+1]=(out2*scale);
		//m_pAudio[i] = out1;
		//m_pAudio[i+1] = out2;

		m_pAudio[i]=(out1*32768.0f);
		m_pAudio[i+1]=(out2*32768.0f);


//		m_pAudio[i] = ( val * 10000.0f);

//		m_pAudio[i+1] = (val_i  * 10000.0f);

		if(m_count % 8000 ==0)
		{ 
			float val1 = output_buffer[0];
			float val2 = (output_buffer[0]*scale);
			sprintf(string, "sc %f c %f  %f  %f", scale, m_dc_agc_filter, 
			val1,val2);

		//	XIO->DPRAMwrite(MSG_TYPE_DEBUG, string); 
		}


#ifdef _VIDEO
		monit_pos2[data_index2++]= (Int16) (input1*12000000);
		monit_pos2[data_index2++]= (Int16) (input2*12000000);
		data_index2+=AUDIO_STEP; 
		monit_pos2[data_index2++]= (Int16) (input1*12000000);
		monit_pos2[data_index2++]= (Int16) (input2*12000000);
		data_index2+=AUDIO_STEP;
#endif
		
	}

	//printf("dummy");


	return(TRUE);

}




TM_BOOL CAMDemod::Output(float *in, Int16* OutMemAudio,
							Int16* OutMemData, Int16* OutMemSpdo, 
							UInt16& left_peak, UInt16& right_peak,
							Int32 length) 

{
	//work out where to place the monitoring data in the output array
//	Int16* restrict trigger	= &OutMemData[NUMBER_OF_AUDIO_CHANNELS*m_free_symbolnum*TOTAL_SYMBOL_COMPLEX_D+2];
	Int16* restrict monit_pos1=&OutMemData[NUMBER_OF_AUDIO_OUT_CHANNELS*m_free_symbolnum*TOTAL_SYMBOL_COMPLEX_D+4];
	Int16* restrict monit_pos2=&OutMemData[NUMBER_OF_AUDIO_OUT_CHANNELS*m_free_symbolnum*TOTAL_SYMBOL_COMPLEX_D+6];
	Int16* restrict audio_pos = OutMemAudio;
#ifdef _SPDIF
	Int32* restrict spdif_data = (Int32*) &OutMemSpdo[NUMBER_OF_SPDIF_CHANNELS*2*m_free_symbolnum*TOTAL_SYMBOL_COMPLEX_D];
#endif

	int audio_index = m_audio_index;
	int data_index1=0;
	int data_index2=0;
	int sp_index=0;

	
	int i;

//	float scale =4000000.0f;
	float scale = 1.0f;

	Int16 left=0, right=0;
	float leftP=0.0f, rightP=0.0f;

//	float leftFsc, rightFsc;
	float leftI, rightI;


	
	for (i=0;i < 2 * length; i++)
	{
	

		//add this line to stop the filtering
		//out[i]=in[i];

		leftI= in[i];
		rightI= in[i];

		//leftI= 20000.0f * sin(i/20.0f);
		//rightI= 20000.0f * sin(i/20.0f);

		

#ifndef _PC_VER //todo tidy up
		leftP = fmux(leftI > leftP, leftI, leftP);
		rightP = fmux(rightI > rightP, rightI, rightP);
#else
		leftP = (leftI > leftP ?  leftI : leftP);
		rightP = (rightI > rightP ? rightI : rightP);
#endif

		left= (Int16) ICLIPI( (Int32) leftI, CLIP_VALUE );
		right= (Int16) ICLIPI( (Int32) rightI, CLIP_VALUE );

		//put back
		audio_pos[audio_index++]= left;
		audio_pos[audio_index++]= right;
		//audio_pos[audio_index++]= left;
		//audio_pos[audio_index++]= right;
		
		audio_index+=AUDIO_STEP;

		//if(audio_index >= AUDIO_OUT_BUFFER_SIZE_SUB)
		//	DP(("ERROR %i \n", audio_index));

#ifdef _VIDEO
		monit_pos1[data_index1++]= (Int16) (x[i]*12000);
		monit_pos1[data_index1++]= (Int16) (x[i]*12000);
	//	monit_pos1[data_index1++]= (Int16) i*20;
	//	monit_pos1[data_index1++]= (Int16) i*20;
		data_index1+=6;
#endif

//		spdif_data[sp_index++] = (( (Int32) leftI ) << 12) & 0x0FFFF000 ;
//		spdif_data[sp_index++] = (( (Int32) rightI ) << 12) & 0x0FFFF000 ;

#ifdef _SPDIF
		spdif_data[sp_index++] = (( (Int32) left ) << 12) & 0x0FFFF000 ;
		spdif_data[sp_index++] = (( (Int32) right ) << 12) & 0x0FFFF000 ;
#endif


	}

	left_peak=(UInt16) leftP;
	right_peak=(UInt16) rightP;



	//Generate the BMW preambles
	Int32 preamb=0;
#ifdef _SPDIF
	for(i=0; i< 2*length; i++)
	{
		preamb =0x1;

	//	DP(("no samples %i \n", no_samples));
		if(m_spdif_frame == 0 )
			preamb = 0x00;
		//	spdif_data[4*i] &= 0x00;
		else if( (m_spdif_frame % 2) == 1)
			preamb = 0x02;
		//	spdif_data[4*i] &= 0x02;
	//	else if ( (m_spdif_frame % 2) == 0)
	//	else 
	//		preamb = 0x01;
		//	spdif_data[4*i] &= 0x01;
		//else 
		//	preamb = 0x03;

		//	spdif_data[i] += preamb;

			spdif_data[i] |= preamb;
			
		m_spdif_frame++;

		if(m_spdif_frame >= 384)
		{
	//	if(m_spdif_frame >= 192){
			m_spdif_frame=0;
		}
	}
#endif

	m_audio_index = audio_index;


	if(m_audio_index >= AUDIO_POINTS_STEREO_SUB)
	//if(m_audio_index >= AUDIO_POINTS_SUB)
	{
	//	fwrite(audio_pos, sizeof(short), AUDIO_POINTS_STEREO_SUB, m_pFile);
		m_audio_index = 0;
		return TRUE;
	}
	else
		return FALSE;


}	



TM_BOOL CAMDemod::ProcessBlock(tUNativeAcc sample_rate_dev, tNative* data_in_fxp, float* data_out, 
							   Int16* OutMemAudio, 
		Int16* OutMemData, Int16* OutMemSpdo, TM_BOOL& monit_ready, int& audio_ready)
{

	float* restrict data_in = m_data_in;

	//convert this back to a float
	float pull_down = sample_rate_dev; 

	pull_down /= (1<<23);

	for(int i=0;i<CAPTURE_BUFFER_POINTS_U;i++)
	{
		data_in[i]=(float)data_in_fxp[i]/(float)(1<<15);
	}
         
	int length=PROCESSING_BUFFER_POINTS_AM/2;

	//int length=CAPTURE_BUFFER_POINTS_AM/2;
	//int length = AUDIO_POINTS_SUB/2;
//	int length = 50;

//	int i;

	float shift =0.0f;

	

//#ifdef _SIM
//	if (m_resamp.resamp(pull_down ,0,data_in, length)) // (340/326*2)-2=0.08588957 fs=50.06135kHz
//#else

	shift = 5000.0f/24000.0f;
	if (m_resamp.resamp(pull_down ,0,data_in, length)) // (340/326*2)-2=0.08588957 fs=50.06135kHz
//#endif
		{

		
			//shift the signal by 5kHz & 24kHz
			m_freq_corrector.Correct(data_in, shift ,length, no_invert);
		
			//add channel filter here
			//if(m_config.channel_filter() == on)
				m_channel_filter.Filter(data_in, length);

			//now find the signal level
			SignalLevels(data_in, m_free_symbolnum, length);

			envelope_det(data_in, data_out, OutMemAudio, OutMemData, OutMemSpdo, length);
			

			if(Output(m_pAudio, OutMemAudio, OutMemData, OutMemSpdo,
				m_leftPeak, m_rightPeak, length) )
			{

				monit_ready=TRUE;
				audio_ready=TRUE;
			}

			return(TRUE);
		}
		else
		{
			return(FALSE);
		}

}

void CAMDemod::SignalLevels(float* pFloat, int symbol, int points)
{
	int i;

//	char string[500];

	float real, im;
	float mag_tot=0;
//	float peak_mag=0;
//	float peak=0;
	float temp_mag;
	for(i=0;i< points;i+=2)
	{
		real=pFloat[i]; im=pFloat[i+1];
		
	//	temp_mag=(real*real+im*im);
		temp_mag=(real*real);
		mag_tot+=temp_mag;
	//	peak_mag=fmux(temp_mag>peak_mag, temp_mag, peak_mag);
	//	peak=fmux(real>peak, real, peak);
		
	}
	//take the mean
	mag_tot /= points;
	mag_tot *=2;
	//take the root
//	mag_tot = fsqrt(mag_tot);

//	int current_sym=sym_count+chan*SYMS_PER_FRAME_D;

	//add 107 to convert from dBm to dBuV
//	m_signal_level[symbol]=(20.0f*log10(mag_tot))+120.0f;

	float temp_sig_lev=mag_tot;

	
	temp_sig_lev=FLOAT FSQRT(temp_sig_lev);
	temp_sig_lev=FLOAT (20.0f*log10(temp_sig_lev))+120.0f;

	if( temp_sig_lev < 0 ) temp_sig_lev = 0;

	m_signal_level[symbol]=temp_sig_lev;

	m_local_signal_level[symbol]=mag_tot;


	//now take the average
	m_signal_level_av=0.0f;

//	int syms=m_config.syms_per_frame();

	for(i=0; i< SYMS_PER_FRAME_D; i++)
	{
		m_signal_level_av+=m_local_signal_level[i];
	}

	m_signal_level_av /= SYMS_PER_FRAME_D;

	m_signal_level_av = FLOAT FSQRT(m_signal_level_av);

	//add 107 to convert from dBm to dBuV
	m_signal_level_av=FLOAT (20.0f*log10(m_signal_level_av))+120.0f;

	if( m_signal_level_av < 0 ) m_signal_level_av = 0;
}

float* CAMDemod::ReadSignalLevel()
{
	return (m_signal_level);
//	return (m_signal_level_av);
}

int CAMDemod::ReadFilterBW()
{
	int freq=m_channel_filter.GetFilterBW();

	//need to convert from spec_occ number to Hz
	switch(freq)
	{
	case 0:
		return 4500;
	case 1:
		return 5000;
	case 2:
		return 9000;
	case 3:
		return 10000;
	case 4:
		return 18000;
	case 5:
		return 20000;
	default:
		return 0;
	}

}

void CAMDemod::SetVolume(int VolL, int VolR)
{
	// set the scaling values

	float left_sc=-((VOLUME_SCALAR/MAX_VOLUME_SCALE)*(float) VolL)+VOLUME_SCALAR;
	float right_sc=-((VOLUME_SCALAR/MAX_VOLUME_SCALE)*(float) VolR)+VOLUME_SCALAR;

	m_left_volume=FLOAT pow(10.0f, -left_sc);
	m_right_volume=FLOAT pow(10.0f, -right_sc);


}

void CAMDemod::SetFilt(int filt_value)
{
	//may need to map to other numbers
	m_channel_filter.SetSpecOcc(filt_value);

}

char CAMDemod::ReadVULeft()
{
//	char val = (char) (VU_SCALE_LOG*log10(m_leftPeak));
	char val = (char) (VU_SCALE*(m_leftPeak));

//	if(val > 100) val = 100;
	return( val );
}

char CAMDemod::ReadVURight()
{
//	char val = (char) (VU_SCALE_LOG*log10(m_leftPeak));
	char val = (char) (VU_SCALE*(m_rightPeak));

//	if(val > 100) val = 100;
	return( val );
}


void CAMDemod::Reset()
{
	m_free_symbolnum=0;
	m_spdif_frame=0;
}



