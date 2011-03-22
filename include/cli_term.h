void term_init( SDL_Surface *screen );
void term_clear();
void term_draw();
void term_write( unsigned char x, unsigned char y, char *s, unsigned char f );
void term_white( unsigned char x, unsigned char y, unsigned char c );
void term_cins( unsigned char x, unsigned char y );
void term_crem();
int term_knows( char c );