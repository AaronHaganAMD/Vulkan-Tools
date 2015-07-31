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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <list>
#include <map>
#include <vector>
#include <fstream>

using namespace std;

#include "vk_loader_platform.h"
#include "vk_dispatch_table_helper.h"
#include "vk_struct_string_helper_cpp.h"
#include "vk_layer_config.h"
// The following is #included again to catch certain OS-specific functions
// being used:
#include "vk_loader_platform.h"
#include "vk_layer_table.h"
#include "vk_layer_extension_utils.h"


struct devExts {
    bool wsi_enabled;
};
static std::unordered_map<void *, struct devExts>     deviceExtMap;
static device_table_map screenshot_device_table_map;

static int globalLockInitialized = 0;
static loader_platform_thread_mutex globalLock;

// unordered map, associates a swap chain with a device, image extent, format, and
// list of images
typedef struct
{
    VkDevice device;
    VkExtent2D imageExtent;
    VkFormat format;
    VkImage *imageList;
} SwapchainMapStruct;
static unordered_map<uint64_t, SwapchainMapStruct *> swapchainMap;

// unordered map, associates an image with a device, image extent, and format
typedef struct
{
    VkDevice device;
    VkExtent2D imageExtent;
    VkFormat format;
} ImageMapStruct;
static unordered_map<uint64_t, ImageMapStruct *> imageMap;

// unordered map, associates a device with a queue and a cmdPool
typedef struct
{
    VkQueue  queue;
    VkCmdPool cmdPool;
} DeviceMapStruct;
static unordered_map<VkDevice, DeviceMapStruct *> deviceMap;

// List of frames to we will get a screenshot of
static vector<int> screenshotFrames;

// Flag indicating we have queried _VK_SCREENSHOT env var
static bool screenshotEnvQueried = false;

static void init_screenshot()
{
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

static void writePPM( const char *filename, VkImage image1)
{
    VkImage image2;
    VkResult err;
    int x, y;
    const char *ptr;
    VkDeviceMemory mem2;
    VkCmdBuffer cmdBuffer;
    VkDevice device = imageMap[image1.handle]->device;
    VkQueue queue = deviceMap[device]->queue;
    int width = imageMap[image1.handle]->imageExtent.width;
    int height = imageMap[image1.handle]->imageExtent.height;
    VkFormat format = imageMap[image1.handle]->format;
    const VkImageSubresource sr = {VK_IMAGE_ASPECT_COLOR, 0, 0};
    VkSubresourceLayout sr_layout;
    const VkImageCreateInfo imgCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        VK_IMAGE_TYPE_2D,
        format,
        {width, height, 1},
        1,
        1,
        1,
        VK_IMAGE_TILING_LINEAR,
        (VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT|VK_IMAGE_USAGE_STORAGE_BIT),
        0
    };
    VkMemoryAllocInfo memAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO,
        NULL,
        0,     // allocationSize, queried later
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_UNCACHED_BIT |
            VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT)
    };
    const VkCmdBufferCreateInfo createCommandBufferInfo = {
        VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
        NULL,
        deviceMap[device]->cmdPool,
        VK_CMD_BUFFER_LEVEL_PRIMARY,
        0
    };
    const VkCmdBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
        NULL,
        VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT |
            VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,
    };
    const VkImageCopy imageCopyRegion = {
        {VK_IMAGE_ASPECT_COLOR, 0, 0},
        {0, 0, 0},
        {VK_IMAGE_ASPECT_COLOR, 0, 0},
        {0, 0, 0},
        {width, height, 1}
    };
    VkMemoryRequirements memRequirements;
    uint32_t num_allocations = 0;
    size_t num_alloc_size = sizeof(num_allocations);
    VkLayerDispatchTable* pTableDevice = get_dispatch_table(screenshot_device_table_map, device);
    VkLayerDispatchTable* pTableQueue = get_dispatch_table(screenshot_device_table_map, queue);
    VkLayerDispatchTable* pTableCmdBuffer;

    if (imageMap.empty() || imageMap.find(image1.handle) == imageMap.end())
        return;

    // The VkImage image1 we are going to dump may not be mappable,
    // and/or it may have a tiling mode of optimal rather than linear.
    // To make sure we have an image that we can map and read linearly, we:
    //     create image2 that is mappable and linear
    //     copy image1 to image2
    //     map image2
    //     read from image2's mapped memeory.

    err = pTableDevice->CreateImage(device, &imgCreateInfo, &image2);
    assert(!err);

    err = pTableDevice->GetImageMemoryRequirements(device, image2, &memRequirements);
    assert(!err);

    memAllocInfo.allocationSize = memRequirements.size;
    err = pTableDevice->AllocMemory(device, &memAllocInfo, &mem2);
    assert(!err);

    err = pTableQueue->BindImageMemory(device, image2, mem2, 0);
    assert(!err);

    err = pTableDevice->CreateCommandBuffer(device, &createCommandBufferInfo,  &cmdBuffer);
    assert(!err);

    screenshot_device_table_map.emplace(cmdBuffer, pTableDevice);
    pTableCmdBuffer = screenshot_device_table_map[cmdBuffer];

    err = pTableCmdBuffer->BeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo);
    assert(!err);

    pTableCmdBuffer->CmdCopyImage(cmdBuffer, image1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   image2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, &imageCopyRegion);

    err = pTableCmdBuffer->EndCommandBuffer(cmdBuffer);
    assert(!err);

    err = pTableQueue->QueueSubmit(queue, 1, &cmdBuffer, VK_NULL_HANDLE);
    assert(!err);

    err = pTableQueue->QueueWaitIdle(queue);
    assert(!err);

    err =  pTableDevice->DeviceWaitIdle(device);
    assert(!err);

    err =  pTableDevice->GetImageSubresourceLayout(device, image2, &sr, &sr_layout);
    assert(!err);

    err = pTableDevice->MapMemory(device, mem2, 0, 0, 0, (void **) &ptr );
    assert(!err);

    ptr += sr_layout.offset;

    ofstream file(filename, ios::binary);

    file << "P6\n";
    file << width << "\n";
    file << height << "\n";
    file << 255 << "\n";

    for (y = 0; y < height; y++) {
        const unsigned int *row = (const unsigned int*) ptr;
        if (format == VK_FORMAT_B8G8R8A8_UNORM)
        {
            for (x = 0; x < width; x++) {
                unsigned int swapped;
                swapped = (*row & 0xff00ff00) | (*row & 0x000000ff) << 16 | (*row & 0x00ff0000) >> 16;
                file.write((char *)&swapped, 3);
                row++;
            }
        }
        else if (format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            for (x = 0; x < width; x++) {
                file.write((char *)row, 3);
                row++;
            }
        }
        else
        {
            // TODO: add support for additional formats
            printf("Unrecognized image format\n");
            break;
       }
       ptr += sr_layout.rowPitch;
    }
    file.close();

    // Clean up
    err = pTableDevice->UnmapMemory(device, mem2);
    assert(!err);
    err = pTableDevice->FreeMemory(device, mem2);
    assert(!err);
    err = pTableDevice->DestroyCommandBuffer(device, cmdBuffer);
    assert(!err);
}


static void createDeviceRegisterExtensions(const VkDeviceCreateInfo* pCreateInfo, VkDevice device)
{
    uint32_t i, ext_idx;
    VkLayerDispatchTable *pDisp  = get_dispatch_table(screenshot_device_table_map, device);
    deviceExtMap[pDisp].wsi_enabled = false;
    for (i = 0; i < pCreateInfo->extensionCount; i++) {
        if (strcmp(pCreateInfo->ppEnabledExtensionNames[i], VK_WSI_DEVICE_SWAPCHAIN_EXTENSION_NAME) == 0)
            deviceExtMap[pDisp].wsi_enabled = true;
    }
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDevice(
    VkPhysicalDevice          gpu,
    const VkDeviceCreateInfo *pCreateInfo,
    VkDevice                 *pDevice)
{
    VkLayerDispatchTable *pDisp  = get_dispatch_table(screenshot_device_table_map, *pDevice);
    VkResult result = pDisp->CreateDevice(gpu, pCreateInfo, pDevice);

    if (result == VK_SUCCESS) {
        init_screenshot();
        createDeviceRegisterExtensions(pCreateInfo, *pDevice);
    }

    return result;
}

/* TODO: Probably need a DestroyDevice as well */

static const VkLayerProperties ss_device_layers[] = {
    {
        "ScreenShot",
        VK_API_VERSION,
        VK_MAKE_VERSION(0, 1, 0),
        "Layer: ScreenShot",
    }
};

VK_LAYER_EXPORT VkResult VKAPI vkGetGlobalExtensionProperties(
        const char *pLayerName,
        uint32_t *pCount,
        VkExtensionProperties* pProperties)
{
    /* ScreenShot does not have any global extensions */
    return util_GetExtensionProperties(0, NULL, pCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI vkGetGlobalLayerProperties(
        uint32_t *pCount,
        VkLayerProperties*    pProperties)
{
    /* ScreenShot does not have any global layers */
    return util_GetLayerProperties(0, NULL,
                                   pCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceExtensionProperties(
        VkPhysicalDevice                            physicalDevice,
        const char*                                 pLayerName,
        uint32_t*                                   pCount,
        VkExtensionProperties*                      pProperties)
{
    /* ScreenShot does not have any physical device extensions */
    return util_GetExtensionProperties(0, NULL, pCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceLayerProperties(
        VkPhysicalDevice                            physicalDevice,
        uint32_t*                                   pCount,
        VkLayerProperties*                          pProperties)
{
    return util_GetLayerProperties(ARRAY_SIZE(ss_device_layers),
                                   ss_device_layers,
                                   pCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI vkGetDeviceQueue(
    VkDevice  device,
    uint32_t  queueNodeIndex,
    uint32_t  queueIndex,
    VkQueue   *pQueue)
{
    VkLayerDispatchTable* pTable = screenshot_device_table_map[device];
    VkResult result = get_dispatch_table(screenshot_device_table_map, device)->GetDeviceQueue(device, queueNodeIndex, queueIndex, pQueue);

    loader_platform_thread_lock_mutex(&globalLock);
    if (screenshotEnvQueried && screenshotFrames.empty()) {
        // We are all done taking screenshots, so don't do anything else
        loader_platform_thread_unlock_mutex(&globalLock);
        return result;
    }

    if (result == VK_SUCCESS) {
        screenshot_device_table_map.emplace(*pQueue, pTable);

        // Create a device to queue mapping
        DeviceMapStruct *deviceMapElem = new DeviceMapStruct;
        deviceMapElem->queue = *pQueue;
        deviceMap.insert(make_pair(device, deviceMapElem));
    }
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateCommandPool(
    VkDevice  device,
    const VkCmdPoolCreateInfo *pCreateInfo,
    VkCmdPool *pCmdPool)
{
    VkLayerDispatchTable* pTable = screenshot_device_table_map[device];
    VkResult result = get_dispatch_table(screenshot_device_table_map, device)->CreateCommandPool(device, pCreateInfo, pCmdPool);

    loader_platform_thread_lock_mutex(&globalLock);
    if (screenshotEnvQueried && screenshotFrames.empty()) {
        // We are all done taking screenshots, so don't do anything else
        loader_platform_thread_unlock_mutex(&globalLock);
        return result;
    }

    // TODO: What if not already in deviceMap???
    deviceMap[device]->cmdPool = *pCmdPool;
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateSwapChainWSI(
    VkDevice                        device,
    const VkSwapChainCreateInfoWSI *pCreateInfo,
    VkSwapChainWSI                 *pSwapChain)
{
    VkLayerDispatchTable* pTable = screenshot_device_table_map[device];
    VkResult result = get_dispatch_table(screenshot_device_table_map, device)->CreateSwapChainWSI(device, pCreateInfo, pSwapChain);

    loader_platform_thread_lock_mutex(&globalLock);
    if (screenshotEnvQueried && screenshotFrames.empty()) {
        // We are all done taking screenshots, so don't do anything else
        loader_platform_thread_unlock_mutex(&globalLock);
        return result;
    }

    if (result == VK_SUCCESS)
    {
        // Create a mapping for a swapchain to a device, image extent, and format
        SwapchainMapStruct *swapchainMapElem = new SwapchainMapStruct;
        swapchainMapElem->device = device;
        swapchainMapElem->imageExtent = pCreateInfo->imageExtent;
        swapchainMapElem->format = pCreateInfo->imageFormat;
        swapchainMap.insert(make_pair(pSwapChain->handle, swapchainMapElem));

        // Create a mapping for the swapchain object into the dispatch table
        screenshot_device_table_map.emplace((void *)pSwapChain->handle, pTable);
    }
    loader_platform_thread_unlock_mutex(&globalLock);

    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetSwapChainInfoWSI(
    VkDevice                device,
    VkSwapChainWSI          swapChain,
    VkSwapChainInfoTypeWSI  infoType,
    size_t                 *pDataSize,
    void                   *pData)
{
    VkResult result = get_dispatch_table(screenshot_device_table_map, device)->GetSwapChainInfoWSI(device, swapChain, infoType, pDataSize, pData);

    loader_platform_thread_lock_mutex(&globalLock);
    if (screenshotEnvQueried && screenshotFrames.empty()) {
        // We are all done taking screenshots, so don't do anything else
        loader_platform_thread_unlock_mutex(&globalLock);
        return result;
    }

    if (result == VK_SUCCESS &&
        pData &&
        !swapchainMap.empty() && swapchainMap.find(swapChain.handle) != swapchainMap.end())
    {   
        VkSwapChainImagePropertiesWSI *swapChainImageInfo = (VkSwapChainImagePropertiesWSI *)pData;
        int i;

        for (i=0; i<*pDataSize/sizeof(VkSwapChainImagePropertiesWSI); i++,swapChainImageInfo++)
        {
            // Create a mapping for an image to a device, image extent, and format
            ImageMapStruct *imageMapElem = new ImageMapStruct;
            imageMapElem->device =  swapchainMap[swapChain.handle]->device;
            imageMapElem->imageExtent = swapchainMap[swapChain.handle]->imageExtent;
            imageMapElem->format = swapchainMap[swapChain.handle]->format;
            imageMap.insert(make_pair(swapChainImageInfo->image.handle, imageMapElem));
        }

        // Add list of images to swapchain to image map
        SwapchainMapStruct *swapchainMapElem = swapchainMap[swapChain.handle];
        if (i >= 1 && swapchainMapElem)
        {
            VkImage *imageList = new VkImage[i];
            swapchainMapElem->imageList = imageList;
            VkSwapChainImagePropertiesWSI *swapChainImageInfo = (VkSwapChainImagePropertiesWSI *)pData;
            for (int j=0; j<i; j++,swapChainImageInfo++)
            {
                swapchainMapElem->imageList[j] =  swapChainImageInfo->image.handle;
            }
        }

    }   
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkQueuePresentWSI(VkQueue queue, VkPresentInfoWSI* pPresentInfo)
{
    static int frameNumber = 0;
    if (frameNumber == 10) {fflush(stdout); /* *((int*)0)=0; */ }
    VkResult result = get_dispatch_table(screenshot_device_table_map, queue)->QueuePresentWSI(queue, pPresentInfo);

    loader_platform_thread_lock_mutex(&globalLock);

    if (!screenshotEnvQueried)
    {
        const char *_vk_screenshot = getenv("_VK_SCREENSHOT");
        if (_vk_screenshot && *_vk_screenshot)
        {
            string spec(_vk_screenshot), word;
            size_t start = 0, comma = 0;

            while (start < spec.size()) {
                int frameToAdd;
                comma = spec.find(',', start);
                if (comma == string::npos)
                    word = string(spec, start);
                else
                    word = string(spec, start, comma - start);
                frameToAdd=atoi(word.c_str());
                // Add the frame number to list, but only do it if the word started with a digit and if
                // it's not already in the list
                if (*(word.c_str()) >= '0' && *(word.c_str()) <= '9' &&
                    find(screenshotFrames.begin(), screenshotFrames.end(), frameToAdd) == screenshotFrames.end())
                {
                    screenshotFrames.push_back(frameToAdd);
                }
                if (comma == string::npos)
                    break;
                start = comma + 1;
            }
        }
        screenshotEnvQueried = true;
    }
    

    if (result == VK_SUCCESS && !screenshotFrames.empty())
    {
        vector<int>::iterator it;
        it = find(screenshotFrames.begin(), screenshotFrames.end(), frameNumber);
        if (it != screenshotFrames.end())
        {
            string fileName;
            fileName = to_string(frameNumber) + ".ppm";

            VkImage image;
            uint64_t swapChain;
            // We'll dump only one image: the first
            swapChain = pPresentInfo->swapChains[0].handle;
            image = swapchainMap[swapChain]->imageList[pPresentInfo->imageIndices[0]];
            writePPM(fileName.c_str(), image);
            screenshotFrames.erase(it);

            if (screenshotFrames.empty())
            {
				// Free all our maps since we are done with them.
                for (auto it = swapchainMap.begin(); it != swapchainMap.end(); it++)
                {
                    SwapchainMapStruct *swapchainMapElem = it->second;
                    delete swapchainMapElem;
                }
                for (auto it = imageMap.begin(); it != imageMap.end(); it++)
                {
                    ImageMapStruct *imageMapElem = it->second;
                    delete imageMapElem;
                }
                for (auto it = deviceMap.begin(); it != deviceMap.end(); it++)
                {
                    DeviceMapStruct *deviceMapElem = it->second;
                    delete deviceMapElem;
                }
                swapchainMap.clear();
                imageMap.clear();
                deviceMap.clear();
            }
        }
    }
    frameNumber++;
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI vkGetDeviceProcAddr(
    VkDevice         dev,
    const char       *funcName)
{
    if (dev == NULL) {
        return NULL;
    }

    /* loader uses this to force layer initialization; device object is wrapped */
    if (!strcmp(funcName, "vkGetDeviceProcAddr")) {
        initDeviceTable(screenshot_device_table_map, (const VkBaseLayerObject *) dev);
        return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    }
    if (!strcmp(funcName, "vkCreateDevice"))
        return (PFN_vkVoidFunction) vkCreateDevice;

    if (!strcmp(funcName, "vkGetDeviceQueue"))
        return (PFN_vkVoidFunction) vkGetDeviceQueue;

    if (!strcmp(funcName, "vkCreateCommandPool"))
        return (PFN_vkVoidFunction) vkCreateCommandPool;

    VkLayerDispatchTable *pDisp =  get_dispatch_table(screenshot_device_table_map, dev);
    if (deviceExtMap.size() == 0 || deviceExtMap[pDisp].wsi_enabled)
    {
        if (!strcmp(funcName, "vkCreateSwapChainWSI"))
            return (PFN_vkVoidFunction) vkCreateSwapChainWSI;
        if (!strcmp(funcName, "vkGetSwapChainInfoWSI"))
            return (PFN_vkVoidFunction) vkGetSwapChainInfoWSI;
        if (!strcmp(funcName, "vkQueuePresentWSI"))
            return (PFN_vkVoidFunction) vkQueuePresentWSI;
    }

    if (pDisp->GetDeviceProcAddr == NULL)
        return NULL;
    return pDisp->GetDeviceProcAddr(dev, funcName);
}


VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI vkGetInstanceProcAddr(VkInstance instance, const char* funcName)
{
    if (instance == VK_NULL_HANDLE) {
        return NULL;
    }

    /* loader uses this to force layer initialization; instance object is wrapped */
    if (!strcmp("vkGetInstanceProcAddr", funcName)) {
        initInstanceTable((const VkBaseLayerObject *) instance);
        return (PFN_vkVoidFunction) vkGetInstanceProcAddr;
    }

    VkLayerInstanceDispatchTable* pTable = instance_dispatch_table(instance);
    if (pTable->GetInstanceProcAddr == NULL)
        return NULL;
    return pTable->GetInstanceProcAddr(instance, funcName);
}
