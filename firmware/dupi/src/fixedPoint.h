/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

#pragma once
#include <stdint.h>

#define FP_SHIFT 8
#define FP_ONE   (1 << FP_SHIFT)

#define FP_MUL(a,b) (((int32_t)(a) * (int32_t)(b)) >> FP_SHIFT)
#define FP_DIV(a,b) (((int32_t)(a) * 256) / (b))
#define FP_ABS(a)   ((a) < 0 ? -(a) : (a))
#define FP_TO_INT(f) ((int16_t)((f) >> FP_SHIFT))
#define INT_TO_FP(i) ((int16_t)((i) << FP_SHIFT))

typedef int16_t fp_t;