#include "gpio.h"
#include "uart.h"
#include "utils.h"

void gpio_init() {
  // Select the right alternative function for each pin we're using
  unsigned int selector = mmio_read32(GPFSEL1);
  selector &= ~(0b111 << 12);  // clear bits for pin 14
  selector |= 0b100 << 12;     // set pin 14 to ALT0 functionality (TXD0)
  selector &= ~(0b111 << 15);  // clear bits for pin 15
  selector |= 0b100 << 15;     // set pin 15 to ALT0 functionality (RXD0)
  selector &= ~(0b111 << 18);  // clear bits for pin 16
  selector |= 0b111 << 18;     // set pin 16 to ALT3 functionality (CTS0)
  mmio_write32(GPFSEL1, selector);

  // Enable the GPIO pins
  mmio_write32(GPPUD, 0);      // disable pull-up/down for pins 14, 15, and 16
  delay_cycles(150);
  // enable clock for pins 14, 15, and 16; a clock signal is necessary so that
  // the configuration change actually gets applied
  mmio_write32(GPPUDCLK0, (0b1 << 14) | (0b1 << 15) | (0b1 << 16));
  delay_cycles(150);
  mmio_write32(GPPUDCLK0, 0);  // disable clock again
}

void uart_init() {
    mmio_write32(UART_CR, 0); //disable it
    // mmio_write32(UART_IBRD, 26); // 115200 baud (QEMU): 48e6/(16*115200)=26.04
    // mmio_write32(UART_FBRD, 3);  // frac: round(0.04*64)=3
    mmio_write32(UART_IBRD, 2500); // 1200 baud (DWP-230): 48e6/(16*1200)=2500
    mmio_write32(UART_FBRD, 0);    // no fractional part
    mmio_write32(UART_LCRH, 0b01110000); // 8N1, no parity (bit4 FEN, bits5-6 WLEN=8) - DWP-230 wants 8 data / no parity / 1 stop
    mmio_write32(UART_CR, (0b1 << 15 | 0b1 << 9 | 0b1 << 8 | 0b1)); // 010000001100000000
}

void uart_send(char c){
	while(mmio_read32(UART_FR) & (0b1<<5)){}
	mmio_write32(UART_DR, c);
}

void uart_send_string(const char* s){
	while(*s != '\0'){
		uart_send(*s);
		s++;
	}
}

