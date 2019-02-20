/* 
	Coursework 2 Mastermind
	By Jordan Walker and Wai Teng Chong
	JW31 WTC1
	2nd Year Computer Science 
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

//=====================================
#define LED 6  
#define LED2 5 //PINS based on BCM Numbering
#define BUTTON 19
#define DELAY 100 // delay for loop iterations (mainly), in ms
// ====================================
#ifndef	TRUE
#define	TRUE	(1==1)
#define	FALSE	(1==2)
#endif

#define	PAGE_SIZE		(4*1024)
#define	BLOCK_SIZE		(4*1024)
#define	INPUT			 0
#define	OUTPUT			 1
#define	LOW			     0
#define	HIGH			 1

// APP constants   ---------------------------------

#define STRB_PIN 24 // Wiring (see call to lcdInit in main, using BCM numbering)
#define RS_PIN   25
#define DATA0_PIN 23
#define DATA1_PIN 17
#define DATA2_PIN 27
#define DATA3_PIN 22

#define	LCD_CLEAR	0x01
#define	LCD_HOME	0x02
#define	LCD_ENTRY	0x04
#define	LCD_CTRL	0x08
#define	LCD_CDSHIFT	0x10
#define	LCD_FUNC	0x20
#define	LCD_CGRAM	0x40
#define	LCD_DGRAM	0x80

// Bits in the entry register

#define	LCD_ENTRY_SH		0x01
#define	LCD_ENTRY_ID		0x02

// Bits in the control register

#define	LCD_BLINK_CTRL		0x01
#define	LCD_CURSOR_CTRL		0x02
#define	LCD_DISPLAY_CTRL	0x04

// Bits in the function register
#define	LCD_FUNC_F	0x04
#define	LCD_FUNC_N	0x08
#define	LCD_FUNC_DL	0x10
#define	LCD_CDSHIFT_RL	0x04
#define	PI_GPIO_MASK	(0xFFFFFFC0)

void delay (unsigned int howLong) // Control the time between events
{
  struct timespec sleeper, dummy ;

  sleeper.tv_sec  = (time_t)(howLong / 1000) ;
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

  nanosleep (&sleeper, &dummy) ;
}
struct lcdDataStruct // data structure holding data on the representation of the LCD
{
  int bits, rows, cols ;
  int rsPin, strbPin ;
  int dataPins [8] ;
  int cx, cy ;
} ;

static int lcdControl ;
static unsigned int gpiobase ;
static uint32_t *gpio ;
void waitForEnter (void);
void digitalWrite (uint32_t *gpio, int pin, int value);
char array[3][10]; //Array used to store values of the game

int failure (int fatal, const char *message, ...) //If Wiring is not compatible return Wiring and Pin codes
{
  va_list argp ;
  if (!fatal) //  
  return -1 ;
  
  va_start (argp, message) ;
  va_end (argp) ;

  exit (EXIT_FAILURE) ;

  return 0 ;
}
/* LCD Features*/
void waitForEnter (void) /*Wait for user input to continue*/
{
  printf ("Press ENTER to continue: ") ;
  (void)fgetc (stdin) ;
}
void delayMicroseconds (unsigned int howLong) //Shorter delay used for LCD
{
  struct timespec sleeper ;
  unsigned int uSecs = howLong % 1000000 ;
  unsigned int wSecs = howLong / 1000000 ;

  /**/ if (howLong ==   0)
    return ;
#if 0
  else if (howLong  < 100)
    delayMicrosecondsHard (howLong) ;
#endif
  else
  {
    sleeper.tv_sec  = wSecs ;
    sleeper.tv_nsec = (long)(uSecs * 1000L) ;
    nanosleep (&sleeper, NULL) ;
  }
}
void strobe (const struct lcdDataStruct *lcd)
{
  digitalWrite (gpio, lcd->strbPin, 1) ; delayMicroseconds (50) ;
  digitalWrite (gpio, lcd->strbPin, 0) ; delayMicroseconds (50) ;
}
void sendDataCmd (const struct lcdDataStruct *lcd, unsigned char data) //Send an data or command byte to the display
{
  register unsigned char myData = data ;
  unsigned char          i, d4 ;

  if (lcd->bits == 4)
  {
    d4 = (myData >> 4) & 0x0F;
    for (i = 0 ; i < 4 ; ++i)
    {
      digitalWrite (gpio, lcd->dataPins [i], (d4 & 1)) ;
      d4 >>= 1 ;
    }
    strobe (lcd) ;

    d4 = myData & 0x0F ;
    for (i = 0 ; i < 4 ; ++i)
    {
      digitalWrite (gpio, lcd->dataPins [i], (d4 & 1)) ;
      d4 >>= 1 ;
    }
  }
  else
  {
    for (i = 0 ; i < 8 ; ++i)
    {
      digitalWrite (gpio, lcd->dataPins [i], (myData & 1)) ;
      myData >>= 1 ;
    }
  }
  strobe (lcd) ;
}
void lcdPutCommand (const struct lcdDataStruct *lcd, unsigned char command) //Send a command byte to the display
{
#ifdef DEBUG
  fprintf(stderr, "lcdPutCommand: digitalWrite(%d,%d) and sendDataCmd(%d,%d)\n", lcd->rsPin,   0, lcd, command);
#endif
  digitalWrite (gpio, lcd->rsPin,   0) ;
  sendDataCmd  (lcd, command) ;
  delay (2) ;
}
void lcdPut4Command (const struct lcdDataStruct *lcd, unsigned char command)
{
  register unsigned char myCommand = command ;
  register unsigned char i ;

  digitalWrite (gpio, lcd->rsPin,   0) ;

  for (i = 0 ; i < 4 ; ++i)
  {
    digitalWrite (gpio, lcd->dataPins [i], (myCommand & 1)) ;
    myCommand >>= 1 ;
  }
  strobe (lcd) ;
}
void lcdHome (struct lcdDataStruct *lcd) //Home the cursor or clear the screen
{
#ifdef DEBUG
  fprintf(stderr, "lcdHome: lcdPutCommand(%d,%d)\n", lcd, LCD_HOME);
#endif
  lcdPutCommand (lcd, LCD_HOME) ;
  lcd->cx = lcd->cy = 0 ;
  delay (5) ;
}
void lcdClear (struct lcdDataStruct *lcd) //Clear the LCD screen
{
#ifdef DEBUG
  fprintf(stderr, "lcdClear: lcdPutCommand(%d,%d) and lcdPutCommand(%d,%d)\n", lcd, LCD_CLEAR, lcd, LCD_HOME);
#endif
  lcdPutCommand (lcd, LCD_CLEAR) ;
  lcdPutCommand (lcd, LCD_HOME) ;
  lcd->cx = lcd->cy = 0 ;
  delay (5) ;
}
void lcdPosition (struct lcdDataStruct *lcd, int x, int y) //lcdPosition: Update the position of the cursor on the display. Ignore invalid locations.
{
  if ((x > lcd->cols) || (x < 0))
    return ;
  if ((y > lcd->rows) || (y < 0))
    return ;

  lcdPutCommand (lcd, x + (LCD_DGRAM | (y>0 ? 0x40 : 0x00)  /* rowOff [y] */  )) ;

  lcd->cx = x ;
  lcd->cy = y ;
}
void lcdDisplay (struct lcdDataStruct *lcd, int state) //Turn the Display
{
  if (state)
    lcdControl |=  LCD_DISPLAY_CTRL ;
  else
    lcdControl &= ~LCD_DISPLAY_CTRL ;

  lcdPutCommand (lcd, LCD_CTRL | lcdControl) ; 
}
void lcdCursor (struct lcdDataStruct *lcd, int state) //Turn the Cursor
{
  if (state)
    lcdControl |=  LCD_CURSOR_CTRL ;
  else
    lcdControl &= ~LCD_CURSOR_CTRL ;

  lcdPutCommand (lcd, LCD_CTRL | lcdControl) ; 
}
void lcdCursorBlink (struct lcdDataStruct *lcd, int state) //Turn the Blinking
{
  if (state)
    lcdControl |=  LCD_BLINK_CTRL ;
  else
    lcdControl &= ~LCD_BLINK_CTRL ;

  lcdPutCommand (lcd, LCD_CTRL | lcdControl) ; 
}
void lcdPutchar (struct lcdDataStruct *lcd, unsigned char data) //Send a data byte to be displayed on the display. 
{
  digitalWrite (gpio, lcd->rsPin, 1) ;
  sendDataCmd  (lcd, data) ;
  if (++lcd->cx == lcd->cols)
  {
    lcd->cx = 0 ;
    if (++lcd->cy == lcd->rows)
      lcd->cy = 0 ;
    lcdPutCommand (lcd, lcd->cx + (LCD_DGRAM | (lcd->cy>0 ? 0x40 : 0x00)   /* rowOff [lcd->cy] */  )) ;
  }
}
void lcdPuts (struct lcdDataStruct *lcd, const char *string) //Send a string to be displayed on the display
{
  while (*string)
    lcdPutchar (lcd, *string++) ;
}
struct lcdDataStruct * lcd;
struct lcdDataStruct * PrepareLCD() //Create a function from a struct to easily call the LCD screen
{
  int bits, rows, cols ;
  unsigned char func ;
  int i;


  bits = 4; 
  cols = 16; 
  rows = 2; 

  lcd = (struct lcdDataStruct *)malloc (sizeof (struct lcdDataStruct)) ;
  if (lcd == NULL)
    return NULL;

  // hard-wired GPIO pins
  lcd->rsPin   = RS_PIN ;
  lcd->strbPin = STRB_PIN ;
  lcd->bits    = 4 ;
  lcd->rows    = rows ;  // # of rows on the display
  lcd->cols    = cols ;  // # of cols on the display
  lcd->cx      = 0 ;     // x-pos of cursor
  lcd->cy      = 0 ;     // y-pos of curosr

  lcd->dataPins [0] = DATA0_PIN ;
  lcd->dataPins [1] = DATA1_PIN ;
  lcd->dataPins [2] = DATA2_PIN ;
  lcd->dataPins [3] = DATA3_PIN ;

  for (i = 0 ; i < bits ; ++i)
  {
    digitalWrite (gpio, lcd->dataPins [i], 0) ;
  }
  delay (35) ; // mS

  if (bits == 4)
  {
    func = LCD_FUNC | LCD_FUNC_DL ;			// Set 8-bit mode 3 times
    lcdPut4Command (lcd, func >> 4) ; delay (35) ;
    lcdPut4Command (lcd, func >> 4) ; delay (35) ;
    lcdPut4Command (lcd, func >> 4) ; delay (35) ;
    func = LCD_FUNC ;					// 4th set: 4-bit mode
    lcdPut4Command (lcd, func >> 4) ; delay (35) ;
    lcd->bits = 4 ;
  }
  else
  {
    failure(TRUE, "setup: only 4-bit connection supported\n");
    func = LCD_FUNC | LCD_FUNC_DL ;
    lcdPutCommand  (lcd, func     ) ; delay (35) ;
    lcdPutCommand  (lcd, func     ) ; delay (35) ;
    lcdPutCommand  (lcd, func     ) ; delay (35) ;
  }

  if (lcd->rows > 1)
  {
    func |= LCD_FUNC_N ;
    lcdPutCommand (lcd, func) ; delay (35) ;
  }

  // Rest of the initialisation sequence
  lcdDisplay     (lcd, TRUE) ;
  lcdCursor      (lcd, FALSE) ;
  lcdCursorBlink (lcd, FALSE) ;
  lcdClear       (lcd) ;

  lcdPutCommand (lcd, LCD_ENTRY   | LCD_ENTRY_ID) ;    // set entry mode to increment address counter after write
  lcdPutCommand (lcd, LCD_CDSHIFT | LCD_CDSHIFT_RL) ;
  return lcd;
}
void setoutput (int pinACT,int off, uint32_t res) /* LEDs And Button  In Assembler*/
{	
#if 1
	asm volatile(/* inline assembler version of setting/clearing LED to ouput" */
	        "\tB   _bonzo0\n"
                /* "vodoo0:  .word %[gpio]\n" */
	        "_bonzo0: NOP\n"
		"\tLDR R1, %[gpio]\n"
	        "\tADD R0, R1, %[off]\n"  /* R0 = GPSET/GPCLR register to write to */
	        "\tMOV R2, #1\n"
	        "\tMOV R1, %[act]\n"
	        "\tAND R1, #31\n"
	        "\tLSL R2, R1\n"          /* R2 = bitmask setting/clearing register %[act] */
	        "\tSTR R2, [R0, #0]\n"    /* write bitmask */
		"\tMOV %[result], R2\n"
		: [result] "=r" (res)
	        : [act] "r" (pinACT)
		, [gpio] "m" (gpio)
		, [off] "r" (off*4)
	        : "r0", "r1", "r2", "cc");
#endif
#if 0

	  asm(/* inline assembler version of setting LED to ouput" */
	        "\tB   _bonzo0\n"
                ".vodoo0:  .word %[gpio]\n"
	        "_bonzo0: NOP\n"
		"\tLDR R0, .vodoo0\n"
	        /* "\tMOV R0, %[gpio]\n" */
	        "\tADD R0, R0, #11\n"
	        "\tMOV R2, #1\n"
	        "\tLDR R1, %[act]\n"
	        "\tAND R1, #31\n"
	        "\tLSL R2, R1\n"
	        "\tSTR R2, [R0, #0]\n"
	        : [act] "=r" (pinACT)
	        : [gpio] "r" (&gpio)
	        : "r0", "r1", "r2", "cc");	
 #endif
}
/*Functions for LEDs and Button to make calling them simpler in the program*/
void setLED1()   // controlling LED pin 6
{
	int res = 0; 
	setoutput(0,18,res);// Sets bits to one = output
}
void setLED2()   // controlling LED pin 5
{
	int res = 0;
	setoutput(0,15,res); // Sets bits to one = output
}
void setButton() //Ready the Button for use
{
 int res = 0;
  setoutput(2,12,res); // Sets bits to one = output
}
void TurnOnLED1() //Send a value of 1 to the LED1
{
	int clrOff;
	clrOff = 7; *(gpio + clrOff) = 1 << (LED & 31);	
}
void TurnOnLED2() //Send a value of 1 to LED2
{
	int clrOff;
	clrOff = 7; *(gpio + clrOff) = 1 << (LED2 & 31);	
}
void TurnOffLED1() //Turn off LEd 1
{
	int clrOff;
	clrOff = 10; *(gpio + clrOff) = 1 << (LED & 31);	
}
void TurnOffLED2() /*Turn Off LED2 */
{
	int clrOff;
	clrOff = 10; *(gpio + clrOff) = 1 << (LED2 & 31);	
}
/* VVVV Main Game Functions VVVVV */
int ButtonLoop() //Main game button loop, allows for the input of user data to play the game
{
	int pinLED = LED, pinButton = BUTTON;
	int  j, i;
	int c, l, k, m = 0;
	int maxloop = 50;
	int theValue;
	
	TurnOffLED1();
	
	for(i=0;i<3;i++){
		TurnOnLED2();
		delay(600);
		TurnOffLED2();
		delay(600);
	}
		
	for (m = 0;m <3 ;m++)
	{
		lcd=PrepareLCD(); lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Next Input" ) ;
		l = 0;
		for (j=0; j< maxloop; j++)
		{
			
			k = c;
			if ((pinButton & 0xFFFFFFC0 /* PI_GPIO_MASK */) == 0){		/* On-Board Pin; bottom 64 pins belong to the Pi*/
				theValue = HIGH ;
					if ((*(gpio + 13 /* GPLEV0 */) & (1 << (BUTTON & 31))) != 0){
						while((*(gpio + 13 /* GPLEV0 */) & (1 << (BUTTON & 31))) != 0){
								TurnOnLED2();
							}
					theValue = LOW ;
					c++ ;
						if (c>k){
						l++;
						}
						if(l == 4){
						l = 1;
						}
	
						char j;
						j = l+'0';
						char aj[2]= {j};
							lcdClear(lcd);
							lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Input: " ) ;
							lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, aj ) ;					
					}
					else{
					theValue = HIGH ;		
					}
				}
			else{
			fprintf(stderr, "only supporting on-board pins\n");          exit(1);
			}

    if ((pinLED & 0xFFFFFFC0 /*PI_GPIO_MASK */) == 0)		
      {	
        if (theValue == HIGH) {
	  TurnOffLED2();
        } else {
	  TurnOnLED2();
		}
      }
      else
      {
        fprintf(stderr, "only supporting on-board pins\n");          
       }
			delay(100); 
    if (l > 3){
		l = 1;
	}
}

 *(gpio + 7) = 1 << (23 & 31) ;
 
 array[m][0] = l+'0';
	 lcdClear(lcd);
	lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Selected: " ) ; lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, array[m]) ;
	delay(700);
  
	if(l==1){
		TurnOnLED1();
		delay(1000);
		TurnOffLED1();
		 lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Next Input" ) ;
		}
		
	if(l==2){
		for(i=0;i<2;i++){
		TurnOnLED1();
		delay(1000);
		TurnOffLED1();
		delay(1000);
		 lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Next Input" ) ;
		}
	}
		
	if(l==3){
		for(i=0;i<3;i++){
		TurnOnLED1();
		delay(1000);
		TurnOffLED1();
		delay(1000);
		 lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Next Input" ) ;
		}
	}
	
	if (l == 0){
		printf("Did not get any intput !! Progream closed \n");
		lcdClear(lcd);
		exit(0);
	}
}
	return 0;
}
int ButtonLoopDebug() //Debug version of the ButtonLoop
{
	int pinLED = LED, pinButton = BUTTON;
	int  j, i;
	int c, l, k, m = 0;
	int maxloop = 50;
	int theValue;
	
	TurnOffLED1();
	
	for(i=0;i<3;i++){
		TurnOnLED2();
		delay(600);
		TurnOffLED2();
		delay(600);
	}
		
	for (m = 0;m <3 ;m++)
	{
		lcd=PrepareLCD(); lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Next Input" ) ;
		l = 0;
		for (j=0; j< maxloop; j++)
		{
			
			k = c;
			if ((pinButton & 0xFFFFFFC0 /* PI_GPIO_MASK */) == 0){		/* On-Board Pin; bottom 64 pins belong to the Pi*/
				theValue = HIGH ;
					if ((*(gpio + 13 /* GPLEV0 */) & (1 << (BUTTON & 31))) != 0){
						while((*(gpio + 13 /* GPLEV0 */) & (1 << (BUTTON & 31))) != 0){
								TurnOnLED2();
							}
					theValue = LOW ;
					c++ ;
						if (c>k){
						l++;
						}
						if(l == 4){
						printf("1\n");
						l = 1;
						}
						else{
						printf("%d\n",l);
						}	
						char j;
						j = l+'0';
						char aj[2]= {j};
							lcdClear(lcd);
							lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Input: " ) ;
							lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, aj ) ;					
					}
					else{
					theValue = HIGH ;		
					}
				}
			else{
			fprintf(stderr, "only supporting on-board pins\n");          exit(1);
			}

    if ((pinLED & 0xFFFFFFC0 /*PI_GPIO_MASK */) == 0)		
      {	
        if (theValue == HIGH) {
	  TurnOffLED2();
        } else {
	  TurnOnLED2();
		}
      }
      else
      {
        fprintf(stderr, "only supporting on-board pins\n");          
       }
			delay(100); 
    if (l > 3){
		l = 1;
	}
}
printf("\b");

 *(gpio + 7) = 1 << (23 & 31) ;
 printf("Input: %d.\n",l);
 
 array[m][0] = l+'0';
	 lcdClear(lcd);
	lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Selected: " ) ; lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, array[m]) ;
	delay(700);
  
	if(l==1){
		TurnOnLED1();
		delay(1000);
		TurnOffLED1();
		 lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Next Input" ) ;
		}
		
	if(l==2){
		for(i=0;i<2;i++){
		TurnOnLED1();
		delay(1000);
		TurnOffLED1();
		delay(1000);
		 lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Next Input" ) ;
		}
	}
		
	if(l==3){
		for(i=0;i<3;i++){
		TurnOnLED1();
		delay(1000);
		TurnOffLED1();
		delay(1000);
		 lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Next Input" ) ;
		}
	}


	
	if (l == 0){
		printf("Did not get any intput !! Progream closed \n");
		lcdClear(lcd);
		exit(0);
	}
}
	return 0;
}
void makeCode(char secretCode[3][10]) //Create a random number sequence that is then stored in array "secretCode"
{
    int i, randColor;
    for(i=0; i<3; i++)  
    {
        randColor = 1 + rand() % 3;     
        switch(randColor)       
        {
            case 1: strcpy(secretCode[i], "1");     break;
            case 2: strcpy(secretCode[i], "2");     break;
            case 3: strcpy(secretCode[i], "3");     break;
        }
    }   
}
void guess(char guessCode[3][10]) //Function to be called that allows the user to enter a guess sequence calls on "ButtonLoop()"
{
    ButtonLoop();
    int i;
    int j;
		
	for(i=0; i<3; i++){
	   for(j = 0;j<10;j++){
		guessCode[i][j] = array[i][j];
		}
	}
}
void guessDebug(char guessCode[3][10]) //debug Function to be called that allows the user to enter a guess sequence calls on "ButtonLoop()"
{
	printf("\nEnter your guess:\n");
    ButtonLoopDebug();
    int i;
    int j;
		
	for(i=0; i<3; i++){
	   for(j = 0;j<10;j++){
		guessCode[i][j] = array[i][j];
		}
	}
}
void winBeep() //Triggered when the user has won the game
{
	
	int i = 0;
	for(i =0;i<3;i++)
			{
					TurnOnLED2();
                    TurnOnLED1();
                    delay(1000);
					TurnOffLED1();
					delay(1000);
			}		
					TurnOffLED1();
					TurnOffLED2();
					lcdClear(lcd);
					exit(0);
}
void codeCheck(char secretCode[3][10], char guessCode[3][10], int *blackPeg, int *whitePeg) /*Compares the values of guessCode and secretCode assigning Black and White pegs respectivley*/
{
    int i, j, checkSecret[3] = {1,1,1}, checkGuess[3] = {1,1,1};
    *blackPeg = *whitePeg = 0;

    for(i=0; i<3; i++)    
        if(strcmp(guessCode[i], secretCode[i]) == 0)   
        {
            ++*blackPeg;
            checkSecret[i] = checkGuess[i] = 0;
        }            

    for(i=0; i<3; i++)
        for(j=0; j<3; j++)       
            if(strcmp(secretCode[i],guessCode[j]) == 0  &&  checkGuess[i]  &&  checkSecret[j]  &&  i != j)        
            {
                ++*whitePeg;
                checkSecret[j] = checkGuess[i] = 0;
            }
}
void displayGuessDebug(char guessCode[3][10], int blackPeg, int whitePeg) //Display the Guess back to the user in Debug mode both textual and visual
{   
    int i;
    lcd = PrepareLCD();
    lcd=PrepareLCD(); lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Result" ) ;
    
    char j;
	j = blackPeg+'0';
	char aj[16]= {'B','l','a','c','k',':',' ',j};
	
	char k;
	k = whitePeg+'0';
	char ak[16]= {'W','h','i','t','e',':',' ',k};
	
	TurnOnLED2();
	delay(500);
	TurnOffLED2();
	delay(500);
	TurnOnLED2();
	delay(500);
	TurnOffLED2();
    
    lcdPosition (lcd, 0, 0) ; lcdPuts (lcd,aj);
	lcdPosition (lcd, 0, 1) ; lcdPuts (lcd,ak);
    printf("\n  Your Guess\t      Your Score\n");   
    for(i=0; i<3; i++) 
        printf("| %s |", guessCode[i]); 
    printf("\t");        
    for(i=0; i<blackPeg; i++) 
        printf("| black |");
			if (i == 1) {

				TurnOnLED1();				
					delay(1000);
				TurnOffLED1();
					delay(1000);
			}  
			if (i == 2) {
	
				lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, aj) ;
				TurnOnLED1();
					delay(1000);
				TurnOffLED1();
					delay(1000);
				TurnOnLED1();
					delay(1000);
				TurnOffLED1();
					delay(1000);
			} 
			else{
				TurnOffLED1();
			}   	
	TurnOnLED2();
	delay(1000);
	TurnOffLED2();	
		
	for(i=0; i<whitePeg; i++)
        printf(" white |");  
        if (i == 1) {
		
			lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, ak) ;
				TurnOnLED1();
					delay(1000);
				TurnOffLED1();
					delay(1000);
			}  
			if (i == 2) {
	
			lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, ak) ;
				TurnOnLED1();
					delay(1000);
				TurnOffLED1();
					delay(1000);
				TurnOnLED1(); ;
					delay(1000);
				TurnOffLED1();
					delay(1000);
			} 
			else{
				TurnOffLED2();
			}   		     
    printf("\n\n");
}
void displayGuess(char guessCode[3][10], int blackPeg, int whitePeg) //Displays The fully correct and partially correct answers back to the user using the LCD and LEDs ONLY
{   
    int i;
    lcd = PrepareLCD();
    lcd=PrepareLCD(); lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Result" ) ;
    
    char j;
	j = blackPeg+'0';
	char aj[16]= {'B','l','a','c','k',':',' ',j};
	
	char k;
	k = whitePeg+'0';
	char ak[16]= {'W','h','i','t','e',':',' ',k};
	
	TurnOnLED2();
	delay(500);
	TurnOffLED2();
	delay(500);
	TurnOnLED2();
	delay(500);
	TurnOffLED2();
    
    lcdPosition (lcd, 0, 0) ; lcdPuts (lcd,aj);
	lcdPosition (lcd, 0, 1) ; lcdPuts (lcd,ak); 
      
    for(i=0; i<blackPeg; i++) 
			if (i == 1) {

				TurnOnLED1();				
					delay(1000);
				TurnOffLED1();
					delay(1000);
			}  
			if (i == 2) {
	
				lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, aj) ;
				TurnOnLED1();
					delay(1000);
				TurnOffLED1();
					delay(1000);
				TurnOnLED1();
					delay(1000);
				TurnOffLED1();
					delay(1000);
			} 
			else{
				TurnOffLED1();
			}   	
	TurnOnLED2();
	delay(1000);
	TurnOffLED2();	
		
	for(i=0; i<whitePeg; i++)
        if (i == 1) {
		
			lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, ak) ;
				TurnOnLED1();
					delay(1000);
				TurnOffLED1();
					delay(1000);
			}  
			if (i == 2) {
	
			lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, ak) ;
				TurnOnLED1();
					delay(1000);
				TurnOffLED1();
					delay(1000);
				TurnOnLED1(); ;
					delay(1000);
				TurnOffLED1();
					delay(1000);
			} 
			else{
				TurnOffLED2();
			}   		     
}
int Debug() //Debug Mode =========================
{
	int   fd ; 


    
	if (geteuid () != 0)
    fprintf (stderr, "setup: Must be root. (Did you forget sudo?)\n") ;

  // -----------------------------------------------------------------------------
  // constants for RPi2
  
	gpiobase = 0x3F200000 ;

  // -----------------------------------------------------------------------------
  // memory mapping 
  // Open the master /dev/memory device

	if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0)
    return failure (FALSE, "setup: Unable to open /dev/mem: %s\n", strerror (errno)) ;

  // GPIO:
	gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, gpiobase) ;
	if ((int32_t)gpio == -1)
    return failure (FALSE, "setup: mmap (GPIO) failed: %s\n", strerror (errno)) ;

	lcd = (struct lcdDataStruct *)malloc (sizeof (struct lcdDataStruct)) ;
	if (lcd == NULL)
    return -1 ;
  // -----------------------------------------------------------------------------
	setLED1();
	setLED2();

	lcd = PrepareLCD(); lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "  Debug Mode") ;
//-------------------

srand(time(NULL));
    int blackPeg, whitePeg, wrongGuess, i;
    char secretCode[3][10], guessCode[3][10], GG[3];
    while(1)
    {
        printf("Debug Mode! \n");
            makeCode(secretCode);          
             printf("the SecretCode is: %s %s %s \n", secretCode[0], secretCode[1], secretCode[2]);              
            for(wrongGuess=1; wrongGuess<=10; wrongGuess++)      //gives 10 turns to guess the sequence
            {
				printf("guess number %d \n ", wrongGuess);				
                guessDebug(guessCode);
                codeCheck(secretCode, guessCode, &blackPeg, &whitePeg);
                displayGuessDebug(guessCode, blackPeg, whitePeg);
                if(blackPeg == 3)           //if the player gets the correct sequence
                {
					char j;
					j = wrongGuess+'0';
					char aj[16]= {'w','o','n',' ','i','n',' ',j,' ','G','u','e','s','s'};					
                    printf("\nYou Won in %d guesses!!!!!\n", wrongGuess); 
                    lcd = PrepareLCD(); lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Gewonnen!") ; lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, aj) ; 
                    winBeep(); 
					break;
				}
			if(wrongGuess == 10) 
				{       //if player cannot guess in 10 rounds
            printf("\nYou Lost! \n");  
            printf("the SecretCode was: %s %s %s \n", secretCode[0], secretCode[1], secretCode[2]); 
			
			for(i = 0; i<3; i++){
             GG [i] = secretCode[i][0];
			}						
			char* str1;
			char* str2;		
			str1 ="      ";
			str2 = GG;
			char * str3 = (char *) malloc(1 + strlen(str1)+ strlen(str2)  );
			strcpy(str3,str1);
			strcat(str3,str2);
			
			printf("str3 is %s",str3);
			
			lcd = PrepareLCD();
			lcdClear(lcd); 
            lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, " sorry you lost") ;
			lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, " The Code Was") ;
			delay(1000);
			lcdClear(lcd); 
			lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, " sorry you lost") ;
			
			
			lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, str3) ;
			}		
      }
      return 0;
    }	
}
int start (int k) //Function to start the Main gameplay mode
{
	while(1){
		if ((*(gpio + 13 /* GPLEV0 */) & (1 << (BUTTON & 31))) != 0){
			k =1;
			return k;
		}		
	}
}
int mainGame() //Main GamePlay Mode ======================
{

  
	int fd, i;
	char GG [3];

	if (geteuid () != 0)
    fprintf (stderr, "setup: Must be root. (Did you forget sudo?)\n") ;

  // -----------------------------------------------------------------------------
  // constants for RPi2
    gpiobase = 0x3F200000 ;

  // -----------------------------------------------------------------------------
  // memory mapping 
  // Open the master /dev/memory device

    if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0)
    return failure (FALSE, "setup: Unable to open /dev/mem: %s\n", strerror (errno)) ;

    // GPIO:
	gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, gpiobase) ;
    if ((int32_t)gpio == -1)
    return failure (FALSE, "setup: mmap (GPIO) failed: %s\n", strerror (errno)) ;
    
    
	setButton();
	setLED1();
	setLED2();
 
	lcd = PrepareLCD();
    lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "  Welcome to") ;
    lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, "  Mastermind") ;
    delay(2000);
    lcdClear(lcd);
	lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "       By") ;
    lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, "Jordan and Tony") ;
    delay(2000);
    lcdClear(lcd);
	lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "  Press Button") ;
    lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, "    To Start") ;
      
	srand(time(NULL));
    int option=0, blackPeg, whitePeg, wrongGuess;
    char secretCode[3][10], guessCode[3][10];
    while(1)
    {
        option = start(option);  
        delay(0.5);    
        if(option == 1)
        {
            makeCode(secretCode);        
             for(i = 0; i<3; i++){
             GG [i] = secretCode[i][0];
			}           
            for(wrongGuess=1; wrongGuess<=10; wrongGuess++)      
            {
                guess(guessCode);              
                codeCheck(secretCode, guessCode, &blackPeg, &whitePeg);
                displayGuess(guessCode, blackPeg, whitePeg);
                if(blackPeg == 3)     
                {
                    char j;
					j = wrongGuess+'0';
					char aj[16]= {'w','o','n',' ','i','n',' ',j,' ','G','o','s'};					
                    lcd = PrepareLCD(); lcdClear(lcd); lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Gewonnen!") ; lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, aj) ; 
                    winBeep(); 
					break;
				}
			if(wrongGuess == 10) {      

	
            for(i = 0; i<3; i++){
             GG [i] = secretCode[i][0];
			}						
			char* str1;
			char* str2;		
			str1 ="      ";
			str2 = GG;
			char * str3 = (char *) malloc(1 + strlen(str1)+ strlen(str2)  );
			strcpy(str3,str1);
			strcat(str3,str2);
						
			lcd = PrepareLCD();
			lcdClear(lcd); 
            lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, " sorry you lost") ;
			lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, " The Code Was") ;
			delay(1000);
			lcdClear(lcd); 
			lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, " sorry you lost") ;
			
			
			lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, str3) ;
			}		
		}
		
      } 
      else
        exit(1);
    }
  
  lcdClear  (lcd) ;
  return 0 ;
}
int main (int argc, char ** argv) //Initial start checks the count of arguments from the command line to see if it should be debug or main game mode. 
{  
  if(argc >= 3){ 
		printf("Incorrect number of arguments\n");
		lcdClear(lcd);
		exit(0);
	}
	if(argc ==1){
		mainGame();
	}		
	if(argc ==2){
		Debug();
	}
	else{	
	lcdClear(lcd);
	exit (0);
	}
	return 0;
}
