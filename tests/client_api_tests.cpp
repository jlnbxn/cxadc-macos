// SPDX-License-Identifier: GPL-2.0-or-later
// CPU-only ABI/argument checks; never enumerates, opens or starts hardware.
#include "../client/cxadc_client.h"
#include <cassert>
#include <cstring>
int main() {
    const auto *api = cxadc_client_get_api_v1();
    assert(api && api->abi_version == 1 && api->struct_bytes == sizeof(*api));
    assert(api->enumerate(nullptr, 1, nullptr) != 0);
    assert(api->open(0, nullptr) != 0);
    assert(api->status(nullptr, nullptr) != 0);
    assert(api->configure(nullptr, nullptr) != 0);
    assert(api->start(nullptr) != 0);
    assert(api->read(nullptr, nullptr, 0, nullptr) != 0);
    assert(api->stop(nullptr) != 0);
    assert(std::strlen(api->error()) != 0);
    api->close(nullptr);
}
