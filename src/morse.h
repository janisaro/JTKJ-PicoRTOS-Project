/**
 * @file morse.h
 * @brief Morse code table: converts Morse symbols to letters and digits.
 */

#ifndef MORSE_H
#define MORSE_H

/**
 * @brief Convert one Morse-coded character to a letter or digit.
 * @param morse null-terminated string of '.' and '-', e.g. ".-"
 * @return the letter or digit, or '?' if the code is not in the table
 */
char morse_to_char(const char *morse);

#endif /* MORSE_H */