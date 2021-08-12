#include <sodium.h>

#include <iostream>
#include <memory>

void print_key(const uint8_t* key, const size_t size) {
    char output[1024];
    sodium_bin2hex(output, sizeof(output), key, size);
    std::cout << output;
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "ERROR: unable to initialize libsodium\n";
        return 1;
    }

    uint8_t ogx_pub[crypto_box_PUBLICKEYBYTES];
    uint8_t ogx_sec[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(ogx_pub, ogx_sec);

    uint8_t user_pub[crypto_box_PUBLICKEYBYTES];
    uint8_t user_sec[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(user_pub, user_sec);

    std::cout << "OGX_PUB : ";
    print_key(ogx_pub, crypto_box_PUBLICKEYBYTES);
    std::cout << "\nOGX_SEC : ";
    print_key(ogx_sec, crypto_box_SECRETKEYBYTES);
    std::cout << "\nUSER_PUB: ";
    print_key(user_pub, crypto_box_PUBLICKEYBYTES);
    std::cout << "\nUSER_SEC: ";
    print_key(user_sec, crypto_box_SECRETKEYBYTES);
    std::cout << "\n";
    return 0;
}