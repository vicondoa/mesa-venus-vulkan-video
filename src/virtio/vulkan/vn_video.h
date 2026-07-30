/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_VIDEO_H
#define VN_VIDEO_H

#include "vn_common.h"

/* Guest-side VK_KHR_video_decode_h264 objects.
 *
 * Venus forwards; the renderer forwards; the host driver decodes. Neither of
 * these types carries any state beyond the object id, because the session's
 * real state lives host side and the guest has no reason to shadow it.
 *
 * Both fall through vn_object_base's generic id path in vn_common.h -- there
 * is no per-type registry to extend.
 */

struct vn_video_session {
   struct vn_object_base base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_video_session,
                               base.vk,
                               VkVideoSessionKHR,
                               VK_OBJECT_TYPE_VIDEO_SESSION_KHR)

struct vn_video_session_parameters {
   struct vn_object_base base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_video_session_parameters,
                               base.vk,
                               VkVideoSessionParametersKHR,
                               VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR)

#endif /* VN_VIDEO_H */
