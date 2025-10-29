/**
 * @file wmi_control.h
 * @brief Water-Methanol Injection control system
 */

#ifndef WMI_CONTROL_H
#define WMI_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

namespace speeduino {
namespace wmi {

bool initialise(void);
void update(void);
void disable(void);
uint8_t getPW(void);
bool isTankEmpty(void);

} // namespace wmi
} // namespace speeduino

#endif
