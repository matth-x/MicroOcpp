#include <MicroOcpp/Core/UuidUtils.h>
#include <MicroOcpp/Platform.h>

#include <stdint.h>

namespace MicroOcpp {

#define UUID_STR_LEN 36

bool generateUUID(char *uuidBuffer, size_t len) {
  if (len < UUID_STR_LEN + 1)
  {
    return false;
  }

  uint32_t ar[4];
  for (uint8_t i = 0; i < 4; i++) {
    ar[i] = mocpp_rng();
  }

  // Conforming to RFC 4122 Specification
  // - byte 7: four most significant bits ==> 0100  --> always 4
  // - byte 9: two  most significant bits ==> 10    --> always {8, 9, A, B}.
  //
  // patch bits for version 1 and variant 4 here
  ar[1] &= 0xFFF0FFFF;   //  remove 4 bits.
  ar[1] |= 0x00040000;   //  variant 4
  ar[2] &= 0xFFFFFFF3;   //  remove 2 bits
  ar[2] |= 0x00000008;   //  version 1

  // loop through the random 16 byte array
  for (uint8_t i = 0, j = 0; i < 16; i++) {
    // multiples of 4 between 8 and 20 get a -.
    // note we are processing 2 digits in one loop.
    if ((i & 0x1) == 0) {
      if ((4 <= i) && (i <= 10)) {
        uuidBuffer[j++] = '-';
      }
    }

    // encode the byte as two hex characters
    uint8_t nr   = i / 4;
    uint8_t xx   = ar[nr];
    uint8_t ch   = xx & 0x0F;
    uuidBuffer[j++] = (ch < 10)? '0' + ch : ('a' - 10) + ch;

    ch = (xx >> 4) & 0x0F;
    ar[nr] >>= 8;
    uuidBuffer[j++] = (ch < 10)? '0' + ch : ('a' - 10) + ch;
  }

  uuidBuffer[UUID_STR_LEN] = 0;
  return true;
}

}
