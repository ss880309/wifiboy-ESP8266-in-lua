/*
 *  Space Worm Battle Game Demo -- WiFiBoy32 Demo Code for Arduino/ESP32
 *  
 *  Make a game to learn WiFi battle & Sin/Cos math for junior high school students
 *  
 *  Nov 11, 2017 Created. (derek@wifiboy.org & ricky@wifiboy.org)
 *
 */
#include <WiFi.h>
#include <wifiboy32.h>
#include <math.h>
#include "wb32_snake8_map.h"

static uint8_t m_x[50], m_v[50], m_c[50]; // starfields
static uint16_t m_y[50];                  // starfields
static uint8_t game_mode;  // 0: single player 1: server 2: client
static float x[36], y[36], r, deg, dstep, ax, ay, score1, score2;
static int16_t ix[36], iy[36];
static uint16_t sx, sy, px, py, si, lastkey, tx[100], ty[100], sn;
static char buf[20];


static uint16_t sfx[8][8]={ // Sound Effects (freq seq)
  {0},
  {523, 0, 0, 0, 0, 0, 0, 0},
  {262, 330, 0, 0, 0, 0, 0, 0},
  {262, 330, 392, 0, 0, 0, 0, 0},
  {262, 330, 392, 523, 660, 0, 0, 0},
  {262, 330, 392, 523, 660, 784, 1047, 0},
  {262, 392, 330, 262, 16, 165, 131, 0},
  {1047,0,0,0,0,0,0,0}
};
static uint16_t task_c, sfx_on, sfxn, sfxc, freq;

void sfx_engine() // multi-tasking sound effect engine
{
  if (sfx_on) {
    if ((++task_c%2)==0) {      // sfx channel
      if (sfxn!=0) {            // sfxn = sound effect#
        freq=sfx[sfxn][sfxc];   // get freq
        if (freq) {
          ledcSetup(1, freq, 8);// ledc PWM
          ledcWrite(1, 30);     // duty=30
        } else ledcWrite(1, 0); // duty=0 (mute)
        sfxc++;
        if (sfxc>7) sfxn=0;     // stop sound effect
      } 
    }
  }
}

void ticker_setup() // multitasking ticker engine
{
  wb32_tickerInit(20000, sfx_engine);   // 20000ns=20ms
  ledcAttachPin(25, 1);                 // GPIO25=speaker
  ledcSetup(1, 200, 8);                 // 8bit resolution is enough
  sfx_on=1;                             // sfx_on=1 (or 0=off)
  sfxn=0;                               // sfxn=0 is no sfx#
  task_c=0;                             // task counter (divider)
}

// make a show string function with our demo spritemap
void blit_str256(const char *str, int x, int y)
{
  for(int i=0; i<strlen(str); i++) {
    if (str[i]>='@'&& str[i]<=']') 
      wb32_blitBuf8(8*(str[i]-'@'),0,240, x+i*8, y, 8, 8, (uint8_t *)sprites);
    if (str[i]>='!'&& str[i]<='>') 
      wb32_blitBuf8(8*(str[i]-'!'),8,240, x+i*8, y, 8, 8, (uint8_t *)sprites);
    if (str[i]=='?') wb32_blitBuf8(8*14,16,240, x+i*8, y, 8, 8, (uint8_t *)sprites);
    if (str[i]=='c') wb32_blitBuf8(8*13,16,240, x+i*8, y, 8, 8, (uint8_t *)sprites);
    if (str[i]=='w') wb32_blitBuf8(7,16,240, x+i*8, y, 26, 8, (uint8_t *)sprites);
    if (str[i]=='x') wb32_blitBuf8(42,16,240, x+i*8, y, 61, 8, (uint8_t *)sprites);
  }
}

// make a show number function with our demo spritemap
void blit_num256(uint16_t num, uint16_t x, uint16_t y, uint8_t color_mode)
{
  uint16_t  d[5]; // five digit number
    
  d[0]=num/10000;
  d[1]=(num-d[0]*10000)/1000;
  d[2]=(num-d[0]*10000-d[1]*1000)/100;
  d[3]=(num-d[0]*10000-d[1]*1000-d[2]*100)/10;
  d[4]=num-d[0]*10000-d[1]*1000-d[2]*100-d[3]/10;
    
  for(int i=0; i<5; i++) {
    wb32_blitBuf8(d[i]*8+120, color_mode*8, 240, x+i*8, y, 8, 8, (uint8_t *)sprites);
  }
}

WiFiServer server(1234);  // we have port 1234 for our game server
WiFiClient client;        // connected client for both side

void setup() {

  wb32_init(); // init WiFiBoy32 engine
  wb32_setTextColor(wbCYAN, wbCYAN);
  
  ticker_setup(); // init Ticker tasks
   
  pinMode(17,INPUT); pinMode(32,INPUT); pinMode(34,INPUT); pinMode(35,INPUT);

  game_mode=0; // single player
  
  if (digitalRead(32)==0) { // two player battle (hold L or R when power-on)
    // init server mode
    wb32_drawString("PLAYER-1", 75, 120, 2, 2);
    WiFi.softAP("Snaker", "12345678");
    server.begin();
    game_mode=1; // server mode
    while(1) {
      client = server.available();
      if (client) break;
    }
    sfxn=2;sfxc=0;
  } else if (digitalRead(17)==0) { 
    // init client mode
    wb32_drawString("PLAYER-2", 75, 120, 2, 2);
    WiFi.begin("Snaker", "12345678");
    while(WiFi.status()!=WL_CONNECTED)delay(500);
    client.connect("192.168.4.1", 1234);
    while(!client.connected());
    sfxn=3;sfxc=0;
    game_mode=2; // client mode
  }

  wb32_initBuf8(); // prepare 76800-byte off-screen buffer
  
  for(int i=0; i<256; i++) // setup palette (8bit = 256 colors)
    wb32_setPal8(i, wb32_color565(sprite_pal[i][0],sprite_pal[i][1],sprite_pal[i][2]));

  for(int i=0; i<50; i++) { // setup starfields = 50 random moving stars
    m_x[i]=random(0,240); m_y[i]=random(0,320);
    m_v[i]=random(10,25); m_c[i]=random(1,16);
  }

  for(int i=0; i<10; i++) { tx[i]=120; ty[i]=120; } // initial tail position
  sn=8;       // tail number = 8
  deg=0.0;    // init degree
  dstep=0.25; // degree between each tail
  px=120;     // origin (x) of the worm moving circle
  py=120;     // origin (y) of the worm moving circle
  lastkey=0;  // keep key status
  r=40.0;     // radius of the worm moving circle
  score1 = score2 = 0;  // init score=0
  
  ax = random(200)+20;  // init a random apple position x
  ay = random(280)+20;  // init a random apple position y
  
  if (game_mode==1) { // sync the apple position
    // send apple pos to client
    sprintf(buf, "ax=%03d ay=%03d", int(ax), int(ay));
    client.print(buf);       
  } else if (game_mode==2) { 
    // get apple pos from server
    while(!client.available());
    uint8_t data[21]; 
    client.read(data, 20);
    ax = (float)((data[3]-48)*100+(data[4]-48)*10+data[5]-48);
    ay = (float)((data[10]-48)*100+(data[11]-48)*10+data[12]-48);
  }
}

void loop() 
{
  float dist;
  uint8_t data[21]; // tcp message buffer

  wb32_clearBuf8(); // clear off-screen buffer
  
  blit_str256("BATTLE-WORM", 10,10);
  blit_str256("GAME DEMO", 10,20);
  if (score1 <2) blit_str256("(PRESS R & B TO CONTROL)", 26,250);
  blit_str256("SCORE", 115,10);
  blit_num256(score1*100, 115, 20, 4);
  blit_str256("c2017 x", 64,300); // c means copyright sign, x means wifiboy.org bitmap
  if (game_mode>0) {
    blit_str256("SCORE2", 175,10);
    blit_num256(score2*100, 175, 20, 5);
  }

  for(int i=0; i<30; i++) { // show starfields
    wb32_setBuf8(m_x[i]+m_y[i]*240, m_c[i]);
    m_y[i] += m_v[i];
    if (m_y[i]>=320) m_y[i]-=320;
  }
  
  deg += dstep;         // moving circle (delta = dstep)
  sx = r * cos(deg)+px; // calculate the worm head position
  sy = r * sin(deg)+py;

  if (sx < 10) px = px + 220; // cross 4 boundaries
  else if (sx > 230) px = px - 220;
  if (sy < 10) py = py + 300;
  else if (sy > 310) py = py - 300;

  sx = r * cos(deg)+px; // re-calculate head position in case of the cross boundary
  sy = r * sin(deg)+py;
  
  for(int j=sn-1; j>0; j--) { // move tails
    tx[j]=tx[j-1];
    ty[j]=ty[j-1];
  }  
  tx[0]=int(sx);  // worm head
  ty[0]=int(sy);

  wb32_blitBuf8(118, 82, 240, tx[0], ty[0], 12, 15, (uint8_t *)sprites); // head
  //wb32_blitBuf8(42, 82, 240, tx[0], ty[0], 12, 15, (uint8_t *)sprites);
  for(int i=1; i<sn; i++) { // draw tails
    wb32_blitBuf8(102, 85, 240, tx[i], ty[i], 10, 10, (uint8_t *)sprites);
  }

  //check message if available
  if (game_mode>0) {
    if (client.available()) {
      client.read(data, 30);
      if (data[0]=='x') { // got a new apple at position ax,ay
        ax = (float)((data[2]-48)*100+(data[3]-48)*10+data[4]-48);
        ay = (float)((data[6]-48)*100+(data[7]-48)*10+data[8]-48);
        score2++; // enemy ate an apple
        sfxn=5;  // generate sound effect #5
        sfxc=0;
      }
    }
  } 
  
  dist = sqrt((ax-sx)*(ax-sx)+(ay-sy)*(ay-sy));
  if (dist < 20) { // got an apple
    ax = random(200)+20; // create next apple position
    ay = random(280)+20;
    sprintf(buf, "x %03d %03d", int(ax), int(ay));
    client.print(buf); // send new apple position to another player
    score1++; // got score
    sfxn=3;  // generate sound effect #3
    sfxc=0;
  }

  //draw apple
  wb32_blitBuf8(65, 82, 240, ax, ay, 16, 16, (uint8_t *)sprites);

  if ((digitalRead(32)==0) && (lastkey==0)) { // key down
    lastkey=1;
    if (dstep>0) {
      dstep = -dstep;           // reverse the moving circle
      px = (sx - px) * 2 + px;  // flip the origin of moving circle 
      py = (sy - py) * 2 + py;  //
      deg = deg + 3.14159265;   // add one pi (180 degree)
    }
  }
  if ((digitalRead(32)==1) && (lastkey==1)) lastkey=0; // key up

  if ((digitalRead(34)==0) && (lastkey==0)) { // key down
    lastkey=2;
    if (dstep<0) {
      dstep = -dstep;
      px = (sx - px) * 2 + px;
      py = (sy - py) * 2 + py;
      deg = deg + 3.14159265;
    }
  }
  if ((digitalRead(34)==1) && (lastkey==2)) lastkey=0; // key up

  wb32_blit8(); // blit to LCD
  delay(50);
}
