#include <stdio.h>
#include "samtabs.h"
#include <stdio.h>
#include <string.h>
#include "recitertabs.h"
#include "sam.h"



static int bufferpos=0;
static int scale=50;
static char *buffer;
static int singmode = 0;


//standard sam sound
static unsigned char speed = 72;
static unsigned char pitch = 64;
static unsigned char mouth = 128;
static unsigned char throat = 128;

static unsigned char A, X, Y;

static unsigned char tab36096[256];   //secure copy of input

static char *input;

static unsigned char wait1 = 7;
static unsigned char wait2 = 6;

static unsigned char mem39;
static unsigned char mem44;
static unsigned char mem47;
static unsigned char mem49;
static unsigned char mem50;
static unsigned char mem51;
static unsigned char mem53;
static unsigned char mem56;

static unsigned char mem59=0;

static unsigned char A, X, Y;

static unsigned char stress[256]; //numbers from 0 to 8
static unsigned char phonemeLength[256]; //tab40160
static unsigned char phonemeindex[256];


static unsigned char phonemeIndexOutput[60]; //tab47296
static unsigned char stressOutput[60]; //tab47365
static unsigned char phonemeLengthOutput[60]; //tab47416

static unsigned char tab44800[256];

static unsigned char tab43008[256];

static unsigned char frequency1[256];
static unsigned char frequency2[256];
static unsigned char frequency3[256];

static unsigned char amplitude1[256];
static unsigned char amplitude2[256];
static unsigned char amplitude3[256];

//timetable for more accurate c64 simulation
static unsigned oldtimetableindex = 0;

static int timetable[5][5] =
{
	162, 167, 167, 127, 128,
	226, 60, 60, 0, 0,
	225, 60, 59, 0, 0,
	200, 0, 0, 54, 55,
	199, 0, 0, 54, 54
};


static void Init();

static int Parser1();
static void Parser2();
static void Code41883();
static void SetPhonemeLength();
static void Code48619();
static void Code41240();
static void Insert(unsigned char position, unsigned char mem60, unsigned char mem59, unsigned char mem58);
static void Code48431();
static void Code47574();
static void Code48547();
static void Code48227();
static void SetMouthThroat(unsigned char mouth, unsigned char throat);

	

// 168=tab43008
// 169=frequency1
// 170=frequency2
// 171=frequency3
// 172=amplitude1
// 173=amplitude2
// 174=amplitude3


static void Init()
{
int i;

SetMouthThroat( mouth, throat);


/*
	freq2data = &mem[45136];
	freq1data = &mem[45056];
        freq3data = &mem[45216];
*/
        //tab43008 = &mem[43008];
/*
        frequency1 = &mem[43264];
        frequency2 = &mem[43520];
        frequency3 = &mem[43776];
*/
/*
        amplitude1 = &mem[44032];
		amplitude2 = &mem[44288];
        amplitude3 = &mem[44544];
*/
        //phoneme = &mem[39904];
/*
        ampl1data = &mem[45296];
        ampl2data = &mem[45376];
	ampl3data = &mem[45456];
*/

        for(i=0; i<256; i++)
        {
                tab43008[i] = 0;
		amplitude1[i] = 0;
		amplitude2[i] = 0;
                amplitude3[i] = 0;
                frequency1[i] = 0;
                frequency2[i] = 0;
		frequency3[i] = 0;
                stress[i] = 0;
                phonemeLength[i] = 0;
                tab44800[i] = 0;
        }
        
        for(i=0; i<60; i++)
		{
                phonemeIndexOutput[i] = 0;
                stressOutput[i] = 0;
                phonemeLengthOutput[i] = 0;
        }

}

//written by me because of different table positions.
// mem[47] = ...
// 168=tab43008
// 169=frequency1
// 170=frequency2
// 171=frequency3
// 172=amplitude1
// 173=amplitude2
// 174=amplitude3
static unsigned char Read(unsigned char p, unsigned char Y)
{
switch(p)
{
        case 168: return tab43008[Y];
        case 169: return frequency1[Y];
        case 170: return frequency2[Y];
        case 171: return frequency3[Y];
	case 172: return amplitude1[Y];
        case 173: return amplitude2[Y];
        case 174: return amplitude3[Y];
}
        printf("Error reading to tables");
        return 0;
}

static void Write(unsigned char p, unsigned char Y, unsigned char value) {
  switch(p) {
      case 168: tab43008[Y] = value; break;
  	  case 169: frequency1[Y] = value; break;
      case 170: frequency2[Y] = value; break;
      case 171: frequency3[Y] = value; break;
      case 172: amplitude1[Y] = value; break;
    	case 173: amplitude2[Y] = value; break;
    	case 174: amplitude3[Y] = value; break;
    	default:
    	  printf("Error writing to tables\n");
  }
}

int sam_speak( char *p_buffer, int *i_buffer, char* p_text ) {
	Init();
	input = p_text;
	buffer = p_buffer;
  bufferpos = 0;
  
	phonemeindex[255] = 32; //to prevent buffer overflow
	
    int temp;
	//mem[39444] = 255;  //maybe errorposition
	if (!Parser1()) {
		*i_buffer = 0;
		return -1;
	}
	Parser2();
	Code41883();
	SetPhonemeLength();
	Code48619();
	Code41240();
	do
	{
		A = phonemeindex[X];
		if (A > 80)
		{
			phonemeindex[X] = 255;
			break; // error: delete all behind it
		}
		X++;
	} while (X != 0);


	
	Code48431();



	Code48547();

  *i_buffer = bufferpos;
	return 0;

}


//seems to delete all zeros in table phoneme and copies it and 2 other tables
static void Code48547()
{
	A = 0;
	X = 0;
	Y = 0;

//pos48551:
while(1)
{
	A = phonemeindex[X];
	if (A == 255)
        {
        	A = 255;
			phonemeIndexOutput[Y] = 255;
			Code47574();
			return;
		}
	if (A == 254)
		{
			X++;
			int temp = X;
			//mem[48546] = X;
			phonemeIndexOutput[Y] = 255;
			Code47574();
        	//X = mem[48546];
                X=temp;
        	Y = 0;
			continue;
		}

	if (A == 0)
        {
        	X++;
        	continue;
        }

	phonemeIndexOutput[Y] = A;
	phonemeLengthOutput[Y] = phonemeLength[X];
	stressOutput[Y] = stress[X];
	X++;
	Y++;
}

}

static void Code48431() {
  unsigned char mem54;
  unsigned char mem55;
  unsigned char index; //variable Y
	unsigned char mem66;
	mem54 = 255;
	//X++;
	mem55 = 0;
	mem66 = 0;
  while( 1 ) {
//pos48440:
		X = mem66;
		index = phonemeindex[ X ];
		if( index == 255 ) return;
		mem55 += phonemeLength[ X ];

		if( mem55 < 232 ) {
			A = flags2[ index ] & 1;
			if( A != 0 ) {
				X++;
				mem55 = 0;
				Insert( X, 254, mem59, 0 );
				mem66++;
				mem66++;
				continue;
			}
			if( index == 0 ) mem54 = X;
			mem66++;
			continue;
		}
		//X = mem54; // This caused weird crashes?
		phonemeindex[ X ] = 31;   // 'Q'
		phonemeLength[ X ] = 4;
		stress[ X ] = 0;
		X++;
		mem55 = 0;
		Insert( X, 254, mem59, 0 );
		X++;
		mem66 = X;
    return;
	}
}


//add 1 to stress under some circumstances
static void Code41883()
{
	unsigned char pos=0; //mem66
	while(1)
	{
	Y = phonemeindex[pos];
	if (Y == 255) return;
	if ((flags[Y] & 64) == 0) {pos++; continue;}
	Y = phonemeindex[pos+1];
	if (Y == 255) //prevent buffer overflow
	{
		if ((65 & 128) == 0)  {pos++; continue;}
	} else
		if ((flags[Y] & 128) == 0)  {pos++; continue;}

	Y = stress[pos+1];
	if (Y == 0)  {pos++; continue;}
	if ((Y & 128) != 0)  {pos++; continue;}
	stress[pos] = Y+1;
	pos++;
	}

}


//void Code41014()
static void Insert(unsigned char position/*var57*/, unsigned char mem60, unsigned char mem59, unsigned char mem58)
{
	int i;
	for(i=254; i >= position; i--)
        {
				phonemeindex[i+1] = phonemeindex[i];
                phonemeLength[i+1] = phonemeLength[i];
                stress[i+1] = stress[i];
        }

	phonemeindex[position] = mem60;
	phonemeLength[position] = mem59;
	stress[position] = mem58;
	return;
}


static int Parser1() {
	int i;
  unsigned char sign1;
  unsigned char sign2;
  unsigned char position=0;
 	X = 0;
	A = 0;
	Y = 0;
  for(i=0; i<256; i++) stress[i] = 0;
  while(1) {
  	sign1 = input[X];
  	if (sign1 == 155) {
      phonemeindex[position] = 255;      //mark endpoint
  		return 1;       //all ok
  	}
	  X++;
	  sign2 = input[X];

	  Y = 0;
pos41095:
    A = signInputTable1[Y];
    if (A == sign1) {
   	  A = signInputTable2[Y];
      if ((A != '*') && (A == sign2)) {
			  phonemeindex[position] = Y;
        position++;
        X++;
        continue;
      }
    }
	  Y++;
	  if (Y != 81) goto pos41095;

	  Y = 0;
pos41134:

	  if (signInputTable2[Y] == '*') {
      if (signInputTable1[Y] == sign1) {
    	  phonemeindex[position] = Y;
    	  position++;
        continue;
      }
    }
	  Y++;
	  if (Y != 81) goto pos41134; //81 is size of table
    
  	Y = 8;
    while( (sign1 != stressInputTable[Y]) && (Y>0)) {
      Y--;
    }

    if (Y == 0) {
  	  //mem[39444] = X;
          //41181: JSR 42043 //Error
  	  return 0;
    }

	  stress[position-1] = Y;
  } //while


}

//change phonemelength depedendent on stress
//void Code41203()
static void SetPhonemeLength()
{
        unsigned char A;
        int position = 0;
		while(phonemeindex[position] != 255 )
        {
        		A = stress[position];
                //41218: BMI 41229
                if ((A == 0) || ((A&128) != 0))
		{
                	phonemeLength[position] = phonemeLengthTable1[phonemeindex[position]];
                } else
                {
                	phonemeLength[position] = phonemeLengthTable2[phonemeindex[position]];
                }
                position++;
		}
}


static void Code41240()
{
		unsigned char pos=0;

while(phonemeindex[pos] != 255)
{
	unsigned char index; //register AC
	X = pos;
	index = phonemeindex[pos];
	if ((flags[index]&2) == 0)
	{
		pos++;
		continue;
	} else
	if ((flags[index]&1) == 0)
	{
		Insert(pos+1, index+1, phonemeLengthTable1[index+1], stress[pos]);
		Insert(pos+2, index+2, phonemeLengthTable1[index+2], stress[pos]);
		pos += 3;
		continue;
	 }

	 do
	 {
		 X++;
		 A = phonemeindex[X];
	 } while(A==0);

	if (A != 255)
	{
		if ((flags[A] & 8) != 0)  {pos++; continue;}
		if ((A == 36) || (A == 37)) {pos++; continue;} // '/H' '/X'
	}

	Insert(pos+1, index+1, phonemeLengthTable1[index+1], stress[pos]);
	Insert(pos+2, index+2, phonemeLengthTable1[index+2], stress[pos]);
	pos += 3;
};

}

//void Code41397()
void Parser2()
{
	unsigned char pos = 0; //mem66;
	unsigned char mem58 = 0;

while(1)
{
	X = pos;
	A = phonemeindex[pos];

	if (A == 0)
	{
		pos++;
		continue;
	}
	if (A == 255) return;

	Y = A;

	if ((flags[A] & 16) == 0) goto pos41457;
	mem58 = stress[pos];
	A = flags[Y] & 32;
	if (A == 0) A = 20; else A = 21;    // 'WX' = 20 'YX' = 21
//pos41443:
	Insert(pos+1, A, mem59, mem58);
	X = pos;
	goto pos41749;

pos41457:
	A = phonemeindex[X];
	if (A != 78) goto pos41487;  // 'UL'
	A = 24;         // 'L'                 //change 'UL' to 'AX L'
pos41466:
	mem58 = stress[X];
	phonemeindex[X] = 13;  // 'AX'
	Insert(X+1, A, mem59, mem58);
    pos++;
	continue;

pos41487:
	if (A != 79) goto pos41495;   // 'UM'
	A = 27; // 'M'  //changle 'UM' to  'AX M'
		goto pos41466;
pos41495:
	if (A != 80) goto pos41503; // 'UN'
	A = 28;         // 'N' //change UN to 'AX N'
		goto pos41466;
pos41503:
	Y = A;
	A = flags[A] & 128;

		if (A != 0)
		{
			A = stress[X];
			if (A != 0)
			{
				X++;
				A = phonemeindex[X];
				if (A == 0)
				{
					X++;
					Y = phonemeindex[X];
					if (Y == 255) //buffer overflow
						A = 65&128;
					else
						A = flags[Y] & 128;

					if (A != 0)
					{
						A = stress[X];
						if (A != 0)
						{
							// 31 = 'Q'
							Insert(X, 31, mem59, 0);
							pos++;
							continue;
						}
					}
				}
			}
		}


	X = pos;
	A = phonemeindex[pos];
	if (A != 23) goto pos41611;     // 'R'
	X--;
	A = phonemeindex[pos-1];
//pos41567:
	if (A == 69)                    // 'T'
	{
		phonemeindex[pos-1] = 42;
		goto pos41779;
	}

	if (A == 57)                    // 'D'
	{
		phonemeindex[pos-1] = 44;
		goto pos41788;
	}

	A = flags[A] & 128;
	if (A != 0) phonemeindex[pos] = 18;  // 'RX'
	pos++;
	continue;

pos41611:
	if (A == 24)    // 'L'
	{
		if ((flags[phonemeindex[pos-1]] & 128) == 0) {pos++; continue;}
		phonemeindex[X] = 19;     // 'LX'
		pos++;
		continue;
	}

	if (A == 32)    // 'S'
	{
		if (phonemeindex[pos-1] != 60) {pos++; continue;}
		phonemeindex[pos] = 38;    // 'Z'
		pos++;
		continue;
	}

	if (A == 72)    // 'K'
	{
		Y = phonemeindex[pos+1];
		A = flags[Y] & 32;
		if (A == 0) phonemeindex[pos] = 75;  // 'KX'
	}
	else
	if (A == 60)   // 'G'
	{
		unsigned char index = phonemeindex[pos+1];
		if (index == 255)
		{
			if ((65 & 32) != 0) {pos++; continue;}//prevent buffer overflow
		}
		else
		if ((flags[index] & 32) != 0) {pos++; continue;}
		phonemeindex[pos] = 63; // 'GX'
		pos++;
		continue;
	}
	Y = phonemeindex[pos];
//pos41719:
	A = flags[Y] & 1;
	if (A == 0) goto pos41749;
	A = phonemeindex[pos-1];
	if (A != 32)    // 'S'
	{
		A = Y;
		goto pos41812;
	}
	phonemeindex[pos] = Y-12;
	pos++;
	continue;



pos41749:
	A = phonemeindex[X];
	if (A == 53)    // 'UW'
	{
		Y = phonemeindex[X-1];
		A = flags2[Y] & 4;
		if (A == 0) {pos++; continue;}
		phonemeindex[X] = 16;
		pos++;
		continue;
	}
pos41779:

	if (A == 42)    // 'CH'
	{
//        pos41783:
		Insert(X+1, A+1, mem59, stress[X]);
		pos++;
		continue;
	}

pos41788:
	if (A == 44) // 'J'
	{
		Insert(X+1, A+1, mem59, stress[X]);
		pos++;
		continue;
	}
pos41812:
	if (A != 69)    // 'T'
		if (A != 57) {pos++; continue;}       // 'D'
//pos41825:

	if ((flags[phonemeindex[X-1]] & 128) == 0) {pos++; continue;}
	X++;
	A = phonemeindex[X];
//pos41841
		if (A != 0)
		{
			if ((flags[A] & 128) == 0) {pos++; continue;}
			if (stress[X] != 0) {pos++; continue;}
pos41856:
			phonemeindex[pos] = 30;       // 'DX'
		} else
		{
			A = phonemeindex[X+1];
			if (A == 255) //prevent buffer overflow
				A = 65 & 128;
			else
			A = flags[A] & 128;

			if (A != 0) phonemeindex[pos] = 30;  // 'DX'
		}

		pos++;

} // while


}

//change phoneme length
static void Code48619()
{
	X = 0;
	unsigned char index;

	unsigned char mem66=0;
	while(1)
	{
		index = phonemeindex[X];
		if (index == 255) break;
		if((flags2[index] & 1) == 0)
		{
			X++;
			continue;
		}
		mem66 = X;
pos48644:
		X--;
		if(X == 0) break;
		index = phonemeindex[X];

		if (index != 255) //inserted to prevent access overrun
		if((flags[index] & 128) == 0) goto pos48644;

		//pos48657:
		do
		{
			index = phonemeindex[X];
			if (index != 255)//inserted to prevent access overrun
			if(((flags2[index] & 32) == 0) || ((flags[index] & 4) != 0))     //nochmal überprüfen
			{
				//A = flags[Y] & 4;
				//if(A == 0) goto pos48688;
				A = phonemeLength[X];
				A = (A >> 1) + A + 1;   // 3/2*A+1 ???
				phonemeLength[X] = A;
			}

			X++;
		} while (X != mem66);
		//	if (X != mem66) goto pos48657;

		X++;
	}  // while

mem66 = 0;
//pos48697

while(1)
{
	X = mem66;
	index = phonemeindex[X];
	if (index == 255) return;
	A = flags[index] & 128;
	if (A != 0)
	{

	X++;
	index = phonemeindex[X];
	if (index == 255) 
		mem56 = 65;
	else
		mem56 = flags[index];

	if ((flags[index] & 64) == 0)
	{
		if ((index == 18) || (index == 19))  // 'RX' & 'LX'
		{
			X++;
			index = phonemeindex[X];
			if ((flags[index] & 64) != 0)
				phonemeLength[mem66]--;
			 mem66++;
			 continue;
		}
		mem66++;
		continue;
	}

	if ((mem56 & 4) == 0)
	{
		if((mem56 & 1) == 0) {mem66++; continue;}
		X--;
		mem56 = phonemeLength[X] >> 3;
		phonemeLength[X] -= mem56;
		mem66++;
		continue;
	}
	A = phonemeLength[X-1];
	phonemeLength[X-1] = (A >> 2) + A + 1;     // 5/4*A + 1
	mem66++;
	continue;
	
	}

//pos48821:

	if((flags2[index] & 8) != 0)
		{
			X++;
			index = phonemeindex[X];
			if (index == 255) A = 65&2;  //prevent buffer overflow
        	else
			A = flags[index] & 2;
			if(A != 0)
				{
					phonemeLength[X] = 6;
					phonemeLength[X-1] = 5;
				}
				mem66++;
				continue;

        }


        if((flags[index] & 2) != 0)
        {
                do
                {
                        X++;
						index = phonemeindex[X];
                } while(index == 0);
			if (index == 255) //buffer overflow
			{
				if ((65 & 2) == 0) {mem66++; continue;}
			} else
				if ((flags[index] & 2) == 0) {mem66++; continue;}
				
			phonemeLength[X] = (phonemeLength[X] >> 1) + 1;
			X = mem66;
        	phonemeLength[mem66] = (phonemeLength[mem66] >> 1) + 1;
			mem66++;
			continue;
        }


	if ((flags2[index] & 16) != 0)
	{
		index = phonemeindex[X-1];
		if((flags[index] & 2) != 0) phonemeLength[X] -= 2;
	}

	mem66++;
	continue;
}


//	goto pos48701;
}

// -------------------------------------------------------------------------

static void Code47503(unsigned char mem52)
{

	Y = 0;
	if ((mem53 & 128) != 0)
	{
		mem53 = -mem53;
		Y = 128;
	}
	mem50 = Y;
	A = 0;
	for(X=8; X > 0; X--)
	{
		int temp = mem53;
		mem53 = mem53 << 1;
		A = A << 1;
		if (temp >= 128) A++;
		if (A >= mem52)
		{
			A = A - mem52;
			mem53++;
		}
	}

	mem51 = A;
	if ((mem50 & 128) != 0) mem53 = -mem53;

}

// -------------------------------------------------------------------------


static void Code48227(unsigned char *mem66)
{
int k;	
int tempA;
int address;
int i;
	mem49 = Y;
	A = mem39&7;
	X = A-1;
	mem56 = X;
	mem53 = tab48426[X];
	mem47 = X;      //46016+mem[56]*256
	A = mem39 & 248;
	if(A == 0)
		{
			Y = mem49;
			A = tab43008[mem49] >> 4;
			goto pos48315;
		}
	Y = A ^ 255;
pos48274:
	mem56 = 8;
		A = randomtable[mem47*256+Y];
pos48280:

		tempA = A;
		A = A << 1;
		//48281: BCC 48290
		if ((tempA & 128) == 0)
		{
			X = mem53;
			//mem[54296] = X;
				//timetable 1
				bufferpos += timetable[oldtimetableindex][1];
				oldtimetableindex = 1;

				for(k=0; k<5; k++)
					buffer[bufferpos/scale + k] = (X & 15)*16;
//				Memo1->Lines->Add(X);
			if(X != 0) goto pos48296;
		}
		//timetable 2;
		bufferpos += timetable[oldtimetableindex][2];
		oldtimetableindex = 2;
		for(k=0; k<5; k++)
			buffer[bufferpos/scale + k] = (5 & 15)*16;

//48295: NOP
pos48296:

		for(i=0; i<wait1; i++) //wait
		X = 0;

		mem56--;
	if (mem56 != 0) goto pos48280;
	Y++;
	if (Y != 0) goto pos48274;
	mem44 = 1;
	Y = mem49;
	return;


unsigned char phase1;

pos48315:
// Error Error Error
  
	phase1 = A ^ 255;
	Y = *mem66;
do
{
//pos48321:

	mem56 = 8;
	//A = Read(mem47, Y);
    A = randomtable[mem47*256+Y];     //???


//pos48327:
	do
	{
	//48327: ASL A
	//48328: BCC 48337
		tempA = A;
		A = A << 1;
		if ((tempA & 128) != 0)
		{
			X = 26;
			//timetable 3
			bufferpos += timetable[oldtimetableindex][3];
			oldtimetableindex = 3;
			for(k=0; k<5; k++)
				buffer[bufferpos/scale + k] = (X & 15)*16;

		} else
		{
			//timetable 4
			X=6;
			bufferpos += timetable[oldtimetableindex][4];
			oldtimetableindex = 4;
			for(k=0; k<5; k++)
				buffer[bufferpos/scale + k] = (X & 15)*16;
		}

		for(X = wait2; X>0; X--); //wait
		mem56--;
	} while(mem56 != 0);

	Y++;
	phase1++;

} while (phase1 != 0);
//	if (phase1 != 0) goto pos48321;
	A = 1;
	mem44 = 1;
	*mem66 = Y;
	Y = mem49;
	return;

//exit(1);


//Error Error Error
}

static void Special1(unsigned char mem48, unsigned char phase1)
{

//pos48372:
//	mem48 = 255;
pos48376:
	mem49 = X;
	A = X;
	int Atemp = A;
	A = A - 30;
	if (Atemp <= 30) A=0; // ???
	X = A;


while( tab43008[X] == 127) X++;


pos48398:
//48398: CLC
//48399: ADC 48
        A += mem48;
	phase1 = A;
	tab43008[X] = A;
pos48406:
	X++;
	if (X == mem49) return; //goto pos47615;
	if (tab43008[X] == 255) goto pos48406;
	A = phase1;
	goto pos48398;
}


static void Code47574()
{
	int k;
	unsigned char phase1;  //mem43
	unsigned char phase2;	
	unsigned char phase3;
	unsigned char mem66;
	unsigned char mem38;
	unsigned char mem40;
	unsigned char speedcounter; //mem45
	unsigned char mem48;
	int i;
	int carry;
	int address;
	if (phonemeIndexOutput[0] == 255) return; //exit if no data

	A = 0;
	X = 0;
	mem44 = 0;
pos47587:
	Y = mem44;
	A = phonemeIndexOutput[mem44];
	mem56 = A;
	if (A == 255) goto pos47694;
	if (A == 1)
	{
		//pos48366:
		A = 1;
		mem48 = 1;
		//goto pos48376;
		Special1(mem48, phase1);
	}
	/*
	if (A == 2) goto pos48372;
	*/
	if (A == 2)
	{
		mem48 = 255;
		Special1(mem48, phase1);
	}
//	pos47615:

	phase1 = tab47492[stressOutput[Y] + 1];
	phase2 = phonemeLengthOutput[Y];
	Y = mem56;
	do
	{
		frequency1[X] = freq1data[Y];
		frequency2[X] = freq2data[Y];
		frequency3[X] = freq3data[Y];
		amplitude1[X] = ampl1data[Y];
		amplitude2[X] = ampl2data[Y];
		amplitude3[X] = ampl3data[Y];
		tab44800[X] = tab45936[Y];
		tab43008[X] = pitch + phase1;
		X++;
		phase2--;
	} while(phase2 != 0);
	mem44++;

	if(mem44 != 0) goto pos47587;

pos47694:

	A = 0;
	mem44 = 0;
	mem49 = 0;
	X = 0;
while(1) //while No. 1
{

//pos47701:
	Y = phonemeIndexOutput[X];
	A = phonemeIndexOutput[X+1];
	X++;
	if (A == 255) break;//goto pos47970;
	X = A;
	mem56 = tab45856[A];
	A = tab45856[Y];
	if (A == mem56)
	{
		phase1 = tab45696[Y];
		phase2 = tab45696[X];
	} else
	if (A < mem56)
	{
		phase1 = tab45776[X];
		phase2 = tab45696[X];
	} else
	{
		phase1 = tab45696[Y];
		phase2 = tab45776[Y];
	}

	Y = mem44;
	A = mem49 + phonemeLengthOutput[mem44];
	mem49 = A;
	A = A + phase2; //Maybe Problem because of carry flag
//47776: ADC 42
	speedcounter = A;
	mem47 = 168;
	phase3 = mem49 - phase1;
	A = phase1 + phase2;
	mem38 = A;
	X = A;
	X -= 2;
//47805: BPL 47810
if ((X & 128) == 0)
do   //while No. 2
{
//pos47810:

	mem40 = mem38;
	if (mem47 == 168)     //for amplitude1
        {
                unsigned char mem36, mem37;
			mem36 = phonemeLengthOutput[mem44] >> 1;
        	mem37 = phonemeLengthOutput[mem44+1] >> 1;
        	mem40 = mem36 + mem37;
        	mem37 += mem49;
        	mem36 = mem49 - mem36;
                A = Read(mem47, mem37);
                //A = mem[address];
                Y = mem36;
				mem53 = A - Read(mem47, mem36);
        } else
        {
                A = Read(mem47, speedcounter);
                Y = phase3;
                mem53 = A - Read(mem47, phase3);
        }
       	Code47503(mem40);
       	X = mem40;
       	Y = phase3;

	mem56 = 0;
//47907: CLC
//pos47908:
		while(1)     //while No. 3
		{
			A = Read(mem47, Y) + mem53; //carry alway cleared

			mem48 = A;
			Y++;
			X--;
			if(X == 0) break;

			mem56 += mem51;
			if (mem56 >= mem40)  //???
			{
			/*
			47927: CMP 40
			47927: BCC 47945
			*/
			//47931: SBC 40
			 mem56 -= mem40; //carry? is set
			 //if ((mem56 & 128)==0)
			 if ((mem50 & 128)==0)
			 {
				 //47935: BIT 50
				 //47937: BMI 47943
				if(mem48 != 0) mem48++;
			 } else mem48--;
			 }
		 //pos47945:
			 Write(mem47, Y, mem48);
		 //47949: CLC
		 //47950: BCC 47908

		//goto pos47908;

        } //while No. 3

//pos47952:
	mem47++;
	//if (mem47 != 175) goto pos47810;
} while (mem47 != 175);     //while No. 2
//pos47963:
	mem44++;
	X = mem44;
}  //while No. 1

//goto pos47701;
//pos47970:

	mem48 = mem49 + phonemeLengthOutput[mem44];
	if (!singmode)
	{
		for(i=0; i<256; i++)
			tab43008[i] -= (frequency1[i] >> 1);
	}

	phase1 = 0;
	phase2 = 0;
    phase3 = 0;
	mem49 = 0;
	speedcounter = 72; //sam standard speed

	//amplitude rescaling
	for(i=255; i>=0; i--)
	{
		amplitude1[i] = amplitudeRescale[amplitude1[i]];
		amplitude2[i] = amplitudeRescale[amplitude2[i]];
		amplitude3[i] = amplitudeRescale[amplitude3[i]];
	}

	Y = 0;
	A = tab43008[0];
	mem44 = A;
	X = A;
	mem38 = A - (A>>2);     // 3/4*A ???

//finally the loop for sound output
//pos48078:
while(1)
{
	A = tab44800[Y];
	mem39 = A;
	A = A & 248;
	if(A != 0)
	{
		Code48227(&mem66);
		Y += 2;
		mem48 -= 2;
	} else
	{
		mem56 = multtable[sinus[phase1] | amplitude1[Y]];

		carry = 0;
		if ((mem56+multtable[sinus[phase2] | amplitude2[Y]] ) > 255) carry = 1;
		mem56 += multtable[sinus[phase2] | amplitude2[Y]];
		A = mem56 + multtable[rectangle[phase3] | amplitude3[Y]] + (carry?1:0);
		A = ((A + 136) & 255) >> 4; //there must be also a carry
		//mem[54296] = A;
		//timetable 0
		bufferpos += timetable[oldtimetableindex][0];
		oldtimetableindex = 0;

		for(k=0; k<5; k++)
			buffer[bufferpos/scale + k] = (A & 15)*16;
//		Memo1->Lines->Add(A);
		speedcounter--;
		if (speedcounter != 0) goto pos48155;
		Y++; //go to next amplitude
		mem48--;
	}
	if(mem48 == 0) return;
	speedcounter = speed;
pos48155:
	mem44--;
	if(mem44 == 0)
	{
pos48159:

		A = tab43008[Y];
		mem44 = A;
		A = A - (A>>2);
		mem38 = A;
		phase1 = 0;
		phase2 = 0;
		phase3 = 0;
		continue;
	}
	mem38--;
	if((mem38 != 0) || (mem39 == 0))
	{
		phase1 += frequency1[Y];
		phase2 += frequency2[Y];
		phase3 += frequency3[Y];
		continue;
	}
	Code48227(&mem66);
	goto pos48159;
} //while

//--------------------------
//pos48315:
int tempA;
	phase1 = A ^ 255;
	Y = mem66;
do
{
//pos48321:

	mem56 = 8;
	A = Read(mem47, Y);

//pos48327:
	do
	{
	//48327: ASL A
	//48328: BCC 48337
		tempA = A;
		A = A << 1;
		if ((tempA & 128) != 0)
		{
			X = 26;
			// mem[54296] = X;
			bufferpos += 150;
			buffer[bufferpos/scale] = (X & 15)*16;
//			Memo1->Lines->Add(X);
		} else
		{
			//mem[54296] = 6;
			X=6; 
			bufferpos += 150;
			buffer[bufferpos/scale] = (X & 15)*16;
//			Memo1->Lines->Add(X);
		}

		for(X = wait2; X>0; X--); //wait
		mem56--;
	} while(mem56 != 0);

	Y++;
	phase1++;

} while (phase1 != 0);
//	if (phase1 != 0) goto pos48321;
	A = 1;
	mem44 = 1;
	mem66 = Y;
	Y = mem49;
	return;
}

//return = (mem39212*mem39213) >> 1
static unsigned char trans(unsigned char mem39212, unsigned char mem39213)
{
//pos39008:
   unsigned char carry;
   int temp;
   unsigned char mem39214, mem39215;
	A = 0;
	mem39215 = 0;
	mem39214 = 0;
	X = 8;
		do
		{
				carry = mem39212 & 1;
				mem39212 = mem39212 >> 1;
				if (carry != 0)
				{
						/*
						39018: LSR 39212
						39021: BCC 39033
						*/
						carry = 0;
					A = mem39215;
						temp = (int)A + (int)mem39213;
						A = A + mem39213;
						if (temp > 255) carry = 1;
					mem39215 = A;
				}
				temp = mem39215 & 1;
				mem39215 = (mem39215 >> 1) | (carry?128:0);
				carry = temp;
				//39033: ROR 39215
			X--;
		} while (X != 0);
		temp = mem39214 & 128;
		mem39214 = (mem39214 << 1) | (carry?1:0);
		carry = temp;
		temp = mem39215 & 128;
		mem39215 = (mem39215 << 1) | (carry?1:0);
		carry = temp;

		return mem39215;
}

static void SetMouthThroat(unsigned char mouth, unsigned char throat)
{

	unsigned char mem39216;
        unsigned char mem39212;
        unsigned char mem39213;
        unsigned char mem39215;
        //unsigned char mouth; //mem38880
        //unsigned char throat; //mem38881

        unsigned char tab39140[30] = {0, 0, 0, 0, 0, 10,
14, 19, 24, 27, 23, 21, 16, 20, 14, 18, 14, 18, 18,
16, 13, 15, 11, 18, 14, 11, 9, 6, 6, 6};

        unsigned char tab39170[30] = {255, 255,
255, 255, 255, 84, 73, 67, 63, 40, 44, 31, 37, 45, 73, 49,
36, 30, 51, 37, 29, 69, 24, 50, 30, 24, 83, 46, 54, 86};

        //there must be no zeros in this 2 tables
        unsigned char tab39200[6] = {19, 27, 21, 27, 18, 13};
        unsigned char tab39206[6] = {72, 39, 31, 43, 30, 34};

        unsigned char pos = 5; //mem39216
//pos38942:
		while(pos != 30)
        {
        	mem39213 = tab39140[pos];
			if(mem39213 != 0) mem39215 = trans(mouth, mem39213);
                freq1data[pos] = mem39215;
        	mem39213 = tab39170[pos];
        	if(mem39213 != 0) mem39215 = trans(throat, mem39213);
        	freq2data[pos] = mem39215;
                pos++;
        }

//pos39059:
	pos = 48;
	Y = 0;
        while(pos != 54)
        {
        	mem39213 = tab39200[Y];
        	mem39215 = trans(mouth, mem39213);
        	freq1data[pos] = mem39215;
        	mem39213 = tab39206[Y];
               	mem39215 = trans(throat, mem39213);
        	freq2data[pos] = mem39215;
        	Y++;
                pos++;
        }

}


void sam_debug()
{
		int pos = 0;
		printf("phoneme  length  stress\n");
		printf("-----------------------\n");

		while((phonemeindex[pos] != 255) && (pos < 255))
		{
			
			if (phonemeindex[pos] < 81)
			{
				printf(" %c%c       %3i       %i\n",
					signInputTable1[phonemeindex[pos]],
					signInputTable2[phonemeindex[pos]],
					phonemeLength[pos],
					stress[pos]
					);
			} else
			{
				printf("unknown %i\n", phonemeindex[pos]);
			}
 		pos++;
		}

}

void sam_params( int i_scale, int i_singmode, unsigned char i_speed, unsigned char i_pitch, unsigned char i_mouth, unsigned char i_throat ) {
  scale = i_scale;
  singmode = i_singmode;
  speed = i_speed;
  pitch = i_pitch;
  mouth = i_mouth;
  throat = i_throat;
}

static void Code37055(unsigned char mem59)
{
	X = mem59;
	X--;
	A = tab36096[X];
	Y = A;
	A = tab36376[Y];
	return;
}

static void Code37066(unsigned char mem58)
{
	X = mem58;
	X++;
	A = tab36096[X];
	Y = A;
	A = tab36376[Y];
}

static unsigned char GetRuleByte(unsigned char mem62, unsigned char mem63, unsigned char Y)
{
	unsigned int address = ((unsigned int)mem62+((unsigned int)mem63<<8));
	address -= 32000;
	//return mem[address+Y];
	return rules[address+Y];
}


/*bool Code36484(char *input)*/
int sam_phenomes(char *input) {
	
	unsigned char mem29;
	unsigned char mem56;      //output position for phonemes
	unsigned char mem57;
	unsigned char mem58;
	unsigned char mem59;
	unsigned char mem60;
	unsigned char mem61;
	unsigned char mem62;
	unsigned char mem63;
	unsigned char mem64;	 // position of '=' or current character
	unsigned char mem65;     // position of ')'
	unsigned char mem66;     // position of '('
	unsigned char mem36653;
	
	tab36096[0] = 32;
	
	// secure copy of input
	// because input will be overwritten by phonemes
	X = 1;
	Y = 0;
	do {
		A = input[Y] & 127;
		if ( A >= 112) A = A & 95;
		else if ( A >= 96) A = A & 79;
			
		tab36096[X] = A;
		X++;
		Y++;
	} while (Y != 255);
	
	X = 255;
	A = 27;
	tab36096[X] = 27;
	mem61 = 255;

pos36550:
	A = 255;
	mem56 = 255;

pos36554:
	while(1) {
		mem61++;
		X = mem61;
		A = tab36096[X];
		mem64 = A;
		if (A == 27) {   // '['
			mem56++;
			X = mem56;
			A = 155;
			input[X] = 155;
			return 0;
		}

		//pos36579:
		if (A != 46) break;   // '.'
		X++;
		Y = tab36096[X];
		A = tab36376[Y] & 1;
		if(A != 0) break;
		mem56++;
		X = mem56;
		A = 46;
		input[X] = 46;
	} //while
	
	A = mem64;
	Y = A;
	A = tab36376[A];
	mem57 = A;
	if((A&2) != 0)
	{
		mem62 = 165;
		mem63 = 146;
		goto pos36700;
	}

	//pos36630:
	A = mem57;
	if(A != 0) goto pos36677;
	A = 32;
	tab36096[X] = 32;
	mem56++;
	X = mem56;
	if (X > 120) goto pos36654;
	input[X] = A;
	goto pos36554;
	
pos36654:
	input[X] = 155;
	A = mem61;
	mem36653 = A;
	return 0;
/*
	go on if there is more input ???
	mem61 = mem36653;
	goto pos36550;
*/	
	
pos36677:
		A = mem57 & 128;
		if(A == 0) {
			return -1;
		}
	
		// go to the right rule for this character.
		X = mem64 - 65;
		mem62 = tab37489[X];
		mem63 = tab37515[X];
	
	// -------------------------------------
	// go to next rule
	// -------------------------------------
	
pos36700:
	// find next rule
	Y = 0;
	do {
		mem62 += 1;
		mem63 += (mem62 == 0)?1:0;
        A = GetRuleByte(mem62, mem63, Y);
	} while ((A & 128) == 0);
	Y++;

	// find '('
	while(1) {
		A = GetRuleByte(mem62, mem63, Y);
		if (A == 40) break;      //'('
		Y++;
	}
	mem66 = Y;

	// find ')'
	do
	{
		Y++;
		A = GetRuleByte(mem62, mem63, Y);
	} while(A != 41);
	mem65 = Y;

	// find '='
	do {
		Y++;
		A = GetRuleByte(mem62, mem63, Y);
		A = A & 127;

	} while (A != 61);   // '='
	mem64 = Y;

	X = mem61;
	mem60 = X;

	Y = mem66;
	Y++;
	while(1) {
		mem57 = tab36096[X];
		A = GetRuleByte(mem62, mem63, Y);
		if (A != mem57) goto pos36700;
		Y++;
		if(Y == mem65) break;
		X++;
		mem60 = X;
	}
	
pos36787:
	A = mem61;
	mem59 = mem61;
	
pos36791:
	while(1) {
	  mem66--;
		Y = mem66;
		A = GetRuleByte(mem62, mem63, Y);
		mem57 = A;
		if ((A & 128) != 0) goto pos37180;
		X = A & 127;
		A = tab36376[X] & 128;
		if (A == 0) break;
		X = mem59-1;
		A = tab36096[X];
		if (A != mem57) goto pos36700;
		mem59 = X;
	}
	
pos36833:
	A = mem57;
	if (A == 32) goto pos36895;     // ' '
	if (A == 35) goto pos36910;     // '#'
	if (A == 46) goto pos36920;
	if (A == 38) goto pos36935;
	if (A == 64) goto pos36967;
	if (A == 94) goto pos37004;
	if (A == 43) goto pos37019;
	if (A == 58) goto pos37040;
	return -1;
	
pos36895:
	Code37055(mem59);
	A = A & 128;
	if(A != 0) goto pos36700;
			
pos36905:
	mem59 = X;
	goto pos36791;
	
pos36910:
	Code37055(mem59);
	A = A & 64;
	if(A != 0) goto pos36905;
	goto pos36700;
	
pos36920:
	Code37055(mem59);
	A = A & 8;
	if(A == 0) goto pos36700;

pos36930:
	mem59 = X;
	goto pos36791;
	
pos36935:
	Code37055(mem59);
	A = A & 16;
	if(A != 0) goto pos36930;
	A = tab36096[X];
	if (A != 72) goto pos36700;
	X--;
	A = tab36096[X];
	if ((A == 67) || (A == 83)) goto pos36930;
	goto pos36700;
	
pos36967:
	Code37055(mem59);
	A = A & 4;
	if(A != 0) goto pos36930;
	A = tab36096[X];
	if (A != 72) goto pos36700;
	if ((A != 84) && (A != 67) && (A != 83)) goto pos36700;
	mem59 = X;
	goto pos36791;
	
pos37004:
	Code37055(mem59);
	A = A & 32;
	if(A == 0) goto pos36700;
		
pos37014:
	mem59 = X;
	goto pos36791;
	
pos37019:
	X = mem59;
	X--;
	A = tab36096[X];
	if ((A == 69) || (A == 73) || (A == 89)) goto pos37014; //'E' 'I' 'Y'
	goto pos36700;
	
pos37040:
	Code37055(mem59);
	A = A & 32;
	if(A == 0) goto pos36791;
	mem59 = X;
	goto pos37040;
	
pos37077:
	X = mem58+1;
	A = tab36096[X];
	if (A != 69) goto pos37157;   // 'E'
	X++;
	Y = tab36096[X];
	X--;
	A = tab36376[Y] & 128;
	if(A == 0) goto pos37108;
	X++;
	A = tab36096[X];
	if (A != 82) goto pos37113;     // 'R'

pos37108:
	mem58 = X;
	goto pos37184;
	
pos37113:
	if ((A == 83) || (A == 68)) goto pos37108;  // 'S' 'D'
	if (A != 76) goto pos37135; // 'L'
	X++;
	A = tab36096[X];
	if (A != 89) goto pos36700;
	goto pos37108;
		
pos37135:
	if (A != 70) goto pos36700;
	X++;
	A = tab36096[X];
	if (A != 85) goto pos36700;
	X++;
	A = tab36096[X];
	if (A == 76) goto pos37108;
	goto pos36700;

pos37157:
	if (A != 73) goto pos36700;
	X++;
	A = tab36096[X];
	if (A != 78) goto pos36700;
	X++;
	A = tab36096[X];
	if (A == 71) goto pos37108;
	goto pos36700;
	
pos37180:
	A = mem60;
	mem58 = A;
	
pos37184:
	Y = mem65 + 1;

	if(Y == mem64) goto pos37455;
	mem65 = Y;
	A = GetRuleByte(mem62, mem63, Y);
	mem57 = A;
	X = A;
	A = tab36376[X] & 128;
	if(A == 0) goto pos37226;
	X = mem58+1;
	A = tab36096[X];
	if (A != mem57) goto pos36700;
	mem58 = X;
	goto pos37184;

pos37226:
	A = mem57;
	if (A == 32) goto pos37295;   // ' '
	if (A == 35) goto pos37310;   // '#'
	if (A == 46) goto pos37320;   // '.'
	if (A == 38) goto pos37335;   // '&'
	if (A == 64) goto pos37367;   // ''
	if (A == 94) goto pos37404;   // ''
	if (A == 43) goto pos37419;   // '+'
	if (A == 58) goto pos37440;   // ':'
	if (A == 37) goto pos37077;   // '%'
	return -1;
	
pos37295:
	Code37066(mem58);
	A = A & 128;
	if(A != 0) goto pos36700;
		
pos37305:
	mem58 = X;
	goto pos37184;
	
pos37310:
	Code37066(mem58);
	A = A & 64;
	if(A != 0) goto pos37305;
	goto pos36700;
	
pos37320:
	Code37066(mem58);
	A = A & 8;
	if(A == 0) goto pos36700;

pos37330:
	mem58 = X;
	goto pos37184;

pos37335:
	Code37066(mem58);
	A = A & 16;
	if(A != 0) goto pos37330;
	A = tab36096[X];
	if (A != 72) goto pos36700;
	X++;
	A = tab36096[X];
	if ((A == 67) || (A == 83)) goto pos37330;
	goto pos36700;

pos37367:
	Code37066(mem58);
	A = A & 4;
	if(A != 0) goto pos37330;
	A = tab36096[X];
	if (A != 72) goto pos36700;
	if ((A != 84) && (A != 67) && (A != 83)) goto pos36700;
	mem58 = X;
	goto pos37184;
	
pos37404:
		Code37066(mem58);
		A = A & 32;
		if(A == 0) goto pos36700;
			
pos37414:
		mem58 = X;
		goto pos37184;
	
pos37419:
	X = mem58;
	X++;
	A = tab36096[X];
	if ((A == 69) || (A == 73) || (A == 89)) goto pos37414;
	goto pos36700;
	
pos37440:
	Code37066(mem58);
	A = A & 32;
	if(A == 0) goto pos37184;
	mem58 = X;
	goto pos37440;

pos37455:
	Y = mem64;
	mem61 = mem60;

pos37461:
	A = GetRuleByte(mem62, mem63, Y);
	mem57 = A;
	A = A & 127;
	if (A != 61) {
		mem56++;
		X = mem56;
		input[X] = A;
	}
	
	if ((mem57 & 128) == 0 ) goto pos37485; //???
	goto pos36554;

pos37485:
	Y++;
	goto pos37461;
}
