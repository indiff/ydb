# Generated by devtools/yamaker.

LIBRARY()

OWNER(
    orivej
    g:cpp-contrib
)

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm12
    contrib/libs/llvm12/include
    contrib/libs/llvm12/lib/Analysis
    contrib/libs/llvm12/lib/IR
    contrib/libs/llvm12/lib/Support
    contrib/libs/llvm12/lib/Transforms/AggressiveInstCombine
    contrib/libs/llvm12/lib/Transforms/InstCombine
    contrib/libs/llvm12/lib/Transforms/Utils
)

ADDINCL(
    contrib/libs/llvm12/lib/Transforms/Scalar
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    ADCE.cpp
    AlignmentFromAssumptions.cpp
    AnnotationRemarks.cpp
    BDCE.cpp
    CallSiteSplitting.cpp
    ConstantHoisting.cpp
    ConstraintElimination.cpp
    CorrelatedValuePropagation.cpp
    DCE.cpp
    DeadStoreElimination.cpp
    DivRemPairs.cpp
    EarlyCSE.cpp
    FlattenCFGPass.cpp
    Float2Int.cpp
    GVN.cpp
    GVNHoist.cpp
    GVNSink.cpp
    GuardWidening.cpp
    IVUsersPrinter.cpp
    IndVarSimplify.cpp
    InductiveRangeCheckElimination.cpp
    InferAddressSpaces.cpp
    InstSimplifyPass.cpp
    JumpThreading.cpp
    LICM.cpp
    LoopAccessAnalysisPrinter.cpp
    LoopDataPrefetch.cpp
    LoopDeletion.cpp
    LoopDistribute.cpp
    LoopFlatten.cpp
    LoopFuse.cpp
    LoopIdiomRecognize.cpp
    LoopInstSimplify.cpp
    LoopInterchange.cpp
    LoopLoadElimination.cpp
    LoopPassManager.cpp
    LoopPredication.cpp
    LoopRerollPass.cpp
    LoopRotation.cpp
    LoopSimplifyCFG.cpp
    LoopSink.cpp
    LoopStrengthReduce.cpp
    LoopUnrollAndJamPass.cpp
    LoopUnrollPass.cpp
    LoopUnswitch.cpp
    LoopVersioningLICM.cpp
    LowerAtomic.cpp
    LowerConstantIntrinsics.cpp
    LowerExpectIntrinsic.cpp
    LowerGuardIntrinsic.cpp
    LowerMatrixIntrinsics.cpp
    LowerWidenableCondition.cpp
    MakeGuardsExplicit.cpp
    MemCpyOptimizer.cpp
    MergeICmps.cpp
    MergedLoadStoreMotion.cpp
    NaryReassociate.cpp
    NewGVN.cpp
    PartiallyInlineLibCalls.cpp
    PlaceSafepoints.cpp
    Reassociate.cpp
    Reg2Mem.cpp
    RewriteStatepointsForGC.cpp
    SCCP.cpp
    SROA.cpp
    Scalar.cpp
    ScalarizeMaskedMemIntrin.cpp
    Scalarizer.cpp
    SeparateConstOffsetFromGEP.cpp
    SimpleLoopUnswitch.cpp
    SimplifyCFGPass.cpp
    Sink.cpp
    SpeculateAroundPHIs.cpp
    SpeculativeExecution.cpp
    StraightLineStrengthReduce.cpp
    StructurizeCFG.cpp
    TailRecursionElimination.cpp
    WarnMissedTransforms.cpp
)

END()