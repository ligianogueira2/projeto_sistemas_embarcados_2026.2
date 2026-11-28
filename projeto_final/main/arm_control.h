#ifndef ARM_CONTROL_H
#define ARM_CONTROL_H

#include <stdint.h>
#include "esp_err.h"

/* Limites da varredura azimutal da base */
#define ARM_SCAN_MIN_DEG        -90.0f
#define ARM_SCAN_MAX_DEG         90.0f
#define ARM_SCAN_STEP_DEG         5.0f
#define ARM_TOF_ANGLE_OFFSET_DEG  0.0f  // Ajuste fino entre VL53L0X e centro da garra

esp_err_t arm_control_init(void);
void arm_move_to_idle_pose(void);
void arm_move_to_scan_pose(void);
esp_err_t arm_base_goto_angle(float angle_deg);
void arm_pick_at_target(float angle_deg, uint16_t distance_mm);
void arm_emergency_stop(void);

#endif // ARM_CONTROL_H