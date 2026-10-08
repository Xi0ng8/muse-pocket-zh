#include "pocket.h"
#include <string.h>
// The private packaging tool patches this fixed-width field in the binary.
// Build sources, sdkconfig and compiler logs never contain the SDK token.
const char muse_pocket_token_slot[128] __attribute__((used)) = "MUSE_POCKET_TOKEN_SLOT_V1";
// The offline coexistence signer fills this slot. Referencing it at runtime
// preserves it through link-time garbage collection without executing data.
const unsigned char muse_pocket_coexist_certificate[384] __attribute__((used)) =
    "MUSE_POCKET_COEXIST_CERT_V1";
const char* pocket_sdk_token(void) {
    // Read through volatile to keep the check at runtime after binary packaging.
    const volatile char* slot = muse_pocket_token_slot;
    const volatile unsigned char* certificate = muse_pocket_coexist_certificate;
    (void)certificate[0];
    return slot[0]=='m' && slot[1]=='g' && slot[2]=='s' && slot[3]=='t' && slot[4]=='_'
        ? muse_pocket_token_slot : NULL;
}
