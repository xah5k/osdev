#pragma once
#include <arch/x86_64/cpu/paging.h>
char KbdReadCode();
void KbdInitalize(virtaddr ioapicbase, uint64_t ivector);