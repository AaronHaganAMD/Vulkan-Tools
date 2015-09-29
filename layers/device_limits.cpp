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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <memory>

#include "vk_loader_platform.h"
#include "vk_dispatch_table_helper.h"
#include "vk_struct_string_helper_cpp.h"
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wwrite-strings"
#endif
#include "vk_struct_size_helper.h"
#include "device_limits.h"
#include "vk_layer_config.h"
#include "vk_debug_marker_layer.h"
// The following is #included again to catch certain OS-specific functions
// being used:
#include "vk_loader_platform.h"
#include "vk_layer_msg.h"
#include "vk_layer_table.h"
#include "vk_layer_debug_marker_table.h"
#include "vk_layer_data.h"
#include "vk_layer_logging.h"
#include "vk_layer_extension_utils.h"
#include "vk_layer_utils.h"


// This struct will be stored in a map hashed by the dispatchable object
struct layer_data {
    debug_report_data *report_data;
    VkDbgMsgCallback logging_callback;
    // Track state of each instance
    unique_ptr<INSTANCE_STATE> instanceState;
    unique_ptr<PHYSICAL_DEVICE_STATE> physicalDeviceState;
    VkPhysicalDeviceFeatures actualPhysicalDeviceFeatures;
    VkPhysicalDeviceFeatures requestedPhysicalDeviceFeatures;
    unique_ptr<VkPhysicalDeviceProperties> physicalDeviceProperties;
    // Vector indices correspond to queueFamilyIndex
    vector<unique_ptr<VkQueueFamilyProperties>> queueFamilyProperties;

    layer_data() :
        report_data(nullptr),
        logging_callback(nullptr),
        instanceState(nullptr),
        physicalDeviceState(nullptr),
        physicalDeviceProperties(nullptr),
        actualPhysicalDeviceFeatures(),
        requestedPhysicalDeviceFeatures()
    {};
};

static std::unordered_map<void *, layer_data *> layer_data_map;
static device_table_map device_limits_device_table_map;
static instance_table_map device_limits_instance_table_map;

static unordered_map<VkDevice, VkPhysicalDevice> deviceMap;

struct devExts {
    bool debug_marker_enabled;
};

static std::unordered_map<void *, struct devExts> deviceExtMap;

static LOADER_PLATFORM_THREAD_ONCE_DECLARATION(g_initOnce);

// TODO : This can be much smarter, using separate locks for separate global data
static int globalLockInitialized = 0;
static loader_platform_thread_mutex globalLock;

template layer_data *get_my_data_ptr<layer_data>(
        void *data_key,
        std::unordered_map<void *, layer_data *> &data_map);

static void init_device_limits(layer_data *my_data)
{
    uint32_t report_flags = 0;
    uint32_t debug_action = 0;
    FILE *log_output = NULL;
    const char *option_str;
    // initialize DeviceLimits options
    report_flags = getLayerOptionFlags("DeviceLimitsReportFlags", 0);
    getLayerOptionEnum("DeviceLimitsDebugAction", (uint32_t *) &debug_action);

    if (debug_action & VK_DBG_LAYER_ACTION_LOG_MSG)
    {
        option_str = getLayerOption("DeviceLimitsLogFilename");
        log_output = getLayerLogOutput(option_str, "DeviceLimits");
        layer_create_msg_callback(my_data->report_data, report_flags, log_callback, (void *) log_output, &my_data->logging_callback);
    }

    if (!globalLockInitialized)
    {
        // TODO/TBD: Need to delete this mutex sometime.  How???  One
        // suggestion is to call this during vkCreateInstance(), and then we
        // can clean it up during vkDestroyInstance().  However, that requires
        // that the layer have per-instance locks.  We need to come back and
        // address this soon.
        loader_platform_thread_create_mutex(&globalLock);
        globalLockInitialized = 1;
    }
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance)
{
    VkLayerInstanceDispatchTable *pTable = get_dispatch_table(device_limits_instance_table_map,*pInstance);
    VkResult result = pTable->CreateInstance(pCreateInfo, pInstance);

    if (result == VK_SUCCESS) {
        layer_data *my_data = get_my_data_ptr(get_dispatch_key(*pInstance), layer_data_map);
        my_data->report_data = debug_report_create_instance(
                                   pTable,
                                   *pInstance,
                                   pCreateInfo->extensionCount,
                                   pCreateInfo->ppEnabledExtensionNames);

        init_device_limits(my_data);
        my_data->instanceState = unique_ptr<INSTANCE_STATE>(new INSTANCE_STATE());
    }
    return result;
}

/* hook DestroyInstance to remove tableInstanceMap entry */
VK_LAYER_EXPORT void VKAPI vkDestroyInstance(VkInstance instance)
{
    dispatch_key key = get_dispatch_key(instance);
    VkLayerInstanceDispatchTable *pTable = get_dispatch_table(device_limits_instance_table_map, instance);
    pTable->DestroyInstance(instance);

    // Clean up logging callback, if any
    layer_data *my_data = get_my_data_ptr(key, layer_data_map);
    if (my_data->logging_callback) {
        layer_destroy_msg_callback(my_data->report_data, my_data->logging_callback);
    }

    layer_debug_report_destroy_instance(my_data->report_data);
    layer_data_map.erase(pTable);
    my_data->instanceState.release();
    device_limits_instance_table_map.erase(key);
}

VK_LAYER_EXPORT VkResult VKAPI vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices)
{
    VkBool32 skipCall = VK_FALSE;
    layer_data *my_data = get_my_data_ptr(get_dispatch_key(instance), layer_data_map);
    if (my_data->instanceState) {
        // For this instance, flag when vkEnumeratePhysicalDevices goes to QUERY_COUNT and then QUERY_DETAILS
        if (NULL == pPhysicalDevices) {
            my_data->instanceState->vkEnumeratePhysicalDevicesState = QUERY_COUNT;
        } else {
            if (UNCALLED == my_data->instanceState->vkEnumeratePhysicalDevicesState) {
                // Flag error here, shouldn't be calling this without having queried count
                skipCall |= log_msg(my_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_INSTANCE, 0, 0, DEVLIMITS_MUST_QUERY_COUNT, "DL",
                    "Invalid call sequence to vkEnumeratePhysicalDevices() w/ non-NULL pPhysicalDevices. You should first call vkEnumeratePhysicalDevices() w/ NULL pPhysicalDevices to query pPhysicalDeviceCount.");
            } // TODO : Could also flag a warning if re-calling this function in QUERY_DETAILS state
            else if (my_data->instanceState->physicalDevicesCount != *pPhysicalDeviceCount) {
                skipCall |= log_msg(my_data->report_data, VK_DBG_REPORT_WARN_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_COUNT_MISMATCH, "DL",
                    "Call to vkEnumeratePhysicalDevices() w/ pPhysicalDeviceCount value %u, but actual count supported by this instance is %u.", *pPhysicalDeviceCount, my_data->instanceState->physicalDevicesCount);
            }
            my_data->instanceState->vkEnumeratePhysicalDevicesState = QUERY_DETAILS;
        }
        if (skipCall)
            return VK_ERROR_VALIDATION_FAILED;
        VkResult result = get_dispatch_table(device_limits_instance_table_map,instance)->EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
        if (NULL == pPhysicalDevices) {
            my_data->instanceState->physicalDevicesCount = *pPhysicalDeviceCount;
        } else { // Save physical devices
            for (uint32_t i=0; i < *pPhysicalDeviceCount; i++) {
                layer_data *phy_dev_data = get_my_data_ptr(get_dispatch_key(pPhysicalDevices[i]), layer_data_map);
                phy_dev_data->physicalDeviceState = unique_ptr<PHYSICAL_DEVICE_STATE>(new PHYSICAL_DEVICE_STATE());
                // Init actual features for each physical device
                get_dispatch_table(device_limits_instance_table_map,instance)->GetPhysicalDeviceFeatures(pPhysicalDevices[i], &(phy_dev_data->actualPhysicalDeviceFeatures));
            }
        }
        return result;
    } else {
        log_msg(my_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_INSTANCE, 0, 0, DEVLIMITS_INVALID_INSTANCE, "DL",
            "Invalid instance (%#" PRIxLEAST64 ") passed into vkEnumeratePhysicalDevices().", instance);
    }
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures)
{
    layer_data *phy_dev_data = get_my_data_ptr(get_dispatch_key(physicalDevice), layer_data_map);
    phy_dev_data->physicalDeviceState->vkGetPhysicalDeviceFeaturesState = QUERY_DETAILS;
    VkResult result = get_dispatch_table(device_limits_instance_table_map, physicalDevice)->GetPhysicalDeviceFeatures(physicalDevice, pFeatures);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties)
{
    VkResult result = get_dispatch_table(device_limits_instance_table_map, physicalDevice)->GetPhysicalDeviceFormatProperties(
                                         physicalDevice, format, pFormatProperties);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties)
{
    VkResult result = get_dispatch_table(device_limits_instance_table_map, physicalDevice)->GetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties)
{
    VkResult result = get_dispatch_table(device_limits_instance_table_map, physicalDevice)->
                                    GetPhysicalDeviceProperties(physicalDevice, pProperties);
    if (VK_SUCCESS == result) {
        // Save Properties
        layer_data *phy_dev_data = get_my_data_ptr(get_dispatch_key(physicalDevice), layer_data_map);
        phy_dev_data->physicalDeviceProperties =
            unique_ptr<VkPhysicalDeviceProperties>(new VkPhysicalDeviceProperties(*pProperties));
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkQueueFamilyProperties* pQueueFamilyProperties)
{
    VkBool32 skipCall = VK_FALSE;
    layer_data *phy_dev_data = get_my_data_ptr(get_dispatch_key(physicalDevice), layer_data_map);
    if (phy_dev_data->physicalDeviceState) {
        if (NULL == pQueueFamilyProperties) {
            phy_dev_data->physicalDeviceState->vkGetPhysicalDeviceQueueFamilyPropertiesState = QUERY_COUNT;
        } else {
            // Verify that for each physical device, this function is called first with NULL pQueueFamilyProperties ptr in order to get count
            if (UNCALLED == phy_dev_data->physicalDeviceState->vkGetPhysicalDeviceQueueFamilyPropertiesState) {
                skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_MUST_QUERY_COUNT, "DL",
                    "Invalid call sequence to vkGetPhysicalDeviceQueueFamilyProperties() w/ non-NULL pQueueFamilyProperties. You should first call vkGetPhysicalDeviceQueueFamilyProperties() w/ NULL pQueueFamilyProperties to query pCount.");
            }
            // Then verify that pCount that is passed in on second call matches what was returned
            if (phy_dev_data->physicalDeviceState->queueFamilyPropertiesCount != *pCount) {
                skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_WARN_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_COUNT_MISMATCH, "DL",
                    "Call to vkGetPhysicalDeviceQueueFamilyProperties() w/ pCount value %u, but actual count supported by this physicalDevice is %u.", *pCount, phy_dev_data->physicalDeviceState->queueFamilyPropertiesCount);
            }
            phy_dev_data->physicalDeviceState->vkGetPhysicalDeviceQueueFamilyPropertiesState = QUERY_DETAILS;
        }
        if (skipCall)
            return VK_ERROR_VALIDATION_FAILED;
        VkResult result = get_dispatch_table(device_limits_instance_table_map, physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(physicalDevice, pCount, pQueueFamilyProperties);
        if (NULL == pQueueFamilyProperties) {
            phy_dev_data->physicalDeviceState->queueFamilyPropertiesCount = *pCount;
        } else { // Save queue family properties
            phy_dev_data->queueFamilyProperties.reserve(*pCount);
            for (uint32_t i=0; i < *pCount; i++) {
                phy_dev_data->queueFamilyProperties.emplace_back(new VkQueueFamilyProperties(pQueueFamilyProperties[i]));
            }
        }
        return result;
    } else {
        log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_INVALID_PHYSICAL_DEVICE, "DL",
            "Invalid physicalDevice (%#" PRIxLEAST64 ") passed into vkGetPhysicalDeviceQueueFamilyProperties().", physicalDevice);
    }
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    VkResult result = get_dispatch_table(device_limits_instance_table_map, physicalDevice)->GetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, uint32_t samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pNumProperties, VkSparseImageFormatProperties* pProperties)
{
    VkResult result = get_dispatch_table(device_limits_instance_table_map, physicalDevice)->GetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pNumProperties, pProperties);
    return result;
}

VK_LAYER_EXPORT void VKAPI vkCmdSetViewport(
    VkCmdBuffer                         cmdBuffer,
    uint32_t                            viewportCount,
    const VkViewport*                   pViewports)
{
    VkBool32 skipCall = VK_FALSE;
    /* TODO: Verify viewportCount < maxViewports from VkPhysicalDeviceLimits */
    if (VK_FALSE == skipCall) {
        get_dispatch_table(device_limits_device_table_map, cmdBuffer)->CmdSetViewport(cmdBuffer, viewportCount, pViewports);
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdSetScissor(
    VkCmdBuffer                         cmdBuffer,
    uint32_t                            scissorCount,
    const VkRect2D*                     pScissors)
{
    VkBool32 skipCall = VK_FALSE;
    /* TODO: Verify scissorCount < maxViewports from VkPhysicalDeviceLimits */
    /* TODO: viewportCount and scissorCount must match at draw time */
    if (VK_FALSE == skipCall) {
        get_dispatch_table(device_limits_device_table_map, cmdBuffer)->CmdSetScissor(cmdBuffer, scissorCount, pScissors);
    }
}

static void createDeviceRegisterExtensions(const VkDeviceCreateInfo* pCreateInfo, VkDevice device)
{
    uint32_t i;
    VkLayerDispatchTable *pDisp =  get_dispatch_table(device_limits_device_table_map, device);
    deviceExtMap[pDisp].debug_marker_enabled = false;

    for (i = 0; i < pCreateInfo->extensionCount; i++) {
        if (strcmp(pCreateInfo->ppEnabledExtensionNames[i], DEBUG_MARKER_EXTENSION_NAME) == 0) {
            /* Found a matching extension name, mark it enabled and init dispatch table*/
            initDebugMarkerTable(device);
            deviceExtMap[pDisp].debug_marker_enabled = true;
        }

    }
}

// Verify that features have been queried and verify that requested features are available
static VkBool32 validate_features_request(layer_data *phy_dev_data)
{
    VkBool32 skipCall = VK_FALSE;
    // Verify that all of the requested features are available
    // Get ptrs into actual and requested structs and if requested is 1 but actual is 0, request is invalid
    VkBool32* actual = (VkBool32*)&(phy_dev_data->actualPhysicalDeviceFeatures);
    VkBool32* requested = (VkBool32*)&(phy_dev_data->requestedPhysicalDeviceFeatures);
    // TODO : This is a nice, compact way to loop through struct, but a bad way to report issues
    //  Need to provide the struct member name with the issue. To do that seems like we'll
    //  have to loop through each struct member which should be done w/ codegen to keep in synch.
    uint32_t errors = 0;
    uint32_t totalBools = sizeof(VkPhysicalDeviceFeatures)/sizeof(VkBool32);
    for (uint32_t i = 0; i < totalBools; i++) {
        if (requested[i] > actual[i]) {
            skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_INVALID_FEATURE_REQUESTED, "DL",
                "While calling vkCreateDevice(), requesting feature #%u in VkPhysicalDeviceFeatures struct, which is not available on this device.", i);
            errors++;
        }
    }
    if (errors && (UNCALLED == phy_dev_data->physicalDeviceState->vkGetPhysicalDeviceFeaturesState)) {
        // If user didn't request features, notify them that they should
        skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_WARN_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_INVALID_FEATURE_REQUESTED, "DL",
                "You requested features that are unavailable on this device. You should first query feature availability by calling vkGetPhysicalDeviceFeatures().");
    }
    return skipCall;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDevice(VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice)
{
    VkBool32 skipCall = VK_FALSE;
    layer_data *phy_dev_data = get_my_data_ptr(get_dispatch_key(gpu), layer_data_map);
    // First check is app has actually requested queueFamilyProperties
    if (!phy_dev_data->physicalDeviceState) {
        skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_MUST_QUERY_COUNT, "DL",
            "Invalid call to vkCreateDevice() w/o first calling vkEnumeratePhysicalDevices().");
    } else if (QUERY_DETAILS != phy_dev_data->physicalDeviceState->vkGetPhysicalDeviceQueueFamilyPropertiesState) {
        skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_WARN_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_INVALID_QUEUE_CREATE_REQUEST, "DL",
            "Invalid call to vkCreateDevice() w/o first calling vkGetPhysicalDeviceQueueFamilyProperties().");
    } else {
        // Check that the requested queue properties are valid
        for (uint32_t i=0; i<pCreateInfo->queueRecordCount; i++) {
            uint32_t requestedIndex = pCreateInfo->pRequestedQueues[i].queueFamilyIndex;
            if (phy_dev_data->queueFamilyProperties.size() <= requestedIndex) { // requested index is out of bounds for this physical device
                skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_INVALID_QUEUE_CREATE_REQUEST, "DL",
                    "Invalid queue create request in vkCreateDevice(). Invalid queueFamilyIndex %u requested.", requestedIndex);
            } else if (pCreateInfo->pRequestedQueues[i].queueCount > phy_dev_data->queueFamilyProperties[requestedIndex]->queueCount) {
                skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_INVALID_QUEUE_CREATE_REQUEST, "DL",
                    "Invalid queue create request in vkCreateDevice(). QueueFamilyIndex %u only has %u queues, but requested queueCount is %u.", requestedIndex, phy_dev_data->queueFamilyProperties[requestedIndex]->queueCount, pCreateInfo->pRequestedQueues[i].queueCount);
            }
        }
    }
    // Check that any requested features are available
    if (pCreateInfo->pEnabledFeatures) {
        phy_dev_data->requestedPhysicalDeviceFeatures = *(pCreateInfo->pEnabledFeatures);
        skipCall |= validate_features_request(phy_dev_data);
    }
    if (skipCall)
        return VK_ERROR_VALIDATION_FAILED;

    VkLayerDispatchTable *pDeviceTable = get_dispatch_table(device_limits_device_table_map, *pDevice);
    VkResult result = pDeviceTable->CreateDevice(gpu, pCreateInfo, pDevice);
    if (result == VK_SUCCESS) {
        VkLayerDispatchTable *pTable = get_dispatch_table(device_limits_device_table_map, *pDevice);
        layer_data *my_device_data = get_my_data_ptr(get_dispatch_key(*pDevice), layer_data_map);
        my_device_data->report_data = layer_debug_report_create_device(phy_dev_data->report_data, *pDevice);
        createDeviceRegisterExtensions(pCreateInfo, *pDevice);
        deviceMap[*pDevice] = gpu;
    }
    return result;
}

VK_LAYER_EXPORT void VKAPI vkDestroyDevice(VkDevice device)
{
    // Free device lifetime allocations
    dispatch_key key = get_dispatch_key(device);
    VkLayerDispatchTable *pDisp = get_dispatch_table(device_limits_device_table_map, device);
    pDisp->DestroyDevice(device);
    deviceExtMap.erase(pDisp);
    device_limits_device_table_map.erase(key);
    tableDebugMarkerMap.erase(pDisp);
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateCommandPool(VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, VkCmdPool* pCmdPool)
{
    // TODO : Verify that requested QueueFamilyIndex for this pool exists
    VkResult result = get_dispatch_table(device_limits_device_table_map, device)->CreateCommandPool(device, pCreateInfo, pCmdPool);
    return result;
}

VK_LAYER_EXPORT void VKAPI vkDestroyCommandPool(VkDevice device, VkCmdPool cmdPool)
{
    get_dispatch_table(device_limits_device_table_map, device)->DestroyCommandPool(device, cmdPool);
}

VK_LAYER_EXPORT VkResult VKAPI vkResetCommandPool(VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags)
{
    VkResult result = get_dispatch_table(device_limits_device_table_map, device)->ResetCommandPool(device, cmdPool, flags);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateCommandBuffer(VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer)
{
    VkResult result = get_dispatch_table(device_limits_device_table_map, device)->CreateCommandBuffer(device, pCreateInfo, pCmdBuffer);
    return result;
}
 
VK_LAYER_EXPORT void VKAPI vkDestroyCommandBuffer(VkDevice device, VkCmdBuffer commandBuffer)
{

    get_dispatch_table(device_limits_device_table_map, device)->DestroyCommandBuffer(device, commandBuffer);
}

VK_LAYER_EXPORT VkResult VKAPI vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
{
    VkBool32 skipCall = VK_FALSE;
    VkPhysicalDevice gpu = deviceMap[device];
    layer_data *phy_dev_data = get_my_data_ptr(get_dispatch_key(gpu), layer_data_map);
    if (queueFamilyIndex >= phy_dev_data->queueFamilyProperties.size()) { // requested index is out of bounds for this physical device
        skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_INVALID_QUEUE_CREATE_REQUEST, "DL",
            "Invalid queueFamilyIndex %u requested in vkGetDeviceQueue().", queueFamilyIndex);
    } else if (queueIndex >= phy_dev_data->queueFamilyProperties[queueFamilyIndex]->queueCount) {
        skipCall |= log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PHYSICAL_DEVICE, 0, 0, DEVLIMITS_INVALID_QUEUE_CREATE_REQUEST, "DL",
            "Invalid queue request in vkGetDeviceQueue(). QueueFamilyIndex %u only has %u queues, but requested queueIndex is %u.", queueFamilyIndex, phy_dev_data->queueFamilyProperties[queueFamilyIndex]->queueCount, queueIndex);
    }
    if (skipCall)
        return VK_ERROR_VALIDATION_FAILED;
    VkResult result = get_dispatch_table(device_limits_device_table_map, device)->GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateImage(
    VkDevice                 device,
    const VkImageCreateInfo *pCreateInfo,
    VkImage                 *pImage)
{
    VkBool32                skipCall       = VK_FALSE;
    VkResult                result         = VK_ERROR_VALIDATION_FAILED;
    VkPhysicalDevice        physicalDevice = deviceMap[device];
    VkImageType             type;
    VkImageFormatProperties ImageFormatProperties = {0};

    // Internal call to get format info.  Still goes through layers, could potentially go directly to ICD.
    get_dispatch_table(device_limits_instance_table_map, physicalDevice)->GetPhysicalDeviceImageFormatProperties(
                       physicalDevice, pCreateInfo->format, pCreateInfo->imageType, pCreateInfo->tiling,
                       pCreateInfo->usage, pCreateInfo->flags, &ImageFormatProperties);

    layer_data *phy_dev_data = get_my_data_ptr(get_dispatch_key(physicalDevice), layer_data_map);
    if (!phy_dev_data->physicalDeviceProperties) {
        skipCall = log_msg(phy_dev_data->report_data, VK_DBG_REPORT_WARN_BIT, VK_OBJECT_TYPE_IMAGE, (uint64_t)pImage, 0,
                       DEVLIMITS_MUST_QUERY_PROPERTIES, "DL",
                       "CreateImage called before querying device properties ");
    }
    
    uint32_t imageGranularity = phy_dev_data->physicalDeviceProperties->limits.bufferImageGranularity;
    imageGranularity = imageGranularity == 1 ? 0 : imageGranularity;

    if ((pCreateInfo->extent.depth  > ImageFormatProperties.maxExtent.depth)  ||
        (pCreateInfo->extent.width  > ImageFormatProperties.maxExtent.width)  ||
        (pCreateInfo->extent.height > ImageFormatProperties.maxExtent.height)) {
        skipCall = log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_IMAGE, (uint64_t)pImage, 0,
                       DEVLIMITS_LIMITS_VIOLATION, "DL",
                       "CreateImage extents exceed allowable limits for format: "
                       "Width = %d Height = %d Depth = %d:  Limits for Width = %d Height = %d Depth = %d for format %s.",
                       pCreateInfo->extent.width, pCreateInfo->extent.height, pCreateInfo->extent.depth,
                       ImageFormatProperties.maxExtent.width, ImageFormatProperties.maxExtent.height, ImageFormatProperties.maxExtent.depth,
                       string_VkFormat(pCreateInfo->format));

    }

    uint64_t totalSize = ((uint64_t)pCreateInfo->extent.width               *
                          (uint64_t)pCreateInfo->extent.height              *
                          (uint64_t)pCreateInfo->extent.depth               *
                          (uint64_t)pCreateInfo->arraySize                  *
                          (uint64_t)pCreateInfo->samples                    *
                          (uint64_t)vk_format_get_size(pCreateInfo->format) +
                          (uint64_t)imageGranularity ) & ~(uint64_t)imageGranularity;

    if (totalSize > ImageFormatProperties.maxResourceSize) {
        skipCall = log_msg(phy_dev_data->report_data, VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_IMAGE, (uint64_t)pImage, 0,
                       DEVLIMITS_LIMITS_VIOLATION, "DL",
                       "CreateImage resource size exceeds allowable maximum "
                       "Image resource size = %#" PRIxLEAST64 ", maximum resource size = %#" PRIxLEAST64 " ",
                       totalSize, ImageFormatProperties.maxResourceSize);
    }
    if (VK_FALSE == skipCall) {
        result = get_dispatch_table(device_limits_device_table_map, device)->CreateImage(device, pCreateInfo, pImage);
    }
    return result;
}


VK_LAYER_EXPORT VkResult VKAPI vkDbgCreateMsgCallback(
    VkInstance                          instance,
    VkFlags                             msgFlags,
    const PFN_vkDbgMsgCallback          pfnMsgCallback,
    void*                               pUserData,
    VkDbgMsgCallback*                   pMsgCallback)
{
    VkLayerInstanceDispatchTable *pTable = get_dispatch_table(device_limits_instance_table_map, instance);
    VkResult res = pTable->DbgCreateMsgCallback(instance, msgFlags, pfnMsgCallback, pUserData, pMsgCallback);
    if (VK_SUCCESS == res) {
        layer_data *my_data = get_my_data_ptr(get_dispatch_key(instance), layer_data_map);
        res = layer_create_msg_callback(my_data->report_data, msgFlags, pfnMsgCallback, pUserData, pMsgCallback);
    }
    return res;
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgDestroyMsgCallback(
    VkInstance                          instance,
    VkDbgMsgCallback                    msgCallback)
{
    VkLayerInstanceDispatchTable *pTable = get_dispatch_table(device_limits_instance_table_map, instance);
    VkResult res = pTable->DbgDestroyMsgCallback(instance, msgCallback);
    layer_data *my_data = get_my_data_ptr(get_dispatch_key(instance), layer_data_map);
    layer_destroy_msg_callback(my_data->report_data, msgCallback);
    return res;
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI vkGetDeviceProcAddr(VkDevice dev, const char* funcName)
{
    if (dev == NULL)
        return NULL;

    /* loader uses this to force layer initialization; device object is wrapped */
    if (!strcmp(funcName, "vkGetDeviceProcAddr")) {
        initDeviceTable(device_limits_device_table_map, (const VkBaseLayerObject *) dev);
        return (PFN_vkVoidFunction) vkGetDeviceProcAddr;
    }
    if (!strcmp(funcName, "vkCreateDevice"))
        return (PFN_vkVoidFunction) vkCreateDevice;
    if (!strcmp(funcName, "vkDestroyDevice"))
        return (PFN_vkVoidFunction) vkDestroyDevice;
    if (!strcmp(funcName, "vkGetDeviceQueue"))
        return (PFN_vkVoidFunction) vkGetDeviceQueue;
    if (!strcmp(funcName, "vkCreateImage"))
        return (PFN_vkVoidFunction) vkCreateImage;
    if (!strcmp(funcName, "CreateCommandPool"))
        return (PFN_vkVoidFunction) vkCreateCommandPool;
    if (!strcmp(funcName, "DestroyCommandPool"))
        return (PFN_vkVoidFunction) vkDestroyCommandPool;
    if (!strcmp(funcName, "ResetCommandPool"))
        return (PFN_vkVoidFunction) vkResetCommandPool;
    if (!strcmp(funcName, "vkCreateCommandBuffer"))
        return (PFN_vkVoidFunction) vkCreateCommandBuffer;
    if (!strcmp(funcName, "vkDestroyCommandBuffer"))
        return (PFN_vkVoidFunction) vkDestroyCommandBuffer;

    VkLayerDispatchTable* pTable = get_dispatch_table(device_limits_device_table_map, dev);
    {
        if (pTable->GetDeviceProcAddr == NULL)
            return NULL;
        return pTable->GetDeviceProcAddr(dev, funcName);
    }
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI vkGetInstanceProcAddr(VkInstance instance, const char* funcName)
{
    PFN_vkVoidFunction fptr;
    if (instance == NULL)
        return NULL;

    /* loader uses this to force layer initialization; instance object is wrapped */
    if (!strcmp(funcName, "vkGetInstanceProcAddr")) {
        initInstanceTable(device_limits_instance_table_map, (const VkBaseLayerObject *) instance);
        return (PFN_vkVoidFunction) vkGetInstanceProcAddr;
    }
    if (!strcmp(funcName, "vkCreateInstance"))
        return (PFN_vkVoidFunction) vkCreateInstance;
    if (!strcmp(funcName, "vkDestroyInstance"))
        return (PFN_vkVoidFunction) vkDestroyInstance;
    if (!strcmp(funcName, "vkEnumeratePhysicalDevices"))
        return (PFN_vkVoidFunction) vkEnumeratePhysicalDevices;
    if (!strcmp(funcName, "vkGetPhysicalDeviceFeatures"))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceFeatures;
    if (!strcmp(funcName, "vkGetPhysicalDeviceFormatProperties"))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceFormatProperties;
    if (!strcmp(funcName, "vkGetPhysicalDeviceImageFormatProperties"))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceImageFormatProperties;
    if (!strcmp(funcName, "vkGetPhysicalDeviceProperties"))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceProperties;
    if (!strcmp(funcName, "vkGetPhysicalDeviceQueueFamilyProperties"))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceQueueFamilyProperties;
    if (!strcmp(funcName, "vkGetPhysicalDeviceMemoryProperties"))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceMemoryProperties;
    if (!strcmp(funcName, "vkGetPhysicalDeviceSparseImageFormatProperties"))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceSparseImageFormatProperties;

    layer_data *my_data = get_my_data_ptr(get_dispatch_key(instance), layer_data_map);
    fptr = debug_report_get_instance_proc_addr(my_data->report_data, funcName);
    if (fptr)
        return fptr;

    {
        VkLayerInstanceDispatchTable* pTable = get_dispatch_table(device_limits_instance_table_map, instance);
        if (pTable->GetInstanceProcAddr == NULL)
            return NULL;
        return pTable->GetInstanceProcAddr(instance, funcName);
    }
}
