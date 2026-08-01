
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_MLA_PREPROCESS_H_
#define ACLNN_MLA_PREPROCESS_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnMlaPreprocessGetWorkspaceSize
 * parameters :
 * input : required
 * gamma0 : required
 * beta0 : required
 * quantScale0 : required
 * quantOffset0 : required
 * wdqkv : required
 * descale0 : required
 * bias0 : required
 * gamma1 : required
 * beta1 : required
 * quantScale1 : required
 * quantOffset1 : required
 * wuq : required
 * descale1 : required
 * bias1 : required
 * gamma2 : required
 * cos : required
 * sin : required
 * wuk : required
 * kvCache : required
 * kvCacheRope : required
 * slotMapping : required
 * ctkvScale : required
 * qNopeScale : required
 * wdqDim : optional
 * qRopeDim : optional
 * kRopeDim : optional
 * epsilon : optional
 * qRotaryCoeff : optional
 * kRotaryCoeff : optional
 * transeposeWdq : optional
 * transeposeWuq : optional
 * transeposeWuk : optional
 * cacheMode : optional
 * quantMode : optional
 * doRmsNorm : optional
 * wdkvSplitCount : optional
 * qOutOut : required
 * kvCacheOutOut : required
 * qRopeOutOut : required
 * krCacheOutOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMlaPreprocessGetWorkspaceSize(
    const aclTensor *input,
    const aclTensor *gamma0,
    const aclTensor *beta0,
    const aclTensor *quantScale0,
    const aclTensor *quantOffset0,
    const aclTensor *wdqkv,
    const aclTensor *descale0,
    const aclTensor *bias0,
    const aclTensor *gamma1,
    const aclTensor *beta1,
    const aclTensor *quantScale1,
    const aclTensor *quantOffset1,
    const aclTensor *wuq,
    const aclTensor *descale1,
    const aclTensor *bias1,
    const aclTensor *gamma2,
    const aclTensor *cos,
    const aclTensor *sin,
    const aclTensor *wuk,
    const aclTensor *kvCache,
    const aclTensor *kvCacheRope,
    const aclTensor *slotMapping,
    const aclTensor *ctkvScale,
    const aclTensor *qNopeScale,
    int64_t wdqDim,
    int64_t qRopeDim,
    int64_t kRopeDim,
    double epsilon,
    int64_t qRotaryCoeff,
    int64_t kRotaryCoeff,
    bool transeposeWdq,
    bool transeposeWuq,
    bool transeposeWuk,
    int64_t cacheMode,
    int64_t quantMode,
    bool doRmsNorm,
    int64_t wdkvSplitCount,
    const aclTensor *qOutOut,
    const aclTensor *kvCacheOutOut,
    const aclTensor *qRopeOutOut,
    const aclTensor *krCacheOutOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnMlaPreprocess
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMlaPreprocess(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
