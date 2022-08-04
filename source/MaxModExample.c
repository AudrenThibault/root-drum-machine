#include <nds.h>
#include <maxmod9.h>
#include <stdio.h>
//#include <tonc.h>
#include <string.h>
#include <stdlib.h>
// pour le delay sur linux #include <unistd.h>
//#include <windows.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <nds/ndstypes.h>

#include "soundbank.h"
#include "soundbank_bin.h"

#include "r6502_portfont_bin.h"

#include "include.h"
#include "palette.h"
#include "help.h"


#define MAPADDRESS		BG_BMP_BASE(31)	// our base map address
#define SCREEN_BUFFER  ((u16 *) 0x6000000)

#define MAX_COLUMNS	16
//nombre de colonnes
int less_columns = MAX_COLUMNS;

//#define MAX_ROWS	12
#define MAX_ROWS	12

//screen modes

#define SCREEN_PATTERN 	0
#define SCREEN_SONG		1
#define SCREEN_SETTINGS	2
#define SCREEN_SAMPLES	3
#define SCREEN_HELP     4

//play

#define PLAY_STOPPED	0
#define PLAY_PATTERN 	1 // loop a pattern
#define PLAY_SONG		2 // no looping
#define PLAY_LIVE 		3 // loop 'chains' of patterns

#define NO_PATTERN 255

u8 play = PLAY_STOPPED;
int play_next = NO_PATTERN;

//loop modes
#define SONG_MODE		0
#define LIVE_MODE		1

const char *loop_mode_text[] = { "SONG", "LIVE"};

//sync modes

#define SYNC_NONE	0
#define SYNC_SLAVE	1 		//lsdj serial
#define SYNC_MASTER 2 		//lsdj serial
#define SYNC_TRIG_SLAVE 3	//pulses
#define SYNC_TRIG_MASTER 4 	//pulses
#define SYNC_MIDI	5 		//midi clock in

const char *sync_mode_text[] = { "NONE", "SLAVE", "MASTER", "NANO SLAVE","NANO MASTER","MIDI" };

//menu opts

#define MENU_ACTION 	0
#define MENU_LEFT 		1
#define MENU_RIGHT 		2
#define MENU_UP 		3
#define MENU_DOWN 		4

unsigned int current_screen;

unsigned int cursor_x;
unsigned int cursor_y;
unsigned int cursor_secondSeq_y;

unsigned int xmod = 0;
unsigned int xmodSecondSeq = 0;

unsigned int ymod = 3;
unsigned int draw_flag = 0;

unsigned int last_xmod=0;
unsigned int last_xmod_secondSeq=0;

unsigned int last_column=0;
unsigned int last_secondSeq_column=0;

int current_column = -1;
int current_secondSeq_column = -1;


mm_sfxhand active_sounds[MAX_ROWS];

const int pitch_table[] = { 512,542, 574, 608, 645, 683, 724, 767, 812, 861, 912, 966, 
	1024, 1084, 1149, 1217, 1290, 1366, 1448, 1534, 1625, 1722, 1824, 1933, 2048 };

const char note_table[] = {'C', 'd', 'D', 'e', 'E', 'F', 'g', 'G', 'a', 'A', 'b', 'B', 
						   'C', 'd', 'D', 'e', 'E', 'F', 'g', 'G', 'a', 'A', 'b', 'B', 'C'};

const char *pan_string[] = {"L  ", "L+R", "  R"};

#define CHANNEL_SAMPLE 0
#define CHANNEL_VOLUME 1
#define CHANNEL_PITCH  2
#define CHANNEL_PAN    3

#define PAN_CENTER 128
#define PAN_LEFT 0
#define PAN_RIGHT 255
#define PAN_OFF 1						   

#define STATUS_STOPPED			0
#define STATUS_PLAYING_PATTERN	1
#define STATUS_PLAYING_SONG		2

#define STATUS_SYNC_STOPPED		3
#define STATUS_SYNC_PATTERN		4
#define STATUS_SYNC_SONG		5

#define STATUS_SAVE				6
#define STATUS_LOAD				7
#define STATUS_CLEAR			8

int serial_ticks = 0;


char *status_text[] = { "            ", "PATTERN PLAY", "SONG PLAY   ", "SYNC WAIT   ", 
		"SYNC PATTERN", "SYNC SONG   ", "SAVED OK    ", "LOADED OK   ", "CLEARED     "};
// j'ai commenté la fonction draw_status

unsigned int status = 0;

//Timing stuff

#define MIN_BPM 30
#define MAX_BPM 300

int current_ticks = 0;

#define TICK 0
#define TOCK 1

int tick_or_tock = 0;
int millis_between_ticks;

int ticks;
int tocks;



#define REG_TM2D *(vu16*)0x4000108 		//4000108h - TM2CNT_L - Timer 2 Counter/Reload (R/W)
#define REG_TM2C *(vu16*)0x400010A 		//400010Ah - TM2CNT_H - Timer 2 Control (R/W)

#define REG_TM3D *(vu16*)0x400010C 		//400010Ch - TM3CNT_L - Timer 3 Counter/Reload (R/W)
#define REG_TM3C *(vu16*)0x400010E   	//400010Eh - TM3CNT_H - Timer 3 Control (R/W)

#define TM_ENABLE 		(vu16)0x80
#define TM_CASCADE 		(vu16)0x4
#define TM_IRQ			(vu16)BIT(6)

#define TM_FREQ_1 		(vu16)0x0
#define TM_FREQ_64 		(vu16)0x1
#define TM_FREQ_256		(vu16)0x2
#define TM_FREQ_1024 	(vu16)0x3

/* REG_BG3CNT bits */
#define RCNT_GPIO_DATA_MASK		0x000f
#define RCNT_GPIO_DIR_MASK		0x00f0
#define RCNT_GPIO_INT_ENABLE	(1 << 8)
#define RCNT_MODE_GPIO			0x8000

u8 *sram = (u8 *) 0x0E000000;

//song data structure
 
typedef struct {
	u8 volume; 		//0=off, 1=normal 2=accent
	s8 pitch;		//1024=normal pitch 
	u8 pan; 		//128= normal, 0=left, 255=right
	u8 random;
	u8 activeNote;
	u8 playOrNot;
	u8 playOrNotCount;
	u8 optionsUp;
} pattern_row; 

typedef struct {
	pattern_row rows[MAX_ROWS];
} pattern_column;  /* 256 bytes */

typedef struct pattern {
	pattern_column columns[MAX_COLUMNS];
	pattern_column columnsSecondSeq[MAX_COLUMNS];
} pattern;

#define MAX_PATTERNS	50
#define MAX_ORDERS		100

#define NO_ORDER		-1
 
#define HEADER_SIZE		150

typedef struct channel {
	u8 sample;
	u8 volume;
	u8 pitch;
	u8 pan;
	u8 status;
} channel;
 
typedef struct song {
	//Constant size (30 bytes)
	char tag[8];
	
	char song_name[8];
	float bpm;
	u8 shuffle;
	u8 loop_mode;
	u8 sync_mode;
	u8 color_mode;
	//u16 samples[MAX_ROWS];
	channel channels[MAX_ROWS];
	
	int pattern_length;
	int order_length;
	
	//char marker[10];
	
	//Dynamic size
	pattern patterns[MAX_PATTERNS];
	short order[MAX_ORDERS+1];
} song;
 
song *current_song;
pattern *current_pattern;
 
int current_pattern_index=0;
int playing_pattern_index;
int order_index;

pattern pattern_buff;

#define NOT_MUTED 	' '
#define MUTED 		'M'
#define SOLOED		'S'

//int channel_status[MAX_ROWS];

u8 order_buff;
int order_page = 0;

int pas = 0; //nombre de fois que le curseur revient au début
int tours = 0;
int iaChangeRandom = false;
int iaPreviousPatern = false;
int randomEnCours = true;
int step = 0;
int bpmIaAugmented = false;
int bpmAugmentedSecondTurn = false;
int randVolOne = 0;
int randVolTwo = 0;
int alternateVol = 0;
int IALimit = 28;
int bpmFolies[] = { 10, -20, 30, -10, 20, -30, 0 }; 
char typeOfSyncs = 0;
int valueBpmFolies = 0;
int randBpmFolies = 0;
int bpmFoliesActivated = false;
int syncDSMode = false;
int bottomPrinted = false;
int touchedBtn = false;
int IARandom = false;
int IABpm = true;
int sound = 0;
int gbSoundStatus = 0;
int gbSoundDecay = 5;
int streamOpen = false;
int streamChange = 0;
int globalPitch = 0;
int superLiveMod = false;
int unSurCombien = 0;
int IAAutoCymbal = false;
int changedPatternOne = 0;
int soundsInCurrentPattern = 0;
int soundsInPattern[50];
int autoLoopPattern = false;
int blanc = "\x1b[37m\x1b[0m";
int gris = "\x1b[30m\x1b[0m";
int rouge = "\x1b[31m\x1b[0m";
int vert = "\x1b[32m\x1b[0m";
int jaune = "\x1b[33m\x1b[0m";
int bleu = "\x1b[34m\x1b[0m";
int violet = "\x1b[35m\x1b[0m";
int cyan = "\x1b[36m\x1b[0m";
// mm_stream mystream;
// mm_word mmStreamGetPosition();
int countColons;
int cursorOnSecondSeq = false;
// int countTest;
int pitchSecondSeq;
int polyRhythm = false;
int cursorPan = false;
int trackPan[] = {128, 0, 255};
int panChange;
int panTab[11] = { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 };

PrintConsole topScreen;
PrintConsole bottomScreen;

touchPosition touch;

// mm_sound_effect gbSyncSound = {
// 	{SFX_GBSYNC},			 // id
// 	(int)(1.0f * (1 << 10)), // rate
// 	0,						 // handle
// 	255,					 // volume
// 	255,					 // panning
// };

// // volca
// mm_sound_effect syncSound = {
// 	{SFX_SYNC},				 // id
// 	(int)(1.0f * (1 << 10)), // rate
// 	0,						 // handle
// 	255,					 // volume ancien: 190
// 	255,					 // panning
// };

// mm_sound_effect ambulance = {
// 	{ SFX_AMBULANCE } ,			// id
// 	(int)(1.0f * (1<<10)),	// rate
// 	0,		// handle
// 	255,	// volume
// 	0,		// panning
// };

// mm_sound_effect boom = {
// 	{ SFX_BOOM } ,			// id
// 	(int)(1.0f * (1<<10)),	// rate
// 	0,		// handle
// 	255,	// volume
// 	255,	// panning
// };



// REG_SND1FREQ = 0;


#define MAX_SAMPLES MSL_NSAMPS

char string_buff[50];

void set_bpm_to_millis(){
	millis_between_ticks = (int) (((1.0f/(current_song->bpm/60.0f))/0.0001f)/4)/6;
}

int set_shuffle(){
	/*
	shuffle	t/t	 %
	1   	1,1	 18	
	2   	2,10 17
	3   	3, 9 25	
	4   	4, 8 33
	5   	5, 7 42
	6   	6, 6 50
	7   	7, 5 58
	8   	8, 4 67
	9   	9, 3 75
	10 	   10, 2 83
	11	   11, 1 92
	*/
	
	ticks = current_song->shuffle;
	tocks = 12-current_song->shuffle;
	
	return  (int) 100.0f/(12.0/current_song->shuffle);
}


void setup_timers(){
	// Overflow every ~0.01 millisecond:
    // 0.0001/59.59e-9 = 1678.133915086424
    // 0x68e ticks @ FREQ_1 
	REG_TM2D= -0x68e;          
    REG_TM2C= TM_FREQ_1; 
	
    // cascade into tm3
    REG_TM3D= -millis_between_ticks;
    REG_TM3C= TM_ENABLE | TM_CASCADE | TM_IRQ;
}

void update_timers(){
    REG_TM3D= -millis_between_ticks;
    REG_TM3C=0;
    REG_TM3C= TM_ENABLE | TM_CASCADE | TM_IRQ;
}

void serial_interrupt(void) __attribute__ ((section(".iwram")));
void gpio_interrupt(void) __attribute__ ((section(".iwram")));	
void timer3_interrupt(void) __attribute__ ((section(".iwram")));
void vblank_interrupt(void) __attribute__ ((section(".iwram")));

void enable_serial_interrupt(){
	//change for serial comms	
	irqSet( IRQ_SPI, serial_interrupt );

	REG_BG3CNT = GFX_NORMAL;//RCNT_MODE_GPIO | RCNT_GPIO_INT_ENABLE;

	//REG_DIVCNT = SIO_8BIT | SIO_IRQ; //desactivé en mode nds

	//set up interrupts for serial
	irqDisable(IRQ_TIMER3);
	irqEnable(IRQ_VBLANK | IRQ_SPI);
}

void disable_serial_interrupt(){
	//change for serial comms
	
	REG_BG3CNT = 0;
	irqDisable(IRQ_SPI);
	irqEnable(IRQ_VBLANK | IRQ_TIMER3);
}

void enable_trig_interrupt(){
	irqSet( IRQ_SPI, gpio_interrupt );
		
	REG_BG3CNT = RCNT_MODE_GPIO | RCNT_GPIO_INT_ENABLE;

	//set up interrupts for serial
	irqDisable(IRQ_TIMER3);
	irqEnable(IRQ_VBLANK | IRQ_SPI);
}

void disable_trig_interrupt(){
	REG_BG3CNT = 0;
	irqDisable(IRQ_SPI);
	irqEnable(IRQ_VBLANK | IRQ_TIMER3);
}

void init_song(){
	//Song Variables	
	memcpy(current_song->tag, "GBA DRUM", 8);
	memcpy(current_song->song_name, "SONGNAME", 8);
	
	current_song->bpm = 120;
	current_song->shuffle = 6; 	//6x6 = 50% shuffle
	set_bpm_to_millis();
	set_shuffle();
	
	current_song->loop_mode = SONG_MODE;
	current_song->color_mode = 0;
	
	//Pattern inits
	int pc;
	int row;
	int column;
	
	for (pc = 0; pc < MAX_PATTERNS; pc++){	
		for (column = 0; column < less_columns; column++){
			for (row = 0; row < MAX_ROWS; row++){
				pattern_row *current_cell = &(current_song->patterns[pc].columns[column].rows[row]);
				current_cell->volume = 0; 
				current_cell->pitch = 0;
				current_cell->pan = PAN_OFF;
			}
		}
	}


	//Orders inits
	int i;
	
	for (i = 0; i <= MAX_ORDERS; i++){
		current_song->order[i]=NO_ORDER;
	}
	
	//Samples inits
	for (i = 0; i < MAX_ROWS; i++){
		current_song->channels[i].sample = i;
		current_song->channels[i].volume = 200;
		current_song->channels[i].pitch = 12;
		current_song->channels[i].pan = PAN_CENTER;
		current_song->channels[i].status = NOT_MUTED;
	}	
}

void rand_pattern(){

	if (tours > IALimit) {
		int row=0;
		int column=0;
		u8 val;

		if (step == 0) {
			randVolOne = 0;
			randVolTwo = 0;
		}
		
		if (tours % 3 == 0 && IAAutoCymbal == true) { //ia auto cymbal
			pattern_row *current_cell;
			if (current_song->bpm >= 200) {
				current_cell = &(current_song->patterns[0].columns[0].rows[10]);
				current_cell->volume = 2;
				current_cell->pan = PAN_OFF;
				current_cell->pitch = 0;
				current_cell = &(current_song->patterns[0].columns[8].rows[10]);
				current_cell->volume = 2;
				current_cell->pan = PAN_OFF;
				current_cell->pitch = 0;
			} else {
				current_cell = &(current_song->patterns[0].columns[0].rows[10]);
				current_cell->volume = 2;
				current_cell->pan = PAN_OFF;
				current_cell->pitch = 0;
				current_cell = &(current_song->patterns[0].columns[4].rows[10]);
				current_cell->volume = 2;
				current_cell->pan = PAN_OFF;
				current_cell->pitch = 0;
				current_cell = &(current_song->patterns[0].columns[8].rows[10]);
				current_cell->volume = 2;
				current_cell->pan = PAN_OFF;
				current_cell->pitch = 0;
				current_cell = &(current_song->patterns[0].columns[12].rows[10]);
				current_cell->volume = 2;
				current_cell->pan = PAN_OFF;
				current_cell->pitch = 0;
			}

		} else if (tours <= IALimit * 2 && IARandom == true) { //ia random bordélique
			//ia random bordélique
			pattern_row *current_cell = &(current_song->patterns[current_pattern_index].columns[column].rows[row]);

			for (column = 0; column < less_columns; column++){
				for (row = 0; row < MAX_ROWS; row++) {	
					// if (row == 4 || row == 7 || row == 10) {
						val = (u8)rand(); //ancien
						int randPan[] = {128, 0, 255}; 
						int randPanRandom = rand() % 3;
						//random pitch entre 10 et 20
						// int lower = 8;
						// int upper = 16;
						// int randPitch = rand() % (upper - lower + 1) + lower;
						int panRandom = randPan[randPanRandom];
						if (val > 245) {  //ancien
							randVolTwo++;
							if (randVolTwo < 5) {
								current_cell->volume = 2;
								current_cell->pan = panRandom;
								// current_cell->pitch=randPitch;
							}
						} else if (val < 245 && val > 220){	
							randVolOne++;
							if (randVolOne < 5) {
								current_cell->volume = 1;
								current_cell->pan = panRandom;
								// current_cell->pitch=randPitch;
							}
						} else {
							current_cell->volume = 0;
						}	
						current_cell->pitch = 0;
						// current_cell->pan = PAN_OFF;
						current_cell++;
					// }		
				}
			}
		}
	}

}

void rand_samples(){
	int i, ii, val;
	
	for (i = 0; i < MAX_ROWS; i++){	
		val = rand() % MAX_SAMPLES;
		for (ii = 0; ii < MAX_ROWS; ii++){
			if (val == current_song->channels[ii].sample){
				val = rand() % MAX_SAMPLES;
				ii = 0;
			}
		}
		current_song->channels[i].sample = val;
	}
}

void clear_pattern(){
	int column, row;
	pattern_row *current_cell;
	
	for (column = 0; column < less_columns; column++){
		for (row = 0; row < MAX_ROWS; row++){
			current_cell = &(current_song->patterns[current_pattern_index].columns[column].rows[row]);
			current_cell->volume = 0; 
			current_cell->pitch = 0;
			current_cell->pan = PAN_OFF;
		}
	}

}

void copy_pattern(){
	pattern *from = &(current_song->patterns[current_pattern_index]);
	pattern *to = &pattern_buff;
	 
	memcpy(to, from, sizeof(pattern));
}		

void paste_pattern(){
	pattern *from = &pattern_buff;
	pattern *to = &(current_song->patterns[current_pattern_index]);
	 
	memcpy(to, from, sizeof(pattern));
}

void copy_order(short *from){
	if (*from != NO_ORDER){
		order_buff = *from;
	}
}

void paste_order(short *to){
	if (order_buff != NO_ORDER){
		*to = order_buff;
	}
}

//son gameboy
int sine;		// sine position
int lfo;		// LFO position
int sine_freq = 0;
int lfo_freq = 0;
int lfo_shift = 0;

//-----------------------------------------------------------------------------
// enum {
//-----------------------------------------------------------------------------
	// waveform base frequency
	// sine_freq = s_freq;
	
	// // LFO frequency
	// lfo_freq = l_freq;
	
	// // LFO output shift amount
	// lfo_shift = 0;
	
	// // blue backdrop
	// bg_colour = 13 << 10,
	
	// // red cpu usage
	// cpu_colour = 31
// };


mm_word on_stream_request( mm_word length, mm_addr dest, mm_stream_formats format ) {
//----------------------------------------------------------------------------------
	
	// s16 *target = dest;
	
	// //------------------------------------------------------------
	// // synthensize a sine wave with an LFO applied to the pitch
	// // the stereo data is interleaved
	// //------------------------------------------------------------
	// int len = 3000;
	// // if (gbSoundDecay != 0) {
	// 	for( ; len; len-- )
	// 	{
	// 		int sample = sinLerp(sine);
			
	// 		// output sample for left
	// 		*target++ = sample;
			
	// 		// output inverted sample for right
	// 		*target++ = -sample;
			
	// 		// sine += sine_freq + (sinLerp(lfo) >> lfo_shift);
	// 		// lfo = (lfo + lfo_freq);

	// 		sine += sine_freq;
	// 	}
	// 	sine = sine_freq; //ne pas enlever sinon ça plante
	// // }
	
	
	// // iprintf("\x1b[0;0Hcaca %d", mmStreamGetPosition());

	// return length;
}


void play_sample(row, sample, vol, pitch, pan) {

	if (globalPitch != 0 && superLiveMod == true) {
		pitch = globalPitch;
	}
	// if (row != 3) {
		if (sample != SFX_SYNC && sample != SFX_GBSYNC) {
			mm_sound_effect sfx = {
				{sample} ,			// id
				(int)(1.0f * (pitch)),	// rate
				0,		// handle
				vol,	// volume
				pan,	// panning
			};
			mmLoadEffect(sample);
			active_sounds[row] = mmEffectEx(&sfx); //mmeffect returns mmhandler into the array
		}
	// } else { //row == 3
	// 	// sine_freq = 900;
	// 	// lfo_freq = 0;
	// 	// lfo_shift = 0;
	// 	// // mystream.sampling_rate	= 50000;					// sampling rate = 25khz
	// 	// mmStreamUpdate();
	// 	// gbSoundDecay = 5;
	// }
}

void play_sample_sync(row, sample) {
	mmLoadEffect(sample);
	mm_sound_effect sfx = {
		{sample} ,			// id
		(int)(1.0f * (1024)),	// rate
		0,		// handle
		255,	// volume
		255,	// panning
	};
	active_sounds[row] = mmEffectEx(&sfx); //mmeffect returns mmhandler into the array
}

void play_sound(int row, int sample, int vol, int pitch, int pan){

	if (active_sounds[row]){ 				//if theres a sound playing in that row
		mmEffectCancel(active_sounds[row]); //cancel it before trigging it again
		mmUnloadEffect(sample);
		// if (row == 3) {
		// 	// mmStreamClose();
		// 	sine_freq = 0;
		// 	lfo_freq = 0;
		// 	lfo_shift = 0;
		// }
	}
	
	if (row != 11) {
		if (current_song->channels[row].status != MUTED){		
			//je passe tout d'un côté du panoramique si le 
			//sync mode est activé, le son d'un côté le sync de l'autre
			//le son d'un côté
			if (syncDSMode == true) {
				pan = 0; //j'écrase pan
			}
			//break out into function set_effect_params?
			play_sample(row, sample, vol, pitch, pan);
		}
	} else { //row est = à 11
		if (syncDSMode == true) {
			if (typeOfSyncs == 0) { //Volca
				play_sample_sync(row, SFX_SYNC);
			} else if (typeOfSyncs == 1) { //Lsdj
				play_sample_sync(row, SFX_GBSYNC);
			}
			else if (typeOfSyncs == 2) { //Wolf1
				play_sample_sync(row, SFX_SYNCWOLF);
			}
		} else {
			play_sample(row, sample, vol, pitch, pan);
		}
	}
}
	
void play_column(void) __attribute__ ((section(".iwram")));

void play_column(){
	int row=0;
	int val,pan,pitch;
	pattern_row *pv;
	pv = &(current_song->patterns[playing_pattern_index].columns[current_column].rows[row]);
	pattern_row *pvSecondSeq;
	pvSecondSeq = &(current_song->patterns[playing_pattern_index].columns[current_secondSeq_column].rows[row]);
	pattern_row *pvToUse;

	if (current_song->bpm > 240) {
		current_song->bpm = current_song->bpm / 2;
		draw_bpm_pattern();
		set_bpm_to_millis();
		update_timers();
	}

	pas++;
	pas = pas % 16;

	

	if (step == 8) {
		iaChangeRandom = !iaChangeRandom;
		iaPreviousPatern = !iaPreviousPatern;
		step = 0;
	}
	if (pas == 0) {
		tours++;
		step++;
		gbSoundDecay++;
	}

	if (gbSoundDecay > 0) {
		gbSoundDecay--;
	} else {
		// soundPause(sound);
		// mmStreamClose();
		// sine_freq = 0;
		// lfo_freq = 0;
		// lfo_shift = 0;
	}

	if (tours == 32 && bpmIaAugmented == false && bpmFoliesActivated == true) {
		current_song->bpm = current_song->bpm + 20;
		draw_bpm_pattern();
		set_bpm_to_millis();
		update_timers();
		bpmIaAugmented = true;
	}
	//64
	if (tours >= IALimit * 2) {
		if (tours % 32 == 0) { //32
			if (bpmAugmentedSecondTurn == false && bpmFoliesActivated == true) {
				randBpmFolies = rand() % 5;
				if (bpmFolies[randBpmFolies] == 0) {
					if (current_song->bpm > 100) {
						current_song->bpm = current_song->bpm/2;
					} else {
						current_song->bpm = current_song->bpm*2;
					}
				} else {
					valueBpmFolies = bpmFolies[randBpmFolies];
					current_song->bpm = current_song->bpm + valueBpmFolies;
				}
				draw_bpm_pattern();
				set_bpm_to_millis();
				update_timers();
				bpmAugmentedSecondTurn = true;
			}
		} else {
			bpmAugmentedSecondTurn = false;
		}
	}
	sprintf(string_buff,"tours:%d ", (int) tours);
	put_string(string_buff,22,18,0);
	sprintf(string_buff,"step:%d ", (int) step);
	put_string(string_buff,22,19,0);
	// sprintf(string_buff,"chPtTwo %d", changedPatternTwo);
	// put_string(string_buff,22,16,0);
	// sprintf(string_buff,"One %d    ", changedPatternOne);
	// put_string(string_buff,0,18,0);
	//la condition tours est dans rand_pattern()
	if (IAAutoCymbal == true || IARandom == true) {
		if (step < 4) {
			if (iaChangeRandom == false) {
				copy_pattern();
				rand_pattern();
				draw_pattern();
				// iaChangeRandom = true;
				iaChangeRandom = !iaChangeRandom;
			}
		} 
		else if (tours > IALimit) {
			if (iaPreviousPatern == false) {
				paste_pattern();
				draw_pattern();
				iaPreviousPatern = !iaPreviousPatern;
				//iaChangeRandom = false;
			}
		}
		
	}
	for (row = 0; row < MAX_ROWS; row++){
		if (polyRhythm == true && (row > 5)) {
			pvToUse = pvSecondSeq;
		} else {
			pvToUse = pv;
		}
		//val = pv->volume;
		if (pvToUse->volume != 0){
			if (pvToUse->pan != PAN_OFF)
				pan = pvToUse->pan;
			else
				pan = current_song->channels[row].pan;
			
			pitch = current_song->channels[row].pitch+pvToUse->pitch;
			
			if (pitch < 0){
				pitch = 0;
			} else if (pitch > 24){
				pitch = 24;
			}
		}
		// if (pvSecondSeq->volume != 0){
		// 	if (pvSecondSeq->pan != PAN_OFF)
		// 		pan = pvSecondSeq->pan;
		// 	else
		// 		pan = current_song->channels[row].pan;
			
		// 	pitchSecondSeq = current_song->channels[row].pitch+pvSecondSeq->pitch;
			
		// 	if (pitchSecondSeq < 0){
		// 		pitchSecondSeq = 0;
		// 	} else if (pitchSecondSeq > 24){
		// 		pitchSecondSeq = 24;
		// 	}
		// }
		
		//joue 1 fois sur 2, 1/3 etc...
		if (pvToUse->playOrNot > 0) {
			// iprintf("\x1b[0;0HplayOrNot %d", pv->playOrNot);
			// iprintf("\x1b[1;0HUnSurCombien %d", pv->playOrNotCount);
			if (pvToUse->playOrNot > 0) {
				if (pvToUse->playOrNotCount % pvToUse->playOrNot == 0) {
					play_sound(row, 
					current_song->channels[row].sample, 
					current_song->channels[row].volume+55, 
					pitch_table[pitch], 
					pan);
				} 
				if (pvToUse->playOrNotCount > pvToUse->playOrNot - 1) {
					pvToUse->playOrNotCount = 0;
				}
			} 
			pvToUse->playOrNotCount++;
		}

		// random perso
		else if (pvToUse->random == true) // R
		{
			int r = rand() % MAX_SAMPLES; /* random int between 0 and 24 */
			//int sampleR = current_song->channels[row].sample;
			int sampleRandom = r;
			int randPan[] = {128, 0, 255}; 
			int randPanRandom = rand() % 3;
			int panRandom = randPan[randPanRandom];
			play_sound(row, 
			sampleRandom, 
			current_song->channels[row].volume, 
			pitch_table[12], //1024 = pitch normal
			panRandom);

		// les autres modes que random (o et O)
		} 
		//else if (polyRhythm == true) {
			// if (row >= 0 && row <= 5) { //seq 1
			// 	if (pv->volume == 1) { // o
			// 		play_sound(row, 
			// 		current_song->channels[row].sample, 
			// 		current_song->channels[row].volume, 
			// 		pitch_table[pitch], 
			// 		pan);
			// 	} else if (pv->volume == 2) { // O
			// 		play_sound(row, 
			// 		current_song->channels[row].sample, 
			// 		current_song->channels[row].volume+55, 
			// 		pitch_table[pitch], 
			// 		pan);
			// 	}
			// } else {
			// 	if (pvSecondSeq->volume == 1) { //seq 2 //o
			// 		pitch = pitchSecondSeq;
			// 		play_sound(row, 
			// 		current_song->channels[row].sample, 
			// 		current_song->channels[row].volume, 
			// 		pitch_table[pitch], 
			// 		pan);
			// 	} else if (pvSecondSeq->volume == 2) {  //O
			// 		pitch = pitchSecondSeq;
			// 		play_sound(row, 
			// 		current_song->channels[row].sample, 
			// 		current_song->channels[row].volume+55, 
			// 		pitch_table[pitch], 
			// 		pan);
			// 	}
			// }
		
		else if (pvToUse->volume == 1) { // o
			play_sound(row, 
			current_song->channels[row].sample, 
			current_song->channels[row].volume, 
			pitch_table[pitch], 
			pan);
		}
		else if (pvToUse->volume == 2) { // O
			//il faut mettre current_song->channels[row].sample+1, le +1 change le sample
			play_sound(row, 
			current_song->channels[row].sample, 
			current_song->channels[row].volume+55, 
			pitch_table[pitch],
			pan);
		}

		pv++;
		pvSecondSeq++;
	}
}


void define_palettes(int palette_option){
	u32 i;
	u16 *temppointer;

	temppointer = BG_PALETTE;
	for(i=0; i<7; i++) {
		*temppointer++ = palettes[palette_option][0][i];		
	}
	temppointer = (BG_PALETTE+16);
	for(i=0; i<7; i++) {
		*temppointer++ = palettes[palette_option][1][i];		;
	}
	temppointer = (BG_PALETTE+32);
	for(i=0; i<7; i++) {
		*temppointer++ = palettes[palette_option][2][i];		;
	}
	temppointer = (BG_PALETTE+48);
		for(i=0; i<7; i++) {
		*temppointer++ = palettes[palette_option][3][i];		;
	}
}

void clear_screen(){
	//pour la gba
	// clear screen map with tile 0 ('space' tile) (256x256 halfwords)
	//*((u32 *)MAP_BASE_ADR(31)) =0;
	//CpuFastSet( MAP_BASE_ADR(31), MAP_BASE_ADR(31), FILL | COPY32 | (0x800/4));
	//pour la ds
	//PA_Clear8bitBg(0); //tester 0 ou 1
}

void put_character(char c, u16 x, u16 y, unsigned char palette) {
	// int offset;
	// u16 value;

	// if (c < 32)
	// 	c = 32;
	// value = ((u16) (c - 32)) | (((u16) palette) << 12);
	// offset = (y * 32) + x;
	
	// *((u16 *)MAPADDRESS + offset) = value;

	//le reste sert peut être plus à rien
	//iprintf("\x1b[%d;%dH%s", y, x, c);

}

void put_string(char *s, u16 x, u16 y, unsigned char palette) {
	u16 x_index = x;
	while (*s != 0 ) {
		if(*s == '\n'){
			s++;
			y++;
			x = x_index;
		}
		dessiner_char(*s, x, y, palette);
		x++;
		s++;
	}
}

void set_palette(u16 x, u16 y, unsigned char palette) {
	int offset;
	u16 value;

	value = (((u16) palette) << 12);
	offset = (y * 32) + x;
	
	*((u16 *)MAPADDRESS + offset) &= 0x42;
	*((u16 *)MAPADDRESS + offset) |= value;
}

void dessiner_char(string, x, y, sertArien) {
	consoleSelect(&topScreen);
	iprintf("\x1b[%d;%dH%c", y, x, string);
}

void dessiner_int(integer, x, y, sertArien) {
	consoleSelect(&topScreen);
	iprintf("\x1b[%d;%dH%d", y, x, integer);
}

void draw_cursor(){
	if (current_screen == SCREEN_PATTERN) {
		if (draw_flag == 0){
			xmod = 5+(cursor_x/4);
			printf(rouge);
			// iprintf("\x1b[1;15Hcursor_xlol %d ", cursor_x);			
			if (cursorPan == true && cursor_x == 0) {
				//rien
			} else {
				dessiner_char('X', cursor_x+xmod, cursor_y+3, 1);
			}
			printf(blanc);
		}
	} else if (current_screen == SCREEN_SONG) {
		dessiner_char('X', 2+(cursor_x)+(cursor_x*5), cursor_y+3, 1);
	} 
	else if (current_screen == SCREEN_SAMPLES) {
		if (draw_flag == 0){
			dessiner_char('>', (cursor_x*6)+4, cursor_y+3, 1);
		}
	}
	else if (current_screen == SCREEN_SETTINGS) {
		if (draw_flag == 0){
			dessiner_char('>', cursor_x, cursor_y+3, 1);
		}
	}
}

void draw_cell_status(){
	//26, 27, 28, 29
	pattern_row *pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
	
	if (cursorPan == false || (cursor_x != 16 || cursor_x != 0 || cursor_x != 15)) {
		if (pv->volume != 0){

			if (pv->optionsUp == 0) {
				put_string("   ",29,3+cursor_y,0);
			} else if (pv->optionsUp > 0) {
				printf(vert);
				if (pv->optionsUp == 1) {
					sprintf(string_buff, "o  ");
					put_string(string_buff,29,3+cursor_y,0);
				} else if (pv->optionsUp == 2) {
					sprintf(string_buff, "O  ");
					put_string(string_buff,29,3+cursor_y,0);
				} else if (pv->optionsUp == 3) {
					sprintf(string_buff, "Rnd");
					put_string(string_buff,29,3+cursor_y,0);
				}
				
				else if (pv->optionsUp > 3) {
					sprintf(string_buff, "1/%x", pv->optionsUp-2);
					put_string(string_buff,29,3+cursor_y,0);
				} 
				// else {
				// 	sprintf(string_buff, "%x", pv->optionsUp);
				// 	put_string(string_buff,29,3+cursor_y,0);	
				// }
				// sprintf(string_buff,"fesss %d", pv->optionsUp);
				// put_string(string_buff,0,0,0);
				printf(blanc);	
			}

			//pitch, pan, solo
			if (pv->pitch == 0){
				put_string("  ",26,3+cursor_y,0);
			} else if (pv->pitch > 0){
				printf(jaune);
				sprintf(string_buff, "+%x", pv->pitch);
				put_string(string_buff,26,3+cursor_y,0);
				printf(blanc);
			} else {
				printf(jaune);
				sprintf(string_buff, "-%x", pv->pitch*-1);
				put_string(string_buff,26,3+cursor_y,0);
				printf(blanc);
			}
			if (pv->pan == PAN_LEFT){
				dessiner_char('L',28,3+cursor_y,0);
			} else if (pv->pan == PAN_RIGHT){
				dessiner_char('R',28,3+cursor_y,0);
			} else if (pv->pan == PAN_CENTER){
				dessiner_char(' ',28,3+cursor_y,0);
				}
		} else {
			put_string("   ",26,3+cursor_y,0);
			put_string("   ",29,(3+cursor_y)-1,0);
			put_string("   ",29,(3+cursor_y)+1,0);
			put_string("   ",29,3+cursor_y,0);
		}
	}
}

void move_cursor(int mod_x, int mod_y){
	int new_x, new_y;
	
	if (current_screen == SCREEN_PATTERN) {
		// if (cursorPan == false) {

			new_x = cursor_x + mod_x;
			new_y = cursor_y + mod_y;			
			pattern_row *pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			
			xmod = 5+(cursor_x/4);
			
			put_string("   ",26,3+cursor_y,0);
			
			if (pv->optionsUp > 0) {
				if (pv->optionsUp == 1) {
					if (pv->volume == 0) {
						dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);
					} else {
						printf(vert);
						dessiner_char('o', cursor_x+xmod,cursor_y+3, 0);
						printf(blanc);
					}
				} else if (pv->optionsUp == 2) {
					if (pv->volume == 0) {
						dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);
					} else {
						printf(vert);
						dessiner_char('O', cursor_x+xmod,cursor_y+3, 0);
						printf(blanc);	
					}		
				} else if (pv->optionsUp == 3) {
					if (pv->random == false) {
						dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);					
					} else {
						printf(vert);
						dessiner_char('R', cursor_x+xmod,cursor_y+3, 0);					
						printf(blanc);			
					}
				}
				else if (pv->optionsUp > 3) {
					//char str[12];
					if (pv->playOrNot == 0) {
						dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);
					} else {
						printf(vert);
						dessiner_int(pv->playOrNot, cursor_x+xmod,cursor_y+3, 0);
						printf(blanc);			
					}
				}
			}
			// random perso
			// if (pv->random == true) {
			// 	dessiner_char('R', cursor_x+xmod,cursor_y+3, 0);
			// } 
			// else if (pv->volume == 0){
			//  	dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);
			// } 
			else {
				if (polyRhythm == true && cursor_y == 6) {
					dessiner_char(' ', cursor_x+xmod,cursor_y+3, 0);
				} else {
					if (cursor_x != 16) {
						dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);
					}
				}
			}

			// if (pv->volume == 1){
			//  	dessiner_char('o', cursor_x+xmod,cursor_y+3, 0);		
			// } else if (pv->volume == 2) {
			//  	dessiner_char('O', cursor_x+xmod,cursor_y+3, 0);		
			// } else {
			//  	dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);		
			// }
			
			if (polyRhythm == true && cursor_y > 6) {
				if (new_x >= MAX_COLUMNS) { 
					cursor_x = 0;
				} else if (new_x < 0) {
					cursor_x=MAX_COLUMNS-1;
				} else cursor_x=new_x;
			} else {
				if (new_x >= less_columns) { 
					cursor_x = 0;
				} else if (new_x < 0) {
					cursor_x=less_columns-1;
				} else cursor_x=new_x;
			}
			
			if (new_y >= MAX_ROWS) {
				cursor_y = 0;
			} else if (new_y < 0) {
				cursor_y=MAX_ROWS-1;
			} else cursor_y=new_y;					
		
			draw_cell_status();
			draw_cursor();
		// }

		
	} else if (current_screen == SCREEN_SONG) {
		new_x = cursor_x + mod_x;
		new_y = cursor_y + mod_y;
		
		if ((new_x < 5) && (new_x >= 0)){
			if ((new_y < 10) && (new_y >= 0)){
				dessiner_char(':',2+(cursor_x)+(cursor_x*5),cursor_y+3,0);
				cursor_x = new_x;
				cursor_y = new_y;
				draw_cursor();
			}
		}
	} else if (current_screen == SCREEN_SAMPLES) {
		new_y = cursor_y + mod_y;
		new_x = cursor_x + mod_x;
		
		if ((new_x < 4) && (new_x >= 0)){
			if ((new_y < MAX_ROWS) && (new_y >= 0)){
				dessiner_char(' ',(cursor_x*6)+4,cursor_y+3,0);
				cursor_x = new_x;
				cursor_y = new_y;
				draw_cursor();
			}
		}
	} else if (current_screen == SCREEN_SETTINGS) {
		new_y = cursor_y + mod_y;
		
		if ((new_y < 13) && (new_y >= 0)){
				dessiner_char(' ',0,cursor_y+3,0);
				if (new_y == 3) new_y+=mod_y;
				if (new_y == 7) new_y+=mod_y;
				if (new_y == 11) new_y+=mod_y;
				cursor_y = new_y;
				draw_cursor();
			}
	} else if (current_screen == SCREEN_HELP) {
		if (mod_x != 0){
			new_x = cursor_x + mod_x;
			
			
			if ((new_x >= 0) && (new_x < 4)){
				cursor_y = 0;
				cursor_x = new_x;
				draw_help();
			}
		}
	} 
}

void draw_column_cursor(){
	if (draw_flag == 0){
		draw_flag = 1;
		xmod = 5+(current_column/4);
		dessiner_char(' ',last_column+last_xmod,2,0);
		dessiner_char('v',current_column+xmod,2,0);
		last_xmod = xmod;
		last_column = current_column;

		if (polyRhythm == true) {
			xmodSecondSeq = 5+(current_secondSeq_column/4);
			dessiner_char(' ',last_secondSeq_column+last_xmod_secondSeq,9,0);
			dessiner_char('v',current_secondSeq_column+xmodSecondSeq,9,0);
			last_xmod_secondSeq = xmodSecondSeq;
			last_secondSeq_column = current_secondSeq_column;
		}
		
		draw_flag = 0;
	}
}

void clear_last_song_cursor(){
	if (draw_flag == 0){
		if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
			dessiner_char(' ',5+(((order_index-order_page-1)/10)*6),((order_index-order_page-1)%10)+3,0);
			draw_cursor();		
		}
	}
}

void clear_last_song_queued_cursor(){
	if (draw_flag == 0){
		if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
			dessiner_char(' ',5+(((play_next)/10)*6),((play_next)%10)+3,0);
			draw_cursor();		
		}
	}
}

void draw_song_queued_cursor(){
	if (draw_flag == 0){
		if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
			dessiner_char('-',5+(((play_next)/10)*6),((play_next)%10)+3,0);
			draw_cursor();		
		}
	}
}

void draw_song_cursor(){
	if (draw_flag == 0){
		if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
			dessiner_char(' ',5+(((order_index-order_page-1)/10)*6),((order_index-order_page-1)%10)+3,0);
			draw_cursor();		
			dessiner_char('<',5+(((order_index-order_page)/10)*6),((order_index-order_page)%10)+3,0);
		}
	}
}

void clear_song_cursor(){
	if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
		dessiner_char(' ',5+(((order_index-order_page)/10)*6),((order_index-order_page)%10)+3,0);
	}
}

void draw_status(){
		// put_string(status_text[status],0,19,0);
}

void draw_temp_status(int temp_status){
		put_string(status_text[temp_status],0,19,0);
}

void set_status(int new_status){
	status = new_status;
	draw_status();
}
	
void draw_pattern(void) __attribute__ ((section(".iwram")));

void draw_bpm_pattern() {
	printf(violet);
	sprintf(string_buff,"BPM:", (int) current_song->bpm);
	put_string(string_buff,0,16,0);
	printf(blanc);
	sprintf(string_buff,"%.2d ", (int) current_song->bpm);
	put_string(string_buff,5,16,0);
	sprintf(string_buff,"X/Y");
	put_string(string_buff,9,16,0);
	if (superLiveMod== true) {
		printf(jaune);
		sprintf(string_buff,"Glob pitch:%x   ", (int) globalPitch);
		put_string(string_buff,17,0,0);
	}
	printf(violet);
	sprintf(string_buff,"Delete note:", (int) current_song->bpm);
	put_string(string_buff,0,18,0);
	printf(blanc);
	sprintf(string_buff,"B", (int) current_song->bpm);
	put_string(string_buff,13,18,0);
	printf(violet);
	sprintf(string_buff,"Note options:", (int) current_song->bpm);
	put_string(string_buff,0,20,0);
	printf(blanc);
	sprintf(string_buff,"A+pad", (int) current_song->bpm);
	put_string(string_buff,14,20,0);
	printf(violet);
	sprintf(string_buff,"Less steps:", (int) current_song->bpm);
	put_string(string_buff,0,22,0);
	printf(blanc);
	sprintf(string_buff,"B+Le/Ri", (int) current_song->bpm);
	put_string(string_buff,12,22,0);
}

void draw_pattern(){ 


	if (draw_flag == 0){
	draw_flag = 1;
	
	int r = 0;
	int c = 0;


	clear_screen();		
	consoleSelect(&topScreen);

	printf(cyan); //cyan
	iprintf("\x1b[0;0HPATTERN %02d ", current_pattern_index);
	printf(blanc); //blanc

	// sprintf(string_buff,"IA limit:%d ", (int) IALimit);
	// put_string(string_buff,20,0,0);	
	// sprintf(string_buff,"B + ->");
	// put_string(string_buff,26,1,0);	
	// sprintf(string_buff,"<-");
	// put_string(string_buff,30,2,0);	

	draw_bpm_pattern();
	
	
	//make array, fill array with chars, make one printf
	char rowbuff[(MAX_ROWS+4)]; 
	int i;

	for (r = 0; r < MAX_ROWS; r++){
		i = 0;			
		for (c = 0; c < less_columns; c++){
			if ( c == 4 || c == 8 || c == 12) { //print extra space for 1/4 divide
				rowbuff[i] =' ';
				i++;
			}
			if (current_song->patterns[current_pattern_index].columns[c].rows[r].playOrNot > 0) {
				int currentPlayOrNot = current_song->patterns[current_pattern_index].columns[c].rows[r].playOrNot;
				if (currentPlayOrNot > 0) {
					char cur = currentPlayOrNot+'0'; //transforme le int en string
					rowbuff[i] = cur;
				}
			} 
			else if (current_song->patterns[current_pattern_index].columns[c].rows[r].random == true) {
				rowbuff[i] ='R'; //random
			} else if (current_song->patterns[current_pattern_index].columns[c].rows[r].volume == 0){
				rowbuff[i] ='-';
			} else if (current_song->patterns[current_pattern_index].columns[c].rows[r].volume == 1) {
				rowbuff[i] ='o';
			} else {
				rowbuff[i] ='O';
			}
			i++; 
		}
		rowbuff[i] ='\0';
		// sprintf(string_buff,"%02d [ %s ]   %c", r, rowbuff,current_song->channels[r].status);
		consoleSelect(&topScreen);
		iprintf("\x1b[%02d;0H%02d [ %s ]   %c", r+3, r, rowbuff,current_song->channels[r].status);
		//initialiser le pan panoramique
		printf(jaune);
		if (current_song->channels[current_pattern_index].pan == 128) {
			iprintf("\x1b[%d;4HC", r+3);					
		}
		else if (current_song->channels[current_pattern_index].pan == 0) {
			iprintf("\x1b[%d;3HL", r+3);					
		}
		else if (current_song->channels[current_pattern_index].pan == 255) {
			iprintf("\x1b[%d;3HR", r+3);		
		}
		printf(blanc);
		//put_string(string_buff,0,r+3,0);
	}

	//2ème sequencer
	// for (r = 0; r < MAX_ROWS; r++){
	// 	i = 0;			
	// 	for (c = 0; c < MAX_COLUMNS; c++){
	// 		if ( c == 4 || c == 8 || c == 12) { //print extra space for 1/4 divide
	// 			rowbuff[i] =' ';
	// 			i++;
	// 		}
	// 		if (current_song->patterns[current_pattern_index].columnsSecondSeq[c].rows[r].playOrNot > 0) {
	// 			int currentPlayOrNot = current_song->patterns[current_pattern_index].columnsSecondSeq[c].rows[r].playOrNot;
	// 			if (currentPlayOrNot > 0) {
	// 				char cur = currentPlayOrNot+'0'; //transforme le int en string
	// 				rowbuff[i] = cur;
	// 			}
	// 		}
	// 		else if (current_song->patterns[current_pattern_index].columnsSecondSeq[c].rows[r].random == true) {
	// 			rowbuff[i] ='R';
	// 		} else if (current_song->patterns[current_pattern_index].columnsSecondSeq[c].rows[r].volume == 0){
	// 			rowbuff[i] ='-';
	// 		} else if (current_song->patterns[current_pattern_index].columnsSecondSeq[c].rows[r].volume == 1) {
	// 			rowbuff[i] ='o';
	// 		} else {
	// 			rowbuff[i] ='O';
	// 		}
	// 		i++; 
	// 	}
	// 	rowbuff[i] ='\0';
	// 	// sprintf(string_buff,"%02d [ %s ]   %c", r, rowbuff,current_song->channels[r].status);
	// 	consoleSelect(&topScreen);
	// 	iprintf("\x1b[%02d;0H%02d [ %s ]   %c", r+9, r, rowbuff,current_song->channels[r].status);
	// 	//put_string(string_buff,0,r+3,0);
	// }
		
	draw_status();
	draw_cell_status();
	draw_flag = 0;
	
	draw_cursor();
	
	}
}

void draw_song(){
	draw_flag = 1;
	
	clear_screen();			
	put_string("SONG",0,0,0);
	
	
	int column,row;
	int order_index,order;
	
	for (row = 0; row < 10; row++){
		for (column = 0; column < 5; column++){
			order_index=(column*10)+row+order_page;
			order = current_song->order[order_index];
			if (order == NO_ORDER){
				sprintf(string_buff, "%02d:-- ", order_index);
				put_string(string_buff,column*6,row+3,0);
			}else{	
				sprintf(string_buff, "%02d:%02d ", order_index,order);
				put_string(string_buff,column*6,row+3,0);
			}
		}
	}	
	draw_cursor();
	
	if (play_next != NO_ORDER){
		draw_song_queued_cursor();
	}
	
	draw_status();
	draw_flag = 0;
}

void draw_samples(){
	clear_screen();			
	put_string("SAMPLES",0,0,0);
	put_string("   [SMPL][VOL ][TUNE][ PAN ]",0,2,0);
	
	/*
	u8 sample;
	u8 volume;
	u8 pitch;
	u8 pan;
	u8 status; 
	*/
	
	int i;
	
	
	for (i = 0; i < MAX_ROWS; i++){	
		sprintf(string_buff, "%02d [ %02d ][ %02x ][ %02d ][ %s ]", 
		i, 
		current_song->channels[i].sample, 
		current_song->channels[i].volume, 
		current_song->channels[i].pitch, 
		pan_string[(current_song->channels[i].pan)/127]);		
		put_string(string_buff,0,i+3,0);
		//if (i == cursor_y) put_string(string_buff,0,i+3,2);
		//else put_string(string_buff,0,i+3,0);
	}
	
	draw_cursor();
	draw_status();
}

void draw_settings(){
	int row_index = 3;	

	clear_screen();			
	put_string("SETTINGS",0,0,0);
	
	sprintf(string_buff," SONG NAME: %s", current_song->song_name);
	put_string(string_buff,0,row_index,0);
	row_index++;
	
	sprintf(string_buff," BPM: %.2d", (int) current_song->bpm);
	put_string(string_buff,0,row_index,0);
	row_index++;
	
	sprintf(string_buff," SHUFFLE: %d%% - %d/%d", set_shuffle(), ticks, tocks);
	put_string(string_buff,0,row_index,0);
	row_index++;
	row_index++;

	sprintf(string_buff, " LOOP MODE: %s", loop_mode_text[current_song->loop_mode]);
	put_string(string_buff,0,row_index,0);
	row_index++;
	
	sprintf(string_buff, " SYNC MODE: %s", sync_mode_text[current_song->sync_mode]);
	put_string(string_buff,0,row_index,0);
	row_index++;
	
	sprintf(string_buff, " COLOR MODE %d", current_song->color_mode);
	put_string(string_buff,0,row_index,0);
	row_index++;
	row_index++;
	
	put_string(" SAVE SONG\n LOAD SONG\n CLEAR SONG\n \n HELP",0,row_index,0);
	
	draw_cursor();
	draw_status();
}

void draw_help(){
	clear_screen();			
	put_string("HELP",0,0,0);
	put_string((char *) help_text[cursor_x],0,2,0);
}

void draw_screen(){
	cursor_x = 0;
	cursor_y = 0;
	
	switch (current_screen) {
	case SCREEN_PATTERN:
		draw_pattern();
		break;
	case SCREEN_SONG:
		draw_song();
		break;
	case SCREEN_SAMPLES:
		draw_samples();
		break;
	case SCREEN_SETTINGS:
		draw_settings();
		break;
	}
} 

int find_chain_start(index){
	int f;
	for (f = index -1; f > 0; f--){
		if (current_song->order[f-1] == NO_ORDER)
			return f;
	}  
	return 0;
}

void stop_song(){
	play = PLAY_STOPPED;
	if (current_screen == SCREEN_SONG){
		clear_song_cursor();
		if (play_next != NO_ORDER){
			clear_last_song_queued_cursor();
		}
	} else if (current_screen == SCREEN_PATTERN){
		dessiner_char(' ',last_column+last_xmod,2,0);
	}		
	order_index = 0;
	current_column = 0;
	current_secondSeq_column = 0;
	play_next=NO_ORDER;
			
	switch (current_song->sync_mode) {
		case SYNC_NONE:
			REG_TM2C &= !TM_ENABLE;
			set_status(STATUS_STOPPED);
			break;
		case SYNC_SLAVE:
			disable_serial_interrupt();	
			set_status(STATUS_SYNC_STOPPED);
			break;
		case SYNC_TRIG_SLAVE:
			disable_trig_interrupt();	
			set_status(STATUS_SYNC_STOPPED);
			break;
			
		}
		
}

void play_pattern(){
	current_column=0;
	current_secondSeq_column =0;
	playing_pattern_index=current_pattern_index;
	play = PLAY_PATTERN;
	
	switch (current_song->sync_mode) {
		case SYNC_NONE:		
			set_status(STATUS_PLAYING_PATTERN);
			REG_TM2C ^= TM_ENABLE;
			break;
		// case SYNC_SLAVE:
		// 	set_status(STATUS_SYNC_PATTERN);
		// 	enable_serial_interrupt();
		// 	break;
		// case SYNC_MASTER:
		// 	//pass
		// 	break;
		// case SYNC_TRIG_SLAVE:
		// 	set_status(STATUS_SYNC_PATTERN);
		// 	enable_trig_interrupt();
		// 	break;
		// case SYNC_TRIG_MASTER:
		// 	//pass
		// 	break;
	}
}

void play_song(){
	current_column=0;
	current_secondSeq_column = 0;
	order_index = order_page+(cursor_x*10)+cursor_y;
	if (current_song->order[order_index]!=NO_ORDER){
		playing_pattern_index = current_song->order[order_page+(cursor_x*10)+cursor_y];
		if (current_song->loop_mode == SONG_MODE) {
			play = PLAY_SONG;
			
		} else if (current_song->loop_mode == LIVE_MODE) {
			play = PLAY_LIVE;		
		}
		
		switch (current_song->sync_mode) {
			case SYNC_NONE:
			case SYNC_MASTER:
			case SYNC_TRIG_MASTER:
				set_status(STATUS_PLAYING_SONG);
				REG_TM2C ^= TM_ENABLE;
				break;
			case SYNC_SLAVE:
				play_next = playing_pattern_index;
				draw_song_queued_cursor();
				set_status(STATUS_SYNC_SONG);
				enable_serial_interrupt();
				break;
			case SYNC_TRIG_SLAVE:
				play_next = playing_pattern_index;
				draw_song_queued_cursor();
				set_status(STATUS_SYNC_SONG);
				enable_trig_interrupt();
				break;
		}
		
	}
}

void next_column(){
	if (play == PLAY_PATTERN){  //pattern play mode
		if (current_column == less_columns){
			playing_pattern_index=current_pattern_index;
			current_column = 0;
		}
		if (current_secondSeq_column == MAX_COLUMNS) {
			playing_pattern_index=current_pattern_index;
			current_secondSeq_column = 0;
		}
		play_column();
	
		if (current_screen == SCREEN_PATTERN){
			if (playing_pattern_index==current_pattern_index){ 
				if (draw_flag == 0){
					draw_column_cursor();
				}
			}
		}
	} else { //song or live play mode
	
		if (current_column == less_columns){
			// if ((play == PLAY_LIVE) && (play_next!=NO_PATTERN)){
			// 	if (current_screen == SCREEN_SONG){
			// 		clear_song_cursor();
			// 	}
			// 	order_index = play_next;
			// 	play_next = NO_PATTERN;
			// } else {
			// 	order_index++; // go to next order in array
			// }
			playing_pattern_index=current_song->order[order_index];
			current_column = 0;
			current_secondSeq_column = 0;
			if (current_screen == SCREEN_PATTERN){ //to remove the remaining cursor when playing patt changes
				
				dessiner_char(' ',last_column+last_xmod,2,0);
			}	
		}
		
		if (current_screen == SCREEN_PATTERN){
			if (playing_pattern_index==current_pattern_index) draw_column_cursor();
		}
	
		if (playing_pattern_index == NO_ORDER){ //end of 'chain'
			if (play == PLAY_SONG ){
				if (current_screen == SCREEN_SONG){
					clear_last_song_cursor();
					clear_song_cursor();
				}
				stop_song();
			} else if (play == PLAY_LIVE){
				if (current_screen == SCREEN_SONG){
					clear_last_song_cursor();
					clear_song_cursor();
				} else if (current_screen == SCREEN_PATTERN){
					if (playing_pattern_index==current_pattern_index)
					dessiner_char(' ',15,2,0); //clear pattern cursor
				}
				order_index = find_chain_start(order_index);
				playing_pattern_index=current_song->order[order_index];
				current_column = 0;	
				current_secondSeq_column = 0;
				if (current_screen == SCREEN_SONG){
					clear_last_song_cursor();
					draw_song_cursor();
				}
				play_column();
			}
		} else {
			if (current_screen == SCREEN_SONG){
				draw_song_cursor();
			}
			play_column();
		}		
	}

	current_column++;
	current_secondSeq_column++;
}

int is_empty_pattern(int pattern_index){
	int c,r;
	
	for (r=0; r<MAX_ROWS; r++){
		for (c=0; c<less_columns; c++){
			if (current_song->patterns[pattern_index].columns[c].rows[r].volume != 0){
				return 0;
			}
		}
	}
	
	return 1;
}

void set_patterns_data(){
	/*Find how many patterns contain data, to discard empty patterns
	 * when saving data. Start at zero, count back until data, 
	 */
	int i;
	
	for (i = MAX_PATTERNS-1; i >= 0; i--){
		if (!is_empty_pattern(i)){
			current_song->pattern_length = i+1;
			//sprintf(string_buff, "%d", current_song->pattern_length);
			//put_string(string_buff,0,0,0);
			break;
		}
	
	}  
}

void set_orders_data(){
	int i;
	
	for (i = MAX_ORDERS; i >= 0; i--){
		if (current_song->order[i]!=NO_ORDER){
			break;
		}
	current_song->order_length = i;
	}
}

void file_option_save(void) {
	u16 *from;
	u8 *to;
	u32 i;
	
	set_patterns_data();
	set_orders_data();	
	
	/*double size = sizeof(song) - (sizeof(pattern) * MAX_PATTERNS) - (sizeof(u8)*(MAX_ORDERS+1));
	
	sprintf(string_buff, "len2 %d , %d, %d", (int) size, current_song->pattern_length, current_song->order_length);
			put_string(string_buff,0,1,0);
	 */
	from = (u16 *) current_song;
	to = (u8 *) sram;
	//Song vars
	
	for (i = 0; i < 195; i++) {
		*to = (u8) (*from & 0x00FF);
		to++;
		*to = (u8) (*from >> 8);
		to++;
		from++;
		
		//memcmp(i, &current_song->patterns)
		if (from == (u16 *) current_song->patterns){	
			//sprintf(string_buff, "len2 %d %d", (int)i, current_song->order_length);
			//put_string(string_buff,0,1,0);
			dessiner_char('b',0,19,0);
			break;
		}
	
	}
	//Pattern data
	from = (u16 *) current_song->patterns;
	int plen = current_song->pattern_length * sizeof(pattern);
	for (i=0; i < plen; i++) {
		*to = (u8) (*from & 0x00FF);
		to++;
		*to = (u8) (*from >> 8);
		to++;
		from++;
	}
	//Order data
	from = (u16 *) current_song->order;
	for (i=0; i < current_song->order_length; i++) {
		*to = (u8) (*from & 0x00FF);
		to++;
		*to = (u8) (*from >> 8);
		to++;
		from++;
	}

	//int saved = (int) (from - ((u16 *) current_song));
	//iprintf("\x1b[19;0Hsaved %d", saved);	//debug
}

void file_option_load(void) {
	u32 i;
	u16 *to;
	u8 *from;
	
	from = (u8 *) sram;
	to = (u16 *) current_song;

	//Song vars
	for (i=0; i < 195; i++) {
		u16 acc = *from;
		from++;
		acc |= ((u16) (*from)) << 8;
		*to = acc;
		to++;
		from++;
		if (to == (u16 *) current_song->patterns){
			//sprintf(string_buff, "len %d %d", (int)i, (int)from);
			//put_string(string_buff,0,1,0);
			break;
		}
	}
	
	//iprintf("\x1b[17;0Hplen %d", current_song->pattern_length);	//debug
	//iprintf("\x1b[18;0Holen %d", current_song->order_length);	//debug
	
	//Pattern data
	to = (u16 *) current_song->patterns;
	int pmax = sizeof(current_song->patterns);
	int plen = current_song->pattern_length * sizeof(pattern); 
	for (i=0; i < pmax; i++) {
		if (i < plen){
			u16 acc = *from;
			from++;
			acc |= ((u16) (*from)) << 8;
			*to = acc;
			to++;
			from++;
		} else {
			*to = 0;
			to++;
		}
	}
	//Order data
	to = (u16 *) current_song->order;
	for (i=0; i < MAX_ORDERS+1; i++) {
		if (i < current_song->order_length){
			u16 acc = *from;
			from++;
			acc |= ((u16) (*from)) << 8;
			*to = acc;
			to++;
			from++;
		} else {
			*to = NO_ORDER;
			to++;
	}
	}
	
	set_bpm_to_millis();
	set_shuffle();
	update_timers();
	draw_screen();
	
	/*
	int loaded = (int) (to - ((u16 *) current_song));
	iprintf("\x1b[19;0Hloaded %d / %d", size,loaded);	//debug
	*/
}

void samples_menu(int channel_index, int channel_item, int action){
	int preview_sound = 0;
	
	if (action == MENU_ACTION){
		preview_sound = 1;	
	} else if (action == MENU_LEFT){
		switch (channel_item) {
			case CHANNEL_SAMPLE:
				if (current_song->channels[channel_index].sample > 0){
					current_song->channels[channel_index].sample--;
					draw_samples();					
				}
				preview_sound = 1;
				break;
			case CHANNEL_VOLUME:
				if (current_song->channels[channel_index].volume > 0){
					current_song->channels[channel_index].volume--;
					draw_samples();					
				}
				preview_sound = 1;
				break;
			case CHANNEL_PITCH:
				if (current_song->channels[channel_index].pitch > 0){
					current_song->channels[channel_index].pitch--;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_PAN:
				if (current_song->channels[channel_index].pan == PAN_CENTER){
					current_song->channels[channel_index].pan = PAN_LEFT;
					draw_samples();
				} else if (current_song->channels[channel_index].pan == PAN_RIGHT){
					current_song->channels[channel_index].pan = PAN_CENTER;
					draw_samples();
				}
				break;
		}
	} else if (action == MENU_RIGHT){
		switch (channel_item) {
			case CHANNEL_SAMPLE:
				//inc sample by 1}
				if (current_song->channels[channel_index].sample < MAX_SAMPLES){
					current_song->channels[channel_index].sample++;
					draw_samples();
					
				}
				preview_sound = 1;
				break;
			case CHANNEL_VOLUME:
				if (current_song->channels[channel_index].volume < 200){
					current_song->channels[channel_index].volume++;
					draw_samples();					
				}
				preview_sound = 1;
				break;
			case CHANNEL_PITCH:
				if (current_song->channels[channel_index].pitch < 24){
					current_song->channels[channel_index].pitch++;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_PAN:
				if (current_song->channels[channel_index].pan == PAN_CENTER){
					current_song->channels[channel_index].pan = PAN_RIGHT;
					draw_samples();
				} else if (current_song->channels[channel_index].pan == PAN_LEFT) {
					current_song->channels[channel_index].pan = PAN_CENTER;
					draw_samples();
				}
				break;
		}
	} else if (action == MENU_UP){
		int item_mod;
		switch (channel_item) {
			case CHANNEL_SAMPLE:
				//inc sample by 10}
				item_mod = current_song->channels[channel_index].sample + 10;
				if (item_mod <= MAX_SAMPLES){
					current_song->channels[channel_index].sample = item_mod;
					draw_samples();
				} else {
					current_song->channels[channel_index].sample = MAX_SAMPLES;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_VOLUME:
				item_mod = current_song->channels[channel_index].volume + 10;
				if (item_mod <= 200){
					current_song->channels[channel_index].volume = item_mod;
					draw_samples();
				} else {
					current_song->channels[channel_index].volume = 200;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_PITCH:
				if (current_song->channels[channel_index].pitch < 14){
					current_song->channels[channel_index].pitch+=10;					
				} else {
					current_song->channels[channel_index].pitch=24;					
				}
				draw_samples();
				preview_sound = 1;
				break;
			case CHANNEL_PAN:
				//do stuff
				break;
		}
	} else if (action == MENU_DOWN){
		int item_mod;
		switch (channel_item) {
			case CHANNEL_SAMPLE:
				//dec sample by 10
				item_mod = current_song->channels[channel_index].sample - 10;
				if (item_mod > 0){
					current_song->channels[channel_index].sample = item_mod;
					draw_samples();
				} else {
					current_song->channels[channel_index].sample = 0;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_VOLUME:
				item_mod = current_song->channels[channel_index].volume - 10;
				if (item_mod > 0){
					current_song->channels[channel_index].volume = item_mod;
					draw_samples();
				} else {
					current_song->channels[channel_index].volume = 0;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_PITCH:
				if (current_song->channels[channel_index].pitch > 10){
					current_song->channels[channel_index].pitch-=10;
					
				} else {
					current_song->channels[channel_index].pitch=0;
				}
				draw_samples();
				preview_sound = 1;

				break;
			case CHANNEL_PAN:
				//do stuff
				break;
		}
	}
	if (preview_sound == 1){
		play_sound(channel_index, 
		current_song->channels[channel_index].sample, 
		current_song->channels[channel_index].volume, 
		pitch_table[current_song->channels[channel_index].pitch],
		current_song->channels[channel_index].pan );
	}
		
}

int name_index;

void settings_menu(int menu_option, int action){
	if (menu_option == 0) {
		//song name
		if (action == MENU_ACTION){
			if (strcmp(current_song->song_name,"SONGNAME")==0){
				memcpy(current_song->song_name,"        ",8);
				draw_settings();
			}
			dessiner_char(current_song->song_name[name_index],12+name_index, 3, 1);
		} else {
		if (action == MENU_LEFT){
			if (name_index > 0){
				dessiner_char(current_song->song_name[name_index],12+name_index, 3, 0);
				name_index--;
				dessiner_char(current_song->song_name[name_index],12+name_index, 3, 1);
			}
		} else if (action == MENU_RIGHT){
			if (name_index < 7){
				dessiner_char(current_song->song_name[name_index],12+name_index, 3, 0);
				name_index++;
				dessiner_char(current_song->song_name[name_index],12+name_index, 3, 1);				
			}
		} else if (action == MENU_UP){
			if ((current_song->song_name[name_index] < 'Z') && (current_song->song_name[name_index] >= 'A')){
				current_song->song_name[name_index]++;
			} else if (current_song->song_name[name_index] < 'A'){
				current_song->song_name[name_index]='A';
			}
				//draw_settings();
				dessiner_char(current_song->song_name[name_index],12+name_index, 3, 1);
			
		} else if (action == MENU_DOWN){
			if ((current_song->song_name[name_index] <= 'Z') && (current_song->song_name[name_index] > 'A')){
				current_song->song_name[name_index]--;
			} else if (current_song->song_name[name_index] < 'A'){
				current_song->song_name[name_index]='A';
			}
				//draw_settings();
				dessiner_char(current_song->song_name[name_index],12+name_index, 3, 1);
			
		}
		} 
	} else if (menu_option == 1) { 	
		//bpm

		if (action == MENU_LEFT){
			if (current_song->bpm > MIN_BPM){
				current_song->bpm--;
			}
		} else if (action == MENU_RIGHT){
			if (current_song->bpm < MAX_BPM){
				current_song->bpm++;
			}
		} else if (action == MENU_UP){
			current_song->bpm+=10;
			if (current_song->bpm > MAX_BPM){
				current_song->bpm=MAX_BPM;
			}
		} else if (action == MENU_DOWN){
			current_song->bpm-=10;
			if (current_song->bpm < MIN_BPM){
				current_song->bpm=MIN_BPM;
			}
		}
		set_bpm_to_millis();
		update_timers();
		draw_settings();
	} else if (menu_option == 2) {
		//shuffle
		if (action == MENU_LEFT){
			if (current_song->shuffle > 1){
				current_song->shuffle--;
				set_shuffle();
				draw_settings();
			}
		} else if (action == MENU_RIGHT){
			if (current_song->shuffle < 11){
				current_song->shuffle++;
				set_shuffle();
				draw_settings();
			}
		}
	} else if (menu_option == 4) {
		//loop mode
		if (action == MENU_LEFT){
			if (current_song->loop_mode == LIVE_MODE){
				current_song->loop_mode = SONG_MODE;
				draw_settings();
			}
		} else if (action == MENU_RIGHT){
			if (current_song->loop_mode == SONG_MODE){
				current_song->loop_mode = LIVE_MODE;
				draw_settings();
			}
		}
	} else if (menu_option == 5) {
		//sync mode
		if (action == MENU_LEFT){			
			if (current_song->sync_mode > 0){
				stop_song();
				current_song->sync_mode--;
				stop_song();
				draw_settings();
			}
		} else if (action == MENU_RIGHT){
			if (current_song->sync_mode < 5){
				stop_song();
				current_song->sync_mode++;				
				stop_song();
				draw_settings();
			}
		}
	} else if (menu_option == 6) {
		//color mode
		if (action == MENU_LEFT){			
			if (current_song->color_mode > 0){
				current_song->color_mode--;
				define_palettes(current_song->color_mode);	
				draw_settings();
			}
		} else if (action == MENU_RIGHT){
			if (current_song->color_mode < 2){
				current_song->color_mode++;
				define_palettes(current_song->color_mode);
				draw_settings();
			}
		}
	} else if (menu_option == 8) {
		if (action == MENU_ACTION){
			stop_song();
			file_option_save();
			draw_settings();
			draw_temp_status(STATUS_SAVE);
		}
	} else if (menu_option == 9) {
		if (action == MENU_ACTION){
			stop_song();
			file_option_load();
			draw_settings();
			draw_temp_status(STATUS_LOAD);
		}
	} else if (menu_option == 10) {
		if (action == MENU_ACTION){
			init_song();
			draw_settings();
			draw_temp_status(STATUS_CLEAR);
		}
	} else if (menu_option == 12) {
		if (action == MENU_ACTION){
			current_screen = SCREEN_HELP;
			draw_help();
		}
	}
}

void process_input(){

	int keys_pressed, keys_held, keys_released, index;

	
			
	//swiWaitForVBlank();
	scanKeys();

	keys_pressed = keysDown();
	keys_released = keysUp();
	keys_held = keysHeld();

	// mmLoadEffect(SFX_SYNC);
	// if ( keys_pressed & KEY_X ) {
	// 	mmEffectEx(&syncSound);
	// }

	//on peut pas toucher l'écran quand la lecture
	//est active sinon ça glitche tout
	if(keys_held & KEY_TOUCH) {
		if (play != PLAY_PATTERN) {
			touchRead(&touch);
			bottomPrinted = false;
		}
	}

	if (bottomPrinted == false) {
		consoleSelect(&bottomScreen);
		iprintf("\x1b[23;23H(%d,%d)", touch.px, touch.py);
		printf(rouge);
		iprintf("\x1b[0;3H-- Works if not playing --");
		printf(blanc);

		iprintf("\x1b[2;0HSync :         (track 11)" );
		if (syncDSMode == true) {
			printf(vert);
			iprintf("\x1b[2;7H| On  |" );
			printf(blanc);
			iprintf("\x1b[4;0HType sync : only Volca for now");
			// if (typeOfSyncs == 0) {
			// 	iprintf("\x1b[4;12H| - || + | Volca");
			// } else if (typeOfSyncs == 1) {
			// 	iprintf("\x1b[4;12H| - || + | Lsdj ");
			// }
			// else if (typeOfSyncs == 2) {
			// 	iprintf("\x1b[4;12H| - || + | Wolf ");
			// }
		} else {
			iprintf("\x1b[4;0H                                        ");
			// iprintf("\x1b[2;0HSync : | Off | (track 11)" );
			printf(vert);
			iprintf("\x1b[2;7H| Off |" );
			printf(blanc);
		}
		
		iprintf("\x1b[6;0HIA Random :" );
		printf(vert);
		if (IARandom == true) {
			iprintf("\x1b[6;12H| On  |" );
		} else {
			iprintf("\x1b[6;12H| Off |" );
		}
		printf(blanc);
		iprintf("\x1b[8;0HIA Crazy bpm :" );
		printf(vert);
		if (bpmFoliesActivated == true) {
			iprintf("\x1b[8;15H| On  |" );
		} else {
			iprintf("\x1b[8;15H| Off |" );
		}
		printf(blanc);
		iprintf("\x1b[11;0HSuper live :" );
		if (superLiveMod == true) {
			printf(vert);
			iprintf("\x1b[11;13H| On  |" );
			printf(blanc);
			iprintf("\x1b[13;0HR/L for global pitch  ");
			printf(vert);
			iprintf("\x1b[13;21H| Reset |");
			printf(blanc);
			iprintf("\x1b[14;0H                              ");
		} else {
			iprintf("\x1b[11;0HSuper live :" );
			printf(vert);
			iprintf("\x1b[11;13H| Off |" );
			printf(blanc);
			iprintf("\x1b[13;0HR/L for select pattern        ");
			iprintf("\x1b[14;0HSelect+R/L for menu           ");
		}

		iprintf("\x1b[17;0HIA limit:");
		printf(vert);
		iprintf("\x1b[17;10H| + || - |");
		printf(blanc);
		iprintf("\x1b[17;21H%d", IALimit);
		iprintf("\x1b[19;0HLoop patterns" );
		printf(vert);
		if (autoLoopPattern == true) {
			iprintf("\x1b[19;16H| On  |" );
		} else {
			iprintf("\x1b[19;16H| Off |" );
		}
		printf(blanc);

		iprintf("\x1b[21;0HIA Auto Cymbal" );
		printf(vert);
		if (IAAutoCymbal == true) {
			iprintf("\x1b[21;17H| On  |" );
		} else {
			iprintf("\x1b[21;17H| Off |" );
		}
		printf(blanc);
		iprintf("\x1b[23;0HPolyrhythm:" );
		printf(vert);
		if (polyRhythm == true) {
			iprintf("\x1b[23;12H| On  |" );
		} else {
			iprintf("\x1b[23;12H| Off |" );
		}
		printf(blanc);
		consoleSelect(&topScreen);
		bottomPrinted = true;
	}

	if ((keys_held & KEY_TOUCH) && touch.px >= 61 && touch.px <= 109 && touch.py >= 15 && touch.py <= 25)
	{
		if (touchedBtn == false) {
			syncDSMode = !syncDSMode;
			touchedBtn = true;
		}
	}

	if ((keys_held & KEY_TOUCH) && touch.px >= 101 && touch.px <= 150 && touch.py >= 47 && touch.py <= 57)
	{
		if (touchedBtn == false) {
			IARandom = !IARandom;
			touchedBtn = true;
		}
	}

	if ((keys_held & KEY_TOUCH) && touch.px >= 125 && touch.px <= 173 && touch.py >= 64 && touch.py <= 74)
	{
		if (touchedBtn == false) {
			bpmFoliesActivated = !bpmFoliesActivated;
			touchedBtn = true;
		}
	}

	// + et - du type sync pour choisir le sync volca lsdj etc...
	if ((keys_held & KEY_TOUCH) && touch.px >= 100 && touch.px <= 133 && touch.py >= 31 && touch.py <= 41)
	{
		if (touchedBtn == false) {
			typeOfSyncs++;
			touchedBtn = true;
		}
	}
	if ((keys_held & KEY_TOUCH) && touch.px >= 141 && touch.px <= 173 && touch.py >= 31 && touch.py <= 41)
	{
		if (touchedBtn == false) {
			typeOfSyncs++;
			touchedBtn = true;
		}
	}

	//super live mod on/off
	if ((keys_held & KEY_TOUCH) && touch.px >= 109 && touch.px <= 157 && touch.py >= 88 && touch.py <= 96)
	{
		if (touchedBtn == false) {
			superLiveMod = !superLiveMod;
			touchedBtn = true;
		}
	}
	//reset global pitch
	if ((keys_held & KEY_TOUCH) && touch.px >= 173 && touch.px <= 238 && touch.py >= 103 && touch.py <= 114)
	{
		if (touchedBtn == false) {
			globalPitch = 0;
			touchedBtn = true;
		}
	}

	//IA limit ++
	if ((keys_held & KEY_TOUCH) && touch.px >= 85 && touch.px <= 117 && touch.py >= 136 && touch.py <= 146)
	{
		if (touchedBtn == false) {
			IALimit++;
			touchedBtn = true;
		}
	}
	//IA limit --
	if ((keys_held & KEY_TOUCH) && touch.px >= 126 && touch.px <= 157 && touch.py >= 136 && touch.py <= 146)
	{
		if (touchedBtn == false) {
			IALimit--;
			touchedBtn = true;
		}
	}

	//IA auto cymbal
	if ((keys_held & KEY_TOUCH) && touch.px >= 141 && touch.px <= 190 && touch.py >= 167 && touch.py <= 177)
	{
		if (touchedBtn == false) {
			IAAutoCymbal = !IAAutoCymbal;
			touchedBtn = true;
		}
	}
	//IA auto loop patern
	if ((keys_held & KEY_TOUCH) && touch.px >= 133 && touch.px <= 181 && touch.py >= 151 && touch.py <= 161)
	{
		if (touchedBtn == false) {
			autoLoopPattern = !autoLoopPattern;
			touchedBtn = true;
		}
	}
	//Polyrhythm
	if ((keys_held & KEY_TOUCH) && touch.px >= 101 && touch.px <= 149 && touch.py >= 183 && touch.py <= 192)
	{
		if (touchedBtn == false) {
			polyRhythm = !polyRhythm;
			if (polyRhythm == true) {
				iprintf("\x1b[9;0H                        ");
			} else {
				iprintf("\x1b[9;0H06 [ ---- ---- ---- ----");
			}
			touchedBtn = true;
		}
	}

	if (typeOfSyncs > 2) { //augmenter si je met plus de sons de sync
		typeOfSyncs = 0; 
	}
	if (keys_released & KEY_TOUCH) {
		touchedBtn = false;
	}

	//PATTERN SCREEN INPUTS input pattern
	if (current_screen == SCREEN_PATTERN){	
		pattern_row *pv;

		// if ( keys_pressed & KEY_B ) {			
		// 	if (keys_held & KEY_SELECT ) {
		// 		copy_pattern();
		// 		rand_pattern();
		// 		draw_pattern();
		// 	}
		// }

		// if ( keys_pressed & KEY_A ) {	
		// 	if (keys_held & KEY_B ) { 		
		// 		draw_cell_status();
		// 	} else if (keys_held & KEY_SELECT ) {
		// 		copy_pattern();
		// 		clear_pattern();
		// 		draw_pattern();
		// 	}
		// }
		
		if ( keys_pressed & KEY_LEFT ) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			// iprintf("\x1b[1;15Hcursor_xlol %d", cursor_x);
			
			if (keys_held & KEY_A ) {       //depitch by 1 semitone
				if (cursorPan == true && (cursor_x == 16 || cursor_x == 0 || cursor_x == 15)) { // colonne 1, PAN
					panChange--;
					if (panChange < 0) {
						panChange = 2;
					}
					iprintf("\x1b[1;1Hcursor_y %d", cursor_y);
					// iprintf("\x1b[2;2HCurrentPatternIndex %d", current_pattern_index);								
					current_song->channels[current_pattern_index].pan = trackPan[panChange];
					panTab[cursor_y] = trackPan[panChange];
				}
				else
				if(pv->volume > 0){
					int mod_pitch = (pv->pitch-1) + current_song->channels[cursor_y].pitch;
					if (mod_pitch>0){
						pv->pitch--;
					} 
					draw_cell_status();					
				} 
			} 
			else if (keys_held & KEY_B ) {
				// 	if(pv->volume > 0){
				// 		xmod = 5+(cursor_x/4);
				// 		if(pv->pan == PAN_RIGHT){
				// 			pv->pan = PAN_CENTER;
				// 		} else {
				// 			pv->pan = PAN_LEFT;
				// 		}
				// 		draw_cell_status();
				// 	}	
				if (polyRhythm == true) {

					if (cursor_y >= 0 && cursor_y <= 5 ) {
						if (less_columns > 0) {
							less_columns--;
							// iprintf("\x1b[1;1Hless_c %d", less_columns);
							// iprintf("\x1b[2;2Hcount c %d", countColons);	
							for (int i=0; i<=5;i++) {
								if (less_columns == 11) {
									// countColons = less_columns;		
									countColons = 1;
								}
								if (less_columns == 7) {
									countColons = 2;						
								}
								if (less_columns == 3) {
									countColons = 3;						
								}
								//dessine un espace vide quand je fais B <-
								iprintf("\x1b[%d;%dH ", i+3, (less_columns-countColons)+8);
							}
						}
					}
				} else {
					if (less_columns > 0) {
						less_columns--;
						// iprintf("\x1b[1;1Hless_c %d", less_columns);
						// iprintf("\x1b[2;2Hcount c %d", countColons);	
						for (int i=0; i<MAX_ROWS;i++) {
							if (less_columns == 11) {
								// countColons = less_columns;		
								countColons = 1;
							}
							if (less_columns == 7) {
								countColons = 2;						
							}
							if (less_columns == 3) {
								countColons = 3;						
							}
							iprintf("\x1b[%d;%dH ", i+3, (less_columns-countColons)+8);
						}
					}	
				}
			} else {
				// iprintf("\x1b[2;1Hcursor_pan %d ", cursorPan);
				//mettre cursor tout à gauche pour panoramique
				// iprintf("\x1b[2;2Hcursor_xlol %d ", cursor_x);
				if (cursor_x == 0) {
					cursorPan = true;
					printf(rouge);
					iprintf("\x1b[%d;4HX", cursor_y+3);
					printf(blanc);
					iprintf("\x1b[%d;5H-", cursor_y+3);
					cursor_x = 16; //16 c'est -1 mais ça évite un bug graphique
				}
				// if (cursorPan == true) {
				// 	cursorPan = false;
				// }
				else {
					if (cursor_x == 16) { //HC"
						int trackP = current_song->channels[current_pattern_index].pan;
						if (trackP == 128) { // centre
							iprintf("\x1b[%d;4HC", cursor_y+3);
						} else if (trackP == 0) { // left
							iprintf("\x1b[%d;4HL", cursor_y+3);
						} else if (trackP == 255) { // right
							iprintf("\x1b[%d;4HR", cursor_y+3);
						}
						iprintf("\x1b[%d;25H]", cursor_y+3);
					}
					if (cursorPan == true || cursor_x != 0) {
						move_cursor(-1,0);
					}
				}


				// if (cursorPan == true) {
				// 	move_cursor(-1,0);
				// 	iprintf("\x1b[2;20Hcaca");
				// }
			}
		}
		
		if ( keys_pressed & KEY_RIGHT ) {		
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			// iprintf("\x1b[1;15Hcursor_xlol %d", cursor_x);
			if (keys_held & KEY_A ) {
				if (cursorPan == true && (cursor_x == 16 || cursor_x == 0 || cursor_x == 15)) { // colonne 1, PAN
					panChange++;
					if (panChange > 2) {
						panChange = 0;
					}
					// iprintf("\x1b[1;1HpanChange %d", panChange);
					// iprintf("\x1b[2;2HCurrentPatternIndex %d", current_pattern_index);								
					current_song->channels[current_pattern_index].pan = trackPan[panChange];
				} 
				else
				if(pv->volume > 0){
					int mod_pitch = (pv->pitch+1) + current_song->channels[cursor_y].pitch;
					if (mod_pitch < 24){
						pv->pitch++;
					}
					draw_cell_status();
				}
			} else if (keys_held & KEY_B ) {
			// 	if(pv->volume > 0){
			// 		if (pv->pan == PAN_LEFT){
			// 			pv->pan=PAN_CENTER;
			// 		} else {	
			// 			pv->pan=PAN_RIGHT;
			// 		}
			// 		draw_cell_status();
			// 	} 
				if (less_columns < 16) {
					less_columns++;
					// iprintf("\x1b[1;1Hless_c %d", less_columns);
					// iprintf("\x1b[2;2Hcount c %d", countColons);

					for (int i=0; i<MAX_ROWS;i++) {
						if (less_columns == 11) {
							countColons = 1;
						} 
						if (less_columns == 12) {
							countColons = 1;						
						}
						if (less_columns == 13) {
							countColons = 0;						
						}
						if (less_columns == 9) {
							countColons = 1;						
						}
						if (less_columns == 5) {
							countColons = 2;						
						}
						iprintf("\x1b[%d;%dH-", i+3, less_columns+(7-countColons));				
					}
				}
			} else {
				//mettre cursor tout à gauche pour panoramique
				// cursor_x += 1;
				if (cursor_x == 15) {
					printf(rouge);
					iprintf("\x1b[%d;4HX", cursor_y+3);
					printf(blanc);
					iprintf("\x1b[%d;5H-", cursor_y+3);	
					// cursor_x = 0;
					// move_cursor(-1,0);
				}
				// iprintf("\x1b[1;15Hcursor_xlal %d", cursor_x);			
				if (cursor_x == 0) {
					int trackP = current_song->channels[current_pattern_index].pan;
					if (trackP == 128) { // centre
						iprintf("\x1b[%d;4HC", cursor_y+3);
					} else if (trackP == 0) { // left
						iprintf("\x1b[%d;4HL", cursor_y+3);
					} else if (trackP == 255) { // right
						iprintf("\x1b[%d;4HR", cursor_y+3);
					}				
				}
				if (cursor_x == 0 && cursorPan == true) {
					cursorPan = false;
					move_cursor(0,0);
					// iprintf("\x1b[%d;4H-", cursor_y+3);
					// printf(rouge);
					// iprintf("\x1b[%d;4HX", cursor_y+3);
					// printf(blanc);
					// iprintf("\x1b[%d;5H-", cursor_y+3);
				} else {
					move_cursor(1,0);
				}	
			}			
		}


		//A perso
		if ( (keys_pressed & KEY_A)) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			
			if (pv->volume == 0){
				pv->pan = PAN_OFF;
				// if (alternateVol == 1) {
				// 	pv->volume=2;
				// } else {
					if (cursorPan == false || (cursor_x != 16 || cursor_x != 0 || cursor_x != 15)) {
						if (polyRhythm == false || (polyRhythm == true && cursor_y != 6)) {
							pv->volume = 1;
							pv->optionsUp = 1;
							soundsInCurrentPattern++;
							//j'enregistre si y'a des sons dans ce pattern là ou pas
							//si oui alors il loopera avec les autres pattern
							soundsInPattern[current_pattern_index] = soundsInCurrentPattern;
						}
					}
				// }
			} 
			// else if (pv->volume > 0)
			// {
			// 	// if (pv->volume == 1) {
			// 	// 	alternateVol = 1;
			// 	// } else {
			// 	// 	alternateVol = 2;
			// 	// }
			// 	//pv->volume=0;
			// 	// soundsInCurrentPattern--;
			// 	// soundsInPattern[current_pattern_index] = soundsInCurrentPattern;
			// }

			draw_cell_status();
		}


		//uniquement pour la DS
		if ( keys_pressed & KEY_X ) {
			current_song->bpm = current_song->bpm + 10;
			draw_bpm_pattern();
			set_bpm_to_millis();
			update_timers();
		}
		if ( keys_pressed & KEY_Y ) {
			current_song->bpm = current_song->bpm - 10;
			draw_bpm_pattern();
			set_bpm_to_millis();
			update_timers();
		}

		
		if ( keys_pressed & KEY_UP ) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			if (keys_held & KEY_A ) {
				// if (pv->volume == 0){
				// 	pv->pan = PAN_OFF;
				// } 
				// if (pv->volume < 2){
				// 	pv->volume++;
				// }
				if (cursorPan == false || (cursor_x != 16 || cursor_x != 0 || cursor_x != 15)) {
					pv->optionsUp++;
					if (pv->optionsUp == 1) {
						pv->volume = 1;
					} else if (pv->optionsUp == 2) {
						pv->volume = 2;
					} else if (pv->optionsUp == 3) {
						pv->random = true;
					} else if (pv->optionsUp == 4) {
						pv->random = false;
					}
					if (pv->optionsUp > 3) {
						pv->playOrNot++;
						if (pv->playOrNot == 1) {
							pv->playOrNot = 2;
						}
					}
					
					draw_cell_status();
				}
			} 
			// else if (keys_held & KEY_B ) {
				
			// 	// current_song->bpm = current_song->bpm + 10;
			// 	// draw_bpm_pattern();

			// } 
			// else if (keys_held & KEY_SELECT ) {
			// 	//MUTE/UNMUTE CHANNEL
			// 	if (current_song->channels[cursor_y].status != MUTED){
			// 		current_song->channels[cursor_y].status = MUTED;
			// 		dessiner_char('M',29,cursor_y+ymod,0);
			// 	} else {
			// 		current_song->channels[cursor_y].status = NOT_MUTED;
			// 		dessiner_char(' ',29,cursor_y+ymod,0);			
			// 	}
			// } 
			else {

				printf(jaune);
				if (panTab[cursor_y+1] == 128) { // centre
					iprintf("\x1b[%d;4HC", cursor_y+4);
				} else if (panTab[cursor_y+1] == 0) { // left
					iprintf("\x1b[%d;4HL", cursor_y+4);
				} else if (panTab[cursor_y+1] == 255) { // right
					iprintf("\x1b[%d;4HR", cursor_y+4);
				}
				printf(blanc);
				printf(rouge);
				iprintf("\x1b[%d;4HX", cursor_y+3);
				printf(blanc);

				move_cursor(0,-1);
			}

			//ici
			set_bpm_to_millis();
			update_timers();
			// draw_pattern();
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);			
			// if (keys_held & KEY_B ) {
				
			// 	// current_song->bpm = current_song->bpm - 10;
			// 	// draw_bpm_pattern();

			// } else 
			if (keys_held & KEY_A ) {
				
				if (cursorPan == false || (cursor_x != 16 || cursor_x != 0 || cursor_x != 15)) {
					pv->optionsUp--;
					if (pv->optionsUp == 0) {
						pv->volume = 0;
						pv->playOrNot = 0;
					}
					else if (pv->optionsUp == 1) {
						pv->volume = 1;
						pv->playOrNot = 0;
					} else if (pv->optionsUp == 2) {
						pv->volume = 2;
						pv->random = false;
						pv->playOrNot = 0;
					} else if (pv->optionsUp == 3) {
						pv->random = true;
						pv->playOrNot = 0;
					} else if (pv->optionsUp == 4) {
						pv->random = false;
					}
					if (pv->optionsUp > 3) {
						pv->playOrNot--; //jouer un sur 2, 1/3 etc...
						if (pv->playOrNot == 1) {
							pv->playOrNot = 2;
						}
					}

					draw_cell_status();
				}
			} 
			// else if (keys_held & KEY_SELECT ) {
			// 	int i;
			// 	if (current_song->channels[cursor_y].status != SOLOED){
			// 	//SOLO/UNSOLO CHANNEL
			// 		for (i = 0; i < MAX_ROWS; i++){					
			// 			if (current_song->channels[i].status != SOLOED){
			// 				current_song->channels[i].status = MUTED;
			// 				dessiner_char('M',29,i+ymod,0);
			// 			} else {
			// 				dessiner_char('S',29,i+ymod,0);
			// 			}
			// 		}
			// 		current_song->channels[cursor_y].status = SOLOED;
			// 		dessiner_char('S',29,cursor_y+ymod,0);
			// 	} else {
			// 		for (i = 0; i < MAX_ROWS; i++){					
			// 			current_song->channels[i].status = NOT_MUTED;
			// 			dessiner_char(' ',29,i+ymod,0);
			// 		}
			// 	}			
			// } 
			else {
				//celui sur lequel j'arrive le pan c'est cursor+1
				//le précédant c'est le normal...
				// iprintf("\x1b[1;1Hcurso %d   ", panTab[cursor_y+1]);
				printf(jaune);
				if (panTab[cursor_y] == 128) { // centre
					iprintf("\x1b[%d;4HC", cursor_y+3);
				} else if (panTab[cursor_y] == 0) { // left
					iprintf("\x1b[%d;4HL", cursor_y+3);
				} else if (panTab[cursor_y] == 255) { // right
					iprintf("\x1b[%d;4HR", cursor_y+3);
				}
				printf(blanc);
				printf(rouge);
				iprintf("\x1b[%d;4HX", cursor_y+4);
				printf(blanc);
				// countTest++;
				// iprintf("\x1b[9;2HcountTest %d ", countTest);		
				move_cursor(0,1);
			}
			set_bpm_to_millis();
			update_timers();
			//todo si je met draw pattern
			//les r disparaissent bizarrement
			//idem pour le draw pattern plus bas du down
			// draw_pattern();
		}
		
		if ( (keys_pressed & KEY_START || keys_pressed & KEY_SELECT)) {
			//pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);			
			if (play == PLAY_STOPPED){ 
				play_pattern();
				pas = 0;
				tours = 0;
				step = 0;
				iaChangeRandom = false;
				iaPreviousPatern = false;
				// mmStreamOpen( &mystream );
			}
			// else if (play == PLAY_LIVE){ //play this column next
			// 	if ( keys_held & KEY_SELECT ) {
			// 		stop_song();
			// 		// sine_freq = 0;
			// 		// lfo_freq = 0;
			// 		// lfo_shift = 0;
			// 		// mmStreamUpdate();
			// 		changedPatternOne = 0;
			// 		step = 0;
			// 		tours = 0;
			// 	}
			// } 
			else {
				stop_song();
				// soundPause(sound);
				// sine_freq = 0;
				// lfo_freq = 0;
				// lfo_shift = 0;
				changedPatternOne = 0;
				step = 0;
				tours = 0;
				// sprintf(string_buff,"fesss %d", changedPatternOne);
				// put_string(string_buff,0,0,0);
				// mmStreamUpdate();
				// mmStreamClose();
				// mmStreamOpen( &mystream );
			}
		}
					
		//toujours dans screen PATTERN
		//si y'a des sons dan le pattern je le loop avec
		//les autres pattern dans lesquels il y a des sons
		if (autoLoopPattern == true) {
			if (step == 3 || step == 7) {
				changedPatternOne = 0;
			}
			if (tours % 4 == 0) { //ça joue tous les 4 temps
				if (soundsInPattern[current_pattern_index+1] > 0) {
					if (changedPatternOne == 0) {
						current_pattern_index = current_pattern_index+1;
						draw_pattern();
						changedPatternOne = 1;
						//changedPatternTwo = false;
					}
				} 
				else if (changedPatternOne == 0) {
					current_pattern_index = 0;
					draw_pattern();
					changedPatternOne = 1;
				}
			}
		}
		if (superLiveMod == false) {
			if ( keys_pressed & KEY_R ) {
				if (keys_held & KEY_SELECT ) {
					//je vire les pages song et settings
					// current_screen = SCREEN_SONG; 
					current_screen = SCREEN_SAMPLES;
					draw_screen();					
				}
				// else if (keys_held & KEY_A){
				// 	paste_pattern();
				// 	draw_pattern();
				// } 
				else {
					if (current_pattern_index < (MAX_PATTERNS-1)) {
						current_pattern_index+=1;
					}
					draw_pattern();
				}			
			}
			if ( keys_pressed & KEY_L ) {
				// if (keys_held & KEY_A){
				// 	copy_pattern();
				// } else 
				if (current_pattern_index > 0) {
					current_pattern_index-=1;				
				}
				draw_pattern();
			}
		} else {
			if ( keys_pressed & KEY_R ) {
				if (globalPitch < 6000) {
					globalPitch+= 20;
				}
				draw_bpm_pattern();
			}
			if ( keys_pressed & KEY_L ) {
				if (globalPitch > 0) {
					globalPitch-= 20;
				}
				draw_bpm_pattern();
			}
		}


		//B delete supprimer tout
		if ( (keys_pressed & KEY_B)) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			// pv->random = !pv->random;
			pv->volume = 0;
			pv->playOrNot = 0;
			pv->random = false;
			soundsInCurrentPattern--;
			soundsInPattern[current_pattern_index] = soundsInCurrentPattern;
		}

	}
	
	//SONG SCREEN INPUTS
	else if (current_screen == SCREEN_SONG){
		if ( keys_pressed & KEY_LEFT ) {
				if (keys_held & KEY_A ) {
					if (current_song->order[order_page+(cursor_x*10)+cursor_y] > 0){
						current_song->order[order_page+(cursor_x*10)+cursor_y]-=1;
						draw_song();
					}
				} else {
					move_cursor(-1,0);
			}
		}
		if ( keys_pressed & KEY_RIGHT ) {
			if (keys_held & KEY_A ) {
					if (current_song->order[order_page+(cursor_x*10)+cursor_y] < MAX_PATTERNS-1){
						current_song->order[order_page+(cursor_x*10)+cursor_y]+=1;
						draw_song();
				}
			} else {
				move_cursor(1,0);
			}
		}
		
		if ( keys_pressed & KEY_UP ) {
			if (keys_held & KEY_A ) {
				index = order_page+cursor_y+(cursor_x*10);
				if (current_song->order[index] < MAX_PATTERNS-11){
					current_song->order[index]+=10;
				} else {
					current_song->order[index] = MAX_PATTERNS-1;	
				}
				draw_song();
			} else {
			move_cursor(0,-1);
			}
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			if (keys_held & KEY_A ) {
				index = order_page+cursor_y+(cursor_x*10);
				if (current_song->order[index] > 10){
					current_song->order[index]-=10;
				} else {
					current_song->order[index] = 0;
				}
				draw_song();				
			} else {
			move_cursor(0,1);
			}
		}
		
		// if ( keys_pressed & KEY_A ) {			
		// 	if (keys_held & KEY_SELECT ) {
		// 		paste_order(&current_song->order[order_page+(cursor_x*10)+cursor_y]);
		// 		move_cursor(0,1);
		// 		draw_song();
		// 	} else if (keys_held & KEY_B ) {
		// 		copy_order(&current_song->order[order_page+(cursor_x*10)+cursor_y]);
		// 		current_song->order[order_page+(cursor_x*10)+cursor_y]=-1;
		// 		draw_song();
		// 	} else if (keys_held == KEY_A ){
		// 		if (current_song->order[order_page+(cursor_x*10)+cursor_y] == NO_ORDER){
		// 			current_song->order[order_page+(cursor_x*10)+cursor_y] = 0;
		// 			draw_song();
		// 		}
		// 	}  
		// }
		
		if ( keys_pressed & KEY_L ) {
			if ( keys_held & KEY_SELECT ) {
				current_screen = SCREEN_PATTERN;
				draw_screen();
			} else {
				order_page=0;
				draw_song();
			}
		}
		
		if ( keys_pressed & KEY_R ) {
			if ( keys_held & KEY_SELECT ) {
				current_screen = SCREEN_SAMPLES;
				draw_screen();
			} else {
				order_page=50;
				draw_song();
			}
		}
				
		if ( keys_pressed & KEY_START ) {
			if (play == PLAY_STOPPED){ //start playing
				play_song();
			} else if (play == PLAY_LIVE){ //if livemode, queue next pattern
				if ( keys_held & KEY_SELECT ) {
					stop_song();
				} else {
					if (current_song->order[order_page+(cursor_x*10)+cursor_y] != NO_ORDER){
						if (play_next != NO_ORDER){
							clear_last_song_queued_cursor();
						}
						play_next = order_page+(cursor_x*10)+cursor_y;
						draw_song_queued_cursor();
					}
				}
			} else { 			//stop playing
				stop_song();	
			}
		}
	} 
	
	//SAMPLE SCREEN INPUTS
	else if (current_screen == SCREEN_SAMPLES){
		if ( keys_pressed & KEY_LEFT ) {
			if (keys_held & KEY_A) {
				samples_menu(cursor_y, cursor_x, MENU_LEFT);
			} else if ((keys_held ^ KEY_LEFT) == 0){
				move_cursor(-1,0);	
			}
		}
		
		if ( keys_pressed & KEY_RIGHT ) {
			if (keys_held & KEY_A) {
				samples_menu(cursor_y, cursor_x,  MENU_RIGHT);
			} else if ((keys_held ^ KEY_RIGHT) == 0){
				move_cursor(1,0);	
			}	
		}
		
		if ( keys_pressed & KEY_UP ) { 
			if (keys_held & KEY_A) {
				samples_menu(cursor_y, cursor_x,  MENU_UP);
			} else if ((keys_held ^ KEY_UP) == 0){
				move_cursor(0,-1);
			}	
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			if (keys_held & KEY_A) {
				samples_menu(cursor_y, cursor_x,  MENU_DOWN);
			} else if ((keys_held ^ KEY_DOWN) == 0){
				move_cursor(0,1);	
			}
		}
		
		if ( keys_pressed & KEY_A ) {
			if ((keys_held ^ KEY_A) == 0){
				samples_menu(cursor_y, cursor_x, MENU_ACTION);
			}
		}
		
		// if ( keys_pressed & KEY_B ) {
		// 	if ( keys_held & KEY_SELECT ) {
		// 		if (cursor_x == 0){
		// 			rand_samples();
		// 			draw_samples();
		// 		}
		// 	}
		// }


		if (superLiveMod == false) {
			if ( keys_pressed & KEY_L ) {
				if ( keys_held & KEY_SELECT ) {
					//je vire les pages song et settings
					//current_screen = SCREEN_SONG;
					current_screen = SCREEN_PATTERN;
					//efface les résiduts de lettres
					iprintf("\x1b[2;0H                              ");
					draw_screen();
				}
			}
		}
			
		// if ( keys_pressed & KEY_R ) {
		// 	if ( keys_held & KEY_SELECT ) {
		// 		current_screen = SCREEN_SETTINGS;
		// 		draw_screen();
		// 	}
		// }
	}	
	
	//SETTINGS SCREEN INPUTS
	else if (current_screen == SCREEN_SETTINGS){
		//process input
		if ( keys_pressed & KEY_LEFT ) {
			if ( keys_held & KEY_SELECT ) {
				//sync time adjust
			} else if (keys_held & KEY_A) {
				settings_menu(cursor_y, MENU_LEFT);
			}
		}
		
		if ( keys_pressed & KEY_RIGHT ) {
			if (keys_held & KEY_A) {
				settings_menu(cursor_y, MENU_RIGHT);
			}	
		}
		
		if ( keys_pressed & KEY_UP ) { 
			if (keys_held & KEY_A) {
				settings_menu(cursor_y, MENU_UP);
			} else if ((keys_held ^ KEY_UP) == 0){
				move_cursor(0,-1);
			}	
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			if (keys_held & KEY_A) {
				settings_menu(cursor_y, MENU_DOWN);
			} else if ((keys_held ^ KEY_DOWN) == 0){
				move_cursor(0,1);	
			}
		}
		
		if ( keys_pressed & KEY_A ) {
			if ((keys_held ^ KEY_A) == 0){
				settings_menu(cursor_y, MENU_ACTION);
			}
		}
		
		if ( keys_pressed & KEY_L ) {
			if ( keys_held & KEY_SELECT ) {
				current_screen = SCREEN_SAMPLES;
				draw_screen();
			}
		}
	} 
	
	//HELP SCREEN INPUTS
	else if (current_screen == SCREEN_HELP){
		if (keys_pressed & KEY_LEFT){
			move_cursor(-1,0);
		}
		if (keys_pressed & KEY_RIGHT){
			move_cursor(1,0);
		}
		
		if ( keys_pressed & KEY_A || keys_pressed & KEY_B) {
			current_screen = SCREEN_SETTINGS;
			draw_screen();
		}
	}
		
}

void vblank_interrupt(){
		//mmVBlank();
		REG_IF = IRQ_VBLANK;
	}


void timer3_interrupt(){
	    current_ticks++;
	    
	    if (current_song->sync_mode == SYNC_MASTER){
			//send a serial byte i guess?
		} else if (current_song->sync_mode == SYNC_TRIG_MASTER){
			//pull so high?
		}
	    
	    if (tick_or_tock == TICK){		
			if (current_ticks >= ticks){				
				tick_or_tock = TOCK;	
				current_ticks = 0;
				next_column();
				
			}
		} else if (tick_or_tock == TOCK){
			if (current_ticks >= tocks){     //if its a tock
				tick_or_tock = TICK;
				current_ticks = 0;
				next_column();
			}
		}
		//REG_IF = REG_IF | IRQ_TIMER3;
		REG_IF = IRQ_TIMER3;
}	

			
void gpio_interrupt(){
	/*
	 * 24ppqn, so each pulse is a tick 24pulse = 1/4, 6 pulse = 1/16
	 * 12ppqn, so each pulse is 2           12 = 1/4, 3 = 1/16 
	*/
	    current_ticks+=2;
	    if (tick_or_tock == TICK){       
			if (current_ticks >= ticks){
				tick_or_tock = TOCK;	
				current_ticks = 0;
				next_column();
				}
			} else if (current_ticks >= tocks){     //if its a tock
				tick_or_tock = TICK;
				current_ticks = 0;
				next_column();
				}
	
	REG_IF = IRQ_SPI; //apparently to 'reset' or acknowledge the interrupt?
}

void serial_interrupt(){
	/* clear the data register */
	//REG_SIODATA8 = 0;

	/* XXX p.134 says to set d03 of SIOCNT ... */
	/* IE=1, external clock */
	//REG_DIVCNT = SIO_IRQ | SIO_SO_HIGH;

	/* XXX then it says to set it 0 again... I don't get it */
	//REG_DIVCNT &= ~SIO_SO_HIGH;
	
	/* start transfer */
	//REG_DIVCNT |= SIO_START;
	
	current_ticks++; 	//1 tick per byte
    
    if (tick_or_tock == TICK){       
		if (current_ticks >= ticks){
			tick_or_tock = TOCK;	
			current_ticks = 0;
			next_column();
			}
		} else if (current_ticks >= tocks){     //if its a tock
			tick_or_tock = TICK;
			current_ticks = 0;
			next_column();
		}

	//serial_ticks++;
	REG_IF = IRQ_SPI; //apparently to 'reset' the interrupt?
}

void setup_interrupts(){
	//irqInit();
	irqSet( IRQ_VBLANK, vblank_interrupt );
	irqSet( IRQ_TIMER3, timer3_interrupt );
	//irqSet( IRQ_SPI, gpio_interrupt );
	irqEnable(IRQ_VBLANK | IRQ_TIMER3);
	//mmSetVBlankHandler( &mmFrame );
}


void setup_display(){
	// screen mode & background to display
	//SetMode( MODE_FB0 );
	
	// set the screen base to 31 (0x600F800) and char base to 0 (0x6000000)
	//BGCTRL[0] = SCREEN_BASE(31);
	// load the palette for the background, 7 colors
	define_palettes(current_song->color_mode);
	// load the font into gba video mem (48 characters, 4bit tiles)   
	//CpuFastSet(r6502_portfont_bin, (u16*)VRAM,(r6502_portfont_bin_size/4));
	// clear screen map with tile 0 ('space' tile) (256x256 halfwords)
	//*((u32 *)MAP_BASE_ADR(31)) =0;
	//CpuFastSet( MAP_BASE_ADR(31), MAP_BASE_ADR(31));
}

// void printBottom(string, x, y) {
// 	consoleSelect(&bottomScreen);
// 	iprintf("\x1b[%d;%dH%c", y, x, string);
// }

// int sound = 0;

//joue son gameboy
// soundEnable();
// sound = soundPlayPSG(50, 4000, 64, 64);	
// soundPause(sound);
//fin joue son gameboy

//int sndStatus = 0;
// int Pressed;
// int Held;
// int Released;

// //son gameboy
// int sine;		// sine position
// int lfo;		// LFO position

// //-----------------------------------------------------------------------------
// enum {
// //-----------------------------------------------------------------------------
// 	// waveform base frequency
// 	sine_freq = 500,
	
// 	// LFO frequency
// 	lfo_freq = 3,
	
// 	// LFO output shift amount
// 	lfo_shift = 4,
	
// 	// blue backdrop
// 	bg_colour = 13 << 10,
	
// 	// red cpu usage
// 	cpu_colour = 31
// };

/***********************************************************************************
 * on_stream_request
 *
 * Audio stream data request handler.
 ***********************************************************************************/
// mm_word on_stream_request( mm_word length, mm_addr dest, mm_stream_formats format ) {
// //----------------------------------------------------------------------------------
	
// 	s16 *target = dest;
	
// 	//------------------------------------------------------------
// 	// synthensize a sine wave with an LFO applied to the pitch
// 	// the stereo data is interleaved
// 	//------------------------------------------------------------
// 	int len = length;
// 	for( ; len; len-- )
// 	{
// 		int sample = sinLerp(sine);
		
// 		// output sample for left
// 		*target++ = sample;
		
// 		// output inverted sample for right
// 		*target++ = -sample;
		
// 		sine += sine_freq + (sinLerp(lfo) >> lfo_shift);
// 		lfo = (lfo + lfo_freq);
// 	}
	
// 	return length;
// }
//fin son gameboy


int main() {


	current_song = (song *) malloc(sizeof(song));
	init_song();

	setup_timers();
	
	
	//c'est le truc qui pose problème TODO
	setup_interrupts();


	//setup_display();
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);


	consoleDemoInit();

	

	mmInitDefaultMem((mm_addr)soundbank_bin);



	//son gameboy
	//----------------------------------------------------------------
	// initialize maxmod without any soundbank (unusual setup)
	//----------------------------------------------------------------
	// mm_ds_system sys;
	// sys.mod_count 			= 0;
	// sys.samp_count			= 0;
	// sys.mem_bank			= 0;
	// sys.fifo_channel		= FIFO_MAXMOD;
	// mmInit( &sys );
	
	//----------------------------------------------------------------
	// open stream
	//----------------------------------------------------------------
	// mm_stream mystream;
	// mystream.sampling_rate	= 25000;					// sampling rate = 25khz
	// mystream.buffer_length	= 1200;						// buffer length = 1200 samples
	// mystream.callback		= on_stream_request;		// set callback function
	// mystream.format			= MM_STREAM_16BIT_STEREO;	// format = stereo 16-bit
	// mystream.timer			= MM_TIMER0;				// use hardware timer 0
	// mystream.manual			= true;		
	
	// 	mystream.sampling_rate	= 25000;					// sampling rate = 25khz
	// 	mystream.buffer_length	= 1200;						// buffer length = 1200 samples

	// 	mystream.callback		= on_stream_request;		// set callback function
	// 	mystream.format			= MM_STREAM_16BIT_STEREO;	// format = stereo 16-bit
	// 	mystream.timer			= MM_TIMER0;				// use hardware timer 0
	// 	mystream.manual			= true;	
	// 					// use manual filling
	// mmStreamOpen( &mystream ); // c'est ça qui fait le SON!!!!!!
	// mmStreamClose();
	
	//----------------------------------------------------------------
	// when using 'automatic' filling, your callback will be triggered
	// every time half of the wave buffer is processed.
	//
	// so: 
	// 25000 (rate)
	// ----- = ~21 Hz for a full pass, and ~42hz for half pass
	// 1200  (length)
	//----------------------------------------------------------------
	// with 'manual' filling, you must call mmStreamUpdate
	// periodically (and often enough to avoid buffer underruns)
	//----------------------------------------------------------------
	
	SetYtrigger( 0 );
	irqEnable( IRQ_VCOUNT );
	//fin son gameboy
	

	//GLOBAL VARS
	cursor_x = 0;
	cursor_y = 0;
	current_screen = SCREEN_PATTERN;
	current_ticks = 0;

	draw_screen();


	consoleSelect(&topScreen); //faut remettre en top pour le reste

	//joue son gameboy
	// soundEnable();
	// sound = soundPlayPSG(DutyCycle_12, 4000, 64, 64);	
	// soundPause(sound);
	//fin joue son gameboy

	while (1) {

		// swiWaitForVBlank();
		// scanKeys();
		// Pressed = keysDown();

		// if(KEY_A & Pressed && sndStatus == 0)
		// {
		// 	soundSetPan(sound, 64);
		// 	soundResume(sound);
		// 	sndStatus = 1;
		// 	iprintf("Sound On\n");
		// }
		// else if(KEY_A & Pressed && sndStatus == 1)
		// {
		// 	soundPause(sound);
		// 	sndStatus = 0;
		// 	iprintf("Sound OFF\n");
		// }
		// soundSetFreq(sound, 4000);
		// soundSetWaveDuty(sound, 50);

		//son gameboy nouvelle manière de faire
		// wait until line 0
		// swiIntrWait( 0, IRQ_VCOUNT);
		
		// update stream
		// mmStreamUpdate();
		//fin son gameboy

		process_input();

	}
}
