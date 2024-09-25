/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright (C) 2020-2023, Horizon Robotics Co., Ltd.
 *                    All rights reserved.
 *************************************************************************/
/**
 * @file hobot_vdsp_dev.c
 *
 * @NO{S05E05C01}
 * @ASIL{B}
 */

#include "hobot_vdsp.h"

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp filp info create
 *
 * @param[in] vdev: vdsp device driver_data
 * @param[in] filp: file pointer
 *
 * @retval vdsp_filp_info: succeed
 * @retval other: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct vdsp_filp_info *vdsp_filp_info_create(struct hobot_vdsp_dev_data *vdev,
	struct file *filp)
{
	struct vdsp_filp_info *vdsp_filp_data;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
	vdsp_filp_data = (struct vdsp_filp_info *)kzalloc(sizeof(struct vdsp_filp_info), GFP_KERNEL);
	if (NULL == vdsp_filp_data) {
		dev_err(vdev->dev, "%s vdsp%d Can't alloc vdsp filp data mem\n",
			__func__, vdev->dsp_id);
		return NULL;
	}

	kref_init(&vdsp_filp_data->ref);
	vdsp_filp_data->vdev = vdev;
	vdsp_filp_data->filp = filp;
	vdsp_filp_data->vdsp_smmu_root = RB_ROOT;

	return vdsp_filp_data;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp filp info destroy by kref
 *
 * @param[in] vdsp_kref: kref of vdsp filp info
 *
 * @retval None
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
//coverity[HIS_metric_violation:SUPPRESS], ## violation reason SYSSW_V_VOCF_06
static void vdsp_filp_info_destroy(struct kref *vdsp_kref)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct vdsp_filp_info *vdsp_filp_data =
			container_of(vdsp_kref, struct vdsp_filp_info, ref);
	struct hobot_vdsp_dev_data *vdev = vdsp_filp_data->vdev;
	struct vdsp_smmu_info *smmu_entry;
	struct rb_node *nsmmu;

	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
	if (!RB_EMPTY_NODE(&vdsp_filp_data->filpnode)) {
		for (nsmmu = rb_first(&vdsp_filp_data->vdsp_smmu_root); nsmmu != NULL; ) {
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
			smmu_entry = rb_entry(nsmmu, struct vdsp_smmu_info, smmunode);
			nsmmu = rb_next(nsmmu);
			//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
			if (!RB_EMPTY_NODE(&smmu_entry->smmunode)) {
				//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
				//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
				//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS] ## violation reason SYSSW_V_8.5_01
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
				dev_dbg(vdev->dev, "%s-%d vdsp%d fd-%d, va=0x%llx, size=0x%llx.\n", __func__, __LINE__,
					vdev->dsp_id, smmu_entry->mem_fd, smmu_entry->mem_va, smmu_entry->mem_size);
				ion_iommu_unmap_ion_fd(vdev->dev, &smmu_entry->dmabuf);
				rb_erase(&smmu_entry->smmunode, &vdsp_filp_data->vdsp_smmu_root);
				kfree(smmu_entry);
			}
		}
		rb_erase(&vdsp_filp_data->filpnode, &vdev->vdsp_filp_root);
	}

	kfree(vdsp_filp_data);
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief try to destroy vdsp filp info by vdsp_kref
 *
 * @param[in] vdsp_filp_data: data of vdsp filp info
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t vdsp_filp_info_put(struct vdsp_filp_info *vdsp_filp_data)
{
	int32_t ret = 0;
	struct hobot_vdsp_dev_data *vdev = vdsp_filp_data->vdev;

	mutex_lock(&vdev->vdsp_smmu_lock);
	ret = kref_put(&vdsp_filp_data->ref, vdsp_filp_info_destroy);
	mutex_unlock(&vdev->vdsp_smmu_lock);

	return ret;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp filp info add to rbtree
 *
 * @param[in] vdev: vdsp device driver_data
 * @param[in] vdsp_filp_info: data of vdsp filp info
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t vdsp_filp_info_add(struct hobot_vdsp_dev_data *vdev,
	struct vdsp_filp_info * vdsp_filp_data)
{
	struct rb_node *parent = NULL;
	struct rb_node **p;
	struct vdsp_filp_info *entry;

	p = &vdev->vdsp_filp_root.rb_node;
	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		entry = rb_entry(parent, struct vdsp_filp_info, filpnode);
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if ((uint64_t)vdsp_filp_data->filp < (uint64_t)entry->filp) {
			p = &(*p)->rb_left;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		} else if ((uint64_t)vdsp_filp_data->filp > (uint64_t)entry->filp) {
			p = &(*p)->rb_right;
		} else {
			dev_err(vdev->dev, "%s-%d: vdsp%d failed.\n",
				__func__, __LINE__, vdev->dsp_id);
			return -EINVAL;
		}
	}
	rb_link_node(&vdsp_filp_data->filpnode, parent, p);
	rb_insert_color(&vdsp_filp_data->filpnode, &vdev->vdsp_filp_root);

	return 0;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief lookup vdsp filp info from rbtree
 *
 * @param[in] vdev: vdsp device driver_data
 * @param[in] filp: file pointer
 *
 * @retval vdsp_filp_info: succeed
 * @retval other: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct vdsp_filp_info *vdsp_filp_info_lookup(struct hobot_vdsp_dev_data *vdev,
	struct file *filp)
{
	struct vdsp_filp_info *entry;
	struct rb_node *n = vdev->vdsp_filp_root.rb_node;

	while (n != NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		entry = rb_entry(n, struct vdsp_filp_info, filpnode);
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if ((uint64_t)filp < (uint64_t)entry->filp)
			n = n->rb_left;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		else if ((uint64_t)filp > (uint64_t)entry->filp)
			n = n->rb_right;
		else
			return entry;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS] ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	dev_dbg(vdev->dev, "%s vdsp%d Can't lookup vdsp filp data.\n",
		__func__, vdev->dsp_id);
	return NULL;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp smmu info create
 *
 * @param[in] vdsp_filp_data: data of vdsp filp info
 * @param[in] fdmem: file descriptor of allocated mem
 * @param[in] va: virtual address of allocated mem
 * @param[in] size: map size
 *
 * @retval vdsp_smmu_info: succeed
 * @retval other: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct vdsp_smmu_info *vdsp_smmu_info_create(struct vdsp_filp_info *vdsp_filp_data,
	int32_t fdmem, uint64_t va, uint64_t size)
{
	struct vdsp_smmu_info *vdsp_smmu_data;
	struct hobot_vdsp_dev_data *vdev = vdsp_filp_data->vdev;
	int32_t vdsp_id = vdev->dsp_id;
	struct device *pdev = vdev->dev;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
	vdsp_smmu_data = (struct vdsp_smmu_info *)kzalloc(sizeof(struct vdsp_smmu_info), GFP_KERNEL);
	if (NULL == vdsp_smmu_data) {
		dev_err(pdev, "%s vdsp%d Can't alloc vdsp smmu data mem\n", __func__, vdsp_id);
		return NULL;
	}

	kref_init(&vdsp_smmu_data->ref);
	vdsp_smmu_data->filpinfo = vdsp_filp_data;
	vdsp_smmu_data->mem_fd = fdmem;
	vdsp_smmu_data->mem_va = va;
	vdsp_smmu_data->mem_size = size;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	dev_dbg(pdev, "%s-%d vdsp%d fd-%d, va=0x%llx, size=0x%llx.\n", __func__, __LINE__,
		vdsp_id, fdmem, va, size);
	return vdsp_smmu_data;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp smmu info destroy by kref
 *
 * @param[in] vdsp_kref: kref of vdsp smmu info
 *
 * @retval None
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void vdsp_smmu_info_destroy(struct kref *vdsp_kref)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct vdsp_smmu_info *vdsp_smmu_data =
			container_of(vdsp_kref, struct vdsp_smmu_info, ref);
	struct hobot_vdsp_dev_data *vdev = vdsp_smmu_data->filpinfo->vdev;

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	dev_dbg(vdev->dev, "%s-%d vdsp%d fd-%d, va=0x%llx, size=0x%llx.\n", __func__, __LINE__,
		vdev->dsp_id, vdsp_smmu_data->mem_fd, vdsp_smmu_data->mem_va, vdsp_smmu_data->mem_size);

	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
	if (!RB_EMPTY_NODE(&vdsp_smmu_data->smmunode))
		rb_erase(&vdsp_smmu_data->smmunode, &vdsp_smmu_data->filpinfo->vdsp_smmu_root);

	kfree(vdsp_smmu_data);
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp smmu info add to rbtree
 *
 * @param[in] vdev: vdsp device driver_data
 * @param[in] vdsp_smmu_data: data of vdsp smmu info
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
//coverity[HIS_metric_violation:SUPPRESS], ## violation reason SYSSW_V_VOCF_06
static int32_t vdsp_smmu_info_add(struct vdsp_smmu_info * vdsp_smmu_data)
{
	struct vdsp_filp_info *vdsp_filp_data = vdsp_smmu_data->filpinfo;
	struct hobot_vdsp_dev_data *vdev = vdsp_filp_data->vdev;
	struct rb_node *parent = NULL;
	struct rb_node **p;
	struct vdsp_smmu_info *entry;

	p = &vdsp_filp_data->vdsp_smmu_root.rb_node;
	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		entry = rb_entry(parent, struct vdsp_smmu_info, smmunode);

		if (vdsp_smmu_data->mem_fd < entry->mem_fd) {
			p = &(*p)->rb_left;
		} else if (vdsp_smmu_data->mem_fd > entry->mem_fd) {
			p = &(*p)->rb_right;
		} else {
			dev_err(vdev->dev, "%s: vdsp%d mem_fd %d failed\n", __func__,
				vdev->dsp_id, vdsp_smmu_data->mem_fd);
			return -EINVAL;
		}
	}
	rb_link_node(&vdsp_smmu_data->smmunode, parent, p);
	rb_insert_color(&vdsp_smmu_data->smmunode, &vdsp_filp_data->vdsp_smmu_root);
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	dev_dbg(vdev->dev, "%s-%d vdsp%d fd-%d, va=0x%llx, size=0x%llx.\n", __func__, __LINE__,
		vdev->dsp_id, vdsp_smmu_data->mem_fd, vdsp_smmu_data->mem_va, vdsp_smmu_data->mem_size);
	return 0;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief lookup vdsp smmu info from rbtree
 *
 * @param[in] vdev: vdsp device driver_data
 * @param[in] fdmem: file descriptor of allocated mem
 *
 * @retval vdsp_smmu_info: succeed
 * @retval other: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct vdsp_smmu_info *vdsp_smmu_info_lookup(struct vdsp_filp_info *vdsp_filp_data,
	int32_t fdmem)
{
	struct rb_node *n = vdsp_filp_data->vdsp_smmu_root.rb_node;
	struct vdsp_smmu_info *entry;

	while (n != NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		entry = rb_entry(n, struct vdsp_smmu_info, smmunode);
		if (fdmem < entry->mem_fd)
			n = n->rb_left;
		else if (fdmem > entry->mem_fd)
			n = n->rb_right;
		else
			return entry;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS] ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	dev_dbg(vdsp_filp_data->vdev->dev, "%s vdsp%d Can't lookup vdsp smmu data: fdmem-%d\n",
		__func__, vdsp_filp_data->vdev->dsp_id, fdmem);
	return NULL;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief try to destroy vdsp smmu info by kref
 *
 * @param[in] vdsp_smmu_data: data of vdsp smmu info
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t vdsp_smmu_info_put(struct vdsp_smmu_info *vdsp_smmu_data)
{
	int32_t ret = 0;
	struct hobot_vdsp_dev_data *vdev = vdsp_smmu_data->filpinfo->vdev;

	mutex_lock(&vdev->vdsp_smmu_lock);
	ret = kref_put(&vdsp_smmu_data->ref, vdsp_smmu_info_destroy);
	mutex_unlock(&vdev->vdsp_smmu_lock);

	return ret;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp smmu info destroy all
 *
 * @param[in] vdev: vdsp device driver_data
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
//coverity[HIS_metric_violation:SUPPRESS], ## violation reason SYSSW_V_VOCF_06
static void vdsp_smmu_info_destroy_all(struct hobot_vdsp_dev_data *vdev)
{
	struct vdsp_filp_info *filp_entry;
	struct vdsp_smmu_info *smmu_entry;
	struct rb_node *nfilp;
	struct rb_node *nsmmu;

	mutex_lock(&vdev->vdsp_smmu_lock);

	for (nfilp = rb_first(&vdev->vdsp_filp_root); nfilp != NULL;) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		filp_entry = rb_entry(nfilp, struct vdsp_filp_info, filpnode);
		nfilp = rb_next(nfilp);
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if (!RB_EMPTY_NODE(&filp_entry->filpnode)) {
			for (nsmmu = rb_first(&filp_entry->vdsp_smmu_root); nsmmu != NULL; ) {
				//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
				//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
				smmu_entry = rb_entry(nsmmu, struct vdsp_smmu_info, smmunode);
				nsmmu = rb_next(nsmmu);
				//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
				if (!RB_EMPTY_NODE(&smmu_entry->smmunode)) {
					//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
					//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
					dev_dbg(vdev->dev, "%s-%d vdsp%d fd-%d, va=0x%llx, size=0x%llx.\n", __func__, __LINE__,
						vdev->dsp_id, smmu_entry->mem_fd, smmu_entry->mem_va, smmu_entry->mem_size);
					ion_iommu_unmap_ion_fd(vdev->dev, &smmu_entry->dmabuf);
					rb_erase(&smmu_entry->smmunode, &filp_entry->vdsp_smmu_root);
					kfree(smmu_entry);
				}
			}
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
			dev_dbg(vdev->dev, "%s-%d vdsp%d rb_erase & kfree.\n", __func__, __LINE__, vdev->dsp_id);
			rb_erase(&filp_entry->filpnode, &vdev->vdsp_filp_root);
			kfree(filp_entry);
		}
	}

	mutex_unlock(&vdev->vdsp_smmu_lock);
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp smmu map corresponding to ion fd
 *
 * @param[in] vdev: vdsp device driver_data
 * @param[in] fdmem: file descriptor of allocated mem
 * @param[in] va: virtual address of allocated mem
 * @param[in] size: map size
 * @param[out] smmuiova: mapped device address by the smmu
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t vdsp_smmu_map_ionfd(struct file *filp,
	int32_t fdmem, uint64_t va, uint64_t size, uint64_t *smmuiova)
{
	int32_t ret = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct hobot_vdsp_dev_data *vdev =
		(struct hobot_vdsp_dev_data *)container_of(filp->private_data,
		struct hobot_vdsp_dev_data, miscdev);
	struct vdsp_filp_info *vdsp_filp_data;
	struct vdsp_smmu_info *vdsp_smmu_data;
	int32_t vdsp_id = vdev->dsp_id;
	struct device *pdev = vdev->dev;
	struct mutex *plock = &vdev->vdsp_smmu_lock;
	struct ion_dma_buf_data *pdmabuf;

	mutex_lock(plock);

	vdsp_filp_data = vdsp_filp_info_lookup(vdev, filp);
	if (IS_ERR_OR_NULL(vdsp_filp_data)) {
		mutex_unlock(plock);
		dev_err(pdev, "vdsp%d Failed to lookup filp rbtree.\n", vdsp_id);
		return -EINVAL;
	}

	vdsp_smmu_data = vdsp_smmu_info_create(vdsp_filp_data, fdmem, va, size);
	if (NULL == vdsp_smmu_data) {
		mutex_unlock(plock);
		dev_err(pdev, "%s: vdsp%d smmu info create error[%ld]\n",
			__func__, vdsp_id, PTR_ERR(vdsp_smmu_data));
		return -ENOMEM;
	}
	pdmabuf = &vdsp_smmu_data->dmabuf;

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	ret = ion_iommu_map_ion_fd(pdev,
		pdmabuf, vdev->vdsp_ion_client, fdmem, IOMMU_WRITE | IOMMU_READ);
	if (ret < 0) {
		kfree((void *)vdsp_smmu_data);
		mutex_unlock(plock);
		dev_err(pdev, "vdsp%d Failed to map smmu.\n", vdsp_id);
		return -EFAULT;
	}

	ret = vdsp_smmu_info_add(vdsp_smmu_data);
	if (ret < 0) {
		ion_iommu_unmap_ion_fd(pdev, pdmabuf);
		kfree((void *)vdsp_smmu_data);
		mutex_unlock(plock);
		dev_err(pdev, "vdsp%d Failed to add smmu rbtree.\n", vdsp_id);
		return -EFAULT;
	}

	*smmuiova = pdmabuf->dma_addr;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	dev_dbg(pdev, "%s-%d vdsp%d fdmem-%d, va=0x%llx, size=0x%llx smmuiova=0x%llx dma_addr=0x%llx.\n",
		__func__, __LINE__, vdsp_id, fdmem, (uint64_t)va, size, *smmuiova,
		pdmabuf->dma_addr);

	mutex_unlock(plock);

	return ret;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp smmu unmap corresponding to ion fd
 *
 * @param[in] vdev: vdsp device driver_data
 * @param[in] fdmem: file descriptor of allocated mem
 * @param[in] va: virtual address of allocated mem
 * @param[in] size: map size
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t vdsp_smmu_unmap_ionfd(struct file *filp,
    int32_t fdmem, uint64_t va, uint64_t size)
{
	int32_t ret = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct hobot_vdsp_dev_data *vdev =
		(struct hobot_vdsp_dev_data *)container_of(filp->private_data,
		struct hobot_vdsp_dev_data, miscdev);
	struct vdsp_filp_info *vdsp_filp_data;
	struct vdsp_smmu_info *vdsp_smmu_data;
	int32_t vdsp_id = vdev->dsp_id;
	struct device *pdev = vdev->dev;
	struct mutex *plock = &vdev->vdsp_smmu_lock;

	mutex_lock(plock);

	vdsp_filp_data = vdsp_filp_info_lookup(vdev, filp);
	if (IS_ERR_OR_NULL(vdsp_filp_data)) {
		mutex_unlock(plock);
		dev_err(pdev, "vdsp%d Failed to lookup filp rbtree.\n", vdsp_id);
		return -EINVAL;
	}

	vdsp_smmu_data = vdsp_smmu_info_lookup(vdsp_filp_data, fdmem);
	if (IS_ERR_OR_NULL(vdsp_smmu_data)) {
		mutex_unlock(plock);
		dev_err(pdev, "vdsp%d Failed to lookup smmu rbtree.\n", vdsp_id);
		return -EINVAL;
	}

	if ((fdmem != vdsp_smmu_data->mem_fd)
		|| (va != vdsp_smmu_data->mem_va)
		|| (size != vdsp_smmu_data->mem_size)) {
		mutex_unlock(plock);
		dev_err(pdev, "vdsp%d Failed to match input(%d, 0x%llx, 0x%llx) <-> expect(%d, 0x%llx, 0x%llx)\n",
			vdsp_id, fdmem, va, size, vdsp_smmu_data->mem_fd,
			vdsp_smmu_data->mem_va, vdsp_smmu_data->mem_size);
		return -EINVAL;
	}

	ion_iommu_unmap_ion_fd(pdev, &vdsp_smmu_data->dmabuf);

	mutex_unlock(plock);

	ret = vdsp_smmu_info_put(vdsp_smmu_data);
	if (ret < 0) {
		dev_err(pdev, "vdsp%d Failed to destroy smmu info\n", vdsp_id);
		return -EFAULT;
	}

	return ret;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp smmu int
 *
 * @param[in] vdev: vdsp device driver_data
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t vdsp_smmu_init(struct hobot_vdsp_dev_data *vdev)
{
	char str_ionc[VDSP_PATH_BUF_SIZE] = {0};
	struct ion_device *local_hb_ion_dev;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	if (dma_set_coherent_mask(vdev->dev, DMA_BIT_MASK(32))) {
		dev_err(vdev->dev, "%s: vdsp%d DMA mask 32bit fail.\n", __func__, vdev->dsp_id);
		return -EFAULT;
	} else {
		dev_info(vdev->dev, "%s: vdsp%d DMA mask 32bit.\n", __func__, vdev->dsp_id);
	}

	snprintf(str_ionc, VDSP_PATH_BUF_SIZE, "%s%d", VDSP_DEV_NAME, vdev->dsp_id);
	local_hb_ion_dev = hobot_ion_get_ion_device();
	vdev->vdsp_ion_client = ion_client_create(local_hb_ion_dev, str_ionc);
	if (IS_ERR((void *)vdev->vdsp_ion_client)) {
		dev_err(vdev->dev, "%s: vdsp%d create ion client failed!!\n", __func__, vdev->dsp_id);
		return -ENOMEM;
	}

	vdev->vdsp_filp_root = RB_ROOT;
	mutex_init(&vdev->vdsp_smmu_lock);

	return 0;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp smmu deint
 *
 * @param[in] vdev: vdsp device driver_data
 *
 * @retval None
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void vdsp_smmu_deinit(struct hobot_vdsp_dev_data *vdev)
{
	vdsp_smmu_info_destroy_all(vdev);
	ion_client_destroy(vdev->vdsp_ion_client);
	mutex_destroy(&vdev->vdsp_smmu_lock);
}

/**
 * @NO{S05E05C01I}
 * @ASIL{B}
 * @brief vdsp acore driver open
 *
 * @param[in] filinode: inode struct
 * @param[in] filp: the filp struct
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int vdsp_char_open(struct inode *filinode, struct file *filp)
{
	int32_t ret = 0;
	struct vdsp_filp_info *vdsp_filp_data;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct hobot_vdsp_dev_data *vdev =
		(struct hobot_vdsp_dev_data *)container_of(filp->private_data,
		struct hobot_vdsp_dev_data, miscdev);

	mutex_lock(&vdev->vdsp_smmu_lock);

	vdsp_filp_data = vdsp_filp_info_lookup(vdev, filp);
	if (IS_ERR_OR_NULL(vdsp_filp_data)) {
		dev_dbg(vdev->dev, "vdsp%d will create filp rbtree.\n", vdev->dsp_id);
	} else {
		kref_get(&vdsp_filp_data->ref);
		mutex_unlock(&vdev->vdsp_smmu_lock);
		dev_info(vdev->dev, "vdsp%d find filp rbtree, ref inc.\n", vdev->dsp_id);
		return 0;
	}

	vdsp_filp_data = vdsp_filp_info_create(vdev, filp);
	if (NULL == vdsp_filp_data) {
		mutex_unlock(&vdev->vdsp_smmu_lock);
		dev_err(vdev->dev, "%s: vdsp%d filp info create error[%ld]\n",
			__func__, vdev->dsp_id, PTR_ERR(vdsp_filp_data));
		return -ENOMEM;
	}

	ret = vdsp_filp_info_add(vdev, vdsp_filp_data);
	if (ret < 0) {
		kfree((void *)vdsp_filp_data);
		mutex_unlock(&vdev->vdsp_smmu_lock);
		dev_err(vdev->dev, "vdsp%d Failed to add filp rbtree.\n", vdev->dsp_id);
		return -EFAULT;
	}

	mutex_unlock(&vdev->vdsp_smmu_lock);

	return 0;
}

static int vdsp_char_release(struct inode *filinode, struct file *filp)
{
	int32_t ret = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct hobot_vdsp_dev_data *vdev =
		(struct hobot_vdsp_dev_data *)container_of(filp->private_data,
		struct hobot_vdsp_dev_data, miscdev);
	struct vdsp_filp_info *vdsp_filp_data;

	mutex_lock(&vdev->vdsp_smmu_lock);

	vdsp_filp_data = vdsp_filp_info_lookup(vdev, filp);
	if (IS_ERR_OR_NULL(vdsp_filp_data)) {
		mutex_unlock(&vdev->vdsp_smmu_lock);
		dev_err(vdev->dev, "vdsp%d Failed to lookup filp rbtree.\n", vdev->dsp_id);
		return -EINVAL;
	}
	mutex_unlock(&vdev->vdsp_smmu_lock);

	ret = vdsp_filp_info_put(vdsp_filp_data);
	if (ret < 0) {
		dev_err(vdev->dev, "vdsp%d Failed to destroy filp info\n", vdev->dsp_id);
		return -EFAULT;
	}

	return 0;
}

/**
 * @NO{S05E05C01I}
 * @ASIL{B}
 * @brief vdsp acore driver read callback
 *
 * @param[in] filp: the file struct
 * @param[in] buffer: read out buffer
 * @param[in] length: length of buffer
 * @param[in] offset: read offset
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static ssize_t vdsp_char_read(struct file *filp,
	char __user *buffer, size_t length, loff_t *offset)
{
	return 0;
}

/**
 * @NO{S05E05C01I}
 * @ASIL{B}
 * @brief vdsp acore driver write callback
 *
 * @param[in] filp: the file struct
 * @param[in] buffer: write buffer
 * @param[in] length: length of buffer
 * @param[in] offset: write offset
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static ssize_t vdsp_char_write(struct file *filp,
	const char *buffer, size_t length, loff_t *offset)
{
	return 0;
}

/**
 * @NO{S05E05C01I}
 * @ASIL{B}
 * @brief vdsp acore driver poll
 *
 * @param[in] filp: the filp struct
 * @param[in] wait: the poll wait
 *
 * @retval "= 0": sleep in wait table
 * @retval "> 0": event occur
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static unsigned int vdsp_char_poll(struct file *filp, poll_table *wait)
{
	int32_t ret = 0;
	uint32_t mask = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct hobot_vdsp_dev_data *vdev =
		(struct hobot_vdsp_dev_data *)container_of(filp->private_data,
		struct hobot_vdsp_dev_data, miscdev);

	poll_wait(filp, &vdev->poll_wait, wait);

	ret = hobot_vdsp_char_async_boot_work_done(vdev->dsp_id);
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	dev_dbg(vdev->dev, "vdsp%d get async boot work done=%d\n", vdev->dsp_id, ret);
	if (1 == ret) {
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		mask |= POLLIN | POLLRDNORM;
	} else if (POLLERR == ret) {
		mask = POLLERR;
	} else {
		mask = 0;
	}

	return mask;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp acore driver wake up
 *
 * @param[in] dsp_id: the filp struct
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void vdsp_char_wake_up_poll(struct work_struct *work)
{
	int32_t ret = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct hobot_vdsp_dev_data *vdev =
		container_of(work, struct hobot_vdsp_dev_data, work_poll);

	if (IS_ERR_OR_NULL(vdev)) {
		pr_err("%s, hobot_vdsp_dev_data is NULL\n", __func__);
		return ;
	}

	//wait vdsp boot async work here
	(void)hobot_vdsp_char_wait_async_boot_work(vdev->dsp_id);
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	dev_dbg(vdev->dev, "vdsp%d wait async boot work return = %d\n", vdev->dsp_id, ret);

	wake_up_interruptible(&vdev->poll_wait);

	return ;
}

/**
 * @NO{S05E05C01I}
 * @ASIL{B}
 * @brief vdsp acore driver ioctl
 *
 * @param[in] filp: the filp struct
 * @param[in] cmd: the operation cmd
 * @param[in] arg: additional data to pass to ioctl
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
//coverity[HIS_metric_violation:SUPPRESS], ## violation reason SYSSW_V_VOCF_05
static long vdsp_char_ioctl(struct file *filp, unsigned int cmd,
	long unsigned int arg)
{
	int32_t ret = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_02
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	struct hobot_vdsp_dev_data *vdev =
		(struct hobot_vdsp_dev_data *)container_of(filp->private_data,
		struct hobot_vdsp_dev_data, miscdev);

	union {
		struct vdsp_info_version_data version_info;
		struct vdsp_info_ctl ctl_info;
		struct vdsp_info_smmu_data smmu_info;
#ifdef CONFIG_HOBOT_VDSP_STL
		struct vdsp_stl_info_ctl stl_info;
#endif
	} data;
	memset(&data, 0x00, sizeof(data));

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	if (_IOC_SIZE(cmd) > (uint32_t)sizeof(data)) {
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		dev_err(vdev->dev, "%s: vdsp%d arg size=%d > data size=%ld",
			__func__, vdev->dsp_id, _IOC_SIZE(cmd), sizeof(data));
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
	if (copy_from_user(&data, (void __user *)arg, (uint64_t)_IOC_SIZE(cmd))) {
		dev_err(vdev->dev, "%s: vdsp%d copy_from_user failed", __func__, vdev->dsp_id);
		return -EFAULT;
	}

	switch(cmd) {
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_START_VDSP:
	{
		(void)memset(vdev->vpathname, 0x00, VDSP_PATHNAME_BUF_SIZE);
		if (data.ctl_info.pathnamelen > 0) {
			if (copy_from_user(vdev->vpathname, (void __user *)data.ctl_info.pathname,
				(uint32_t)data.ctl_info.pathnamelen)) {
				//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
				dev_err(vdev->dev, "%s: vdsp%d copy_from_user failed", __func__, vdev->dsp_id);
				return -EFAULT;
			}
		}
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
		dev_dbg(vdev->dev, "%s-%d: vdsp%d start timeout=%d pathname=%s %d.\n",
			__func__, __LINE__, vdev->dsp_id, data.ctl_info.timeout,
			vdev->vpathname, data.ctl_info.pathnamelen);
		if (VDSP_BOOT_MODE_ASYNC == data.ctl_info.timeout) {
			queue_work(vdev->wq_poll, &(vdev->work_poll));
		}

		ret = hobot_remoteproc_boot_vdsp(data.ctl_info.dsp_id,
			data.ctl_info.timeout, vdev->vpathname);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_SET_FIRMWARE_PATH:
	{
		(void)memset(vdev->vpathname, 0x00, VDSP_PATHNAME_BUF_SIZE);
		if (copy_from_user(vdev->vpathname, (void __user *)data.ctl_info.pathname, (uint32_t)data.ctl_info.pathnamelen)) {
			dev_err(vdev->dev, "%s: vdsp%d copy_from_user failed", __func__, vdev->dsp_id);
			return -EFAULT;
		}
		ret = hb_rproc_set_vdsp_fwpath(data.ctl_info.dsp_id, vdev->vpathname);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_SET_FIRMWARE_NAME:
	{
		(void)memset(vdev->vpathname, 0x00, VDSP_PATHNAME_BUF_SIZE);
		if (copy_from_user(vdev->vpathname, (void __user *)data.ctl_info.pathname, (uint32_t)data.ctl_info.pathnamelen)) {
			dev_err(vdev->dev, "%s: vdsp%d copy_from_user failed", __func__, vdev->dsp_id);
			return -EFAULT;
		}
		ret = hb_rproc_set_vdsp_fwname(data.ctl_info.dsp_id, vdev->vpathname);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_GET_VDSP_STATUS:
	{
		ret = hobot_remoteproc_get_vdsp_status(data.ctl_info.dsp_id,
			&data.ctl_info.status);
		if (0 == ret) {
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
			//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
			//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
			if (copy_to_user((void __user *)arg, &data, (uint64_t)_IOC_SIZE(cmd))) {
				dev_err(vdev->dev, "%s:%d vdsp%d get status copy_to_user failed.",
					__func__, __LINE__, vdev->dsp_id);
				return -EFAULT;
			}
		}
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_STOP_VDSP:
	{
		cancel_work(&(vdev->work_poll));
		ret = hobot_remoteproc_shutdown_vdsp(data.ctl_info.dsp_id);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_RESET_VDSP:
	{
		cancel_work(&(vdev->work_poll));
		ret = hobot_remoteproc_reset_vdsp(data.ctl_info.dsp_id);
		break;
	}
#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_STL_RESET_VDSP:
	{
		ret = hobot_remoteproc_stl_reset_vdsp(data.stl_info.dsp_id,
			&data.stl_info.reset_cnt);
		if (0 == ret) {
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
			//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
			//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
			if (copy_to_user((void __user *)arg, &data, (uint64_t)_IOC_SIZE(cmd))) {
				dev_err(vdev->dev, "%s:%d vdsp%d reset_cnt copy_to_user failed.",
					__func__, __LINE__, vdev->dsp_id);
				return -EFAULT;
			}
		}
		break;
	}
#endif
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_MEM_ALLOC:
	{
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
		dev_dbg(vdev->dev, "%s:%d vdsp%d mem alloc and smmu map.",
			__func__, __LINE__, vdev->dsp_id);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_MEM_FREE:
	{
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
		dev_dbg(vdev->dev, "%s:%d vdsp%d mem free and smmu unmap.",
			__func__, __LINE__, vdev->dsp_id);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_MMU_MAP:
	{
		ret = vdsp_smmu_map_ionfd(filp, data.smmu_info.smmufd, data.smmu_info.va,
			data.smmu_info.size, &data.smmu_info.smmuiova);
		if (0 == ret) {
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
			//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
			//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
			if (copy_to_user((void __user *)arg, &data, (uint64_t)_IOC_SIZE(cmd))) {
				dev_err(vdev->dev, "%s:%d vdsp%d smmu info copy_to_user failed.",
					__func__, __LINE__, vdev->dsp_id);
				return -EFAULT;
			}
		}
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_MMU_UNMAP:
	{
		ret = vdsp_smmu_unmap_ionfd(filp,
			data.smmu_info.smmufd, data.smmu_info.va, data.smmu_info.size);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case VDSP_IOCTL_GET_VERSION_INFO:
	{
		data.version_info.major = VDSP_DRIVER_API_VERSION_MAJOR;
		data.version_info.minor = VDSP_DRIVER_API_VERSION_MINOR;
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
		if (copy_to_user((void __user *)arg, &data, (uint64_t)_IOC_SIZE(cmd))) {
			dev_err(vdev->dev, "%s:%d vdsp%d version copy_to_user failed.",
				__func__, __LINE__, vdev->dsp_id);
			return -EFAULT;
		}
		break;
	}
	default:
		dev_err(vdev->dev, "%s: vdsp%d invalid ioctl cmd %d\n", __func__, vdev->dsp_id, cmd);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		dev_err(vdev->dev, "%s: vdsp%d cmd %d execute error, ret =%d\n",
			__func__, vdev->dsp_id, cmd, ret);
		return -EFAULT;
	}

	return ret;
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static struct file_operations vdsp_fops = {
	.open = vdsp_char_open,
	.release = vdsp_char_release,
	.read = vdsp_char_read,
	.write = vdsp_char_write,
	.poll = vdsp_char_poll,
	.unlocked_ioctl = vdsp_char_ioctl,
	.compat_ioctl = vdsp_char_ioctl
};

/* sysfs attr groups */
extern const struct attribute_group vdsp_ctrl_attr_group;

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static const struct attribute_group *vdsp_attr_groups[] = {
	&vdsp_ctrl_attr_group,
	NULL,
};

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp state init
 *
 * @param[in] vdev: vdsp device driver_data
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t vdsp_state_init(struct hobot_vdsp_dev_data *vdev)
{
	vdev->vdsp_state.is_wwdt_enable = 0;
	vdev->vdsp_state.loglevel = INFO_TYPE_LEVEL;
	vdev->vdsp_state.is_trace_on = 0;
	vdev->vdsp_state.uart_switch = 1;

	return 0;
}

static const struct of_device_id hobot_vdsp_dt_ids[] = {
	{ .compatible = "hobot,vdsp0", },
	{ .compatible = "hobot,vdsp1", },
	{ /* sentinel */ }
};

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp acore driver probe
 *
 * @param[in] pdev: platform device
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_vdsp_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	char str_poll_wq[VDSP_PATH_BUF_SIZE] = {0};
	const struct of_device_id *device_id = NULL;
	struct hobot_vdsp_dev_data *pdata = NULL;

	dev_info(&pdev->dev, "%s start\n", __func__);

#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	(void)diag_report_probe_sta(ModuleId_Vdsp0, MODULE_STATE_BEGIN);
#endif

	device_id = of_match_device(hobot_vdsp_dt_ids, &pdev->dev);
	if (NULL == device_id) {
		dev_err(&pdev->dev, "%s invalid match device\n", __func__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
	pdata = (struct hobot_vdsp_dev_data *)kzalloc(sizeof(struct hobot_vdsp_dev_data),
		GFP_KERNEL);
	if (NULL == pdata) {
		dev_err(&pdev->dev, "%s Can't alloc vdsp dev data mem\n", __func__);
		return -ENOMEM;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS] ## violation reason SYSSW_V_10.8_01
	pdata->vpathname = (char *)kzalloc(VDSP_PATHNAME_BUF_SIZE, GFP_KERNEL);
	if (NULL == pdata->vpathname) {
		dev_err(&pdev->dev, "%s Can't alloc vdsp fw pathname mem\n", __func__);
		kfree(pdata);
		return -ENOMEM;
	}

	if (0 == strcmp(device_id->compatible, "hobot,vdsp0")) {
		pdata->dsp_id = VDSP0_IDX;

	} else {
		pdata->dsp_id = VDSP1_IDX;
	}
	platform_set_drvdata(pdev, pdata);
	pdata->dev = &pdev->dev;

	ret = vdsp_smmu_init(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s vdsp%d vdsp_smmu_init failed\n", __func__, pdata->dsp_id);
		kfree(pdata->vpathname);
		kfree(pdata);
		return ret;
	}

	pdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (VDSP0_IDX == pdata->dsp_id) {
		pdata->miscdev.name	= VDSP0_DEV_NAME;
	} else if (VDSP1_IDX == pdata->dsp_id) {
		pdata->miscdev.name	= VDSP1_DEV_NAME;
	} else {
		dev_err(&pdev->dev, "invalid dsp_id:%d\n", pdata->dsp_id);
	}
	pdata->miscdev.fops	= &vdsp_fops;
	pdata->miscdev.groups = vdsp_attr_groups;

	ret = misc_register(&pdata->miscdev);
	if (ret != 0) {
		dev_err(&pdev->dev, "vdsp%d Register vdsp char device failed\n", pdata->dsp_id);
		vdsp_smmu_deinit(pdata);
		kfree(pdata->vpathname);
		kfree(pdata);
		return ret;
	}
	dev_set_drvdata(pdata->miscdev.this_device, pdata);

	init_waitqueue_head(&pdata->poll_wait);
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	INIT_WORK(&(pdata->work_poll), vdsp_char_wake_up_poll);

	snprintf(str_poll_wq, sizeof(str_poll_wq), "vdsp%d_wq_poll", pdata->dsp_id);
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	pdata->wq_poll = create_singlethread_workqueue(str_poll_wq);
	if (NULL == pdata->wq_poll) {
		dev_err(&pdev->dev, "vdsp%d create_singlethread_workqueue %s error\n", pdata->dsp_id, str_poll_wq);
		vdsp_smmu_deinit(pdata);
		misc_deregister(&pdata->miscdev);
		kfree(pdata->vpathname);
		kfree(pdata);
		return ret;
	}

	vdsp_state_init(pdata);

#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	(void)diag_report_probe_sta(ModuleId_Vdsp0, MODULE_STATE_END);
#endif

	dev_info(&pdev->dev, "%s vdsp%d Registered successfully\n", __func__, pdata->dsp_id);

	return 0;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp acore driver suspend
 *
 * @param[in] dev: device
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_vdsp_suspend(struct device *dev)
{
	int32_t ret = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata =
		(struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	(void)diag_report_suspend_sta(ModuleId_Vdsp0, MODULE_STATE_BEGIN);
#endif

	dev_info(dev, "%s vdsp%d\n", __func__, pdata->dsp_id);

#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	(void)diag_report_suspend_sta(ModuleId_Vdsp0, MODULE_STATE_END);
#endif

	return ret;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp acore driver resume
 *
 * @param[in] dev: device
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_vdsp_resume(struct device *dev)
{
	int32_t ret = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata =
		(struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	(void)diag_report_resume_sta(ModuleId_Vdsp0, MODULE_STATE_BEGIN);
#endif

	dev_info(dev, "%s vdsp%d\n", __func__, pdata->dsp_id);

#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	(void)diag_report_resume_sta(ModuleId_Vdsp0, MODULE_STATE_END);
#endif

	return ret;
}

static const struct dev_pm_ops hobot_vdsp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(hobot_vdsp_suspend, hobot_vdsp_resume) /*PRQA S ALL*/
};

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp acore driver remove
 *
 * @param[in] pdev: platform device
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_vdsp_remove(struct platform_device *pdev)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata =
		(struct hobot_vdsp_dev_data *)dev_get_drvdata(&pdev->dev);

#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	(void)diag_report_remove_sta(ModuleId_Vdsp0, MODULE_STATE_BEGIN);
#endif

	dev_info(pdata->dev, "%s vdsp%d\n", __func__, pdata->dsp_id);

	destroy_workqueue(pdata->wq_poll);
	misc_deregister(&pdata->miscdev);
	vdsp_smmu_deinit(pdata);
	kfree(pdata->vpathname);
	kfree(pdata);

#ifdef CONFIG_HOBOT_VDSP_STL
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	(void)diag_report_remove_sta(ModuleId_Vdsp0, MODULE_STATE_END);
#endif

	return 0;
}

static struct platform_driver hobot_vdsp_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "hobot-vdsp",
		.of_match_table = hobot_vdsp_dt_ids,
		.pm	= &hobot_vdsp_pm_ops,
	},
	.probe = hobot_vdsp_probe,
	.remove = hobot_vdsp_remove,
};

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp acore driver register
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t __init hobot_vdsp_init(void)
{
	int32_t ret = 0;

	pr_info("%s start\n", __func__);

	ret = platform_driver_register(&hobot_vdsp_driver);
	if (ret)
		pr_err("%s platform_driver_register error\n", __func__);

	pr_info("%s end\n", __func__);

	return ret;
}

/**
 * @NO{S05E05C01U}
 * @ASIL{B}
 * @brief vdsp acore driver unregister
 *
 * @retval "0": succeed
 * @retval <0: failed
 *
 * @data_read None
 * @data_updated None
 * @compatibility HW: J5/J6
 * @compatibility SW: 1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void __exit hobot_vdsp_exit(void)
{
	pr_info("%s start\n", __func__);
	platform_driver_unregister(&hobot_vdsp_driver);
	pr_info("%s end\n", __func__);
}

//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS] ## violation reason SYSSW_V_8.5_01
module_init(hobot_vdsp_init);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS] ## violation reason SYSSW_V_8.5_01
module_exit(hobot_vdsp_exit);

MODULE_VERSION(HB_VDSP_DRIVER_VERSION_STR);
MODULE_DESCRIPTION("Hobot VDSP Acore Drivers");
MODULE_LICENSE("GPL");
