/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#if defined(__OBJC__)

#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

namespace et
{

struct MetalState
{
	CAMetalLayer* layer = nil;
	
	id<MTLDevice> device = nil;
	id<MTLCommandQueue> queue = nil;

	id<MTLCommandBuffer> mainCommandBuffer = nil;
	id<CAMetalDrawable> mainDrawable = nil;
};

}
#endif
