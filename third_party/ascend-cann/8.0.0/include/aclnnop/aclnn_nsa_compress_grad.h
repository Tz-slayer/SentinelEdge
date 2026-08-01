
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_NSA_COMPRESS_GRAD_H_
#define ACLNN_NSA_COMPRESS_GRAD_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnNsaCompressGradGetWorkspaceSize
 * parameters :
 * outputGrad : required
 * input : required
 * weight : required
 * actSeqLenOptionalOptional : optional
 * compressBlockSize : required
 * compressStride : required
 * actSeqLenType : required
 * layoutOptionalOptional : optional
 * inputGradOut : required
 * weightGradOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnNsaCompressGradGetWorkspaceSize(
    const aclTensor *outputGrad,
    const aclTensor *input,
    const aclTensor *weight,
    const aclIntArray *actSeqLenOptionalOptional,
    int64_t compressBlockSize,
    int64_t compressStride,
    int64_t actSeqLenType,
    char *layoutOptionalOptional,
    const aclTensor *inputGradOut,
    const aclTensor *weightGradOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

__attribute__((visibility("default")))
aclnnStatus aclnnNsaCompressGradTensorGetWorkspaceSize(
    const aclTensor *outputGrad,
    const aclTensor *input,
    const aclTensor *weight,
    const aclTensor *actSeqLenOptionalOptional,
    int64_t compressBlockSize,
    int64_t compressStride,
    int64_t actSeqLenType,
    char *layoutOptionalOptional,
    const aclTensor *inputGradOut,
    const aclTensor *weightGradOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnNsaCompressGrad
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnNsaCompressGrad(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
