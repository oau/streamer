#ifndef _SAM_QUEUE_H_
#define _SAM_QUEUE_H_

void sam_open();
void sam_poll();
void sam_queue( char* speak );
int  sam_state();
int  sam_vis( char **buffer );

#endif