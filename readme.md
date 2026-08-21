buggy os

# implemented things:
* x86_64 arch specific (interrupts, apic, ioapic, rtc clock)
* proper driver interface (though most drivers are in the kernel for now)
* very basic read only tarfs just for reading off initrd
* ext2 read and write implementation (no delete/mkdir yet)
* have a very buggy but works userspace (with mlibc)
* pci device list and ahci disk driver (SATA only though)
* basic pipes (one way as in one end for reading and one end for writing)
* networking (rtl8139 driver)
* ARP protocol
* IPv4 protocol
* ICMP protocol (only echo packet)
* UDP protocol (basic implementation)
* DHCP protocol (basic impl)  

# things i still want to add:
* job control (probably going to delay adding this)
* port an actual shell (like busybox ash or bash even) with some coreutils
* port doom (cuz why not)
* gui of some sort (?? probably too unrealistic)

# external projects used:
* ```external/bootboot.h``` - Made by bzt and is licensed under the MIT license.
* ```external/elf.h``` - Taken from standard library and is licensed as GNU LGPL
* ```external/printf.[c][h]``` - Made by mpaland and is licensed under the MIT license.
* ```external/posix/*``` - Also taken from stdlib and is licensed as GNU LGPL.
* ```external/utsname.h``` - Same as above.
* ```external/uACPI``` - Licensed under the MIT license was made by Daniil Tatianin