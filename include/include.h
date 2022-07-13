void define_palettes(int palette_option);
void clear_screen(void);
void put_character(char c, u16 x, u16 y, unsigned char palette);
void put_string(char *s, u16 x, u16 y, unsigned char palette);
void set_palette(u16 x, u16 y, unsigned char palette);
void draw_cursor(void);
void move_cursor(int mod_x, int mod_y);
void draw_column_cursor(void);
void clear_last_song_cursor(void);
void draw_song_cursor(void);
void clear_song_cursor(void);
void draw_status(void);
void draw_temp_status(int temp_status);
void draw_pattern(void) __attribute__ ((section(".iwram")));
void draw_song(void);
void draw_samples(void);
void draw_settings(void);
void draw_help(void);
void draw_screen(void);
void setup_display(void);

int name_index;

void samples_menu(int channel_index, int channel_item, int action);
void settings_menu(int menu_option, int action);
void process_input(void);


void enable_serial_interrupt(void);
void disable_serial_interrupt(void);
void vblank_interrupt(void) __attribute__ ((section(".iwram")));
void timer3_interrupt(void) __attribute__ ((section(".iwram")));
void gpio_interrupt(void) __attribute__ ((section(".iwram")));
void setup_interrupts(void);
void setup_timers(void);
void update_timers(void);

void play_sound(int row, int sample, int vol, int tune, int pan);
void play_column(void) __attribute__ ((section(".iwram")));
void set_status(int new_status);
int find_chain_start(int index);
void stop_song(void);
void play_pattern(void);
void play_song(void);
void next_column(void);
int is_empty_pattern(int pattern_index);

void set_bpm_to_millis(void);
int set_shuffle(void);
void init_song(void);
void rand_pattern(void);
void copy_pattern(void);
void paste_pattern(void);
void copy_order(short *from);
void paste_order(short *to);
void set_patterns_data(void);
void set_orders_data(void);
void file_option_save(void);
void file_option_load(void);
