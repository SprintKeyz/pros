/**
 * \file registry.c
 *
 * \brief VDML Device Registry
 *
 * This file is the VDML (Vex Data Management Layer) Registry. It keeps track of what devices
 * are in use on the V5. Therefore, in order to use V5 devices with PROS, they must be
 * registered and deregistered using the registry
 *
 * \copyright (c) 2017, Purdue University ACM SIGBots.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "vdml/registry.h"
#include "ifi/v5_api.h"
#include "vdml/vdml.h"
#include "vdml/vdml_public.h"
#include <errno.h>
#include <stdio.h>

#include "api.h"
#include "kapi.h"

static v5_smart_device_s_t registry[NUM_V5_PORTS];
static V5_DeviceType registry_types[NUM_V5_PORTS];

/*
 * \brief Initializes the registry
 *
 * Initializes the registry, automatically pulling via the API information
 * regarding the devices that are already plugged in.
 */
void registry_init() {
	int i;
	kprint("[VDML][INFO]Initializing registry\n");
	registry_update_types();
	for (i = 0; i < NUM_V5_PORTS; i++) {
		registry[i].device_type = (v5_device_e)registry_types[i];
		registry[i].device_info = vexDeviceGetByIndex(i);
		if (registry[i].device_type != E_DEVICE_NONE) {
			kprintf("[VDML][INFO]Register device in port %d", i + 1);
		}
	}
	kprint("[VDML][INFO]Done initializing registry\n");
}

void registry_update_types() {
	vexDeviceGetStatus(registry_types);
}

int registry_bind_port(unsigned int port, v5_device_e device_type) {
	if (!VALIDATE_PORT_NO(port)) {
		kprintf("[VDML][ERROR]Registration: Invalid port number %d\n", port + 1);
		errno = EINVAL;
		return PROS_ERR;
	}
	if (registry[port].device_type != E_DEVICE_NONE) {
		kprintf("[VDML][ERROR]Registration: Port already in use %d\n", port + 1);
		errno = EADDRINUSE;
		return PROS_ERR;
	}
	if ((v5_device_e)registry_types[port] != device_type && (v5_device_e)registry_types[port] != E_DEVICE_NONE) {
		kprintf("[VDML][ERROR]Registration: Device mismatch in port %d\n", port + 1);
		errno = EINVAL;
		return PROS_ERR;
	}
	kprintf("[VDML][INFO]Registering device in port %d\n", port + 1);
	v5_smart_device_s_t device;
	device.device_type = device_type;
	device.device_info = vexDeviceGetByIndex(port);
	registry[port] = device;
	return 1;
}

int registry_unbind_port(unsigned int port) {
	port--;
	if (!VALIDATE_PORT_NO(port)) {
		errno = EINVAL;
		return PROS_ERR;
	}
	registry[port].device_type = E_DEVICE_NONE;
	registry[port].device_info = NULL;
	return 1;
}

v5_smart_device_s_t registry_get_device(unsigned int port) {
	if (!VALIDATE_PORT_NO(port)) {
		errno = EINVAL;
		v5_smart_device_s_t ret = {.device_type = E_DEVICE_NONE, .device_info = NULL };
		return ret;
	}
	return registry[port];
}

v5_device_e registry_get_bound_type(unsigned int port) {
	if (!VALIDATE_PORT_NO(port)) {
		errno = EINVAL;
		return E_DEVICE_UNDEFINED;
	}
	return registry[port].device_type;
}

v5_device_e registry_get_plugged_type(unsigned int port) {
	if (!VALIDATE_PORT_NO(port)) {
		errno = EINVAL;
		return -1;
	}
	return registry_types[port];
}

int32_t registry_validate_binding(unsigned int port, v5_device_e expected_t) {
	if (!VALIDATE_PORT_NO(port)) {
		errno = EINVAL;
		return PROS_ERR;
	}

	// Get the registered and plugged types
	v5_device_e registered_t = registry_get_bound_type(port);
	v5_device_e actual_t = registry_get_plugged_type(port);

	// Auto register the port if needed
	if (registered_t == E_DEVICE_NONE && actual_t != E_DEVICE_NONE) {
		registry_bind_port(port, actual_t);
		registered_t = registry_get_bound_type(port);
	}

	if ((expected_t == registered_t || expected_t == E_DEVICE_NONE) && registered_t == actual_t) {
		// All are same OR expected is none (bgp) AND reg = act.
		// All good
		return 0;
	} else if (actual_t == E_DEVICE_NONE) {
		// Warn about nothing plugged
		if (!vdml_get_port_error(port)) {
			kprintf("[VDML][WARNING] No device in port %d. Is it plugged in?\n", port + 1);
			vdml_set_port_error(port);
		}
		return 1;
	} else {
		// Warn about a mismatch
		if (!vdml_get_port_error(port)) {
			kprintf("[VDML][WARNING] Device mistmatch in port %d.\n", port + 1);
			vdml_set_port_error(port);
		}
		return 2;
	}
}
