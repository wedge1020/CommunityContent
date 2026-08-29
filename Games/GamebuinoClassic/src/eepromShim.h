#ifndef EEPROMSHIM_H
#define EEPROMSHIM_H

// -----------------------------------------------------------------------------
// A "fake eeprom.h" - reproduces the surface of AVR-libc's own <avr/eeprom.h>
// (the real primitive layer Arduino's own EEPROM.h library is itself built
// on top of, and what a ported game's own EEPROM.read()/EEPROM.write()/
// EEPROM.update() call sites need mechanically renaming to - this dialect has
// no class/method support at all, so that object's dot-call syntax can't be
// preserved literally) so a ported game's dropped high-score-persistence code
// can be restored with minimal changes. Direct port of the sibling
// tinyjoypad_vircon32 project's own eepromShim.h/.c, adapted for real
// Gamebuino Classic hardware's own actual EEPROM size: the ATmega328 has a
// genuine 1024-byte EEPROM (4x the ATtiny85's 256-1024 byte range that
// project's own EEPROM_SLOT_DATA_SIZE=512 was originally sized around - see
// eepromShim.c's own EEPROM_SLOT_DATA_SIZE for the exact value used here),
// confirmed directly against the real `<avr/io.h>` ATmega328P part
// definition rather than assumed.
//
// Backed by Vircon32's real memory card (machineDependent.h's md_card*()
// primitives, themselves thin wrappers around memcard.h) instead of a real
// EEPROM chip - each of up to MAX_GAMES games gets its own independent
// 1024-cell address space (one Vircon32 int per conceptual AVR EEPROM byte
// address, matching every other place in this project that widens a
// uint8_t to a plain int rather than trying to preserve real byte-packed
// AVR memory layout - only the *behavior* needs to survive a reboot, not a
// bit-for-bit-identical representation).
//
// Games do not select their own slot - eepromSelectGame() is called once,
// automatically, by portVircon32.c's own dispatch loop right before a
// newly-chosen game's init() runs, keyed by that game's own menu title
// rather than its registration index (see eepromShim.c's own header
// comment for why) - every eeprom_*() call below implicitly operates on
// whichever slot was most recently selected.
// -----------------------------------------------------------------------------

// Called once by portVircon32.c's dispatch loop, before a game's init() -
// not something a game itself ever needs to call.
void eepromSelectGame( int* title );

int  eeprom_read_byte( int address );
void eeprom_write_byte( int address, int value );
void eeprom_update_byte( int address, int value );

int  eeprom_read_word( int address );
void eeprom_write_word( int address, int value );

int  eeprom_read_dword( int address );
void eeprom_write_dword( int address, int value );

void eeprom_read_block( void* dest, int address, int size );
void eeprom_write_block( void* src, int address, int size );

// no-op - Vircon32's memory card has no write latency to wait out, kept
// only so upstream eeprom_busy_wait() call sites port with zero changes.
void eeprom_busy_wait();

#endif
