#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include "../avr_common/uart.h"
#define __DELAY_BACKWARD_COMPATIBLE__
#include <util/delay.h>


typedef struct {
  uint8_t status: 1; //  first bit: 1=pressed, 0=released
  uint8_t key:   7; //  key number (between 0 and 16)
} KeyEvent;


uint16_t key_status; // this is the full status of our keyboard, 1 bit per key
char* note[]= {"do","do#","re","re#","mi","fa","fa#","sol","sol#","la","la#","si","do_2","do#_2","re_2","re#_2"};

uint8_t keyScan(KeyEvent* events){
  uint16_t new_status=0; // new key status
  int num_events=0;      // number of events
  uint8_t key_num=0;         // key number (0..16)
  for (int r=0; r<4; ++r){

    // this is the mask for the output
    // all 1 and a 0 in position r+4, ex: 01111111, 10111111
    uint8_t row_mask= ~(1<<(r+4));

    // we zero the row r
    PORTA =row_mask;

    // wait 100 us to stabilize signal
    _delay_us(100);

    //read first 4  bit (and negate them)
    // we will have a 1 in correpondence
    // of the key pressed on that row
    uint8_t row= ~(0x0F & PINA);

    for (int c=0;c<4;++c){
      //key_num = numero tasto che sto leggendo
      //c serve a identificare il tasto di quella riga (4 tasti per riga)
      uint16_t key_mask=1<<key_num;
      uint16_t col_mask=1<<c;

      // 1 if key pressed
      uint8_t cs=(row & col_mask)!=0;

      // if key pressed toggle to 1
      // the corresoinding bit
      // of the new keymap (new_status is the mask of the current status)
      if (cs){
        new_status |= key_mask;
      }

      //fetch the previous status of that key
      //if 0 => different from before
      uint8_t ps=(key_mask&key_status)!=0;

      // if different from before, shoot an event;
      if (cs!=ps){
        KeyEvent e;
        e.key=key_num;
        e.status = cs;
        events[num_events]=e;
        ++num_events;
      }
      ++key_num;
    }
    row_mask=row_mask<<1;
  }
  key_status=new_status;
  return num_events;
}

#define MAX_EVENTS 16

//*********************************************************************************************************************//


int main(void){
  // this initializes the printf/uart thingies
  printf_init();

  DDRA=0xF0;  // 4 most significant bits as output, rest as input;
  PORTA=0x0F; // pull up on input buts

  KeyEvent events[MAX_EVENTS];

  while(1){
    uint8_t num_events=keyScan(events);
    for (uint8_t k=0; k<num_events; ++k){
      KeyEvent e=events[k];
      //Inserire creazione di MIDI message e print su seriale
      if((int)e.key+1 <10)
        printf("%d0%d\n", (int)e.status, (int)e.key+1);
      else
        printf("%d%d\n", (int)e.status, (int)e.key+1);
      //, note[(int)e.key]
    }
  }
}
