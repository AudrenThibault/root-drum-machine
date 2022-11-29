#include <nds.h>
#include <maxmod9.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// à inclure pour le delay
// for linux delay : #include <unistd.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <nds/ndstypes.h>
//à inclure pour l'image - include this for image
// #include <gl2d.h>
#include "soundbank.h"
#include "soundbank_bin.h"
#include "r6502_portfont_bin.h"
#include "include.h"
#include "palette.h"
#include "help.h"

//image
#include "anya.h"

#define MAPADDRESS		BG_BMP_BASE(31)	// our base map address
#define SCREEN_BUFFER  ((u16 *) 0x6000000)

#define MAX_COLUMNS	16

//nombre de colonnes - number of columns
int less_columns = MAX_COLUMNS; //1st seq
int less_columns_second_seq = MAX_COLUMNS; //2nd seq
int less_columns_third_seq = MAX_COLUMNS; //3rd seq

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
unsigned int xmodThirdSeq = 0;

unsigned int ymod = 3;
unsigned int draw_flag = 0;

unsigned int last_xmod=0;
unsigned int last_xmod_secondSeq=0;
unsigned int last_xmod_thirdSeq=0;

unsigned int last_column=0;
unsigned int last_secondSeq_column=0;
unsigned int last_thirdSeq_column=0;

int current_column = -1;
int current_secondSeq_column = -1;
int current_thirdSeq_column = -1;

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
	channel channels[MAX_ROWS];
	int pattern_length;
	int order_length;
		
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

u8 order_buff;
int order_page = 0;

int pas = 0; //nombre de fois que le curseur revient au début - number of time cursor goes back to start
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
int IALimit = 4;
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
int countColonsSeqOne;
int countColonsSeqTwo;
int countColonsSeqFree;
int cursorOnSecondSeq = false;
int pitchSecondSeq;
int polyRhythm = false;
int cursorPan = false;
int trackPan[] = {128, 0, 255};
int panChange;
int panTab[11] = { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 };
int changeSample = 0;
int randCursor = false; //quick fix
int numberOfSeq = 2;
int page2 = false;
int IAChangeSample = false;
int IAChangePitch = false;
int IARandomPitch = 0;
int changedPage = false;
int IAChangeSampleNumber = 1;
int IAChangePitchNumber = 1;
int changeSampleDoneCounter = 0;
int autoChangedSample = false;
int changedAutoChangedSample = false;
int goBackSamples = false;
int arpeg = false;
int arpegSample = 0;
int arpegRowVolume = 0;
int arpegEcartement = 0;
int arpegPitch1 = 1024+200;
int arpegPitch2 = 1024+400;
int arpegPitch3 = 1024+600;
int arpegPitch4 = 1024+800;

PrintConsole topScreen;
PrintConsole bottomScreen;

touchPosition touch;

#define MAX_SAMPLES MSL_NSAMPS

char string_buff[50];

void set_bpm_to_millis(){
	millis_between_ticks = (int) (((1.0f/(current_song->bpm/60.0f))/0.0001f)/4)/6;
}

int set_shuffle(){
	ticks = current_song->shuffle;
	tocks = 12-current_song->shuffle;
	return  (int) 100.0f/(12.0/current_song->shuffle);
}


void setup_timers(){
	// Overflow every ~0.01 millisecond:
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

void print_random(vue) {
	if (vue == true) {
		printf(rouge);
		iprintf("\x1b[1;11HRANDOM!");
		printf(blanc);
	} else {
		iprintf("\x1b[1;11H       ");
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

		int IAlimitCoef = 0;

		if (IALimit < 10) {
			IAlimitCoef = 16;
		} else {
			IAlimitCoef = 4;
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

		} else if (tours <= IALimit * IAlimitCoef && IARandom == true) { //ia random
			print_random(true); //pour afficher le mot "Random" - to display "Random" word
			randCursor = true;
			randomize_pattern();
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

//son gameboy - gameboy sound
int sine;		// sine position
int lfo;		// LFO position
int sine_freq = 0;
int lfo_freq = 0;
int lfo_shift = 0;

mm_word on_stream_request( mm_word length, mm_addr dest, mm_stream_formats format ) {}


void play_sample(row, sample, vol, pitch, pan) {

	if (globalPitch != 0 && superLiveMod == true) {
		pitch = globalPitch;
	}
	if (IAChangePitch == true) {
		pitch = IARandomPitch;
	}

	if (sample != SFX_SYNC) {
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
	}
	
	if (row != 11) {
		if (current_song->channels[row].status != MUTED){		
			//je passe tout d'un côté du panoramique si le 
			//sync mode est activé, le son d'un côté le sync de l'autre
			//everything goes to left or right
			if (syncDSMode == true) {
				pan = 0; //j'écrase pan - pan is erased
			}
			play_sample(row, sample, vol, pitch, pan);
		}
	} else {
		if (syncDSMode == true) {
			if (typeOfSyncs == 0) { //Volca
				play_sample_sync(row, SFX_SYNC);
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
	pattern_row *pvThirdSeq;
	pvThirdSeq = &(current_song->patterns[playing_pattern_index].columns[current_thirdSeq_column].rows[row]);

	pattern_row *pvToUse;

	if (current_song->bpm > 240) {
		current_song->bpm = current_song->bpm / 2;
		draw_bpm_pattern();
		set_bpm_to_millis();
		update_timers();
	}

	pas++;
	pas = pas % less_columns;

	if (IAChangeSample == true) {
		if (IARandom == true) {
			if (step == 8) {
				iaChangeRandom = !iaChangeRandom;
				iaPreviousPatern = !iaPreviousPatern;
				step = 0;
			}
		}
		else {
			if (step == 2*IAChangeSampleNumber) {
				iaChangeRandom = !iaChangeRandom;
				iaPreviousPatern = !iaPreviousPatern;
				step = 0;
			}
		}
	} else {
		if (step == 8) {
			iaChangeRandom = !iaChangeRandom;
			iaPreviousPatern = !iaPreviousPatern;
			step = 0;
		}
	}
	
	if (pas == 0) {
		tours++;
		step++;
		gbSoundDecay++;
	}

	if (gbSoundDecay > 0) {
		gbSoundDecay--;
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
	if (IAAutoCymbal == true || IARandom == true || 
		IAChangeSample == true || IAChangePitch == true) {
		if (step < 4) {
			if (iaChangeRandom == false) { 
				copy_pattern();
				rand_pattern();
				
				//affiche les notes random sur la grille - print random notes
				if (polyRhythm == false) {
					custom_redessiner_seq(0, MAX_ROWS, less_columns);
				} else {
					custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, less_columns);
				}
				iaChangeRandom = !iaChangeRandom;
			}
		} 
		else if (tours > IALimit) {
			if (iaPreviousPatern == false) {
				paste_pattern();
				print_random(false);
				
				//Réaffiche les notes précédantes PAS random sur la grille - print previous not random notes on the grid
				if (polyRhythm == false) {
					custom_redessiner_seq(0, MAX_ROWS, less_columns);
				} else {
					custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, less_columns);
				}
				iaPreviousPatern = !iaPreviousPatern;
			}
		}

		if (IAChangeSample == true) {
			if (pas == 0 && step % IAChangeSampleNumber == 0) {
				if (goBackSamples == false) {
					if (changeSample < MAX_SAMPLES-1) {
						changeSample++;
						if (changeSample == SFX_SYNC) {
							changeSample++;
						}
					} else {
						goBackSamples = true;
					}
				} else { //si goBackSamples est true on repart en arrière - if goBackSamples is true we go back return
					if (changeSample > 0) {
						changeSample--;
						if (changeSample == SFX_SYNC) {
							changeSample--;
						}
					} else {
						goBackSamples = false; //on repart à l'endroit - upside down
					}
				}
			} 
		}
		if (IAChangePitch == true) {
			if (pas == 0 && step % IAChangePitchNumber == 0) {
				IARandomPitch = rand() % 4000;
				pitch = IARandomPitch;
			} 
		}	
	}
	for (row = 0; row < MAX_ROWS; row++){
		if (polyRhythm == true) {
			if (numberOfSeq == 2) {
				if (row > 5) {
					pvToUse = pvSecondSeq;	
				} else {
					pvToUse = pv;
				}
			} else if (numberOfSeq == 3) {
				if (row >= 0 && row <= 2) { //seq1
					pvToUse = pv;
				} else if (row >= 4 && row <= 6 ) { //seq2
					pvToUse = pvSecondSeq;
				} else if (row >= 8 && row <= 11) { //seq3
					pvToUse = pvThirdSeq;
				}
			}
		}
		else {
			pvToUse = pv;
		}
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

		//joue 1 fois sur 2, 1/3 etc... - play 1/2, 1/3 times
		if (pvToUse->playOrNot > 0) {
			if (pvToUse->playOrNot > 0) {
				if (pvToUse->playOrNotCount % pvToUse->playOrNot == 0) {
					play_sound(row, 
					current_song->channels[row].sample+changeSample, 
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

		// random additionnel - additionnal random
		else if (pvToUse->random == true) // R
		{
			int r = rand() % MAX_SAMPLES; /* random int between 0 and 24 */
			int sampleRandom = r;
			int randPan[] = {128, 0, 255}; 
			int randPanRandom = rand() % 3;
			int panRandom = randPan[randPanRandom];
			play_sound(row, 
			sampleRandom, 
			current_song->channels[row].volume, 
			pitch_table[12], //1024 = pitch normal
			panRandom);

		// les autres modes que random (o et O) - others mods than random
		} 
		
		else if (pvToUse->volume == 1) { // o
			arpegSample = current_song->channels[row].sample+changeSample;
			arpegRowVolume = current_song->channels[row].volume;
			play_sound(row, 
			current_song->channels[row].sample+changeSample, 
			current_song->channels[row].volume, 
			pitch_table[pitch], 
			pan);
		}
		else if (pvToUse->volume == 2) { // O
			play_sound(row, 
			current_song->channels[row].sample+changeSample, 
			current_song->channels[row].volume+55, 
			pitch_table[pitch],
			pan);
		}
		pv++;
		pvSecondSeq++;
		pvThirdSeq++;
	}

	if (arpeg == true) {
		if (pas == 2 + arpegEcartement) {
			mmEffectCancel(arpegSample); //cancel it before trigging it again
			mmUnloadEffect(arpegSample);
			play_sound(row, 
				arpegSample, 
				arpegRowVolume, 
				arpegPitch1, 
				//pitch_table[pitch], 
				0); //pan left
		}
		if (pas == 3 + arpegEcartement*arpegEcartement) {
			mmEffectCancel(arpegSample); //cancel it before trigging it again
			mmUnloadEffect(arpegSample);
			play_sound(row, 
				arpegSample, 
				arpegRowVolume, 
				arpegPitch2, 
				//pitch_table[pitch], 
				255); //pan right
		}
		if (pas == 4 + arpegEcartement*arpegEcartement) {
			mmEffectCancel(arpegSample); //cancel it before trigging it again
			mmUnloadEffect(arpegSample);
			play_sound(row, 
				arpegSample, 
				arpegRowVolume, 
				arpegPitch3, 
				//pitch_table[pitch], 
				0); //pan left
		}
		if (pas == 5 + arpegEcartement*arpegEcartement) {
			mmEffectCancel(arpegSample); //cancel it before trigging it again
			mmUnloadEffect(arpegSample);
			play_sound(row, 
				arpegSample, 
				arpegRowVolume, 
				arpegPitch4, 
				//pitch_table[pitch], 
				255); //pan right
		}
		if (arpegEcartement >= 16) {
			arpegEcartement = 0;
		}
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

void clear_screen(){}

void put_character(char c, u16 x, u16 y, unsigned char palette) {}

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

void custom_draw_pv() {
	pattern_row *pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);

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
			if (pv->playOrNot == 0) {
				dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);
			} else {
				printf(vert);
				dessiner_int(pv->playOrNot, cursor_x+xmod,cursor_y+3, 0);
				printf(blanc);			
			}
		}
	}
	else {
		if (numberOfSeq == 2 && (polyRhythm == true && cursor_y == 6)) {
			dessiner_char(' ', cursor_x+xmod,cursor_y+3, 0);
		} else if (numberOfSeq == 3 && (polyRhythm == true && cursor_y == 3)) {
			dessiner_char(' ', cursor_x+xmod,cursor_y+3, 0);
		} else if (numberOfSeq == 3 && (polyRhythm == true && cursor_y == 7))
			dessiner_char(' ', cursor_x+xmod,cursor_y+3, 0);		
		else {
			if (cursor_x != 16) {
				dessiner_char('-', cursor_x+xmod,cursor_y+3, 0);
			}
		}
	}
}

void move_cursor(int mod_x, int mod_y){
	int new_x, new_y;
	if (current_screen == SCREEN_PATTERN) {
		new_x = cursor_x + mod_x;
		new_y = cursor_y + mod_y;			
		xmod = 5+(cursor_x/4);
		put_string("   ",26,3+cursor_y,0);
		custom_draw_pv();

		if (polyRhythm == true) {
			if (numberOfSeq == 2) {
				if (cursor_y > 6) {
					if (new_x >= less_columns_second_seq) { 
						cursor_x = 0;
					} else if (new_x < 0) {
						cursor_x=less_columns_second_seq-1;
					} else cursor_x=new_x;
				} else {
					if (new_x >= less_columns) { 
						cursor_x = 0;
					} else if (new_x < 0) {
						cursor_x=less_columns-1;
					} else cursor_x=new_x;
				}
			} else if (numberOfSeq == 3) {
				if (cursor_y > 3 && cursor_y <= 6) { //seq 2
					if (new_x >= less_columns_second_seq) { 
						cursor_x = 0;
					} else if (new_x < 0) {
						cursor_x=less_columns_second_seq-1;
					} else cursor_x=new_x;

				} else  if (cursor_y > 7) { //seq 3
					if (new_x >= less_columns_third_seq) { 
						cursor_x = 0;
					} else if (new_x < 0) {
						cursor_x=less_columns_third_seq-1;
					} else cursor_x=new_x;
				}
				else { //seq 1
					if (new_x >= less_columns) { 
						cursor_x = 0;
					} else if (new_x < 0) {
						cursor_x=less_columns-1;
					} else cursor_x=new_x;
				}
			}
		}
		else {
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

		if (current_column > less_columns) {
			current_column = 0;
		}

		draw_flag = 1;
		xmod = 5+(current_column/4);
		dessiner_char(' ',last_column+last_xmod,2,0);
		dessiner_char('v',current_column+xmod,2,0);
		last_xmod = xmod;
		last_column = current_column;
		
		if (current_secondSeq_column > less_columns_second_seq) {
			current_secondSeq_column = 0;
		}

		if (polyRhythm == true && numberOfSeq == 2) {
			xmodSecondSeq = 5+(current_secondSeq_column/4);
			dessiner_char(' ',last_secondSeq_column+last_xmod_secondSeq,9,0);
			dessiner_char('v',current_secondSeq_column+xmodSecondSeq,9,0);
			last_xmod_secondSeq = xmodSecondSeq;
			last_secondSeq_column = current_secondSeq_column;
		}

		if (current_thirdSeq_column > less_columns_third_seq) {
			current_thirdSeq_column = 0;
		}

		if (polyRhythm == true && numberOfSeq == 3) {
			//2eme seq
			xmodSecondSeq = 5+(current_secondSeq_column/4);
			dessiner_char(' ',last_secondSeq_column+last_xmod_secondSeq,6,0);
			dessiner_char('v',current_secondSeq_column+xmodSecondSeq,6,0);
			last_xmod_secondSeq = xmodSecondSeq;
			last_secondSeq_column = current_secondSeq_column;
			//3eme seq
			xmodThirdSeq = 5+(current_thirdSeq_column/4);
			dessiner_char(' ',last_thirdSeq_column+last_xmod_thirdSeq,10,0);
			dessiner_char('v',current_thirdSeq_column+xmodThirdSeq,10,0);
			last_xmod_thirdSeq = xmodThirdSeq;
			last_thirdSeq_column = current_thirdSeq_column;
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

void draw_status(){}

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
	sprintf(string_buff,"BPM:");
	put_string(string_buff,0,16,0);
	printf(blanc);
	sprintf(string_buff,"%.2d ", (int) current_song->bpm);
	put_string(string_buff,5,16,0);
	sprintf(string_buff,"X/Y");
	put_string(string_buff,9,16,0);
	sprintf(string_buff,"(%d)", (int) current_song->bpm / 2);
	put_string(string_buff,13,16,0);
	printf(violet);
	sprintf(string_buff,"Random:");
	put_string(string_buff,21,16,0);
	printf(blanc);
	sprintf(string_buff,"B+A");
	put_string(string_buff,29,16,0);

	if (superLiveMod== true) {
		printf(jaune);
		sprintf(string_buff,"G.Pitch:%x   ", (int) globalPitch);
		put_string(string_buff,20,0,0);
	}
	printf(violet);
	sprintf(string_buff,"Delete note:");
	put_string(string_buff,0,18,0);
	printf(blanc);
	sprintf(string_buff,"B");
	put_string(string_buff,13,18,0);
	printf(violet);
	sprintf(string_buff,"Note options:");
	put_string(string_buff,0,20,0);
	printf(blanc);
	sprintf(string_buff,"A+pad");
	put_string(string_buff,14,20,0);
	printf(violet);
	sprintf(string_buff,"Less steps:");
	put_string(string_buff,0,22,0);
	printf(blanc);
	sprintf(string_buff,"B+ <- / ->");
	put_string(string_buff,12,22,0);
	printf(violet);
	sprintf(string_buff,"Change Sample:");
	put_string(string_buff,0,23,0);
	printf(blanc);
	sprintf(string_buff,"B+Up/Down");
	put_string(string_buff,15,23,0);
}

void custom_redessiner_all_seq_polyrhythm(rowToStart, nb_rows, less_columns) {
	int r = rowToStart;
	int c = 0;
	char rowbuff[(nb_rows+4)]; 
	int i;
	for (r = rowToStart; r < nb_rows; r++){
		i = 0;
		if (numberOfSeq == 2) {
			//1er Seq
			if (r >= 0 && r <= 5) { //1er Seq
				less_columns = less_columns;
			//2ème Seq
			} else if (r >= 7 && r <= 11) { //2ème Seq
				less_columns = less_columns_second_seq;
			}
		} else if (numberOfSeq == 3) {
			//1er Seq
			if (r >= 0 && r <= 2) { //1er Seq
				less_columns = less_columns;
			//2ème Seq
			} else if (r >= 4 && r <= 6) { //2ème Seq
				less_columns = less_columns_second_seq;
			}
			else if (r >= 8 && r <= 11) {
				less_columns = less_columns_third_seq;
			}
		}
	
		for (c = 0; c <= less_columns-1; c++){
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
		consoleSelect(&topScreen);
		iprintf("\x1b[%02d;0H%02d   %s     %c", r+3, r, rowbuff,current_song->channels[r].status);

		//initialiser le panoramique - initialize panoramic
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
	}
	if (numberOfSeq == 2) {
		iprintf("\x1b[9;4H                          ");						
	} else {
		iprintf("\x1b[6;4H                          ");						
		iprintf("\x1b[10;4H                          ");						

	}
	printf(cyan); //cyan
	iprintf("\x1b[0;13H%02d ", current_pattern_index);
	printf(blanc); //blanc
}

void custom_redessiner_seq(rowToStart, nb_rows, nb_columns) {
	int r = rowToStart;
	int c = 0;
	char rowbuff[(nb_rows+4)]; 
	int i;
	for (r = rowToStart; r < nb_rows; r++){
		i = 0;			
		for (c = 0; c < nb_columns; c++){
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
		consoleSelect(&topScreen);

		iprintf("\x1b[%02d;0H%02d   %s     %c", r+3, r, rowbuff,current_song->channels[r].status);

		//initialiser le pan panoramique - initialize panoramic
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
		printf(cyan); //cyan
		iprintf("\x1b[0;13H%02d ", current_pattern_index);
		printf(blanc); //blanc
	}	
}

void draw_pattern(){ 

	if (draw_flag == 0){
		draw_flag = 1;
		clear_screen();		
		consoleSelect(&topScreen);
		printf(rouge);
		iprintf("\x1b[0;0HR-"); 
		printf(jaune);
		iprintf("\x1b[0;2HO-");
		printf(vert);
		iprintf("\x1b[0;4HO-");
		printf(cyan);
		iprintf("\x1b[0;6HT");
		printf(bleu);
		iprintf("\x1b[0;8HD."); 
		printf(violet);
		iprintf("\x1b[0;10HM.");
		printf(cyan); //cyan
		iprintf("\x1b[0;13H%02d ", current_pattern_index);
		printf(blanc); //blanc	
		draw_bpm_pattern();
		custom_redessiner_seq(0, MAX_ROWS, less_columns);
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
	
	int i;
	
	for (i = 0; i < MAX_ROWS; i++){	
		sprintf(string_buff, "%02d [ %02d ][ %02x ][ %02d ][ %s ]", 
		i, 
		current_song->channels[i].sample, 
		current_song->channels[i].volume, 
		current_song->channels[i].pitch, 
		pan_string[(current_song->channels[i].pan)/127]);		
		put_string(string_buff,0,i+3,0);
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
	current_thirdSeq_column = 0;
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
	current_thirdSeq_column =0;

	playing_pattern_index=current_pattern_index;
	play = PLAY_PATTERN;
	
	switch (current_song->sync_mode) {
		case SYNC_NONE:		
			set_status(STATUS_PLAYING_PATTERN);
			REG_TM2C ^= TM_ENABLE;
			break;
	}
}

void play_song(){
	current_column=0;
	current_secondSeq_column = 0;
	current_thirdSeq_column =0;

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
		if (current_secondSeq_column == less_columns_second_seq) {
			playing_pattern_index=current_pattern_index;
			current_secondSeq_column = 0;
		}
		if (current_thirdSeq_column == less_columns_third_seq) {
			playing_pattern_index=current_pattern_index;
			current_thirdSeq_column = 0;
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
			playing_pattern_index=current_song->order[order_index];
			current_column = 0;
			current_secondSeq_column = 0;
			current_thirdSeq_column = 0;
			
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
				current_thirdSeq_column = 0;

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
	current_thirdSeq_column++;
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
	
	from = (u16 *) current_song;
	to = (u8 *) sram;
	//Song vars
	
	for (i = 0; i < 195; i++) {
		*to = (u8) (*from & 0x00FF);
		to++;
		*to = (u8) (*from >> 8);
		to++;
		from++;
		
		if (from == (u16 *) current_song->patterns){	
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
			break;
		}
	}
	
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
	
	current_song->bpm = 120;
	set_bpm_to_millis();
	set_shuffle();
	update_timers();
	draw_screen();
}

void randomize_pattern() {
	int row=0;
	int column=0;
	u8 val;
	pattern_row *current_cell = &(current_song->patterns[current_pattern_index].columns[column].rows[row]);

	for (int column = 0; column < less_columns; column++){
		for (int row = 0; row < MAX_ROWS; row++) {	
			val = (u8)rand();
			int randPan[] = {128, 0, 255}; 
			int randPanRandom = rand() % 3;
			int panRandom = randPan[randPanRandom];
			if (val > 245) {
				randVolTwo++;
				if (randVolTwo < 5) {
					current_cell->volume = 2;
					current_cell->pan = panRandom;
				}
			} else if (val < 245 && val > 220){	
				randVolOne++;
				if (randVolOne < 5) {
					current_cell->volume = 1;
					current_cell->pan = panRandom;
				}
			} else {
				current_cell->volume = 0;
			}	
			current_cell->pitch = 0;
			current_cell++;
		}
	}

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
				dessiner_char(current_song->song_name[name_index],12+name_index, 3, 1);
				
			} else if (action == MENU_DOWN){
				if ((current_song->song_name[name_index] <= 'Z') && (current_song->song_name[name_index] > 'A')){
					current_song->song_name[name_index]--;
				} else if (current_song->song_name[name_index] < 'A'){
					current_song->song_name[name_index]='A';
				}
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
	scanKeys();
	keys_pressed = keysDown();
	keys_released = keysUp();
	keys_held = keysHeld();

	//on peut pas toucher l'écran quand la lecture
	//est active sinon ça glitche - we can't touch screen when playing to prevent glitch
	if(keys_held & KEY_TOUCH) {
		if (play != PLAY_PATTERN) {
			touchRead(&touch);
			bottomPrinted = false;
		}
	}

	if (bottomPrinted == false) {
		consoleSelect(&bottomScreen);
		iprintf("\x1b[0;22Hz(%d,%d)", touch.px, touch.py);
		printf(rouge);
		iprintf("\x1b[0;0H-Works if not playing-");
		printf(blanc);
		if (page2 == true) {
			consoleSelect(&bottomScreen);
			if (changedPage == false) {
				for(int i = 1; i < 24; i++) {
					iprintf("\x1b[%d;0H                                ", i);
				}
				changedPage = true;
			}
			printf(cyan);
			iprintf("\x1b[2;25H | <= |" );
			printf(blanc);
			if (IAChangeSample == true) {
				iprintf("\x1b[2;0HIA sample change" );
				printf(vert);
				iprintf("\x1b[2;17H| On  |" );
				printf(blanc);
				iprintf("\x1b[4;0HEach");
				printf(vert);
				iprintf("\x1b[4;6H| + || - |");
				printf(blanc);
				iprintf("\x1b[4;17H%d  ", IAChangeSampleNumber);
			} else {
				iprintf("\x1b[2;0HIA sample change" );
				printf(vert);
				iprintf("\x1b[2;17H| Off |" );
				printf(blanc);				
				iprintf("\x1b[4;0H                  ");
			}
			if (IAChangePitch == true) {
				iprintf("\x1b[6;0HIA pitch change " );
				printf(vert);
				iprintf("\x1b[6;17H| On  |" );
				printf(blanc);
				iprintf("\x1b[8;0HEach");
				printf(vert);
				iprintf("\x1b[8;6H| + || - |");
				printf(blanc);
				iprintf("\x1b[8;17H%d  ", IAChangePitchNumber);
			} else {
				iprintf("\x1b[6;0HIA pitch change " );
				printf(vert);
				iprintf("\x1b[6;17H| Off |" );
				printf(blanc);				
				iprintf("\x1b[8;0H                  ");
			}
			if (arpeg == true) {
				iprintf("\x1b[10;0H IA Arpeggiator " );
				printf(vert);
				iprintf("\x1b[10;17H| On  |" );
				printf(blanc);
				iprintf("\x1b[12;0H R/L for change ecart");
				iprintf("\x1b[14;0H Pitch 1 : ");
				printf(vert);
				iprintf("\x1b[14;11H| + || - |");
				printf(blanc);
				iprintf("\x1b[14;22H%d    ", arpegPitch1);
				iprintf("\x1b[16;0H Pitch 2 : ");
				printf(vert);
				iprintf("\x1b[16;11H| + || - |");
				printf(blanc);
				iprintf("\x1b[16;22H%d    ", arpegPitch2);
				iprintf("\x1b[18;0H Pitch 3 : ");
				printf(vert);
				iprintf("\x1b[18;11H| + || - |");
				printf(blanc);
				iprintf("\x1b[18;22H%d    ", arpegPitch3);
				iprintf("\x1b[20;0H Pitch 4 : ");
				printf(vert);
				iprintf("\x1b[20;11H| + || - |");
				printf(blanc);
				iprintf("\x1b[20;22H%d    ", arpegPitch4);
			} else {
				iprintf("\x1b[10;0H IA Arpeggiator " );
				printf(vert);
				iprintf("\x1b[10;17H| Off |" );
				printf(blanc);
				for (int i=0;i<=8;i++) {
					iprintf("\x1b[%d;0H                             ", 12+i);
				}				
			}
			printf(blanc);
		} else {
			consoleSelect(&bottomScreen);
			if (changedPage == false) {
				for(int i = 1; i < 24; i++) {
					iprintf("\x1b[%d;0H                                ", i);
				}
				changedPage = true;
			}
			iprintf("\x1b[2;0HSync :         (track 11)" );
			printf(cyan);
			iprintf("\x1b[2;25H | => |" );
			printf(blanc);
			if (syncDSMode == true) {
				printf(vert);
				iprintf("\x1b[2;7H| On  |" );
				printf(blanc);
				iprintf("\x1b[4;0HWorks with Korg machines");
			} else {
				iprintf("\x1b[4;0H                                        ");
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
				iprintf("\x1b[14;0HB+Up/Down = change all samples");
				iprintf("\x1b[15;0H                              ");
			} else {
				iprintf("\x1b[11;0HSuper live :" );
				printf(vert);
				iprintf("\x1b[11;13H| Off |" );
				printf(blanc);
				iprintf("\x1b[13;0HR/L = select pattern          ");
				iprintf("\x1b[14;0HB+Up/Down = change one sample ");
				iprintf("\x1b[15;0HSelect+R/L = menu           ");
			}
			iprintf("\x1b[17;0HIA limit:");
			printf(vert);
			iprintf("\x1b[17;10H| + || - |");
			printf(blanc);
			iprintf("\x1b[17;21H%d  ", IALimit);
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
				iprintf("\x1b[23;12H| On  | | 3 || 2 |" );
			} else {
				iprintf("\x1b[23;12H| Off |           " );
			}
			page2 = false;
			changedPage = false;
		}
		printf(blanc);
		consoleSelect(&topScreen);
		bottomPrinted = true;
	}

	if (page2 == false) { //page 1
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

		// + et - du type sync pour choisir le sync volca lsdj etc... - choose sync
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
				if (arpeg == true && superLiveMod == false) {
					arpeg = false;
					iprintf("\x1b[0;17H               ");
					draw_bpm_pattern();
				}
				superLiveMod = !superLiveMod;
				if (superLiveMod == false) {
					iprintf("\x1b[0;17H             ");
					draw_bpm_pattern();
				}
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
					if (numberOfSeq == 2) {
						//dessine la partie bas du seq - draw bottom sequencer part
						less_columns = MAX_COLUMNS;
						less_columns_second_seq = MAX_COLUMNS;
						less_columns_third_seq = MAX_COLUMNS;
						countColonsSeqOne = 0;
						countColonsSeqTwo = 0;
						countColonsSeqFree = 0;
						custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, MAX_COLUMNS);
						iprintf("\x1b[9;0H                        ");
					} else if (numberOfSeq == 3) {
						//dessine la partie bas du seq - draw bottom sequencer part
						less_columns = MAX_COLUMNS;
						less_columns_second_seq = MAX_COLUMNS;
						less_columns_third_seq = MAX_COLUMNS;
						countColonsSeqOne = 0;
						countColonsSeqTwo = 0;
						countColonsSeqFree = 0;	
						custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, MAX_COLUMNS);
						iprintf("\x1b[6;0H                        ");
						iprintf("\x1b[10;0H                        ");
					}
				} else {
					less_columns = MAX_COLUMNS;
					less_columns_second_seq = MAX_COLUMNS;
					less_columns_third_seq = MAX_COLUMNS;

					countColonsSeqOne = 0;
					countColonsSeqTwo = 0;
					countColonsSeqFree = 0;

					custom_redessiner_seq(0, MAX_ROWS, MAX_COLUMNS);
				}
				touchedBtn = true;
			}
		}
		//number of seq 3 polyrhythm
		if ((keys_held & KEY_TOUCH) && touch.px >= 166 && touch.px <= 198 && touch.py >= 184 && touch.py <= 191)
		{
			if (touchedBtn == false) {
				numberOfSeq = 3;
				less_columns = MAX_COLUMNS;
				less_columns_second_seq = MAX_COLUMNS;
				less_columns_third_seq = MAX_COLUMNS;
				countColonsSeqOne = 0;
				countColonsSeqTwo = 0;
				countColonsSeqFree = 0;
				//dessine la partie bas du seq - draw bottom sequencer
				custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, MAX_COLUMNS);
				iprintf("\x1b[6;0H                        ");
				iprintf("\x1b[9;0H06   ---- ---- ---- ----");
				printf(jaune);
				iprintf("\x1b[9;4HC");
				printf(blanc);
				iprintf("\x1b[10;0H                        ");
				touchedBtn = true;
			}
		}
		//number of seq 2 polyrhythm
		if ((keys_held & KEY_TOUCH) && touch.px >= 206 && touch.px <= 238 && touch.py >= 184 && touch.py <= 191)
		{
			if (touchedBtn == false) {
				numberOfSeq = 2;
				less_columns = MAX_COLUMNS;
				less_columns_second_seq = MAX_COLUMNS;
				less_columns_third_seq = MAX_COLUMNS;
				countColonsSeqOne = 0;
				countColonsSeqTwo = 0;
				countColonsSeqFree = 0;	
				//dessine la partie bas du seq - draw bottom sequencer
				custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, MAX_COLUMNS);
				iprintf("\x1b[9;0H                        ");
				iprintf("\x1b[6;0H03   ---- ---- ---- ----");
				printf(jaune);
				iprintf("\x1b[6;4HC");
				printf(blanc);
				touchedBtn = true;
			}
		}	
	} else if (page2 == true) { //page 2
		//IA Change sample
		if ((keys_held & KEY_TOUCH) && touch.px >= 141 && touch.px <= 189 && touch.py >= 15 && touch.py <= 25)
		{
			if (touchedBtn == false) {
				IAChangeSample = !IAChangeSample;	
				touchedBtn = true;
			}
		}
		//IA Change sample each number +
		if ((keys_held & KEY_TOUCH) && touch.px >= 53 && touch.px <= 85 && touch.py >= 31 && touch.py <= 41)
		{
			if (touchedBtn == false) {
				IAChangeSampleNumber++;	
				touchedBtn = true;
			}
		}
		//IA Change sample each number -
		if ((keys_held & KEY_TOUCH) && touch.px >= 93 && touch.px <= 125 && touch.py >= 31 && touch.py <= 41)
		{
			if (touchedBtn == false) {
				if (IAChangeSampleNumber > 1) {
					IAChangeSampleNumber--;	
				}
				touchedBtn = true;
			}
		}
		//IA Change pitch
		if ((keys_held & KEY_TOUCH) && touch.px >= 141 && touch.px <= 189 && touch.py >= 47 && touch.py <= 58)
		{
			if (touchedBtn == false) {
				IAChangePitch = !IAChangePitch;	
				touchedBtn = true;
			}
		}
		//IA Change pitch each number +
		if ((keys_held & KEY_TOUCH) && touch.px >= 53 && touch.px <= 85 && touch.py >= 63 && touch.py <= 72)
		{
			if (touchedBtn == false) {
				IAChangePitchNumber++;	
				touchedBtn = true;
			}
		}
		//IA Change pitch each number -
		if ((keys_held & KEY_TOUCH) && touch.px >= 93 && touch.px <= 125 && touch.py >= 63 && touch.py <= 72)
		{
			if (touchedBtn == false) {
				if (IAChangePitchNumber > 1) {
					IAChangePitchNumber--;	
				}
				touchedBtn = true;
			}
		}
		//Arpeggiator
		if ((keys_held & KEY_TOUCH) && touch.px >= 141 && touch.px <= 189 && touch.py >= 79 && touch.py <= 90)
		{
			if (touchedBtn == false) {
				if (superLiveMod == true && arpeg == false) {
					superLiveMod = false;
					iprintf("\x1b[0;17H             ");
					draw_bpm_pattern();
				}
				arpeg = !arpeg;
				touchedBtn = true;
			}
		}

		//Arpeggiator Change pitch1 +
		int numberArpegChangePitch = 50;
		if ((keys_held & KEY_TOUCH) && touch.px >= 93 && touch.px <= 125 && touch.py >= 111 && touch.py <= 121)
		{
			if (touchedBtn == false) {
				arpegPitch1+=numberArpegChangePitch;
				consoleSelect(&bottomScreen);
				iprintf("\x1b[14;22H%d    ", arpegPitch1);
				touchedBtn = true;
			}
		}
		//Arpeggiator Change pitch1 -
		if ((keys_held & KEY_TOUCH) && touch.px >= 133 && touch.px <= 165 && touch.py >= 111 && touch.py <= 121)
		{
			if (touchedBtn == false) {
				if (arpegPitch1 > numberArpegChangePitch) {
					arpegPitch1-=numberArpegChangePitch;
					consoleSelect(&bottomScreen);
					iprintf("\x1b[14;22H%d    ", arpegPitch1);
				}
				touchedBtn = true;
			}
		}
		//Arpeggiator Change pitch2 +
		if ((keys_held & KEY_TOUCH) && touch.px >= 93 && touch.px <= 125 && touch.py >= 128 && touch.py <= 136)
		{
			if (touchedBtn == false) {
				arpegPitch2+=numberArpegChangePitch;	
				touchedBtn = true;
			}
		}
		//Arpeggiator Change pitch2 -
		if ((keys_held & KEY_TOUCH) && touch.px >= 133 && touch.px <= 165 && touch.py >= 128 && touch.py <= 136)
		{
			if (touchedBtn == false) {
				if (arpegPitch2 > numberArpegChangePitch) {
					arpegPitch2-=numberArpegChangePitch;	
				}
				touchedBtn = true;
			}
		}//Arpeggiator Change pitch3 +
		if ((keys_held & KEY_TOUCH) && touch.px >= 93 && touch.px <= 125 && touch.py >= 143 && touch.py <= 153)
		{
			if (touchedBtn == false) {
				arpegPitch3+=numberArpegChangePitch;	
				touchedBtn = true;
			}
		}
		//Arpeggiator Change pitch3 -
		if ((keys_held & KEY_TOUCH) && touch.px >= 133 && touch.px <= 165 && touch.py >= 143 && touch.py <= 153)
		{
			if (touchedBtn == false) {
				if (arpegPitch3 > numberArpegChangePitch) {
					arpegPitch3-=numberArpegChangePitch;	
				}
				touchedBtn = true;
			}
		}//Arpeggiator Change pitch4 +
		if ((keys_held & KEY_TOUCH) && touch.px >= 93 && touch.px <= 125 && touch.py >= 159 && touch.py <= 168)
		{
			if (touchedBtn == false) {
				arpegPitch4+=numberArpegChangePitch;	
				touchedBtn = true;
			}
		}
		//Arpeggiator Change pitch4 -
		if ((keys_held & KEY_TOUCH) && touch.px >= 133 && touch.px <= 165 && touch.py >= 159 && touch.py <= 168)
		{
			if (touchedBtn == false) {
				if (arpegPitch4 > numberArpegChangePitch) {
					arpegPitch4-=numberArpegChangePitch;	
				}
				touchedBtn = true;
			}
		}
	}
	//Page 2
	if ((keys_held & KEY_TOUCH) && touch.px >= 213 && touch.px <= 253 && touch.py >= 16 && touch.py <= 26)
	{
		if (touchedBtn == false) {
			page2 = !page2;	
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

		if ( keys_pressed & KEY_A ) {	
			if (keys_held & KEY_B ) { 		
				copy_pattern();
				randomize_pattern();
				randVolOne = 0;
				randVolTwo = 0;
				if (polyRhythm == false) {
					custom_redessiner_seq(0, MAX_ROWS, less_columns);
				} else {
					custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, less_columns);
				}				
			} 
		}

		if ( keys_pressed & KEY_LEFT ) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			
			if (keys_held & KEY_A ) {       //depitch by 1 semitone
				if (cursorPan == true && (cursor_x == 16 || cursor_x == 0 || cursor_x == 15)) { // colonne 1, PAN
					panChange--;
					if (panChange < 0) {
						panChange = 2;
					}							
					current_song->channels[cursor_y].pan = trackPan[panChange];
					panTab[cursor_y] = trackPan[panChange];
					printf(jaune);
					if (trackPan[panChange] == 128) { // centre
						iprintf("\x1b[%d;26HCenter", cursor_y+3);
					} 
					else if (trackPan[panChange] == 0) { // left
						iprintf("\x1b[%d;26HLeft  ", cursor_y+3);
					} else if (trackPan[panChange] == 255) { // right
						iprintf("\x1b[%d;26HRight ", cursor_y+3);
					}
					printf(blanc);
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
				if (polyRhythm == true && numberOfSeq == 2) {

					if (cursor_y >= 0 && cursor_y <= 5 ) {
						if (less_columns > 0) {
							less_columns--;
							for (int i=0; i<=5;i++) {
								if (less_columns == 11) {
									countColonsSeqOne = 1;
								}
								if (less_columns == 7) {
									countColonsSeqOne = 2;						
								}
								if (less_columns == 3) {
									countColonsSeqOne = 3;						
								}
								//dessine un espace vide quand je fais B <-
								//draw an empty space when press B+left
								iprintf("\x1b[%d;%dH ", i+3, (less_columns-countColonsSeqOne)+8);
							}
						}
					} else {
						if (less_columns_second_seq > 0) {
							less_columns_second_seq--;
							for (int i=7; i<=11;i++) {
								if (less_columns_second_seq == 11) {
									countColonsSeqTwo = 1;
								}
								if (less_columns_second_seq == 7) {
									countColonsSeqTwo = 2;						
								}
								if (less_columns_second_seq == 3) {
									countColonsSeqTwo = 3;						
								}
								//dessine un espace vide quand je fais B <-
								//draw an empty space when press B+left
								iprintf("\x1b[%d;%dH ", i+3, (less_columns_second_seq-countColonsSeqTwo)+8);
							}
						}
					}
				} 
				else if (polyRhythm == true && numberOfSeq == 3) {

					if (cursor_y >= 0 && cursor_y <= 2 ) { //seq1
						if (less_columns > 0) {
							less_columns--;
							for (int i=0; i<=2;i++) {
								if (less_columns == 11) {
									countColonsSeqOne = 1;
								}
								if (less_columns == 7) {
									countColonsSeqOne = 2;						
								}
								if (less_columns == 3) {
									countColonsSeqOne = 3;						
								}
								//dessine un espace vide quand je fais B <-
								//draw an empty space when press B+left
								iprintf("\x1b[%d;%dH ", i+3, (less_columns-countColonsSeqOne)+8);
							}
						}
					} else if (cursor_y >= 4 && cursor_y <= 6) { //seq2
						if (less_columns_second_seq > 0) {
							less_columns_second_seq--;
							for (int i=4; i<=6;i++) { //seq2
								if (less_columns_second_seq == 11) {
									countColonsSeqTwo = 1;
								}
								if (less_columns_second_seq == 7) {
									countColonsSeqTwo = 2;						
								}
								if (less_columns_second_seq == 3) {
									countColonsSeqTwo = 3;						
								}
								//dessine un espace vide quand je fais B <-
								//draw an empty space when press B+left
								iprintf("\x1b[%d;%dH ", i+3, (less_columns_second_seq-countColonsSeqTwo)+8);
							}
						}
					} else if (cursor_y >= 8 && cursor_y <= 11) { //seq3
						if (less_columns_third_seq > 0) {
							less_columns_third_seq--;
							for (int i=8; i<=11;i++) {
								if (less_columns_third_seq == 11) {
									countColonsSeqFree = 1;
								}
								if (less_columns_third_seq == 7) {
									countColonsSeqFree = 2;						
								}
								if (less_columns_third_seq == 3) {
									countColonsSeqFree = 3;						
								}
								//dessine un espace vide quand je fais B <-
								//draw an empty space when press B+left
								iprintf("\x1b[%d;%dH ", i+3, (less_columns_third_seq-countColonsSeqFree)+8);
							}
						}
					}
				} 
				
				//seq (seq1) quand polyrhythm est à false - seq when polyrhythm is false
				else {
					if (less_columns > 0) {
						less_columns--;
						for (int i=0; i<MAX_ROWS;i++) {
							if (less_columns == 11) {
								countColonsSeqOne = 1;
							}
							if (less_columns == 7) {
								countColonsSeqOne = 2;						
							}
							if (less_columns == 3) {
								countColonsSeqOne = 3;						
							}
							iprintf("\x1b[%d;%dH ", i+3, (less_columns-countColonsSeqOne)+8);
						}

					}	
				}
			} else {
				if (cursor_x == 0) {
					cursorPan = true;
					printf(rouge);
					iprintf("\x1b[%d;4HX", cursor_y+3);
					printf(blanc);
					iprintf("\x1b[%d;5H-", cursor_y+3);
					cursor_x = 16; //16 c'est -1 mais ça évite un bug graphique
					printf(vert);
					if (pv->random == true) {
						iprintf("\x1b[%d;5HR", cursor_y+3);
					}
					else if (pv->volume > 0) {
						if (pv->volume == 1) {
							iprintf("\x1b[%d;5Ho", cursor_y+3);
						} else if (pv->volume == 2) {
							iprintf("\x1b[%d;5HO", cursor_y+3);
						}
						if (pv->optionsUp > 3) {
							iprintf("\x1b[%d;5H%x", cursor_y+3, pv->optionsUp-2);
						}
					}
					printf(blanc);
				}
				else {
					if (cursor_x == 16) { //HC"
						printf(jaune);
						if (panTab[cursor_y] == 128) { // centre
							iprintf("\x1b[%d;4HC", cursor_y+3);
						} else if (panTab[cursor_y] == 0) { // left
							iprintf("\x1b[%d;4HL", cursor_y+3);
						} else if (panTab[cursor_y] == 255) { // right
							iprintf("\x1b[%d;4HR", cursor_y+3);
						}
						printf(blanc);
						iprintf("\x1b[%d;25H]", cursor_y+3);
					}
					if (cursor_x == 1 && cursorPan == true) {
						printf(rouge);
						iprintf("\x1b[%d;5HX", cursor_y+3);
						printf(blanc);						
					}
					if (cursorPan == true || cursor_x != 0) {
						move_cursor(-1,0);
					}
				}
				if ((cursor_x < 1 || cursor_x == 16 || cursor_x == 15 && cursor_y == 6) && (polyRhythm == true && numberOfSeq == 2)) {
					iprintf("\x1b[9;4H  ");									
				}
			}
		}
				
		if ( keys_pressed & KEY_RIGHT ) {		
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			if (keys_held & KEY_A ) {
				if (cursorPan == true && (cursor_x == 16 || cursor_x == 0 || cursor_x == 15)) { // colonne 1, PAN
					panChange++;
					if (panChange > 2) {
						panChange = 0;
					}							
					current_song->channels[cursor_y].pan = trackPan[panChange];
					panTab[cursor_y] = trackPan[panChange];
					printf(jaune);
					if (trackPan[panChange] == 128) { // centre
						iprintf("\x1b[%d;26HCenter", cursor_y+3);
					} 
					else if (trackPan[panChange] == 0) { // left
						iprintf("\x1b[%d;26HLeft  ", cursor_y+3);
					} else if (trackPan[panChange] == 255) { // right
						iprintf("\x1b[%d;26HRight ", cursor_y+3);
					}
					printf(blanc);
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
				if (polyRhythm == true && numberOfSeq == 2) {
					if (cursor_y >= 0 && cursor_y <= 5 ) {
						if (less_columns < 16) {
							less_columns++;

							for (int i=0; i<=5;i++) {
								if (less_columns == 11) {
									countColonsSeqOne = 1;
								} 
								if (less_columns == 12) {
									countColonsSeqOne = 1;						
								}
								if (less_columns == 13) {
									countColonsSeqOne = 0;						
								}
								if (less_columns == 9) {
									countColonsSeqOne = 1;						
								}
								if (less_columns == 5) {
									countColonsSeqOne = 2;						
								}
								iprintf("\x1b[%d;%dH-", i+3, less_columns+(7-countColonsSeqOne));				
							}
						}
					} else {
						if (less_columns_second_seq < 16) {
							less_columns_second_seq++;

							for (int i=7; i<=11;i++) {
								if (less_columns_second_seq == 11) {
									countColonsSeqTwo = 1;
								} 
								if (less_columns_second_seq == 12) {
									countColonsSeqTwo = 1;						
								}
								if (less_columns_second_seq == 13) {
									countColonsSeqTwo = 0;						
								}
								if (less_columns_second_seq == 9) {
									countColonsSeqTwo = 1;						
								}
								if (less_columns_second_seq == 5) {
									countColonsSeqTwo = 2;						
								}
								iprintf("\x1b[%d;%dH-", i+3, less_columns_second_seq+(7-countColonsSeqTwo));				
							}
						}
					}
				} 
				else if (polyRhythm == true && numberOfSeq == 3) {
					if (cursor_y >= 0 && cursor_y <= 2 ) {
						if (less_columns < 16) {
							less_columns++;

							for (int i=0; i<=2;i++) {
								if (less_columns == 11) {
									countColonsSeqOne = 1;
								} 
								if (less_columns == 12) {
									countColonsSeqOne = 1;						
								}
								if (less_columns == 13) {
									countColonsSeqOne = 0;						
								}
								if (less_columns == 9) {
									countColonsSeqOne = 1;						
								}
								if (less_columns == 5) {
									countColonsSeqOne = 2;						
								}
								iprintf("\x1b[%d;%dH-", i+3, less_columns+(7-countColonsSeqOne));				
							}
						}
					} else if ( cursor_y >= 4 && cursor_y <= 6) { //seq2
						if (less_columns_second_seq < 16) {
							less_columns_second_seq++;

							for (int i=4; i<=6;i++) {
								if (less_columns_second_seq == 11) {
									countColonsSeqTwo = 1;
								} 
								if (less_columns_second_seq == 12) {
									countColonsSeqTwo = 1;						
								}
								if (less_columns_second_seq == 13) {
									countColonsSeqTwo = 0;						
								}
								if (less_columns_second_seq == 9) {
									countColonsSeqTwo = 1;						
								}
								if (less_columns_second_seq == 5) {
									countColonsSeqTwo = 2;						
								}
								iprintf("\x1b[%d;%dH-", i+3, less_columns_second_seq+(7-countColonsSeqTwo));				
							}
						}
					} else if ( cursor_y >= 8 && cursor_y <= 11) { //seq3
						if (less_columns_third_seq < 16) {
							less_columns_third_seq++;

							for (int i=8; i<=11;i++) {
								if (less_columns_third_seq == 11) {
									countColonsSeqFree = 1;
								} 
								if (less_columns_third_seq == 12) {
									countColonsSeqFree = 1;						
								}
								if (less_columns_third_seq == 13) {
									countColonsSeqFree = 0;						
								}
								if (less_columns_third_seq == 9) {
									countColonsSeqFree = 1;						
								}
								if (less_columns_third_seq == 5) {
									countColonsSeqFree = 2;						
								}
								iprintf("\x1b[%d;%dH-", i+3, less_columns_third_seq+(7-countColonsSeqFree));				
							}
						}
					}
				} 
				//polythythm = false, seq = normal (seq1)
				else {
					if (less_columns < 16) {
						less_columns++;

						for (int i=0; i<MAX_ROWS;i++) {
							if (less_columns == 11) {
								countColonsSeqOne = 1;
							} 
							if (less_columns == 12) {
								countColonsSeqOne = 1;						
							}
							if (less_columns == 13) {
								countColonsSeqOne = 0;						
							}
							if (less_columns == 9) {
								countColonsSeqOne = 1;						
							}
							if (less_columns == 5) {
								countColonsSeqOne = 2;						
							}
							iprintf("\x1b[%d;%dH-", i+3, less_columns+(7-countColonsSeqOne));				
						}

					}	
				}
			} else {
				//mettre cursor tout à gauche pour panoramique
				//cursor to far left for pan			
				if (cursor_x == 15) {
					iprintf("\x1b[%d;5H-", cursor_y+3);	
				}
				if (cursor_x == 0) {
					printf(jaune);
					if (panTab[cursor_y] == 128) { // centre
						iprintf("\x1b[%d;4HC", cursor_y+3);
					} else if (panTab[cursor_y] == 0) { // left
						iprintf("\x1b[%d;4HL", cursor_y+3);
					} else if (panTab[cursor_y] == 255) { // right
						iprintf("\x1b[%d;4HR", cursor_y+3);
					}	
					printf(blanc);			
				}
				if (cursor_x == 0 && cursorPan == true) {
					cursorPan = false;
					move_cursor(0,0);
				} else {
					move_cursor(1,0);
				}	
				if ((cursor_x <= 1 || cursor_x == 16 || cursor_x == 15 && cursor_y == 6) && (polyRhythm == true && numberOfSeq == 2)) {
					iprintf("\x1b[9;4H  ");									
				}
			}			
		}

		//A perso - additionnal A
		if ( (keys_pressed & KEY_A)) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			if (pv->volume == 0){
				pv->pan = PAN_OFF;
				if (cursorPan == false && (cursor_x != 16 || cursor_x != 0 || cursor_x != 15)) {
					if (polyRhythm == false || (numberOfSeq == 2 && (polyRhythm == true && cursor_y != 6))
						|| numberOfSeq == 3 && (polyRhythm == true && cursor_y != 3)
						|| numberOfSeq == 3 && (polyRhythm == true && cursor_y != 7)) {
						pv->volume = 1;
						pv->optionsUp = 1;
						soundsInCurrentPattern++;
						//j'enregistre si y'a des sons dans ce pattern là ou pas
						//si oui alors il loopera avec les autres pattern
						//if there is samples in this pattern then loop with other patterns
						soundsInPattern[current_pattern_index] = soundsInCurrentPattern;
					}
				}
			} 
			draw_cell_status();
		}

		//uniquement pour la DS
		//only for nintendo DS
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
			else if (keys_held & KEY_B ) {
				if (superLiveMod == false) {
					if (current_song->channels[cursor_y].sample < MAX_SAMPLES-1){
						current_song->channels[cursor_y].sample++;
					} 
					if (current_song->channels[cursor_y].sample == SFX_SYNC) {
						current_song->channels[cursor_y].sample++;
					}
					printf(cyan);
					iprintf("\x1b[%d;2H%02d",cursor_y+3, current_song->channels[cursor_y].sample);
					printf(blanc);
				} else {
					if (changeSample < MAX_SAMPLES-1) {
						changeSample++;
					}
					if (changeSample == SFX_SYNC) {
						changeSample++;
					}
					printf(cyan);
					for (int i = 0; i <= 11; i++) {
						iprintf("\x1b[%d;2H%02d",i+3, current_song->channels[i+3].sample+changeSample);
					}
					printf(blanc);			
				}
			} 
			else {
				if (cursor_x == 16) {
					cursor_x = 0;
				}
				printf(jaune);
				if (polyRhythm == true && numberOfSeq == 3) {
					if (cursor_y != 3 && cursor_y != 7) {
						if (panTab[cursor_y] == 128) { // centre
							iprintf("\x1b[%d;4HC", cursor_y+3);
						} else if (panTab[cursor_y] == 0) { // left
							iprintf("\x1b[%d;4HL", cursor_y+3);
						} else if (panTab[cursor_y] == 255) { // right
							iprintf("\x1b[%d;4HR", cursor_y+3);
						}
					}
				} else if ((polyRhythm == true && numberOfSeq == 2) || polyRhythm == false) {
					if (panTab[cursor_y] == 128) { // centre
						iprintf("\x1b[%d;4HC", cursor_y+3);
					} else if (panTab[cursor_y] == 0) { // left
						iprintf("\x1b[%d;4HL", cursor_y+3);
					} else if (panTab[cursor_y] == 255) { // right
						iprintf("\x1b[%d;4HR", cursor_y+3);
					}
				}

				printf(blanc);
				if (cursor_x == 0 && cursor_y != 0 && cursorPan == true) {
					if ((polyRhythm == true && numberOfSeq == 2) || polyRhythm == false) {
						printf(rouge);
						iprintf("\x1b[%d;4HX", cursor_y+2);
						printf(blanc);
					} else if (polyRhythm == true && numberOfSeq == 3) {
						if (cursor_y != 4 && cursor_y != 8) {
							printf(rouge);
							iprintf("\x1b[%d;4HX", cursor_y+2);
							printf(blanc);
						}
					}
				}
				else if (cursor_y == 0 && cursorPan == true) {
					printf(rouge);
					iprintf("\x1b[14;4HX");
					printf(blanc);					
				}
				if (cursor_y == 11) {
					printf(jaune);
					if (panTab[cursor_y-1] == 128) { // centre - center
						iprintf("\x1b[14;4HC");
					} else if (panTab[cursor_y] == 0) { // left
						iprintf("\x1b[14;4HL");
					} else if (panTab[cursor_y] == 255) { // right
						iprintf("\x1b[14;4HR");
					}
					printf(blanc);	
				}
				if (cursor_y == 6 && (polyRhythm == true && numberOfSeq == 2)) {
					iprintf("\x1b[9;4H ");								
				}
				move_cursor(0,-1);
			}
			set_bpm_to_millis();
			update_timers();
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);			
			if (keys_held & KEY_B ) {
				if (superLiveMod == false) {
					if (current_song->channels[cursor_y].sample > 0){
						current_song->channels[cursor_y].sample--;
					}
					if (current_song->channels[cursor_y].sample == SFX_SYNC) {
						current_song->channels[cursor_y].sample--;
					}
					printf(cyan);
					iprintf("\x1b[%d;2H%02d",cursor_y+3, current_song->channels[cursor_y].sample);
					printf(blanc);
				} else {
					if (changeSample > 0) {
						changeSample--;
					}
					if (changeSample == SFX_SYNC) {
						changeSample--;
					}
					printf(cyan);
					for (int i = 0; i <= 11; i++) {
						iprintf("\x1b[%d;2H%02d",i+3, current_song->channels[i+3].sample+changeSample);
					}
					printf(blanc);	
				}
			} else 
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
			else {
				//pan = cursor+1
				//precedant one is normal
				if (cursor_x == 16) {
					cursor_x = 0;
				}
				printf(jaune);
				if (polyRhythm == true && numberOfSeq == 3) {
					if (cursor_y != 3 && cursor_y != 7) {
						if (panTab[cursor_y] == 128) { // centre
							iprintf("\x1b[%d;4HC", cursor_y+3);
						} else if (panTab[cursor_y] == 0) { // left
							iprintf("\x1b[%d;4HL", cursor_y+3);
						} else if (panTab[cursor_y] == 255) { // right
							iprintf("\x1b[%d;4HR", cursor_y+3);
						}
					}
				} else if ((polyRhythm == true && numberOfSeq == 2) || polyRhythm == false) {				
					if (panTab[cursor_y] == 128) { // centre
						iprintf("\x1b[%d;4HC", cursor_y+3);
					} else if (panTab[cursor_y] == 0) { // left
						iprintf("\x1b[%d;4HL", cursor_y+3);
					} else if (panTab[cursor_y] == 255) { // right
						iprintf("\x1b[%d;4HR", cursor_y+3);
					}
				}
				printf(blanc);
				if (cursor_x == 0 && cursor_y !=11 && cursorPan == true) {
					if ((polyRhythm == true && numberOfSeq == 2) || polyRhythm == false) {
						printf(rouge);
						iprintf("\x1b[%d;4HX", cursor_y+4);
						printf(blanc);
					} else if (polyRhythm == true && numberOfSeq == 3) {
						if (cursor_y != 2 && cursor_y != 6) {
							printf(rouge);
							iprintf("\x1b[%d;4HX", cursor_y+4);
							printf(blanc);
						}
					}
				}
				if (cursor_y == 11) {
					printf(jaune);
					if (panTab[cursor_y-1] == 128) { // centre
						iprintf("\x1b[14;4HC");
					} else if (panTab[cursor_y] == 0) { // left
						iprintf("\x1b[14;4HL");
					} else if (panTab[cursor_y] == 255) { // right
						iprintf("\x1b[14;4HR");
					}
					if (cursorPan == true) {
						printf(blanc);
						printf(rouge);
						iprintf("\x1b[3;4HX");
						printf(blanc);
					}
				}
				if (cursor_y == 6 && (polyRhythm == true && numberOfSeq == 2)) {
					iprintf("\x1b[9;4H ");				
				}	
				move_cursor(0,1);
			}
			set_bpm_to_millis();
			update_timers();
			//todo si je met draw pattern
			//les r disparaissent bizarrement
			//idem pour le draw pattern plus bas du down
			// draw_pattern();
			// if draw_pattern() then r disappears.
		}
		
		if ( (keys_pressed & KEY_START || keys_pressed & KEY_SELECT)) {		
			if (play == PLAY_STOPPED){ 
				play_pattern();
				pas = 0;
				tours = 0;
				step = 0;
				iaChangeRandom = false;
				iaPreviousPatern = false;
			}
			else {
				stop_song();
				changedPatternOne = 0;
				step = 0;
				tours = 0;
			}
		}
					
		//steel in screen PATTERN
		//si y'a des sons dan le pattern je le loop avec
		//les autres pattern dans lesquels il y a des sons
		//if there is samples in pattern, then loop with other patterns where there is samples
		if (autoLoopPattern == true) {
			if (step == 3 || step == 7) {
				changedPatternOne = 0;
			}
			if (tours % 4 == 0) { //ça joue tous les 4 temps - it plays 1/4 time
				if (soundsInPattern[current_pattern_index+1] > 0) {
					if (changedPatternOne == 0) {
						current_pattern_index = current_pattern_index+1;
						draw_pattern();
						changedPatternOne = 1;
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
			if (arpeg == true) {
				if ( keys_pressed & KEY_R ) {
					arpegEcartement++;
					if (arpegEcartement >= 16) {
						arpegEcartement = 0;
					}
					printf(jaune);
					iprintf("\x1b[0;24HEcart %d  ", arpegEcartement);
					printf(blanc);
				}
				if ( keys_pressed & KEY_L ) {
					if (arpegEcartement > 0) {
						arpegEcartement--;
						printf(jaune);
						iprintf("\x1b[0;24HEcart %d  ", arpegEcartement);
						printf(blanc);
					}
				}
			} else {
				if ( keys_pressed & KEY_R ) {
					if (keys_held & KEY_SELECT ) {
						current_screen = SCREEN_SETTINGS; 
						draw_screen();					
					}
					else {
						if (current_pattern_index < (MAX_PATTERNS-1)) {
							current_pattern_index+=1;
						}
						if (polyRhythm == false) {
							custom_redessiner_seq(0, MAX_ROWS, less_columns);
						} else {
							custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, less_columns);
						}
					}			
				}
				if ( keys_pressed & KEY_L ) {
					if (current_pattern_index > 0) {
						current_pattern_index-=1;				
					}
					if (polyRhythm == false) {
						custom_redessiner_seq(0, MAX_ROWS, less_columns);
					} else {
						custom_redessiner_all_seq_polyrhythm(0, MAX_ROWS, less_columns);
					}
				}
			}
		} else {
			if ( keys_pressed & KEY_R ) {
				if (globalPitch < 4000) {
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

		//B delete note
		if ( (keys_pressed & KEY_B)) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
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
			} else { //stop playing
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

		if (superLiveMod == false) {
			if ( keys_pressed & KEY_L ) {
				if ( keys_held & KEY_SELECT ) {
					current_screen = SCREEN_PATTERN;
					//efface les résiduts de lettres - delete letters
					iprintf("\x1b[2;0H                              ");
					draw_screen();
				}
			}
		}
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

	REG_IF = IRQ_SPI; //apparently to 'reset' the interrupt?
}

void setup_interrupts(){
	irqSet( IRQ_VBLANK, vblank_interrupt );
	irqSet( IRQ_TIMER3, timer3_interrupt );
	irqEnable(IRQ_VBLANK | IRQ_TIMER3);
}


void setup_display(){
	define_palettes(current_song->color_mode);
}

int main() {
	current_song = (song *) malloc(sizeof(song));
	init_song();
	setup_timers();
	
	setup_interrupts();

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	consoleDemoInit();

	mmInitDefaultMem((mm_addr)soundbank_bin);
	
	SetYtrigger( 0 );
	irqEnable( IRQ_VCOUNT );	

	//GLOBAL VARS
	cursor_x = 0;
	cursor_y = 0;
	current_screen = SCREEN_PATTERN;
	current_ticks = 0;

	draw_screen();

	consoleSelect(&topScreen); //faut remettre en top pour le reste - it must be back on top then

	while (1) {
		process_input();
	}
}
