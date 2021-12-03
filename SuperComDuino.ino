//////////////////////////////////////////////////////////////////////////////
// Project     : SuperComDuino
// Description : Implementation of the classic 'Supercompter' blinking lights (and more) made famous by Big Clive,
//               using 3rd party multiplexed dot matrix modules
//                 o 16x16 LED Dot Matrix Module sold under the brand duinotech and sold at the electroncis store Jaycar in Australia.
// Contributors : Jaycar electronicx, 28-Nov-2021 Maikshifter
// 
//////////////////////////////////////////////////////////////////
// History:
//   28-Nov-2021 Maikshifter
//     o Used original Jaycar https://github.com/Jaycar-Electronics/Audio-Matrix-Spectrum tutorial as a starting point (ripping out the audio stuff)
//     o Developed and tested using Ardunio UNO R3 compatible.
//     o Uploaded first version to github.

//Duinotech    XC4607  16x16 LED Dot Matrix Module
//   16x16 pixels, multiplexed 16 columns 16 bit word per column
// https://www.jaycar.com.au/arduino-compatible-16x16-led-dot-matrix-module/p/XC4607
//
/*
 

Connection Table

UNO R3 Board   1616LED Matrix   Function
5v             +5v               Power
GND            GND               Ground
D2             LAT               Display Latch
D3             CLK               Display Clock
D4             DI                Display Data
D5             G                 Display Enable
D6             A                 Column Mux Select Bit 0 (1's)  LSB    'Column Multiplexor Register'
D7             B                 Column Mux Select Bit 1 (2's)
D8             C                 Column Mux Select Bit 2 (4's)
D9             D                 Column Mux Select Bit 3 (8's)  MSB

 
D12        PushButton         
 
 */


#define MATRIX1616_LATCH 2 
#define MATRIX1616_CLK   3
#define MATRIX1616_DATA  4
#define MATRIX1616_EN    5
//ROWBIT(1) => Pin 6; change to your set up if different.
#define MATRIX1616_ROWBIT(x) (x+5)


#define PUSHBUTTON_1 12

void matrix1616SendData(uint8_t row, uint16_t bitmask)
{
  digitalWrite(MATRIX1616_EN, HIGH);
  digitalWrite(MATRIX1616_ROWBIT(1),row & 1); // 0001
  digitalWrite(MATRIX1616_ROWBIT(2),row & 2); // 0010
  digitalWrite(MATRIX1616_ROWBIT(3),row & 4); // 0100
  digitalWrite(MATRIX1616_ROWBIT(4),row & 8); // 1000
  digitalWrite(MATRIX1616_LATCH, LOW);

  shiftOut(MATRIX1616_DATA, MATRIX1616_CLK, LSBFIRST, ~((bitmask)      & 0xFF)); //lower portion
  shiftOut(MATRIX1616_DATA, MATRIX1616_CLK, LSBFIRST, ~((bitmask >> 8) & 0xFF)); //higher portion
  digitalWrite(MATRIX1616_LATCH, HIGH);
  digitalWrite(MATRIX1616_EN, LOW);
}


//256 pixels
//each with approx 1sec pulse rate
//randomised drift for each one

uint16_t g_au16Pixels[16];

void GenerateHashPattern(uint8_t bOdds)
{
  uint16_t u16Column = (bOdds!=0) ? 0xAAAA : 0x5555;
  for (int i=0; i<16; i++)
  {
    g_au16Pixels[i] = u16Column;
    u16Column ^= 0xFFFF;
  }
}

//Initial thoughts...
//we could write hash pattern function...with a starting value seed
//then change loop to render from g_au16Pixels
//
//need 256 counters...guess they will be 8 bit...right?

//we have 2k of RAM

int8_t ai8Drift[256];
uint16_t au16Clock[256];

uint8_t g_u8MasterClock;



uint16_t au16FlasherPeriod[] = {500,1000,2000,500};
uint8_t  au8FlasherRandInt[] = {30, 30,  30,  0};
int8_t   ai8FlasherDrift[]   = {16, 24,  24,  16};

//1000 with 0.5ms clock looked cool   +-16

#define MASTER_CLOCK_DIVISOR 125
#define MASTER_CLOCK_US      500

/* old 
 
//Favorite settings
#if 0//500 with 0.5ms clock looked cool    +-16

#define BASE_FLASHER_PERIOD  (uint16_t)500
#define MAX_DRIFT            16 //32 was  too much spread  probably need finer control...at least 0.5ms per clock...

#define RANDOMIZE_INTERVAL  0

#endif

#if 0 //very slow to get going but is quite nice when it finally does - traditional

#define BASE_FLASHER_PERIOD (uint16_t)2000 //500
#define MAX_DRIFT           24

#define RANDOMIZE_INTERVAL  0

#endif

#if 1 //experimental

#define BASE_FLASHER_PERIOD (uint16_t)500
#define MAX_DRIFT           16

#define RANDOMIZE_INTERVAL 30

#endif
*/

//Things to try next...
//  larger drift than 16 so we get less duplicates
//  2x faster flash rate
//  1/2 flash rate
//  re-randomising the entire board every N seconds
//
//other patterns
//  matrix code....
//  mdulating squares
//  3d cube
//  star field


uint8_t g_u8MuxColumn = 0; //0-15

volatile uint16_t g_u16TimerTicks = 0;
volatile uint8_t  g_bMuxDisplay = 0;

//ISR(TIMER0_COMPA_vect) //??ms ticks

ISR(TIMER2_COMPA_vect) //1ms ticks
{
  //g_b8DoMUX595Process = TRUE;
  //or
  //MUX595_Process();
  g_u16TimerTicks++;
  g_bMuxDisplay = 1;
}

void muxLEDMatrix1616();

//with 256 flashers 32 values, 8 flashers will be duplicated %100 in sync
//

void ReRandomiseFlashers(uint8_t u8Type)
{
  int8_t i8Max = ai8FlasherDrift[u8Type];
  int8_t i8Min = -i8Max;
  for (int i=0; i<256; i++)
  {
//    ai8Drift[i] = (int8_t)random(-MAX_DRIFT,MAX_DRIFT);
    ai8Drift[i] = (int8_t)random(i8Min,i8Max);

    if ( g_bMuxDisplay )
      muxLEDMatrix1616();
  }
}





uint8_t g_u8ReRandomiseCounter = 0;

uint16_t g_u16TicksBefore;

void SetupFlashers(uint8_t u8Type)
{
  int8_t i8Max = ai8FlasherDrift[u8Type];
  int8_t i8Min = -i8Max;
  uint16_t u16Base = au16FlasherPeriod[u8Type];
  
  for (int i=0; i<256; i++)
  {
//    ai8Drift[i] = (int8_t)random(-MAX_DRIFT,MAX_DRIFT);
//    au16Clock[i] = BASE_FLASHER_PERIOD + ai8Drift[i];
    ai8Drift[i] = (int8_t)random(i8Min,i8Max);
    au16Clock[i] = u16Base + ai8Drift[i];
  }
  uint16_t u16TicksNow = g_u16TimerTicks;
  g_u16TicksBefore = u16TicksNow;
}

void RunFlashers(uint8_t u8Type)
{
  uint16_t u16TicksNow = g_u16TimerTicks;
  uint16_t u16TicksElapsed;
  if ( u16TicksNow < g_u16TicksBefore )
    u16TicksElapsed = (65535-g_u16TicksBefore)+u16TicksNow;
  else
    u16TicksElapsed = u16TicksNow-g_u16TicksBefore;
  g_u16TicksBefore = u16TicksNow;
  //
  uint16_t u16Base = au16FlasherPeriod[u8Type];
  uint8_t u8ReRandInt = au8FlasherRandInt[u8Type];
  for (int i=0; i<256; i++)
  {
    uint16_t u16Clock = au16Clock[i];
    if ( u16Clock <= u16TicksElapsed )
    {
      u16Clock = u16TicksElapsed - u16Clock; //overflow
      //u16Clock += BASE_FLASHER_PERIOD + ai8Drift[i];
      u16Clock += u16Base + ai8Drift[i];
      au16Clock[i] = u16Clock;
      
      uint8_t col = i>>4;
      uint8_t row = i&15;
      g_au16Pixels[col] ^= (1<<row);
      
//#if RANDOMIZE_INTERVAL!=0
      if ( (u8ReRandInt!=0) && (i==15) )
      {
//        if ( (++g_u8ReRandomiseCounter)==RANDOMIZE_INTERVAL )
        if ( (++g_u8ReRandomiseCounter)==u8ReRandInt )
        {
          g_u8ReRandomiseCounter = 0;
          ReRandomiseFlashers(u8Type);
        }
      }
//#endif
    }
    else
      au16Clock[i] -= u16TicksElapsed;
    if ( g_bMuxDisplay )
      muxLEDMatrix1616();
  }
}

void DrawClear()
{
  memset(&g_au16Pixels,0,2*16);
}

#define _OP_COPY  0
#define _OP_OR    1
#define _OP_XOR   2
#define _OP_NAND  3

void DrawRectangle(uint8_t cx, uint8_t cy, uint8_t eOp)
{
  if ( (cx>16) || (cy>16) )
    return;
  
  cy = 16-cy; //
  uint16_t u16Top = (uint16_t)0xFFFF >> (cy/2);
  uint16_t u16Bot = (uint16_t)0xFFFF << (cy-(cy/2));
  uint16_t u16LineColumn = u16Top & u16Bot;
  u16Top          = (uint16_t)0x8000 >> (cy/2);
  u16Bot          = (uint16_t)0x0001 << (cy-(cy/2));
  uint16_t u16DotColumn = u16Top | u16Bot;

  uint16_t *ptr = &g_au16Pixels[8-(cx/2)];
  for (uint8_t i=0; i<cx; i++)
  {
    switch (eOp)
    {
      case _OP_COPY:
        if ( (i==0)||(i==(cx-1)) )
          *ptr = u16LineColumn;
        else
          *ptr = u16DotColumn;
        break;
      case _OP_OR:
        if ( (i==0)||(i==(cx-1)) )
          *ptr |= u16LineColumn;
        else
          *ptr |= u16DotColumn;
        break;
      case _OP_XOR:
        if ( (i==0)||(i==(cx-1)) )
          *ptr ^= u16LineColumn;
        else
          *ptr ^= u16DotColumn;
        break;
    }
    ptr++;
  }
}

void DrawCross(uint8_t cx, uint8_t eOp)
{
  if ( (cx<1) || (cx>16) ) return;
  cx &= 0xFE;
   
  uint16_t u16Top = (uint16_t)0x0100 << ((cx/2)-1);
  uint16_t u16Bot = (uint16_t)0x0080 >> ((cx/2)-1);
  uint16_t *ptr = &g_au16Pixels[8-(cx/2)];
  for (uint8_t i=1; i<=cx; i++) 
  {
    switch (eOp)
    {
      case _OP_COPY:
          *ptr = (u16Top|u16Bot);
        break;
      case _OP_OR:
        *ptr |= (u16Top|u16Bot);
        break;
      case _OP_XOR:
        *ptr ^= (u16Top|u16Bot);
        break;
      case _OP_NAND:
        *ptr &= ~(u16Top|u16Bot);
        break;
    }
    if ( i==(cx/2) )
    ;
    else if ( i<(cx/2) )
    {
      u16Top >>= 1;
      u16Bot <<= 1;
    }
    else
    {
      u16Top <<= 1;
      u16Bot >>= 1;
    }
    ptr++;
  }
}


uint8_t g_u8Twirl = 0;



void setup() 
{

  TIMSK0 = 0; // turn off timer0 for lower jitter
  //ADCSRA = 0xe5; // set the adc to free running mode
  //ADMUX = 0x40;  // use adc0
  //DIDR0 = 0x01;  // turn off the digital input for adc0


  ////////////// Timer 2 TIMER2 ///////////////////////////////////////////////////////////

  TCCR2A = 0;      // set entire TCCR2A register to 0
  TCCR2B = 0;     // same for TCCR2B  (note WGM22 is 0)

  //[OC2A Pin Output:2 | OC2A Pin Output:2 | rsvd:2 | WGM : 3]
  //  0 0 = off          0 0 = off            0 0      
  //
  // Mode   WGM2 WGM1 WGM0
  //  0
  //  1
  //  2       0    1    0       CTC mode (counter clear on reaching OCR2A value)  counts from 0xFF down to OCR2A
//  TCCR2A |= _BV(WGM21) | _BV(WGM20);// turn on Fast PWM mode, OCRA is top, 
  TCCR2A |= _BV(WGM21);// CTC mode, OCRA is top, 
  
  //[-- -- -- -- -- CS22 CS21 CS20]
  //                  0    0    0   no clock source (stopped counter)
  //                  0    0    1   clk / 1 (no prescaler)
  //                  0    1    0   clk / 8                   2,000,000
  //                  0    1    1   clk / 32                    500,000
  //                  1    0    0   clk / 64                    250,000
  //                  1    0    1   clk / 128                   125,000
  //                  1    1    0   clk / 256                    62,500
  //                  1    1    1   clk / 1024                    8,192
  //TCCR2B = _BV(CS21);     //Timer cock selection   clk/8
  //OCR2A = (255-32);  //7000 Hz (0.14ms ticks)
  
  TCCR2B = _BV(CS22); //Timer cock selection   clk/64    250,000
  
  //OCR2A = 250;  //1ms
  OCR2A = MASTER_CLOCK_DIVISOR; //125;  //0.5ms

  //[-- -- -- -- -- OCIE2B OCIE2A TOIE]
  // note TOIE is used when timer overflows..
  TIMSK2 |= _BV(OCIE2A);// enable timer compare interrupt for OCR2A only (eg only OCR2A will make interrupts)
//  TIMSK2 |= _BV(OCIE2A) | _BV(OCIE2B);// enable timer compare interrupt OCR2A and OCR2B



  pinMode( MATRIX1616_LATCH, OUTPUT);  //set up pins
  pinMode( MATRIX1616_CLK, OUTPUT);
  pinMode( MATRIX1616_DATA, OUTPUT);
  pinMode( MATRIX1616_EN, OUTPUT);
  pinMode( MATRIX1616_ROWBIT(1), OUTPUT);
  pinMode( MATRIX1616_ROWBIT(2), OUTPUT);
  pinMode( MATRIX1616_ROWBIT(3), OUTPUT);
  pinMode( MATRIX1616_ROWBIT(4), OUTPUT);


  // if analog input pin 0 is unconnected, random analog
  // noise will cause the call to randomSeed() to generate
  // different seed numbers each time the sketch runs.
  // randomSeed() will then shuffle the random function.
  randomSeed(analogRead(0));  
  //
  // // print a random number from 0 to 299
  //randNumber = random(300);
  // print a random number from 10 to 19
  //randNumber = random(10, 20);

  SetupFlashers(0);

  pinMode( PUSHBUTTON_1, INPUT_PULLUP);
  //digitalWrite(PUSHBUTTON_1,TRUE);

//  g_u8MasterClock = 0;
}

uint8_t g_u8LastSlowTick = 0;


void SquarePatterns()
{
//Variation 3 is by far my favorite!
  
  uint16_t u16TicksNow = g_u16TimerTicks;
  uint8_t u8SlowTick = (u16TicksNow>>7) & 255;
  if ( u8SlowTick != g_u8LastSlowTick )
  {
    g_u8LastSlowTick = u8SlowTick;
    g_u8Twirl++;
    uint8_t u8ValueA = g_u8Twirl & 15;
    uint8_t u8ValueB = (g_u8Twirl>>4) & 14;
    uint8_t cx;
    uint8_t cy;
    if ( u8ValueA<8 )
    {
      //0 1 2 3 4  5  6  7
      //2,4,6,8,10,12,14,16
      cx = cy = ((u8ValueA+1)*2);
#if 1 //variation 1 & 3
      cy -= u8ValueB;
      if ( cy>16 ) cy=2;
#endif
    }
    else
    {
      //8 9 10 11 12 13 14 15
      //
      cx = cy = (32-(u8ValueA*2));
#if 1 //variation 3
      cx -= u8ValueB;
      if ( cx>16 ) cx=2;
#endif
    }
#if 0 //variation 2
      cy -= u8ValueB;
      if ( cy>16 ) cy=2;
#endif
    DrawRectangle(cx, cy, _OP_XOR);
  }
}

uint8_t u8LastCX = 0;
uint8_t u8LastCY = 0; 

uint8_t b8FirstFrame = 1;


//1    2   3    4     5     6    7     8     9   10    11    12    13    14   15   16
//2    2   2    4     4     4    6     6     8     8     10    10    12   12   14  16


void ExperimentalPattern()
{
  uint16_t u16TicksNow = g_u16TimerTicks;
  uint8_t u8SlowTick = (u16TicksNow>>7) & 255;
  if ( u8SlowTick != g_u8LastSlowTick )
  {
    g_u8LastSlowTick = u8SlowTick;
 #if 1
    uint8_t u8PhaseA;
    uint8_t u8PhaseB;
    uint8_t cx;
    uint8_t cy;
    #define _PHASE_OFFSET 8

    //remove previous box
    u8PhaseA = (g_u8Twirl&14);
    u8PhaseB = (g_u8Twirl+_PHASE_OFFSET)&14;
      cx = 2+u8PhaseA;
      cy = cx;
      DrawRectangle(cx, cy, _OP_XOR);
      cx = 2+u8PhaseB;
      cy = cx;
//      DrawRectangle(cx, cy, _OP_XOR);

    //if ( g_u8Twirl&16 ) 
    //  DrawCross(2+u8PhaseA, _OP_XOR);
    
    g_u8Twirl++;
    u8PhaseA = (g_u8Twirl&14);
    u8PhaseB = (g_u8Twirl+_PHASE_OFFSET)&14;
    cx = 2+u8PhaseA;
    cy = cx;
    DrawRectangle(cx, cy, _OP_XOR);

    //if ( (g_u8Twirl&31)!=15 )
    DrawCross(2+u8PhaseA, (g_u8Twirl&16)?_OP_OR:_OP_NAND);
    

    
    cx = 2+u8PhaseB;
    cy = cx;
    //DrawRectangle(cx, cy, _OP_XOR);


    
 #else    
    g_u8Twirl++;

   //need 2 boxes out of phase with eachother

   //1     2   3  4  5  6  7  8  9  10  11  12  13 14 15 16   1   2    3   4    5   6   7    8  
   //-    -    -   -   -   -  -  1  2   3   4   5   6  7  8   9   10   11  12   13  14  15   16

   //means we need to remember previous seed for each channel....
    
    uint8_t u8ValueA = g_u8Twirl&15;
    uint8_t u8ValueB = g_u8Twirl&3;
    uint8_t cx;
    uint8_t cy;
    if ( u8LastCX!=255 )
      DrawRectangle(u8LastCX, u8LastCY, _OP_XOR);
    {
      //0 1 2 3 4  5  6  7
      //2,4,6,8,10,12,14,16
      cx = cy = 1+u8ValueA;
    }
    u8LastCX = cx;
    u8LastCY = cy;
    //else
    //{
    //  //8 9 10 11 12 13 14 15
    //  //
    //  cx = cy = (32-(u8ValueA*2));
    //  cx = random(1,8)*2;
    //}
    DrawRectangle(cx, cy, _OP_XOR);
#endif
  }
}

#define PATTERN_SUPERCOMPUTER_A 0
#define PATTERN_SUPERCOMPUTER_B 1
#define PATTERN_SUPERCOMPUTER_C 2
#define PATTERN_SUPERCOMPUTER_D 3
#define PATTERN_SQUARE_V3       4
#define PATTERN_EXPERIMENTAL    5     
#define NUM_PATTERNS            6

uint8_t u8Pattern = PATTERN_SUPERCOMPUTER_A;


uint8_t g_u8LastButton = 0;

uint8_t ReadButtons()
{
  uint8_t u8ButtonId = 0;
  if ( digitalRead(PUSHBUTTON_1)==0 )
    u8ButtonId = 1;
  return u8ButtonId;
}


void muxLEDMatrix1616()
{
  if ( g_bMuxDisplay )
  {
    g_bMuxDisplay = 0;
    //
    #if 1
    g_u8MuxColumn = (g_u8MuxColumn+1)&15;
    matrix1616SendData(g_u8MuxColumn,g_au16Pixels[g_u8MuxColumn]);  
    #endif
  }
}


// the loop function runs over and over again forever
void loop() 
{
  switch ( u8Pattern )
  {
    case PATTERN_SUPERCOMPUTER_A:
    case PATTERN_SUPERCOMPUTER_B:
    case PATTERN_SUPERCOMPUTER_C: RunFlashers(u8Pattern); break;
    case PATTERN_SQUARE_V3:       SquarePatterns(); break;
    case PATTERN_EXPERIMENTAL:    ExperimentalPattern(); break;
  }
  
#if 1
  muxLEDMatrix1616();
#else  
  for (int i=0; i<16; i++)
  {
    matrix1616SendData(i,g_au16Pixels[i]);  
    delayMicroseconds(250); //500); //250);
  }
#endif
  uint8_t u8Button = ReadButtons();
  if ( u8Button != g_u8LastButton )
  {
    g_u8LastButton = u8Button;
    switch ( u8Button )
    {
       case 1:
       {
         b8FirstFrame = 1;
         DrawClear();
         if ( (++u8Pattern)==NUM_PATTERNS )
           u8Pattern = 0;
           
         //When we switch patterns, we may want to 'set-up' for 'synchronise' . We can do that here.
         switch ( u8Pattern )
         {
           case PATTERN_SUPERCOMPUTER_A:
           case PATTERN_SUPERCOMPUTER_B:
           case PATTERN_SUPERCOMPUTER_C:
           case PATTERN_SUPERCOMPUTER_D:
           {
             //force a catchup of the flashers clocks to 'now' time
             uint16_t u16TicksNow = g_u16TimerTicks;
             g_u16TicksBefore = u16TicksNow;
             break;
           }
         }
       }
       break;
    }
  }


}
