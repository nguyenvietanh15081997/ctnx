#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#define ENCODER_A_GPIO 15
#define ENCODER_B_GPIO 16

#define EVT_ENCODER_LEFT  (1 << 0)
#define EVT_ENCODER_RIGHT (1 << 1)

void encoder_signal_init(void);

#ifdef __cplusplus
}
#endif