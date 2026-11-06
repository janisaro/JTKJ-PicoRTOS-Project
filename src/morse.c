/**
 * @file morse.c
 * @brief Morse code table: converts Morse symbols to letters and digits.
 *
 * The table and the lookup function were written by a team member
 * (originally in main.c). Moved here so that the UI task can use them.
 */

#include <stddef.h>
#include <string.h>

#include "morse.h"

/** One entry of the Morse table. */
typedef struct {
    const char *morse;
    char letter;
} Morse_code;

/** International Morse code for letters and digits. */
static const Morse_code morse_table[] = {
    {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},
    {".", 'E'}, {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'},
    {"..", 'I'}, {".---", 'J'}, {"-.-", 'K'}, {".-..", 'L'},
    {"--", 'M'}, {"-.", 'N'}, {"---", 'O'}, {".--.", 'P'},
    {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
    {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'},
    {"-.--", 'Y'}, {"--..", 'Z'},
    {".----", '1'}, {"..---", '2'}, {"...--", '3'},
    {"....-", '4'}, {".....", '5'}, {"-....", '6'},
    {"--...", '7'}, {"---..", '8'}, {"----.", '9'},
    {"-----", '0'}
};

/** Number of entries in the table. */
#define MORSE_TABLE_SIZE (sizeof(morse_table) / sizeof(Morse_code))

char morse_to_char(const char *morse)
{
    for (size_t i = 0; i < MORSE_TABLE_SIZE; ++i) {
        if (strcmp(morse_table[i].morse, morse) == 0) {
            return morse_table[i].letter;
        }
    }
    return '?'; /* not found in the table */
}