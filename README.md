Overview
For this assignment, we were required to implement a simple version of the MasterMind board-game using C and Assembly. The application needed to be run on a Raspberry Pi2, with the following attached devices: two LEDs, a button, and an LCD (with attached potentiometer). The devices should be connected to the RPi2 via a breadboard, using the RPi2 kit that was handed out early in the course. 
The main aspects of the program are as follows;

1.	The application proceeds in rounds of guesses and answers, as in the sample for the board game.
2.	In each round the player enters a sequence of numbers.
3.	A number is entered using the button as an input device. Each number is entered as the number of button presses, i.e. press twice for an input of two etc.
4.	A fixed time-out should be used to separate the input of successive numbers. Use timers (either on C or Assembler level), as introduced in the lectures.
5.	The red control LED should blink once to acknowledge input of a number.
6.	Then the green data LED should repeat the input number by blinking as many times as the button has been pressed.
7.	Repeat this sequence of button-input and LED-echo for each element of the input sequence.
8.	Once all values have been entered and echoed, the red control LED should blink two times to indicate the end of the input.
9.	As an answer to the guess sequence, the application has to calculate the numbers of exact matches (same colour and location) and approximate matches (same colour but different location).
10.	To communicate the answer, the green data LED should first blink the number of exact matches. Then, as a separator, the red control LED should blink once. Finally, the green data LED should blink the number of approximate matches.
11.	Finally, the red control LED should blink three times to indicate the start of a new round.
12.	If the hidden sequence has been guessed successfully, the green LED should blink three times while the red LED is turned on, otherwise the application should enter the next turn.
13.	When an LCD is connected, the output of exact and approximate matches should additionally be displayed as two separate numbers on an 16x2 LCD display.
14.	On successful finish, a message “SUCCESS” should be printed on the LCD, followed by the number of attempts required.

