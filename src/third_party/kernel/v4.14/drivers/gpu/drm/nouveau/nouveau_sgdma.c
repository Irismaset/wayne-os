// SPDX-License-Identifier: GPL-2.0
#include <linux/pagemap.h>
#include <linux/slab.h>

#include "nouveau_drv.h"
#include "nouveau_ttm.h"

struct nouveau_sgdma_be {
	/* this has to be the first field so populate/unpopulated in
	 * nouve_bo.c works properly, otherwise have to move them here
	 */
	struct ttm_dma_tt ttm;
	struct nvkm_mem *node;
};

static void
nouveau_sgdma_destroy(struct ttm_tt *ttm)
{
	struct nouveau_sgdma_be *nvbe = (struct nouveau_sgdma_be *)ttm;

	if (ttm) {
		ttm_dma_tt_fini(&nvbe->ttm);
		kfree(nvbe);
	}
}

static int
nv04_sgdma_bind(struct ttm_tt *ttm, struct ttm_mem_reg *reg)
{
	struct nouveau_sgdma_be *nvbe = (struct nouveau_sgdma_be *)ttm;
	struct nvkm_mem *node = reg->mm_node;

	if (ttm->sg) {
		node->sg    = ttm->sg;
		node->pages = NULL;
	} else {
		node->sg    = NULL;
		node->pages = nvbe->ttm.dma_address;
	}
	node->size = (reg->num_pages << PAGE_SHIFT) >> 12;

	nvkm_vm_map(&node->vma[0], node);
	nvbe->node = node;
	return 0;
}

static int
nv04_sgdma_unbind(struct ttm_tt *ttm)
{
	struct nouveau_sgdma_be *nvbe = (struct nouveau_sgdma_be *)ttm;
	nvkm_vm_unmap(&nvbe->node->vma[0]);
	return 0;
}

static struct ttm_backend_func nv04_sgdma_backend = {
	.bind			= nv04_sgdma_bind,
	.unbind			= nv04_sgdma_unbind,
	.destroy		= nouveau_sgdma_destroy
};

static int
nv50_sgdma_bind(struct ttm_tt *ttm, struct ttm_mem_reg *reg)
{
	struct nouveau_sgdma_be *nvbe = (struct nouveau_sgdma_be *)ttm;
	struct nvkm_mem *node = reg->mm_node;

	/* noop: bound in move_notify() */
	if (ttm->sg) {
		node->sg    = ttm->sg;
		node->pages = NULL;
	} else {
		node->sg    = NULL;
		node->pages = nvbe->ttm.dma_address;
	}
	node->size = (reg->num_pages << PAGE_SHIFT) >> 12;
	return 0;
}

static int
nv50_sgdma_unbind(struct ttm_tt *ttm)
{
	/* noop: unbound in move_notify() */
	return 0;
}

static struct ttm_backend_func nv50_sgdma_backend = {
	.bind			= nv50_sgdma_bind,
	.unbind			= nv50_sgdma_unbind,
	.destroy		= nouveau_sgdma_destroy
};

struct ttm_tt *
nouveau_sgdma_create_ttm(struct ttm_buffer_object *bo, uint32_t page_flags)
{
	struct nouveau_drm *drm = nouveau_bdev(bo->bdev);
	struct nouveau_sgdma_be *nvbe;

	nvbe = kzalloc(sizeof(*nvbe), GFP_KERNEL);
	if (!nvbe)
		return NULL;

	if (drm->client.device.info.family < NV_DEVICE_INFO_V0_TESLA)
		nvbe->ttm.ttm.func = &nv04_sgdma_backend;
	else
		nvbe->ttm.ttm.func = &nv50_sgdma_backend;

	if (ttm_dma_tt_init(&nvbe->ttm, bo, page_flags)) {
		kfree(nvbe);
		return NULL;
	}
	return &nvbe->ttm.ttm;
}
