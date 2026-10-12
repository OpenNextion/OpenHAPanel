#include "uuid4.h"

#include <stdint.h>
#include "esp_random.h"

static char uuid4_hex(uint8_t value)
{
    static const char hex[] = "0123456789abcdef";
    return hex[value & 0x0F];
}

void uuid4_init(void)
{
    /*
     * Compatibility function.
     *
     * The original uuid4 library requires uuid4_init(), usually to seed
     * a pseudo-random generator.
     *
     * ESP-IDF provides esp_fill_random(), so we do not need to seed rand().
     */
}

void uuid4_generate(char *buf)
{
    uint8_t b[16];

    if (buf == NULL) {
        return;
    }

    esp_fill_random(b, sizeof(b));

    /*
     * UUID v4:
     * - version field: 0100xxxx
     * - variant field: 10xxxxxx
     */
    b[6] = (uint8_t)((b[6] & 0x0F) | 0x40);
    b[8] = (uint8_t)((b[8] & 0x3F) | 0x80);

    /*
     * Format:
     * 8-4-4-4-12
     *
     * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
     */
    buf[0] = uuid4_hex(b[0] >> 4);
    buf[1] = uuid4_hex(b[0]);
    buf[2] = uuid4_hex(b[1] >> 4);
    buf[3] = uuid4_hex(b[1]);
    buf[4] = uuid4_hex(b[2] >> 4);
    buf[5] = uuid4_hex(b[2]);
    buf[6] = uuid4_hex(b[3] >> 4);
    buf[7] = uuid4_hex(b[3]);
    buf[8] = '-';

    buf[9] = uuid4_hex(b[4] >> 4);
    buf[10] = uuid4_hex(b[4]);
    buf[11] = uuid4_hex(b[5] >> 4);
    buf[12] = uuid4_hex(b[5]);
    buf[13] = '-';

    buf[14] = uuid4_hex(b[6] >> 4);
    buf[15] = uuid4_hex(b[6]);
    buf[16] = uuid4_hex(b[7] >> 4);
    buf[17] = uuid4_hex(b[7]);
    buf[18] = '-';

    buf[19] = uuid4_hex(b[8] >> 4);
    buf[20] = uuid4_hex(b[8]);
    buf[21] = uuid4_hex(b[9] >> 4);
    buf[22] = uuid4_hex(b[9]);
    buf[23] = '-';

    buf[24] = uuid4_hex(b[10] >> 4);
    buf[25] = uuid4_hex(b[10]);
    buf[26] = uuid4_hex(b[11] >> 4);
    buf[27] = uuid4_hex(b[11]);
    buf[28] = uuid4_hex(b[12] >> 4);
    buf[29] = uuid4_hex(b[12]);
    buf[30] = uuid4_hex(b[13] >> 4);
    buf[31] = uuid4_hex(b[13]);
    buf[32] = uuid4_hex(b[14] >> 4);
    buf[33] = uuid4_hex(b[14]);
    buf[34] = uuid4_hex(b[15] >> 4);
    buf[35] = uuid4_hex(b[15]);

    buf[36] = '\0';
}