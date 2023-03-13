
void ControlPanelInit(int key_interrupt_level_);
void ControlPanelInitAddr(char * adresse,int key_interrupt_level_);
void lcd_putc(char c);
void lcd_str(char * s);
void lcd_com(char c);
void led_j(int on_off);
void led_r(int on_off);
void led_v(int on_off);
int KeyboardGetLastKey(void);
void KeyboardSetLastKey(char key);

extern unsigned short ad00,ad01;
extern QueueHandle_t QdmTouche;
extern unsigned char BP_Gr,BP_Mr,BP_Dr;
extern unsigned char BP_Gs,BP_Ms,BP_Ds;
#define BP_G (BP_Gr | BP_Gs)
#define BP_D (BP_Dr | BP_Ds)
#define BP_M (BP_Mr | BP_Ms)
extern int last_key;
