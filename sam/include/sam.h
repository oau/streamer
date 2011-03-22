#ifndef _SAM_H_
#define _SAM_H_

#define SAM_SCALE   50
#define SAM_SING     0
#define SAM_SPEED   72
#define SAM_PITCH   64
#define SAM_MOUTH  128
#define SAM_THROAT 128

void sam_params  ( int i_scale, int i_singmode, unsigned char i_speed, unsigned char i_pitch, unsigned char i_mouth, unsigned char i_throat );
int  sam_phenomes( char *input );
int  sam_speak   ( char *p_buffer, int *i_buffer, char* p_text );

void sam_debug();

#endif