// config.h — INDRA Pin Definitions & Calibration Thresholds
// Calibrate thresholds using Serial Monitor raw values

#ifndef INDRA_CONFIG_H
#define INDRA_CONFIG_H

#define PIN_TRIGGER       34  // TCRT5000 (input-only, interrupt)

// TCS3200
#define PIN_TCS_S2        25
#define PIN_TCS_S3        26
#define PIN_TCS_OUT       27
#define PIN_TCS_LED       14

#define PIN_UV_EMITTER    32  // 395nm UV LEDs
#define PIN_UV_RECEIVER   35  // BPW34 via LM358 (ADC)

#define PIN_IR_EMITTER    33  // 850/940nm IR LEDs
#define PIN_IR_RECEIVER   36  // IR photodiode (ADC)

// OLED: SDA=GPIO21, SCL=GPIO22 (default I2C)

#define PIN_BUZZER        4
#define PIN_LED_GREEN     16
#define PIN_LED_RED       17


// Genuine absorbs UV -> low ADC. Fake fluoresces -> high ADC.
#define UV_THRESHOLD      1500

// Genuine IR ink -> dropout -> low ADC. No ink -> high ADC.
#define IR_THRESHOLD      1800

#define RESULT_HOLD_MS    3000

#endif
