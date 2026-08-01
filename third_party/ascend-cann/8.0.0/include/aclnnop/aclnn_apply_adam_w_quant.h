
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_APPLY_ADAM_WQUANT_H_
#define ACLNN_APPLY_ADAM_WQUANT_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnApplyAdamWQuantGetWorkspaceSize
 * parameters :
 * varRef : required
 * grad : required
 * mRef : required
 * vRef : required
 * qmapM : required
 * qmapV : required
 * absmaxMRef : required
 * absmaxVRef : required
 * step : required
 * lr : optional
 * beta1 : optional
 * beta2 : optional
 * weightDecay : optional
 * eps : optional
 * gnormScale : optional
 * quantModeOptional : optional
 * blockSize : optional
 * varRef : required
 * mRef : required
 * vRef : required
 * absmaxMRef : required
 * absmaxVRef : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnApplyAdamWQuantGetWorkspaceSize(
    aclTensor *varRef,
    const aclTensor *grad,
    aclTensor *mRef,
    aclTensor *vRef,
    const aclTensor *qmapM,
    const aclTensor *qmapV,
    aclTensor *absmaxMRef,
    aclTensor *absmaxVRef,
    const aclTensor *step,
    double lr,
    double beta1,
    double beta2,
    double weightDecay,
    double eps,
    double gnormScale,
    char *quantModeOptional,
    int64_t blockSize,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnApplyAdamWQuant
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnApplyAdamWQuant(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
