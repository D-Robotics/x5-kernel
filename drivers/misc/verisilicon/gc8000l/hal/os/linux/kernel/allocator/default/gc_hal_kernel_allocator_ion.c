#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_allocator.h"

#include <linux/ion.h>
#include <linux/seq_file.h>
#include <linux/mman.h>
#include <asm/cacheflush.h>

#define _GC_OBJ_ZONE gcvZONE_OS

#define gcdION_ALIGN 64

#define ION_MODULE_TYPE_GPU 0x4

extern struct ion_device *hb_ion_dev;

struct ion_priv {
	atomic_t usage;
	struct ion_client *client;
};

struct mdl_ion_priv {
	struct ion_handle *ionHandle;
};

/*
 * Debugfs support.
 */
static int gc_ion_usage_show(struct seq_file *m, void *data)
{
	gcsINFO_NODE *node     = m->private;
	gckALLOCATOR Allocator = node->device;
	struct ion_priv *priv  = Allocator->privateData;
	long long usage	       = (long long)atomic_read(&priv->usage);

	seq_puts(m, "type        n pages        bytes\n");
	seq_printf(m, "normal   %10llu %12llu\n", usage, usage * PAGE_SIZE);

	return 0;
}

static gcsINFO _InfoList[] = {
	{"ionusage", gc_ion_usage_show},
};

static void _DebugfsInit(gckALLOCATOR Allocator, gckDEBUGFS_DIR Root)
{
	gcmkVERIFY_OK(gckDEBUGFS_DIR_Init(&Allocator->debugfsDir, Root->root, "ion"));

	gcmkVERIFY_OK(gckDEBUGFS_DIR_CreateFiles(&Allocator->debugfsDir, _InfoList,
						 gcmCOUNTOF(_InfoList), Allocator));
}

static void _DebugfsCleanup(gckALLOCATOR Allocator)
{
	gcmkVERIFY_OK(gckDEBUGFS_DIR_RemoveFiles(&Allocator->debugfsDir, _InfoList,
						 gcmCOUNTOF(_InfoList)));

	gckDEBUGFS_DIR_Deinit(&Allocator->debugfsDir);
}

static gceSTATUS _IonAlloc(gckALLOCATOR Allocator, PLINUX_MDL Mdl, gctSIZE_T NumPages,
			   gctUINT32 Flags)
{
	gceSTATUS status;
	struct ion_priv *priv	     = Allocator->privateData;
	struct mdl_ion_priv *mdlPriv = gcvNULL;
	gckOS os		     = Allocator->os;
	unsigned int heap_id_mask    = ~0u;
	unsigned int ionFlags	     = ION_MODULE_TYPE_GPU << 28;

	gcmkHEADER_ARG("Mdl=%p NumPages=0x%zx Flags=0x%x", Mdl, NumPages, Flags);

	gcmkONERROR(gckOS_Allocate(os, sizeof(struct mdl_ion_priv), (gctPOINTER *)&mdlPriv));

	if (Flags & gcvALLOC_FLAG_CACHEABLE) {
		ionFlags |= ION_FLAG_CACHED;
	}

	mdlPriv->ionHandle =
		ion_alloc(priv->client, NumPages * PAGE_SIZE, gcdION_ALIGN, heap_id_mask, ionFlags);
	if (IS_ERR(mdlPriv->ionHandle)) {
		gcmkONERROR(gcvSTATUS_NOT_SUPPORTED);
	}

	Mdl->priv = mdlPriv;

	/* Statistic. */
	atomic_add(NumPages, &priv->usage);

	gcmkFOOTER_NO();
	return gcvSTATUS_OK;

OnError:
	if (mdlPriv)
		gckOS_Free(os, mdlPriv);

	gcmkFOOTER();
	return status;
}

static gceSTATUS _IonGetSGT(gckALLOCATOR Allocator, PLINUX_MDL Mdl, gctSIZE_T Offset,
			    gctSIZE_T Bytes, gctPOINTER *SGT)
{
	struct page **pages	     = gcvNULL;
	struct page *page	     = gcvNULL;
	struct sg_table *sgt	     = NULL;
	struct ion_priv *priv	     = Allocator->privateData;
	struct mdl_ion_priv *mdlPriv = Mdl->priv;

	gceSTATUS status    = gcvSTATUS_OK;
	gctSIZE_T offset    = Offset & ~PAGE_MASK;  /* Offset to the first page */
	gctSIZE_T skipPages = Offset >> PAGE_SHIFT; /* skipped pages */
	gctSIZE_T numPages  = (PAGE_ALIGN(Offset + Bytes) >> PAGE_SHIFT) - skipPages;
	phys_addr_t phys;
	size_t len;
	gctSIZE_T i;

	gcmkASSERT(Offset + Bytes <= Mdl->numPages << PAGE_SHIFT);

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL | gcdNOWARN);
	if (!sgt)
		gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);

	pages = kmalloc_array(numPages, sizeof(struct page *), GFP_KERNEL | gcdNOWARN);
	if (!pages)
		gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);

	if (ion_phys(priv->client, mdlPriv->ionHandle->id, &phys, &len) < 0)
		gcmkONERROR(gcvSTATUS_NOT_SUPPORTED);

	page = phys_to_page(phys);
	for (i = 0; i < numPages; ++i)
		pages[i] = nth_page(page, i + skipPages);

	if (sg_alloc_table_from_pages(sgt, pages, numPages, offset, Bytes, GFP_KERNEL) < 0)
		gcmkONERROR(gcvSTATUS_GENERIC_IO);

	*SGT = (gctPOINTER)sgt;

OnError:
	kfree(pages);

	if (gcmIS_ERROR(status) && sgt)
		kfree(sgt);

	return status;
}

static void _IonFree(gckALLOCATOR Allocator, PLINUX_MDL Mdl)
{
	struct ion_priv *priv	     = Allocator->privateData;
	struct mdl_ion_priv *mdlPriv = Mdl->priv;
	gckOS os		     = Allocator->os;

	ion_free(priv->client, mdlPriv->ionHandle);

	gckOS_Free(os, mdlPriv);

	/* Statistic. */
	atomic_sub(Mdl->numPages, &priv->usage);
}

static gceSTATUS _IonMmap(gckALLOCATOR Allocator, PLINUX_MDL Mdl, gctBOOL Cacheable,
			  gctSIZE_T skipPages, gctSIZE_T numPages, struct vm_area_struct *vma)
{
	struct mdl_ion_priv *mdlPriv = Mdl->priv;
	int ret;

	ret = ion_mmap(mdlPriv->ionHandle->buffer, vma);

	if (ret)
	{
		pr_err("ion_mmap failed: ret=%d, handle=%p\n", ret, mdlPriv->ionHandle);
		return gcvSTATUS_GENERIC_IO;
	}
	return gcvSTATUS_OK;
}

static void _IonUnmapUser(gckALLOCATOR Allocator, PLINUX_MDL Mdl, PLINUX_MDL_MAP MdlMap,
			  gctUINT32 Size)
{
	if (unlikely(current->mm == gcvNULL))
		/* Do nothing if process is exiting. */
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	if (vm_munmap((unsigned long)MdlMap->vmaAddr, Size) < 0) {
		gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_OS, "%s(%d): vm_munmap failed",
			       __FUNCTION__, __LINE__);
	}
#else
	down_write(&current_mm_mmap_sem);
	if (do_munmap(current->mm, (unsigned long)MdlMap->vmaAddr, Size) < 0) {
		gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_OS, "%s(%d): do_munmap failed",
			       __FUNCTION__, __LINE__);
	}
	up_write(&current_mm_mmap_sem);
#endif

	MdlMap->vma	= NULL;
	MdlMap->vmaAddr = NULL;
}

static gceSTATUS _IonMapUser(gckALLOCATOR Allocator, PLINUX_MDL Mdl, PLINUX_MDL_MAP MdlMap,
			     gctBOOL Cacheable)
{
	gctPOINTER userLogical = gcvNULL;
	gceSTATUS status       = gcvSTATUS_OK;

	gcmkHEADER_ARG("Allocator=%p Mdl=%p Cacheable=%d", Allocator, Mdl, Cacheable);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
#if gcdANON_FILE_FOR_ALLOCATOR
	userLogical = (gctPOINTER)vm_mmap(Allocator->anon_file,
#else
	userLogical = (gctPOINTER)vm_mmap(gcvNULL,
#endif
					  0L, Mdl->numPages * PAGE_SIZE, PROT_READ | PROT_WRITE,
					  MAP_SHARED | MAP_NORESERVE, 0);
#else
	down_write(&current_mm_mmap_sem);
	userLogical = (gctPOINTER)do_mmap_pgoff(gcvNULL, 0L, Mdl->numPages * PAGE_SIZE,
						PROT_READ | PROT_WRITE, MAP_SHARED, 0);
	up_write(&current_mm_mmap_sem);
#endif

	gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "%s(%d): vmaAddr->%p for phys_addr->%p",
		       __FUNCTION__, __LINE__, userLogical, Mdl);

	if (IS_ERR(userLogical)) {
		gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "%s(%d): do_mmap_pgoff error",
			       __FUNCTION__, __LINE__);

		userLogical = gcvNULL;

		gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
	}

	down_write(&current_mm_mmap_sem);

	do {
		struct vm_area_struct *vma = find_vma(current->mm, (unsigned long)userLogical);

		if (vma == gcvNULL) {
			gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "%s(%d): find_vma error",
				       __FUNCTION__, __LINE__);

			gcmkERR_BREAK(gcvSTATUS_OUT_OF_RESOURCES);
		}

		gcmkERR_BREAK(_IonMmap(Allocator, Mdl, Cacheable, 0, Mdl->numPages, vma));

		MdlMap->vmaAddr	  = userLogical;
		MdlMap->cacheable = Cacheable;
		MdlMap->vma	  = vma;
	} while (gcvFALSE);

	up_write(&current_mm_mmap_sem);

OnError:
	if (gcmIS_ERROR(status) && userLogical) {
		MdlMap->vmaAddr = userLogical;
		_IonUnmapUser(Allocator, Mdl, MdlMap, Mdl->numPages * PAGE_SIZE);
	}
	gcmkFOOTER();
	return status;
}

static gceSTATUS _IonMapKernel(gckALLOCATOR Allocator, PLINUX_MDL Mdl, gctSIZE_T Offset,
			       gctSIZE_T Bytes, gctPOINTER *Logical)
{
	struct ion_priv *priv	     = Allocator->privateData;
	struct mdl_ion_priv *mdlPriv = Mdl->priv;
	void *vaddr;

	vaddr = ion_map_kernel(priv->client, mdlPriv->ionHandle);
	if (IS_ERR(vaddr)) {
		long err = PTR_ERR(vaddr);

		pr_err("ion_map_kernel failed: handle=%p, err=%ld\n",
		       mdlPriv->ionHandle, err);

		switch (err) {
		case -EINVAL:
			return gcvSTATUS_INVALID_ARGUMENT;
		case -ENODEV:
			return gcvSTATUS_NOT_FOUND;
		default:
			return gcvSTATUS_GENERIC_IO;
		}
	}

	*Logical = (uint8_t *)vaddr + Offset;
	return gcvSTATUS_OK;
}

static gceSTATUS _IonUnmapKernel(gckALLOCATOR Allocator, PLINUX_MDL Mdl, gctPOINTER Logical)
{
	struct ion_priv *priv	     = Allocator->privateData;
	struct mdl_ion_priv *mdlPriv = Mdl->priv;

	ion_unmap_kernel(priv->client, mdlPriv->ionHandle);

	return gcvSTATUS_OK;
}

static gceSTATUS _IonCache(gckALLOCATOR Allocator, PLINUX_MDL Mdl, gctSIZE_T Offset,
			   gctPOINTER Logical, gctSIZE_T Bytes, gceCACHEOPERATION Operation)
{
	struct ion_priv *priv	     = Allocator->privateData;
	struct mdl_ion_priv *mdlPriv = Mdl->priv;
	phys_addr_t paddr;
	size_t len;
	int ret;

	ret = ion_phys(priv->client, mdlPriv->ionHandle->id, &paddr, &len);
	if (ret) {
		pr_err("ion_phys failed in _IonCache: handle=%d, ret=%d\n",
		       mdlPriv->ionHandle->id, ret);
		return gcvSTATUS_NOT_FOUND;
	}

	switch (Operation) {
	case gcvCACHE_CLEAN:
		dcache_clean_poc((unsigned long)phys_to_virt(paddr), (unsigned long)phys_to_virt(paddr) + len);
		break;
	case gcvCACHE_FLUSH:
	case gcvCACHE_INVALIDATE:
		dcache_inval_poc((unsigned long)phys_to_virt(paddr), (unsigned long)phys_to_virt(paddr) + len);
		break;
	default:
		return gcvSTATUS_INVALID_ARGUMENT;
	}

	return gcvSTATUS_OK;
}

static gceSTATUS _IonPhysical(gckALLOCATOR Allocator, PLINUX_MDL Mdl, unsigned long Offset,
			      gctPHYS_ADDR_T *Physical)
{
	struct ion_priv *priv	     = Allocator->privateData;
	struct mdl_ion_priv *mdlPriv = Mdl->priv;
	size_t len;
	int ret = 0;

	ret = ion_phys(priv->client, mdlPriv->ionHandle->id, Physical, &len);

	if (ret) {
		pr_err("ion_phys failed: id=%d, ret=%d\n", mdlPriv->ionHandle->id, ret);
		return gcvSTATUS_NOT_FOUND;
	}
	*Physical += Offset;

	return gcvSTATUS_OK;
}

/* Ion allocator operations. */
static gcsALLOCATOR_OPERATIONS IonAllocatorOperations = {
	.Alloc	     = _IonAlloc,
	.Free	     = _IonFree,
	.Mmap	     = _IonMmap,
	.MapUser     = _IonMapUser,
	.UnmapUser   = _IonUnmapUser,
	.MapKernel   = _IonMapKernel,
	.UnmapKernel = _IonUnmapKernel,
	.Cache	     = _IonCache,
	.Physical    = _IonPhysical,
	.GetSGT	     = _IonGetSGT,
};

static void _IonAllocatorDestructor(gcsALLOCATOR *Allocator)
{
	struct ion_priv *priv = Allocator->privateData;

	ion_client_destroy(priv->client);

	_DebugfsCleanup(Allocator);

	kfree(Allocator->privateData);

	kfree(Allocator);
}

/* Ion allocator entry. */
gceSTATUS _IonAllocatorInit(gckOS Os, gcsDEBUGFS_DIR *Parent, gckALLOCATOR *Allocator)
{
	gceSTATUS status;
	gckALLOCATOR allocator;
	struct ion_priv *priv = NULL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL | gcdNOWARN);

	if (!priv)
		gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);

	atomic_set(&priv->usage, 0);

	priv->client = ion_client_create(hb_ion_dev, "galcore");
	if (IS_ERR(priv->client)) {
		gcmkTRACE_ZONE(gcvLEVEL_ERROR, _GC_OBJ_ZONE, "failed to ion_client_create\n");
		gcmkONERROR(gcvSTATUS_NOT_SUPPORTED);
	}

	gcmkONERROR(gckALLOCATOR_Construct(Os, &IonAllocatorOperations, &allocator));

	/* TODO:
	 *   need to determine capability
	 */
	allocator->capability = gcvALLOC_FLAG_CONTIGUOUS | gcvALLOC_FLAG_DMABUF_EXPORTABLE |
				gcvALLOC_FLAG_4GB_ADDR | gcvALLOC_FLAG_CPU_ACCESS |
				gcvALLOC_FLAG_FROM_USER | gcvALLOC_FLAG_CACHEABLE
#if gcdENABLE_40BIT_VA
				| gcvALLOC_FLAG_32BIT_VA | gcvALLOC_FLAG_PRIOR_32BIT_VA
#endif
#if gcdENABLE_VIDEO_MEMORY_MIRROR
				| gcvALLOC_FLAG_WITH_MIRROR
#endif
		;

	/* Register private data. */
	allocator->privateData = priv;
	allocator->destructor  = _IonAllocatorDestructor;

	_DebugfsInit(allocator, Parent);

	*Allocator = allocator;

	return gcvSTATUS_OK;

OnError:

	if (priv && priv->client && !IS_ERR(priv->client)) {
		ion_client_destroy(priv->client);
	}

	if (priv) {
		kfree(priv);
	}

	return status;
}
