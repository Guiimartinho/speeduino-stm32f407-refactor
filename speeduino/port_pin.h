#pragma once

#include <Arduino.h>

#if defined(NATIVE_BUILD) || defined(UNIT_TEST)
  // For native tests, use simple fixed types (defined in globals.h)
  // Types are already typedef'd in globals.h, just ensure they're visible
  #ifndef PORT_TYPE
    typedef uint32_t PORT_TYPE;
  #endif
  #ifndef PINMASK_TYPE
    typedef uint32_t PINMASK_TYPE;
  #endif
#else
  // Automatic type deduction for hardware builds
  namespace type_detection_detail {
    // These are used for automatic type detection *at compile time* and are never executed.

    // Get the return type of a function call
    template <typename R, typename... Args> R return_type_of(R(*)(Args...));

    // portOutputRegister() and digitalPinToBitMask() on AVR is a GCC return expression, which needs to
    // be wrapped in a function to be used in this context.
    static inline auto detectPortRegisterType(void) { return portOutputRegister(digitalPinToPort(0)); };
    static inline auto detectDigitalPinToBitMask(void) { return digitalPinToBitMask(0); };
  }

  /** @brief The return type of a "call" to portOutputRegister() */
  using PORT_TYPE = decltype(type_detection_detail::return_type_of(&type_detection_detail::detectPortRegisterType));

  /** @brief The return type of a "call" to digitalPinToBitMask() */
  using PINMASK_TYPE = decltype(type_detection_detail::return_type_of(&type_detection_detail::detectDigitalPinToBitMask));
#endif