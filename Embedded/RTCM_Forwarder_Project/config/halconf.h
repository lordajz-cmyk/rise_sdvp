#ifndef _HALCONF_H_
#define _HALCONF_H_

#include "chconf.h"
#include "mcuconf.h"

#define HAL_USE_PAL                     TRUE
#define HAL_USE_SERIAL                  TRUE
#define HAL_USE_USB                     TRUE
#define HAL_USE_RTC                     FALSE
#define HAL_USE_SERIAL_USB 				TRUE

#define SERIAL_USB_BUFFERS_NUMBER       2
#define USB_USE_USB1                    TRUE
#define USB_USB1_IRQ_PRIORITY           13

#define STM32_USB_USE_OTG1          TRUE
#define STM32_USB_USE_OTG2          FALSE


/* Other HAL modules, enable or disable as needed */
#define HAL_USE_I2C                 FALSE
#define HAL_USE_SPI                 FALSE
#define HAL_USE_SERIAL                  TRUE
#define HAL_USE_SDE                 FALSE

#endif /* _HALCONF_H_ */
