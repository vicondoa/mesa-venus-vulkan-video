/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vn_video.h"

#include "venus-protocol/vn_protocol_driver_command_buffer.h"
#include "venus-protocol/vn_protocol_driver_device.h"
#include "venus-protocol/vn_protocol_driver_transport.h"

#include "vn_device.h"
#include "vn_instance.h"
#include "vn_physical_device.h"

/* Guest-side VK_KHR_video_decode_h264 entrypoints.
 *
 * FEASIBILITY SPIKE -- NOT THE HARDENED IMPLEMENTATION.
 *
 * Mesa's entrypoint generator emits WEAK references
 * (vk_entrypoints_gen.py --weak), so a missing vn_* implementation is not a
 * build error. It becomes a NULL dispatch slot, and the extension can be
 * advertised while every call through it crashes or silently does nothing.
 * That is why advertising the three extensions in
 * vn_physical_device_get_passthrough_extensions() is necessary but nowhere
 * near sufficient, and why all thirteen entrypoints are here.
 *
 * The generated protocol already does the substantial work -- struct and pNext
 * serialization, including VkVideoProfileListInfoKHR on image and buffer
 * creation. What is left is dispatch glue and object lifetime.
 *
 * Creates are SYNCHRONOUS (vn_call_*, not vn_async_*) even though the
 * acceleration-structure precedent in this tree is async. Video session
 * creation genuinely fails: an unsupported profile, an unsupported format, or
 * a capability the host driver does not have all produce a real VkResult that
 * the caller branches on. Making those async would report VK_SUCCESS for a
 * session the host refused, and the failure would resurface later as a decode
 * against a handle that was never valid.
 */

/* --- physical device queries -------------------------------------------- */

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetPhysicalDeviceVideoCapabilitiesKHR(
   VkPhysicalDevice physicalDevice,
   const VkVideoProfileInfoKHR *pVideoProfile,
   VkVideoCapabilitiesKHR *pCapabilities)
{
   struct vn_physical_device *phys_dev =
      vn_physical_device_from_handle(physicalDevice);

   return vn_call_vkGetPhysicalDeviceVideoCapabilitiesKHR(
      phys_dev->instance->ring.ring, physicalDevice, pVideoProfile,
      pCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetPhysicalDeviceVideoFormatPropertiesKHR(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceVideoFormatInfoKHR *pVideoFormatInfo,
   uint32_t *pVideoFormatPropertyCount,
   VkVideoFormatPropertiesKHR *pVideoFormatProperties)
{
   struct vn_physical_device *phys_dev =
      vn_physical_device_from_handle(physicalDevice);

   return vn_call_vkGetPhysicalDeviceVideoFormatPropertiesKHR(
      phys_dev->instance->ring.ring, physicalDevice, pVideoFormatInfo,
      pVideoFormatPropertyCount, pVideoFormatProperties);
}

/* --- video session ------------------------------------------------------- */

VKAPI_ATTR VkResult VKAPI_CALL
vn_CreateVideoSessionKHR(VkDevice device,
                         const VkVideoSessionCreateInfoKHR *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkVideoSessionKHR *pVideoSession)
{
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.vk.alloc;

   struct vn_video_session *sess =
      vk_zalloc(alloc, sizeof(*sess), VN_DEFAULT_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!sess)
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&sess->base, VK_OBJECT_TYPE_VIDEO_SESSION_KHR,
                       &dev->base);

   VkVideoSessionKHR sess_handle = vn_video_session_to_handle(sess);
   VkResult result = vn_call_vkCreateVideoSessionKHR(
      dev->primary_ring, device, pCreateInfo, NULL, &sess_handle);
   if (result != VK_SUCCESS) {
      vn_object_base_fini(&sess->base);
      vk_free(alloc, sess);
      return vn_error(dev->instance, result);
   }

   *pVideoSession = sess_handle;
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
vn_DestroyVideoSessionKHR(VkDevice device,
                          VkVideoSessionKHR videoSession,
                          const VkAllocationCallbacks *pAllocator)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_video_session *sess = vn_video_session_from_handle(videoSession);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.vk.alloc;

   if (!sess)
      return;

   vn_async_vkDestroyVideoSessionKHR(dev->primary_ring, device, videoSession,
                                     NULL);

   vn_object_base_fini(&sess->base);
   vk_free(alloc, sess);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetVideoSessionMemoryRequirementsKHR(
   VkDevice device,
   VkVideoSessionKHR videoSession,
   uint32_t *pMemoryRequirementsCount,
   VkVideoSessionMemoryRequirementsKHR *pMemoryRequirements)
{
   struct vn_device *dev = vn_device_from_handle(device);

   return vn_call_vkGetVideoSessionMemoryRequirementsKHR(
      dev->primary_ring, device, videoSession, pMemoryRequirementsCount,
      pMemoryRequirements);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_BindVideoSessionMemoryKHR(
   VkDevice device,
   VkVideoSessionKHR videoSession,
   uint32_t bindSessionMemoryInfoCount,
   const VkBindVideoSessionMemoryInfoKHR *pBindSessionMemoryInfos)
{
   struct vn_device *dev = vn_device_from_handle(device);

   return vn_call_vkBindVideoSessionMemoryKHR(
      dev->primary_ring, device, videoSession, bindSessionMemoryInfoCount,
      pBindSessionMemoryInfos);
}

/* --- video session parameters -------------------------------------------- */

VKAPI_ATTR VkResult VKAPI_CALL
vn_CreateVideoSessionParametersKHR(
   VkDevice device,
   const VkVideoSessionParametersCreateInfoKHR *pCreateInfo,
   const VkAllocationCallbacks *pAllocator,
   VkVideoSessionParametersKHR *pVideoSessionParameters)
{
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.vk.alloc;

   struct vn_video_session_parameters *params =
      vk_zalloc(alloc, sizeof(*params), VN_DEFAULT_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!params)
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&params->base,
                       VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR,
                       &dev->base);

   VkVideoSessionParametersKHR params_handle =
      vn_video_session_parameters_to_handle(params);
   VkResult result = vn_call_vkCreateVideoSessionParametersKHR(
      dev->primary_ring, device, pCreateInfo, NULL, &params_handle);
   if (result != VK_SUCCESS) {
      vn_object_base_fini(&params->base);
      vk_free(alloc, params);
      return vn_error(dev->instance, result);
   }

   *pVideoSessionParameters = params_handle;
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_UpdateVideoSessionParametersKHR(
   VkDevice device,
   VkVideoSessionParametersKHR videoSessionParameters,
   const VkVideoSessionParametersUpdateInfoKHR *pUpdateInfo)
{
   struct vn_device *dev = vn_device_from_handle(device);

   /* Synchronous: the host tracks an update sequence number and rejects an
    * out-of-order update. Reporting success for a rejected update would leave
    * guest and host disagreeing about which parameter sets exist.
    */
   return vn_call_vkUpdateVideoSessionParametersKHR(
      dev->primary_ring, device, videoSessionParameters, pUpdateInfo);
}

VKAPI_ATTR void VKAPI_CALL
vn_DestroyVideoSessionParametersKHR(
   VkDevice device,
   VkVideoSessionParametersKHR videoSessionParameters,
   const VkAllocationCallbacks *pAllocator)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_video_session_parameters *params =
      vn_video_session_parameters_from_handle(videoSessionParameters);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.vk.alloc;

   if (!params)
      return;

   vn_async_vkDestroyVideoSessionParametersKHR(dev->primary_ring, device,
                                               videoSessionParameters, NULL);

   vn_object_base_fini(&params->base);
   vk_free(alloc, params);
}

/* --- command buffer recording -------------------------------------------
 *
 * The four vkCmd*Video* entrypoints are NOT here. They live in
 * vn_command_buffer.c with every other vn_Cmd* entrypoint, because
 * VN_CMD_ENQUEUE expands to a call to the file-static vn_cmd_submit(). Moving
 * the macro to a header would mean un-static-ing that function -- an upstream
 * refactor to accommodate a fork, which is exactly the kind of change that
 * makes a fork hard to rebase. Following upstream's existing file split costs
 * nothing and keeps the diff to additions.
 */
