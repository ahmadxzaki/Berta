#include <string.h>
#include <stdio.h>
#include "acme_lora.h"
#include "iolist.h"
#include "xtimer.h"

// payload
static const uint8_t payload[] = "Hello from BertaH10 fast prototyping system";

int main(void)
{
    lora_state_t lora = {0};

    // Hardcoded gateway parameters
    lora.bandwidth        = 125;          /* kHz */
    lora.spreading_factor = 7;            /* SF7 */
    lora.coderate         = 5;            /* 4/5 */
    lora.channel          = 915000000;    /* Hz */
    lora.power            = 14;           /* dBm */

    printf("INIT\n");

    if (lora_init(&lora) != 0) {
        return 1;
    }

    // send message
    iolist_t iov = {
        .iol_base = (void*)payload,
        .iol_len  = sizeof(payload) - 1,
        .iol_next = NULL
    };
    
    xtimer_sleep(1);

    printf("ENTERING\n");

    for (int i = 0; i < 1000; i++) {
    // while(1) {
        printf("hi\n");
        (void)lora_write(&iov);
        xtimer_sleep(2);   /* wait 2 seconds between transmissions */
    }

    return 0;
}

