/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 * Description: 梯度切分功能对外头文件
 */

#ifndef GRADIENT_SEGMENT_PUB_H
#define GRADIENT_SEGMENT_PUB_H

#include <map>
#include <memory>
#include <vector>
#include <mutex>
#include <hccl/hccl_types.h>
#include "hccl/base.h"

namespace hccl {
/*
实现梯度切分功能的类，
目前支持通过用户基于梯度层数或数据量进行配置。
若无配置，则按数据量固定切分。
*/
class GradientSegment {
public:
    explicit GradientSegment();
    virtual ~GradientSegment();

    /* 执行梯度切分功能 */
    HcclResult GetGradientSegmentExecutor(const std::string &group, const struct model_feature *feature,
        std::vector<u32>& segment_index, bool &isUseFusionLib,
        GradSplitForceMode force = GradSplitForceMode::FORCE_NONE,
        OriginalGraphShapeType shapeType = OriginalGraphShapeType::KNOWN_SHAPE);

protected:
private:
    HcclResult GetSegmentByIndex(const std::string &group, u32 featGradNum, std::vector<u32> &segList) const;
    HcclResult GetSegmentBySize(const std::string &group, u32 featGradNum, std::vector<u32> &segList,
        const std::vector<float> &accumGradList);
    HcclResult GetSplitResInEachSegment(const std::vector<float> &accumGradList, float gradSize,         \
        std::vector<u32> &segList, float &allocGradSize, float &preSizeLeft);
    HcclResult GetSegmentByDefaultRatio(const std::vector<float> &accumGradList, u32 featGradNum,
        std::vector<u32> &segList);
    HcclResult CheckAndConfigSegment(std::vector<float> &segmentSizeProportion, float totalSize,  \
        std::vector<float> &segmentSize) const;
    OriginalGraphShapeType shapeType_;
    HcclResult GetIdxByBinarySearch(const std::vector<float> &accumGradList, const float &curSize, u32 &segGradIdx);
    HcclResult GetNearIdxByDataSize(const std::vector<float> &accumGradList, u32 &segGradIdx,
        float gradSize, s32 midIdx) const;
    HcclResult GetFixedSizeSegmentByDefaultRatio(const std::vector<float> &accumGradList, u32 featGradNum,
        std::vector<u32> &segList);
    HcclResult GetTwoSegmentByDefaultRatio(const std::vector<float> &accumGradList, u32 featGradNum,
        std::vector<u32> &segList);
};
    extern std::map<std::string, std::vector<u32>> g_segmentIdxMap;
    extern std::map<std::string, std::vector<float>> g_segmentSizeMap;
    extern std::mutex g_segmentIdxMapLock;
    extern std::mutex g_segmentSizeMapLock;
}  // namespace hccl

#endif /* GRADIENT_SEGMENT_PUB_H */
