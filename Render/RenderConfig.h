#pragma once

#ifndef EnableFrameResourceFence
#define EnableFrameResourceFence 1
#endif

static_assert(EnableFrameResourceFence, "Frame resources using WRITE_NO_OVERWRITE require GPU fences.");
