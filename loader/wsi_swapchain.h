/*
 *
 * Copyright (C) 2015 Valve Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Ian Elliott <ian@lunarg.com>
 *
 */

#include "vk_loader_platform.h"
#include "loader.h"
#include "vulkan/VK_KHR_surface.h"

bool wsi_swapchain_instance_gpa(struct loader_instance *ptr_instance,
                                 const char* name, void **addr);
void wsi_swapchain_add_instance_extensions(
        const struct loader_instance *inst,
        struct loader_extension_list *ext_list);

void wsi_swapchain_create_instance(
        struct loader_instance *ptr_instance,
        const VkInstanceCreateInfo *pCreateInfo);


VKAPI_ATTR VkResult VKAPI_CALL loader_GetPhysicalDeviceSurfaceSupportKHR(
        VkPhysicalDevice                        physicalDevice,
        uint32_t                                queueNodeIndex,
        const VkSurfaceDescriptionKHR*          pSurfaceDescription,
        VkBool32*                               pSupported);
