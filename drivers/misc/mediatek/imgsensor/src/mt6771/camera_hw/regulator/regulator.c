/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "regulator.h"

#include <mt-plat/upmu_common.h>
#if defined(OV16B10P671F50_MIPI_RAW)
static int regulator_status[REGULATOR_TYPE_MAX_NUM] = {0};
static void check_for_regulator_get(struct REGULATOR *preg, struct device *pdevice, int index);
static void check_for_regulator_put(struct REGULATOR *preg, int index);
static DEFINE_MUTEX(g_regulator_state_mutex);
struct device *cam_device = NULL;

static struct device_node *of_node_record = NULL;
#endif
static const int regulator_voltage[] = {
	REGULATOR_VOLTAGE_0,
	REGULATOR_VOLTAGE_1000,
	REGULATOR_VOLTAGE_1100,
	REGULATOR_VOLTAGE_1200,
	REGULATOR_VOLTAGE_1210,
	REGULATOR_VOLTAGE_1220,
	REGULATOR_VOLTAGE_1500,
	REGULATOR_VOLTAGE_1800,
	REGULATOR_VOLTAGE_2500,
	REGULATOR_VOLTAGE_2800,
	REGULATOR_VOLTAGE_2900,
};

struct REGULATOR_CTRL regulator_control[REGULATOR_TYPE_MAX_NUM] = {
	{"vcama"},
	{"vcamd"},
	{"vcamio"},
	{"vcama_sub"},
	{"vcamd_sub"},
	{"vcamio_sub"},
	{"vcama_main2"},
	{"vcamd_main2"},
	{"vcamio_main2"},
	{"vcama_sub2"},
	{"vcamd_sub2"},
	{"vcamio_sub2"},
	{"vcama_main3"},
	{"vcamd_main3"},
	{"vcamio_main3"}
};

static struct REGULATOR reg_instance;

static enum IMGSENSOR_RETURN regulator_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	struct REGULATOR      *preg            = (struct REGULATOR *)pinstance;
	struct REGULATOR_CTRL *pregulator_ctrl = regulator_control;
	int i;

#if defined(OV16B10P671F50_MIPI_RAW)
	cam_device = &pcommon->pplatform_device->dev;
	if (cam_device != NULL) {
		of_node_record = cam_device->of_node;
	}
#endif
	for (i = 0; i < REGULATOR_TYPE_MAX_NUM; i++, pregulator_ctrl++) {
		preg->pregulator[i] = regulator_get(
				&pcommon->pplatform_device->dev,
				pregulator_ctrl->pregulator_type);
		if (preg->pregulator[i] == NULL)
			PK_PR_ERR("regulator[%d]  %s fail!\n",
						i, pregulator_ctrl->pregulator_type);
		atomic_set(&preg->enable_cnt[i], 0);
#if defined(OV16B10P671F50_MIPI_RAW)
		regulator_status[i] = 1;
#endif
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_release(void *pinstance)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int i;

	for (i = 0; i < REGULATOR_TYPE_MAX_NUM; i++) {
		if (preg->pregulator[i] != NULL) {
			for (; atomic_read(&preg->enable_cnt[i]) > 0; ) {
#if defined(S5K3L6_MIPI_RAW) && defined(S5K4H7_MIPI_RAW) && defined(SP2509V_MIPI_RAW)
				if (!(i == REGULATOR_TYPE_MAIN_VCAMIO || i == REGULATOR_TYPE_SUB_VCAMIO
					|| i == REGULATOR_TYPE_MAIN2_VCAMIO))
					regulator_disable(preg->pregulator[i]);
#else
				regulator_disable(preg->pregulator[i]);
#endif
				atomic_dec(&preg->enable_cnt[i]);
			}
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct regulator     *pregulator;
	struct REGULATOR     *preg = (struct REGULATOR *)pinstance;
	enum   REGULATOR_TYPE reg_type_offset;
	atomic_t             *enable_cnt;


	if (pin > IMGSENSOR_HW_PIN_DOVDD   ||
	    pin < IMGSENSOR_HW_PIN_AVDD    ||
	    pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	    pin_state >= IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		return IMGSENSOR_RETURN_ERROR;

	reg_type_offset =
		(sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN)  ? REGULATOR_TYPE_MAIN_VCAMA :
		(sensor_idx == IMGSENSOR_SENSOR_IDX_SUB)   ? REGULATOR_TYPE_SUB_VCAMA :
		(sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN2) ? REGULATOR_TYPE_MAIN2_VCAMA :
		(sensor_idx == IMGSENSOR_SENSOR_IDX_SUB2)   ? REGULATOR_TYPE_SUB2_VCAMA :
		REGULATOR_TYPE_MAIN3_VCAMA;
#if defined(OV16B10P671F50_MIPI_RAW)
	if (pin == IMGSENSOR_HW_PIN_DVDD) {
		check_for_regulator_get(preg, cam_device, (reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD));
	}
#endif
	pregulator = preg->pregulator[reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD];
	enable_cnt = preg->enable_cnt + (reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD);

	#if defined(S5K3L6_MIPI_RAW) || defined(OV16B10P671F50_MIPI_RAW)
	if ((pin == IMGSENSOR_HW_PIN_DVDD) && (pin_state == IMGSENSOR_HW_PIN_STATE_LEVEL_1100)
	 && (sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN)) {
		PK_DBG("%s:set S5K3L6 sensor DVDD to 1.05V\n", __func__);
		pin_state = IMGSENSOR_HW_PIN_STATE_LEVEL_1000;
		pmic_config_interface(MT6358_PMIC_VCAMD, 5, 0xf, 0);
	}
	#endif
	if (pregulator) {
		if (pin_state != IMGSENSOR_HW_PIN_STATE_LEVEL_0) {
			if (regulator_set_voltage(pregulator,
					regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0],
					regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0])) {

				PK_PR_ERR("[regulator]fail to regulator_set_voltage, powertype:%d powerId:%d\n",
						pin,
						regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
			}
			if (regulator_enable(pregulator)) {
				PK_PR_ERR("[regulator]fail to regulator_enable, powertype:%d powerId:%d\n",
						pin,
						regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
				return IMGSENSOR_RETURN_ERROR;
			}
			atomic_inc(enable_cnt);
		} else {
			if (regulator_is_enabled(pregulator))
				PK_DBG("[regulator]%d is enabled\n", pin);

			if (regulator_disable(pregulator)) {
				PK_PR_ERR("[regulator]fail to regulator_disable, powertype: %d\n", pin);
				return IMGSENSOR_RETURN_ERROR;
			}
#if defined(OV16B10P671F50_MIPI_RAW)
			if (pin == IMGSENSOR_HW_PIN_DVDD) {
				check_for_regulator_put(preg, (reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD));
			}
#endif
			atomic_dec(enable_cnt);
		}
	} else {
		PK_PR_ERR("regulator == NULL %d %d %d\n",
				reg_type_offset,
				pin,
				IMGSENSOR_HW_PIN_AVDD);
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_dump(void *pinstance)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int i;

	for (i = REGULATOR_TYPE_MAIN_VCAMA; i < REGULATOR_TYPE_MAX_NUM; i++) {
#if defined(OV16B10P671F50_MIPI_RAW)
		if (IS_ERR_OR_NULL(preg->pregulator[i]))
			 continue;
#endif
		if (regulator_is_enabled(preg->pregulator[i]) &&
				atomic_read(&preg->enable_cnt[i]) != 0)
			PK_DBG("%s = %d\n", regulator_control[i].pregulator_type,
				regulator_get_voltage(preg->pregulator[i]));
	}

	return IMGSENSOR_RETURN_SUCCESS;
}
#if defined(OV16B10P671F50_MIPI_RAW)
static void check_for_regulator_get(struct REGULATOR *preg, struct device *pdevice, int index)
{
	 struct device_node *pof_node;

	 mutex_lock(&g_regulator_state_mutex);
	 if (regulator_status[index] == 0) {
		 if ((pdevice != NULL) && (pdevice->of_node != NULL)) {
			 pof_node = pdevice->of_node;
			 pdevice->of_node = of_node_record;
			 preg->pregulator[index] = regulator_get(pdevice, regulator_control[index].pregulator_type);
			 pdevice->of_node = pof_node;
			 regulator_status[index] = 1;
			 PK_DBG("regulator_dbg regulator_get %s, of_node:%p\n",
				regulator_control[index].pregulator_type, of_node_record);
		 } else {
				PK_PR_ERR("%s:regulator == NULL\n", __func__);
		 }
	 }
	 mutex_unlock(&g_regulator_state_mutex);
}

static void check_for_regulator_put(struct REGULATOR *preg, int index)
{
	mutex_lock(&g_regulator_state_mutex);
	if (regulator_status[index] == 1) {
		regulator_put(preg->pregulator[index]);
		preg->pregulator[index] = NULL;
		regulator_status[index] = 0;
		PK_DBG("regulator_dbg regulator_put %s\n", regulator_control[index].pregulator_type);
	}
	mutex_unlock(&g_regulator_state_mutex);
}
#endif
static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_REGULATOR,
	.pinstance = (void *)&reg_instance,
	.init      = regulator_init,
	.set       = regulator_set,
	.release   = regulator_release,
	.dump      = regulator_dump
};

enum IMGSENSOR_RETURN imgsensor_hw_regulator_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

