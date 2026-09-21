#ifndef _DRIVERS_UART_H
#define _DRIVERS_UART_H

#include "memlayout.h"
#include "types.h"

// PL011 UART (p. 175ff, §13; note errata)
//
// TODO: You'll need to change all offsets below from 0x0 to their true
// value (0x0 is a placeholder). We also only give you the UART_BASE
// constant and one example register (UART_DR). You will need to fill in
// the entries for the necessary registers and their offsets based on the
// information in the BCM2835 ARM Peripherals datasheet or the PL011 UART
// datasheet.
#define UART_BASE (PERIPHERALS_BASE + 0x201000)
#define UART_DR (UART_BASE + 0x0)  // data register
#define UART_FR (UART_BASE + 0x18) 
#define UART_IBRD (UART_BASE + 0x24) 
#define UART_FBRD (UART_BASE + 0x28) 
#define UART_LCRH (UART_BASE + 0x2c) 
#define UART_CR (UART_BASE + 0x30) 

// Declare public functions like `uart_init` here!
void gpio_init(void);
void uart_init(void);
void uart_send(char c);
void uart_send_string(const char* s);

#endif  // _DRIVERS_UART_H
