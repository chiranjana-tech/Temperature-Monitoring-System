// Mihisara Chiranajana
#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Pin Definitions
#define INCREMENT_BUTTON PD0
#define DECREMENT_BUTTON PD1
#define RESET_BUTTON PD2
#define MODE_BUTTON PD7
#define TOGGLE_BUTTON PD4 // Added button for toggling display mode

#define COLD_LED PD5
#define NORMAL_LED PD6
#define WARMTH_LED PD3
#define HOT_LED PB3
#define ALARM_LED PB1

#define DIN_PIN PB0
#define CS_PIN PB2
#define CLK_PIN PB5

// Constants
#define THRESHOLD_TEMP 40

// Global Variables
volatile uint8_t count = 0;
volatile uint8_t temp_unit = 0; // 0 for Celsius, 1 for Fahrenheit
volatile uint8_t overflow = 0;  // 0 for not overflow, 1 for overflow
volatile uint8_t mode = 1;      // 1 for Celsius, 0 for Fahrenheit
volatile uint8_t displayMode = 1; // 1 for 7-segment display, 0 for LED display
volatile uint8_t alarmBlinkFlag = 0; // Flag to indicate LED blinking state

// Segment patterns for displaying characters
const uint8_t C_pattern =  0b01001110; // Pattern for 'C'
const uint8_t F_pattern =  0b01000111  ; // Pattern for 'F'

// Custom segment patterns for digits 0 to 9
const uint8_t digit_patterns[10] = {
	0b01111110, // 0
	0b00110000, // 1
	0b01101101, // 2
	0b01111001, // 3
	0b00110011, // 4
	0b01011011, // 5
	0b01011111, // 6
	0b01110000, // 7
	0b01111111, // 8
	0b01111011  // 9
};

// Function Prototypes
void MAX7219_write(uint8_t address, uint8_t data);
void MAX7219_init(void);
void displayCount(uint8_t count);
void displayCharacter(uint8_t position, uint8_t pattern);
void displayTempUnit(uint8_t temp_unit);
float celsius_to_fahrenheit(int celsius);
void updateLEDs(uint8_t temperature);
void clearDisplay(void); // Function to clear the 7-segment display
void clearLEDs(void); // Function to clear the LEDs
void setupPWM(void);

void setup()
{
	// Set button pins as input with pull-up resistors
	DDRD &= ~(1 << INCREMENT_BUTTON) & ~(1 << DECREMENT_BUTTON) & ~(1 << RESET_BUTTON) & ~(1 << MODE_BUTTON) & ~(1 << TOGGLE_BUTTON);
	PORTD |= (1 << INCREMENT_BUTTON) | (1 << DECREMENT_BUTTON) | (1 << RESET_BUTTON) | (1 << MODE_BUTTON) | (1 << TOGGLE_BUTTON);

	// Set LED pins as output
	DDRD |= (1 << COLD_LED) | (1 << NORMAL_LED) | (1 << WARMTH_LED);
	DDRB |= (1 << HOT_LED) | (1 << ALARM_LED);

	// Set MAX7219 pins as output
	DDRB |= (1 << DIN_PIN) | (1 << CS_PIN) | (1 << CLK_PIN);

	// Initialize MAX7219
	MAX7219_init();

	// Setup PWM
	setupPWM();
}

void MAX7219_write(uint8_t address, uint8_t data)
{
	PORTB &= ~(1 << CS_PIN); // Select the MAX7219

	// Send the address
	for (uint8_t i = 8; i > 0; i--)
	{
		if (address & (1 << (i - 1)))
		PORTB |= (1 << DIN_PIN);
		else
		PORTB &= ~(1 << DIN_PIN);
		PORTB |= (1 << CLK_PIN);
		PORTB &= ~(1 << CLK_PIN);
	}

	// Send the data
	for (uint8_t i = 8; i > 0; i--)
	{
		if (data & (1 << (i - 1)))
		PORTB |= (1 << DIN_PIN);
		else
		PORTB &= ~(1 << DIN_PIN);
		PORTB |= (1 << CLK_PIN);
		PORTB &= ~(1 << CLK_PIN);
	}

	PORTB |= (1 << CS_PIN); // Deselect the MAX7219
}

void MAX7219_init(void)
{
	MAX7219_write(0x09, 0x00); // Disable decoding for all digits
	MAX7219_write(0x0A, 0x0F); // Set intensity (0x00 - 0x0F)
	MAX7219_write(0x0B, 0x07); // Scan limit (8 digits)
	MAX7219_write(0x0C, 0x01); // Shutdown register (normal operation)
	MAX7219_write(0x0F, 0x00); // Display test register (off)
}

void displayCount(uint8_t count)
{
	MAX7219_write(2, digit_patterns[count % 10]);          // Display ones digit
	MAX7219_write(3, digit_patterns[(count / 10) % 10]);   // Display tens digit
	MAX7219_write(4, digit_patterns[(count / 100) % 10]);  // Display hundreds digit
}

void displayCharacter(uint8_t position, uint8_t pattern)
{
	MAX7219_write(position, pattern);
}

void displayTempUnit(uint8_t temp_unit)
{
	// Display 'C' or 'F' on a specific digit
	if (temp_unit == 0)
	{
		displayCharacter(1, C_pattern); // Display 'C' on digit 1
	}
	else
	{
		displayCharacter(1, F_pattern); // Display 'F' on digit 1
	}
}

float celsius_to_fahrenheit(int celsius)
{
	return (celsius * 9 / 5) + 32;
}

void updateLEDs(uint8_t count)
{
	// Initialize PWM duty cycles to 0 (LEDs off)
	OCR0A = 0; // PWM for NORMAL_LED
	OCR0B = 0; // PWM for COLD_LED
	OCR2B = 0; // PWM for WARMTH_LED
	OCR2A = 0; // PWM for HOT_LED

	// Turn off HOT_LED and Alarm LED
	PORTB &= ~(1 << HOT_LED);
	PORTB &= ~(1 << ALARM_LED);

	if (count <= 15)
	{
		OCR0B = (count * 255) / 15; // Increase brightness for COLD_LED in 0 - 15 range
	}
	else if (count <= 25)
	{
		OCR0A = ((count - 16) * 255) / 10; // Increase brightness for NORMAL_LED in 16 - 25 range
	}
	else if (count <= 35)
	{
		OCR2B = ((count - 26) * 255) / 10; // Increase brightness for WARMTH_LED in 26 - 35 range
	}
	else if (count <= 40)
	{
		OCR2A = ((count - 36) * 255) / 5; // Increase brightness for HOT_LED in 36 - 40 range
	}
	else
	{
		PORTB |= (1 << HOT_LED); // Turn on HOT_LED
	}
}

void clearDisplay(void)
{
	for (uint8_t i = 1; i <= 8; i++)
	{
		MAX7219_write(i, 0x00); // Clear all digits
	}
}

void clearLEDs(void)
{
	OCR0A = 0; // Turn off NORMAL_LED
	OCR0B = 0; // Turn off COLD_LED
	OCR2B = 0; // Turn off WARMTH_LED
	OCR2A = 0; // Turn off HOT_LED
	PORTB &= ~(1 << HOT_LED); // Turn off HOT_LED
}

void setupPWM(void)
{
	// Setup Timer0 for PWM on PD5 (OC0B) and PD6 (OC0A)
	TCCR0A |= (1 << WGM00) | (1 << WGM01); // Fast PWM mode
	TCCR0A |= (1 << COM0A1) | (1 << COM0B1); // Non-inverting mode for OC0A and OC0B
	TCCR0B |= (1 << CS00); // No prescaler

	// Setup Timer2 for PWM on PD3 (OC2B) and PB3 (OC2A)
	TCCR2A |= (1 << WGM20) | (1 << WGM21); // Fast PWM mode
	TCCR2A |= (1 << COM2A1) | (1 << COM2B1); // Non-inverting mode for OC2A and OC2B
	TCCR2B |= (1 << CS20); // No prescaler
}

int main(void)
{
	setup();

	uint8_t prevState[5] = {1, 1, 1, 1, 1}; // Previous state of the buttons (including TOGGLE_BUTTON)
	uint8_t currentState[5];
	uint8_t pressed[5] = {0, 0, 0, 0, 0}; // Flags for button pressed states (including TOGGLE_BUTTON)

	while (1)
	{
		currentState[0] = !(PIND & (1 << INCREMENT_BUTTON));
		currentState[1] = !(PIND & (1 << DECREMENT_BUTTON));
		currentState[2] = !(PIND & (1 << RESET_BUTTON));
		currentState[3] = !(PIND & (1 << MODE_BUTTON));
		currentState[4] = !(PIND & (1 << TOGGLE_BUTTON)); // Read TOGGLE_BUTTON state

		// Detect button presses (falling edge)
		for (uint8_t i = 0; i < 5; i++)
		{
			if (currentState[i] && !prevState[i])
			{
				pressed[i] = 1; // Button pressed
			}
			else
			{
				pressed[i] = 0; // Button not pressed
			}
		}

		// Handle button presses
		if (pressed[0])
		{
			count++;
			if (count >= THRESHOLD_TEMP)
			{
				overflow = 1;
			}
			if (count >= 200)
			count = 0;
		}

		if (pressed[1])
		{
			if (count > 0)
			{
				count--;
			}
			else
			{
				count = 200;
				overflow = 1;
			}
			if (count < THRESHOLD_TEMP)
			{
				overflow = 0; // Reset flag to stop LED blinking
				PORTB &= ~(1 << ALARM_LED); // Turn off the LED
			}
		}

		if (pressed[2])
		{
			count = 0;
			overflow = 0; // Reset overflow
			alarmBlinkFlag = 0; // Reset alarm
			PORTB &= ~(1 << ALARM_LED); // Turn off the alarm LED
		}

		if (pressed[3])
		{
			mode = !mode; // Toggle mode between Celsius and Fahrenheit
		}

		if (pressed[4])
		{
			displayMode = !displayMode; // Toggle display mode between 7-segment and LEDs
			if (displayMode)
			{
				clearLEDs(); // Turn off LEDs when switching to 7-segment display
			}
			else
			{
				clearDisplay(); // Turn off 7-segment display when switching to LEDs
			}
		}

		// Update the display based on the display mode
		if (displayMode)
		{
			// Display on 7-segment display
			if (mode == 1)
			{
				displayCount(count);
				displayTempUnit(0); // Display 'C'
				
			}
			else
			{
				displayCount((uint8_t)celsius_to_fahrenheit(count));
				displayTempUnit(1); // Display 'F'
			}
		}
		else
		{
			// Display on LEDs
			updateLEDs(count);
		}

		// Handle the alarm LED blinking
		if (overflow)
		{
			alarmBlinkFlag = !alarmBlinkFlag;
			if (alarmBlinkFlag)
			{
				PORTB |= (1 << ALARM_LED); // Turn on the LED
			}
			else
			{
				PORTB &= ~(1 << ALARM_LED); // Turn off the LED
			}
		}

		// Store the current state as previous state for the next loop iteration
		for (uint8_t i = 0; i < 5; i++)
		{
			prevState[i] = currentState[i];
		}

		_delay_ms(5); // Small delay to prevent rapid polling
	}

	return 0;
}
