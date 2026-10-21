// SPDX-License-Identifier: GPL-2.0-or-later
#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <cstring>
#include "../common/Probe.hpp"

static NSString *hex(uint64_t value) {
    return [NSString stringWithFormat:@"0x%llx", (unsigned long long)value];
}
static id property(io_registry_entry_t entry, const char *key) {
    CFStringRef name = CFStringCreateWithCString(nullptr, key, kCFStringEncodingUTF8);
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, name, kCFAllocatorDefault, 0);
    CFRelease(name);
    return CFBridgingRelease(value);
}
static uint32_t pciID(io_registry_entry_t entry, const char *key) {
    id value = property(entry, key);
    if (![value isKindOfClass:[NSData class]] || [value length] != 4) return UINT32_MAX;
    uint32_t result; [value getBytes:&result length:4];
    return result; // Native arm64 and IORegistry PCI data are little endian.
}
static NSDictionary *probeJSON(NSData *data) {
    if (data.length != sizeof(cxadc::Probe)) return @{@"error": @"unexpected probe size"};
    cxadc::Probe p; memcpy(&p, data.bytes, sizeof(p));
    if (p.version != 1 && p.version != 2) return @{@"error": @"unsupported probe version"};
    NSMutableDictionary *regs = [NSMutableDictionary dictionary];
    if (p.mmioRead && p.mmioValid)
        for (unsigned i=0; i<cxadc::registerCount; ++i)
            regs[@(cxadc::registers[i].name)] = @{@"offset":hex(cxadc::registers[i].offset), @"value":hex(p.values[i])};
    return @{@"snapshot_time":@"driver attachment; not a live read",
             @"api_version":@(p.version), @"pci_command":hex(p.command),
             @"pci_command_after_probe": p.version >= 2 ? hex(p.commandAfter) : [NSNull null],
             @"bus_master_enabled":@((p.command & 4) != 0), @"bar0_config":hex(p.bar0),
             @"bar_status":hex(p.barStatus), @"bar_index":@(p.barIndex),
             @"bar_type":hex(p.barType), @"bar_size_bytes":@(p.barSize),
             @"interrupt_config":hex(p.interruptConfig), @"interrupt_query_status":hex(p.interruptStatus),
             @"interrupt_type":hex(p.interruptType), @"mmio_attempted":@(p.mmioRead != 0),
             @"mmio_valid":@(p.mmioValid != 0), @"registers":regs};
}
int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc != 2 || (strcmp(argv[1], "--list") && strcmp(argv[1], "--info"))) {
            fprintf(stderr, "Usage: cxadc-capture --list|--info\nStage A only: capture and ADC configuration are not implemented.\n");
            return 64;
        }
        io_iterator_t devices = 0;
        auto result = IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOPCIDevice"), &devices);
        if (result) { fprintf(stderr,"PCI enumeration failed: 0x%x\n", result); return 1; }
        NSMutableArray *found = [NSMutableArray array];
        io_registry_entry_t device;
        while ((device = IOIteratorNext(devices))) {
            if (pciID(device,"vendor-id") != 0x14f1 || pciID(device,"device-id") != 0x8800) {
                IOObjectRelease(device); continue;
            }
            uint64_t entryID = 0; IORegistryEntryGetRegistryEntryID(device, &entryID);
            NSMutableDictionary *item = [@{@"vendor_id":@"0x14f1", @"device_id":@"0x8800",
                @"subsystem_vendor_id":hex(pciID(device,"subsystem-vendor-id")),
                @"subsystem_device_id":hex(pciID(device,"subsystem-id")),
                @"revision":hex(pciID(device,"revision-id")), @"registry_entry_id":hex(entryID),
                @"capture_implemented":@NO, @"adc_configuration_applied":@NO,
                @"sample_rate_hz": [NSNull null], @"clock_verified":@NO} mutableCopy];
            id address = property(device,"assigned-addresses");
            if ([address isKindOfClass:[NSData class]]) {
                const uint8_t *b = static_cast<const uint8_t *>([address bytes]); NSMutableString *s = [NSMutableString string];
                for (NSUInteger i=0;i<[address length];++i) [s appendFormat:@"%02x",b[i]];
                item[@"assigned_addresses_le_hex"] = s;
            }
            id bdf = property(device,"pcidebug"); if (bdf) item[@"pci_location"] = bdf;
            id page = property(device,"IOPCIDeviceMapperPageSize"); if(page) item[@"mapper_page_size"] = page;
            io_iterator_t children = 0; NSMutableArray *drivers = [NSMutableArray array];
            bool ours = false;
            if (!IORegistryEntryGetChildIterator(device,kIOServicePlane,&children)) {
                io_registry_entry_t child;
                while ((child=IOIteratorNext(children))) {
                    id bundle=property(child,"CFBundleIdentifier");
                    if(bundle) [drivers addObject:bundle];
                    id snapshot=property(child,"CXADCProbe");
                    if ([snapshot isKindOfClass:[NSData class]]) {
                        ours = true;
                        item[@"attachment_probe"] = probeJSON(snapshot);
                    }
                    IOObjectRelease(child);
                }
                IOObjectRelease(children);
            }
            item[@"attached_driver_bundles"] = drivers;
            item[@"cxadc_probe_attached"] = @(ours);
            [found addObject:item]; IOObjectRelease(device);
        }
        IOObjectRelease(devices);
        NSError *error = nil;
        NSData *json = [NSJSONSerialization dataWithJSONObject:@{@"devices":found, @"stage":@"A", @"capture_available":@NO}
            options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];
        if (!json) { fprintf(stderr,"JSON failed: %s\n",error.description.UTF8String); return 1; }
        if (fwrite(json.bytes,1,json.length,stdout)!=json.length || fputc('\n',stdout)==EOF || fflush(stdout)) return 1;
        return found.count ? 0 : 2;
    }
}
