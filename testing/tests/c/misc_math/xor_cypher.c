/*
 * Simple XOR cipher.
 * Implements encryption and decryption with XOR key.
 * Prints "SUCCESS" if decrypted output matches original.
 */

// Include
#include <stdio.h>
#include <string.h>

void DataCorruption_Handler(void) {}
void SigMismatch_Handler(void) {}

// Global key
__attribute__((annotate("to_duplicate")))
unsigned char key = 0x5A;

// Encrypt/decrypt function
unsigned char xor_crypt(unsigned char data, unsigned char k) {
    return data ^ k;
}

// Process buffer function
__attribute__((annotate("to_duplicate")))
void process_buffer(unsigned char *buf, size_t len, unsigned char k) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = xor_crypt(buf[i], k);
    }
}

int main() {
    unsigned char original[] = "HELLOWORLD";
    unsigned char buffer[sizeof(original)];
    memcpy(buffer, original, sizeof(original));

    process_buffer(buffer, sizeof(buffer) - 1, key);   // Encrypt
    process_buffer(buffer, sizeof(buffer) - 1, key);   // Decrypt

    // Check if decrypted buffer matches original
    if (memcmp(buffer, original, sizeof(original) - 1) == 0) {
        printf("SUCCESS");
    } else {
        printf("FAIL");
    }

    return 0;
}

// expected output
// SUCCESS
