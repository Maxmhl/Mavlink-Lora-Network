#pragma once
// Board variant selector — exactly one VARIANT_* macro is set per PlatformIO env.

#if defined(VARIANT_HELTEC_V3)
#include "variants/heltec_v3.h"
#elif defined(VARIANT_TBEAM_1W)
#include "variants/tbeam_1w.h"
#elif defined(VARIANT_TBEAM_V12)
#include "variants/tbeam_v12.h"
#elif defined(VARIANT_SENSECAP_P1)
#include "variants/sensecap_p1.h"
#else
#error "No board variant selected (define VARIANT_HELTEC_V3 / VARIANT_TBEAM_1W / VARIANT_TBEAM_V12 / VARIANT_SENSECAP_P1)"
#endif

#ifndef HAS_PMU
#define HAS_PMU 0
#endif
#ifndef HAS_GPS
#define HAS_GPS 0
#endif
#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0
#endif
#ifndef HAS_DISPLAY
#define HAS_DISPLAY 0
#endif
