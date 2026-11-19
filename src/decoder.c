#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

typedef struct {
    const char *morse;
    char letter;
} Morse_code;  //luon taulukon morse_code kirjaimille ja numeroille, jotta voin muuntaa kirjaimet ja numerot morsekoodiksi.

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

#define MORSE_TABLE_SIZE (sizeof(morse_table) / sizeof(Morse_code))   //lasketaan morsekooditaulukon koko, jotta voidaan käyttää sitä myöhemmin morsekoodin muuntamiseen kirjaimiksi ja numeroiksi.

static char morse_to_char(const char *morse) {      //luodaan funktio, joka muuntaa morsekoodin kirjaimeksi tai numeroksi.
    for (size_t i = 0; i < MORSE_TABLE_SIZE; ++i) {  //käydään läpi morsekooditaulukko ja verrataan syötettyä morsekoodia taulukon morsekoodiin.
        if (strcmp(morse_table[i].morse, morse) == 0) {  //jos syötetty morsekoodi löytyy taulukosta, palautetaan vastaava kirjain tai numero.
            return morse_table[i].letter;
        }
    }
    return '?'; //palautetaan kysymysmerkki, jos morsekoodia ei löydy taulukosta.
}

// rakennetaan seuraavaksi decoder-funktio, joka ottaa morsekoodin merkkijonona ja palauttaa vastaavan kirjaimen tai numeron.
static void decode_morse(const char *input, char *output, size_t output_size)
{
    size_t outputlength = 0;    // alustetaan output-merkkijonon pituus nollaksi
    if (output_size == 0){
        return;
    }
    output[0] = '\0'; // alustetaan output tyhjäksi merkkijonoksi

    while (*input != '\0'){     // käydään läpi input-merkkijono merkki kerrallaan
        if (*input == ' '){     // jos merkki on välilyönti, lisätään välilyönti output-merkkijonoon
            size_t space_count = 0;     // alustetaan välilyöntien laskuri nollaksi
            while (*input == ' '){      // käydään läpi kaikki peräkkäiset välilyönnit
                space_count++;
                input++;
            }
            if (space_count >= 2){     // jos välilyöntejä on kaksi tai enemmän, lisätään välilyönti output-merkkijonoon
                if (outputlength + 1 < output_size){   // tarkistetaan, että output-merkkijonoon mahtuu vielä yksi merkki
                    output[outputlength++] = ' ';
                    output[outputlength] = '\0';    // lisätään merkkijonon loppuun nollamerkki
                }
            }
        }
    }
}