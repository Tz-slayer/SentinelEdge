
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_SWI_GLU_QUANT_V2_H_
#define ACLNN_SWI_GLU_QUANT_V2_H_
#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnSwiGluQuantV2GetWorkspaceSize
 * parameters :
 * x : required
 * smoothScalesOptional : optional
 * offsetsOptional : optional
 * groupIndexOptional : optional
 * activateLeft : optional
 * quantModeOptional : optional
 * groupListType : optional
 * dstType : optional
 * yOut : required
 * scaleOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnSwiGluQuantV2GetWorkspaceSize(
    const aclTensor *x,
    const aclTensor *smoothScalesOptional,
    const aclTensor *offsetsOptional,
    const aclTensor *groupIndexOptional,
    bool activateLeft,
    char *quantModeOptional,
    int64_t groupListType,
    int64_t dstType,
    const aclTensor *yOut,
    const aclTensor *scaleOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnSwiGluQuantV2
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnSwiGluQuantV2(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
