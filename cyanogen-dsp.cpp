/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef DEBUG
#define LOG_TAG "DSP-entry"

#include <log/log.h>
#endif

#include <string.h>
#include "media/AudioEffect.h"
#include "hardware/audio_effect.h"

#include "Effect.h"
#include "MainEffect.h"


/* 只保留Compression为主要模块启用 */
static effect_descriptor_t main_module_descriptor = {
	{ 0x09e8ede0, 0xddde, 0x11db, 0xb4f6, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // SL_IID_VOLUME
	{ 0xc3b61114, 0xdef3, 0x5a85, 0xa39d, { 0x5c, 0xc4, 0x02, 0x0a, 0xb8, 0xaf } }, // own UUID
	EFFECT_CONTROL_API_VERSION,
	EFFECT_FLAG_OUTPUT_DIRECT | EFFECT_FLAG_INPUT_DIRECT | EFFECT_FLAG_TYPE_INSERT | EFFECT_FLAG_INSERT_FIRST,
	20, /* 2 MIPS. FIXME: should be measured. */
	2,
	"CyanogenMod's DSP Module Effects",
	"Antti S. Lankila"
};

/* Library mandatory methods. */
extern "C" {

struct effect_module_s {
	const struct effect_interface_s *itfe;
	Effect *effect;
	effect_descriptor_t *descriptor;
};

static int32_t generic_process(effect_handle_t self, audio_buffer_t *in, audio_buffer_t *out) {
	struct effect_module_s *e = (struct effect_module_s *) self;
	return e->effect->process(in, out);
}

/* 接收指令 */
static int32_t generic_command(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize, void *pCmdData, uint32_t *replySize, void *pReplyData) {
	struct effect_module_s *e = (struct effect_module_s *) self;
	return e->effect->command(cmdCode, cmdSize, pCmdData, replySize, pReplyData);
}

static int32_t generic_getDescriptor(effect_handle_t self, effect_descriptor_t *pDescriptor) {
	struct effect_module_s *e = (struct effect_module_s *) self;
	memcpy(pDescriptor, e->descriptor, sizeof(effect_descriptor_t));
	return 0;
}

static const struct effect_interface_s generic_interface = {
	generic_process,
	generic_command,
	generic_getDescriptor,
	NULL
};

int32_t EffectCreate(const effect_uuid_t *uuid, int32_t __attribute__((unused))sessionId, int32_t __attribute__((unused))ioId, effect_handle_t *pEffect) {
	if (memcmp(uuid, &main_module_descriptor.uuid, sizeof(effect_uuid_t)) == 0) {
		struct effect_module_s *e = (struct effect_module_s *) calloc(1, sizeof(struct effect_module_s));
		e->itfe = &generic_interface;
		e->effect = new MainEffect();
		e->descriptor = &main_module_descriptor;
		*pEffect = (effect_handle_t) e;
		return 0;
	}

	return -EINVAL;
}

int32_t EffectRelease(effect_handle_t ei) {
	struct effect_module_s *e = (struct effect_module_s *) ei;
	delete e->effect;
	free(e);
	return 0;
}

int32_t EffectGetDescriptor(const effect_uuid_t *uuid, effect_descriptor_t *pDescriptor) {
	if (memcmp(uuid, &main_module_descriptor.uuid, sizeof(effect_uuid_t)) == 0) {
		memcpy(pDescriptor, &main_module_descriptor, sizeof(effect_descriptor_t));
		return 0;
	}

	return -EINVAL;
}

/* 设置可见度, 仅被自己调用 */
__attribute__ ((visibility ("default")))
audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
	.tag = AUDIO_EFFECT_LIBRARY_TAG,
	.version = EFFECT_LIBRARY_API_VERSION,
	.name = "CyanogenMod's Effect Library",
	.implementor = "Antti S. Lankila",
	.create_effect = EffectCreate,
	.release_effect = EffectRelease,
	.get_descriptor = EffectGetDescriptor,
};

}
