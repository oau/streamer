typedef struct {
  unsigned char trust_srv;
  unsigned char trust_cli;
  int timer;
} disp_data_t;

typedef struct {
  long mx;
  long my;
  unsigned char kb;
} ctrl_t;

typedef struct {
  unsigned char trust_srv;
  unsigned char trust_cli;
  ctrl_t ctrl;
} ctrl_data_t;

#define KB_LEFT 1
#define KB_RIGHT 2
#define KB_UP 4
#define KB_DOWN 8