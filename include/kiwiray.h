typedef struct {
  unsigned char extra_id;
  int timer;
} disp_data_t;

typedef struct {
  long ctrl_mx;
  long ctrl_my;
  unsigned char ctrl_kb;
} ctrl_data_t;

#define KB_LEFT 1
#define KB_RIGHT 2
#define KB_UP 4
#define KB_DOWN 8