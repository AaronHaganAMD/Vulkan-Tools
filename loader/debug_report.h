/*
 * Vulkan
 *
 * Copyright (C) 2015 LunarG, Inc.
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
 * Authors:
 *   Jon Ashburn <jon@lunarg.com>
 *   Courtney Goeltzenleuchter <courtney@lunarg.com>
 */

#include "vk_loader_platform.h"
#include "loader.h"
#include "vk_debug_report_lunarg.h"
    /*
     * CreateMsgCallback is global and needs to be
     * applied to all layers and ICDs.
     * What happens if a layer is enabled on both the instance chain
     * as well as the device chain and a call to CreateMsgCallback is made?
     * Do we need to make sure that each layer / driver only gets called once?
     * Should a layer implementing support for CreateMsgCallback only be allowed (?)
     * to live on one chain? Or maybe make it the application's responsibility.
     * If the app enables DRAW_STATE on at both CreateInstance time and CreateDevice
     * time, CreateMsgCallback will call the DRAW_STATE layer twice. Once via
     * the instance chain and once via the device chain.
     * The loader should only return the DEBUG_REPORT extension as supported
     * for the GetGlobalExtensionSupport call. That should help eliminate one
     * duplication.
     * Since the instance chain requires us iterating over the available ICDs
     * and each ICD will have it's own unique MsgCallback object we need to
     * track those objects to give back the right one.
     * This also implies that the loader has to intercept vkDestroyObject and
     * if the extension is enabled and the object type is a MsgCallback then
     * we must translate the object into the proper ICD specific ones.
     * DestroyObject works on a device chain. Should not be what's destroying
     * the MsgCallback object. That needs to be an instance thing. So, since
     * we used an instance to create it, we need a custom Destroy that also
     * takes an instance. That way we can iterate over the ICDs properly.
     * Example use:
     * CreateInstance: DEBUG_REPORT
     *   Loader will create instance chain with enabled extensions.
     *   TODO: Should validation layers be enabled here? If not, they will not be in the instance chain.
     * fn = GetProcAddr(INSTANCE, "vkCreateMsgCallback") -> point to loader's vkCreateMsgCallback
     * App creates a callback object: fn(..., &MsgCallbackObject1)
     * Have only established the instance chain so far. Loader will call the instance chain.
     * Each layer in the instance chain will call down to the next layer, terminating with
     * the CreateMsgCallback loader terminator function that creates the actual MsgCallbackObject1 object.
     * The loader CreateMsgCallback terminator will iterate over the ICDs.
     * Calling each ICD that supports vkCreateMsgCallback and collect answers in icd_msg_callback_map here.
     * As result is sent back up the chain each layer has opportunity to record the callback operation and
     * appropriate MsgCallback object.
     * ...
     * Any reports matching the flags set in MsgCallbackObject1 will generate the defined callback behavior
     * in the layer / ICD that initiated that report.
     * ...
     * CreateDevice: MemTracker:...
     * App does not include DEBUG_REPORT as that is a global extension.
     * TODO: GetExtensionSupport must not report DEBUG_REPORT when using instance.
     * App MUST include any desired validation layers or they will not participate in the device call chain.
     * App creates a callback object: fn(..., &MsgCallbackObject2)
     * Loader's vkCreateMsgCallback is called.
     * Loader sends call down instance chain - this is a global extension - any validation layer that was
     * enabled at CreateInstance will be able to register the callback. Loader will iterate over the ICDs and
     * will record the ICD's version of the MsgCallback2 object here.
     * ...
     * Any report will go to the layer's report function and it will check the flags for MsgCallbackObject1
     * and MsgCallbackObject2 and take the appropriate action as indicated by the app.
     * ...
     * App calls vkDestroyMsgCallback( MsgCallbackObject1 )
     * Loader's DestroyMsgCallback is where call starts. DestroyMsgCallback will be sent down instance chain
     * ending in the loader's DestroyMsgCallback terminator which will iterate over the ICD's destroying each
     * ICD version of that MsgCallback object and then destroy the loader's version of the object.
     * Any reports generated after this will only have MsgCallbackObject2 available.
     */

void debug_report_add_instance_extensions(
        const struct loader_instance *inst,
        struct loader_extension_list *ext_list);

void debug_report_create_instance(
        struct loader_instance *ptr_instance,
        const VkInstanceCreateInfo *pCreateInfo);

bool debug_report_instance_gpa(
        struct loader_instance *ptr_instance,
        const char* name,
        void **addr);

VkResult VKAPI loader_DbgCreateMsgCallback(
    VkInstance                          instance,
    VkFlags                             msgFlags,
    const PFN_vkDbgMsgCallback          pfnMsgCallback,
    void*                               pUserData,
    VkDbgMsgCallback*                   pMsgCallback);

VkResult VKAPI loader_DbgDestroyMsgCallback(
    VkInstance                          instance,
    VkDbgMsgCallback                    msgCallback);
