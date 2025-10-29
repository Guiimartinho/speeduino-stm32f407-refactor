/**
 * @file nitrous_control.h
 * @brief Nitrous oxide injection control system
 */

#ifndef NITROUS_CONTROL_H
#define NITROUS_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

namespace speeduino {
namespace nitrous {

bool initialise(void);
void update(void);
void disable(void);
bool isActive(void);
uint8_t getStatus(void);

} // namespace nitrous
} // namespace speeduino

#endif
