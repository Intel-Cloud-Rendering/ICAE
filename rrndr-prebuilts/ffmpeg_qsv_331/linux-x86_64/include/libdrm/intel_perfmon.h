/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

/**
 * @file intel_perfmon.h
 *
 * Wrapper functions for perfmon Ioctl.
 */

#ifndef INTEL_PERFMON_H
#define INTEL_PERFMON_H

#define DRM_INTEL_PERFMON_VERSION 2


int drm_intel_perfmon_set_irq(int fd, unsigned int enable);

int drm_intel_perfmon_cancel_wait_irq(int fd);

int drm_intel_perfmon_open(int fd);

int drm_intel_perfmon_close(int fd);

int drm_intel_perfmon_enable_config(int fd, int enable);

int drm_intel_perfmon_set_config(int fd,
	enum DRM_I915_PERFMON_CONFIG_TARGET target,
	unsigned int pid,
	struct drm_i915_perfmon_config_entry *oa_entries,
	unsigned int num_oa_entries,
	unsigned int oa_id,
	struct drm_i915_perfmon_config_entry *gp_entries,
	unsigned int num_gp_entries,
	unsigned int gp_id);

int drm_intel_perfmon_load_config(int fd, drm_intel_context *ctx,
				uint32_t *oa_cfg_id,
				uint32_t *gp_cfg_id);

int drm_intel_perfmon_get_hw_ctx_id(int fd, drm_intel_context *ctx,
				unsigned int *hw_ctx_id);

int drm_intel_perfmon_get_hw_ctx_ids(int fd, int pid, unsigned int *hw_ctx_ids,
				unsigned int *hw_ctx_ids_count);

int drm_intel_perfmon_pin_oa_buffer(int fd, drm_intel_bo *bo, 
				uint32_t alignment);

int drm_intel_perfmon_unpin_oa_buffer(int fd, drm_intel_bo *bo);

#endif /* INTEL_PERFMON_H */
