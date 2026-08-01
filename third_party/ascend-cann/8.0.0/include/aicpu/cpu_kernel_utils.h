/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * Description: api of cpu kernel utils
 */

#ifndef CPU_KERNEL_UTILS_H
#define CPU_KERNEL_UTILS_H
#include <memory>
#include <functional>

#include "cpu_tensor.h"
#include "cpu_node_def.h"
#include "cpu_attr_value.h"
#include "cpu_context.h"

namespace aicpu {
class AICPU_VISIBILITY CpuKernelUtils {
public:
    /*
     * create Tensor.
     * @return std::shared_ptr<Tensor>: Tensor ptr
     */
    static std::shared_ptr<Tensor> CreateTensor();

    /*
     * create Tensor.
     * @param tensor: Tensor impl
     * @return std::shared_ptr<Tensor>: Tensor ptr
     */
    static std::shared_ptr<Tensor> CreateTensor(TensorImpl *tensor);

    /*
     * get tensor impl.
     */
    static std::shared_ptr<TensorImpl> GetImpl(const Tensor *tensor);

    /*
     * get tensor name.
     */
    static std::string GetTensorName(const Tensor *tensor);

    /*
     * set tensor name.
     */
    static void SetTensorName(const std::string &name, std::shared_ptr<Tensor> &tensor);

    /*
     * create Tensor shape.
     * @return std::shared_ptr<TensorShape>: TensorShape ptr
     */
    static std::shared_ptr<TensorShape> CreateTensorShape();

    /*
     * create Tensor Shape.
     * @param tensorShape: Tensor shape impl
     * @return std::shared_ptr<TensorShape>: TensorShape ptr
     */
    static std::shared_ptr<TensorShape> CreateTensorShape(TensorShapeImpl *tensorShape);

    /*
     * get tensor shape impl.
     */
    static std::shared_ptr<TensorShapeImpl> GetImpl(const TensorShape *tensorShape);

    /*
     * create attr value.
     * @return std::shared_ptr<AttrValue>: attr value ptr
     */
    static std::shared_ptr<AttrValue> CreateAttrValue();

    /*
     * create attr value.
     * @param attrValue: attr value impl
     * @return std::shared_ptr<AttrValue>: attr value ptr
     */
    static std::shared_ptr<AttrValue> CreateAttrValue(AttrValueImpl *attrValue);

    /*
     * get attr value impl.
     */
    static std::shared_ptr<AttrValueImpl> GetImpl(const AttrValue *attrValue);

    /*
     * create node def.
     * @return std::shared_ptr<NodeDef>: node def ptr
     */
    static std::shared_ptr<NodeDef> CreateNodeDef();

    /*
     * ParallelFor shards the "total" units of work.
     * @param ctx: context info of kernel
     * @param total: size of total work
     * @param perUnitSize: expect size of per unit work
     * @param work: process of per unit work
     * @return uint32_t: 0->sucess other->failed
     */
    static uint32_t ParallelFor(const CpuKernelContext &ctx, int64_t total, int64_t perUnitSize,
        const std::function<void(int64_t, int64_t)> &work);

    /*
     * Get CPU number
     * @param ctx: context info of kernel
     * @return CPU number
     */
    static uint32_t GetCPUNum(const CpuKernelContext &ctx);

	/*
     * UpdateCustWorkSpaceInfo
     * @param ctx: context info of kernel
     * @param workspace_size: size of  workspace
     * @param workspace_addr: addr of workspce
     */
    static void UpdateCustWorkSpaceInfo(CpuKernelContext *ctx, uint64_t workspace_size,
                                        uint64_t workspace_addr);
};
} // namespace aicpu
#endif // CPU_KERNEL_UTILS_H
