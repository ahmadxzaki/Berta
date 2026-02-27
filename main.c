#include <stdio.h>

#include "tracker.h"
#include "saml21_backup_mode.h"

#define INIT_FAILURE_BACKOFF_SECONDS 5
#define FW_VERSION "1.0.0"

#define EXTWAKE \
    { .pin = EXTWAKE_PIN6, \
      .polarity = EXTWAKE_HIGH, \
      .flags = EXTWAKE_IN_PU \
    }
static saml21_extwake_t extwake = EXTWAKE;

int main(void)
{
    
    switch (saml21_wakeup_cause()) {
    case BACKUP_EXTWAKE:
        printf("tracker woken up by button\n");
        tracker_wakeup();
        break;
    case BACKUP_RTC:
        tracker_wakeup();
        break;
    default:
        printf("\n");
        printf("-------------------------------------\n");
        printf("-      elec398 capstone group 1      -\n");
        printf("-  Version  : %s              -\n", FW_VERSION);
        printf("-  Compiled : %s %s  -\n", __DATE__, __TIME__);
        printf("-------------------------------------\n");
        printf("\n");

        tracker_init();
        break;
    }

    printf("tracker initialization failed\n");
    saml21_backup_mode_enter(0, extwake, INIT_FAILURE_BACKOFF_SECONDS, 1);
    return 0;
}