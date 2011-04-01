enum kb_bitmask_e {
  KB_LEFT  = 1,
  KB_RIGHT = 2,
  KB_UP    = 4,
  KB_DOWN  = 8
};

// DISP packet
typedef struct {
  unsigned char trust_srv;
  unsigned char trust_cli;
  int timer;
} disp_data_t;

// Control data
typedef struct {
  long mx;
  long my;
  unsigned char kb; // kb_bitmask_e
} ctrl_t;

// DATA packet
typedef struct {
  unsigned char trust_srv;
  unsigned char trust_cli;
  ctrl_t ctrl;
} ctrl_data_t;

// Linked buffer
struct linked_buf_t {
  char data[ 8192 ];
  int size;
  struct linked_buf_t *next;
};
typedef struct linked_buf_t linked_buf_t;