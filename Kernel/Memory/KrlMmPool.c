/**********************************************************
        内存池文件KrlMmPool.c
***********************************************************
                彭东
**********************************************************/
#include "BaseType.h"
#include "List.h"
#include "KrlMmManage.h"
#include "KrlMmPool.h"
#include "KrlMmAlloc.h"

DefinedMEMData(GMemPoolManage, GMemPoolData);
PoolParam PoolParamArr[] = {
    {2, 32}, {2, 64}, {2, 96}, {2, 128}, 
    {2, 160}, {2, 192}, {2, 224}, {2, 256}, 
    {2, 288}, {2, 320}, {2, 352}, {2, 384}, 
    {4, 416}, {4, 448}, {4, 480}, {4, 512}, 
    {4, 544}, {4, 576}, {4, 608}, {4, 640}, 
    {4, 672}, {4, 704}, {4, 736}, {4, 768}, 
    {4, 800}, {4, 832}, {4, 864}, {4, 896}, 
    {8, 928}, {8, 960}, {8, 992}, {8, 1024}, 
    {8, 1056}, {8, 1088}, {8, 1120}, {8, 1152}, 
    {8, 1184}, {8, 1216}, {8, 1248}, {8, 1280}, 
    {8, 1312}, {8, 1344}, {8, 1376}, {8, 1408}, 
    {8, 1440}, {8, 1472}, {8, 1504}, {8, 1536}, 
    {8, 1568}, {8, 1600}, {8, 1632}, {8, 1664}, 
    {8, 1696}, {8, 1728}, {8, 1760}, {8, 1796}, 
    {16, 1824}, {16, 1856}, {16, 1888}, {16, 1920}, 
    {16, 1952}, {16, 1984}, {16, 2016}, {16, 2048}, 
};

private void POEntitiesInit(POEntities* init)
{
    IF_NULL_RETURN_NULL(init);
    INIT_OBJOFPTR_ZERO(init);
    ListInit(&init->Lists);
    return;
}

private void KMemPoolInit(KMemPool* init)
{
    IF_NULL_RETURN(init);
    INIT_OBJOFPTR_ZERO(init);
    ListInit(&init->Lists);
    ListInit(&init->ObjLists);
    ListInit(&init->PMSADsLists);
    MLockInit(&init->Lock);
    return;
}

private void GMemPoolManageInit(GMemPoolManage* init)
{
    IF_NULL_RETURN(init);
    INIT_OBJOFPTR_ZERO(init);
    ListInit(&init->Lists);
    MLockInit(&init->Lock);
    KMemPoolInit(&init->KMemPoolMain);
    return;
}

private KMemPool* ForSizeRetKMemPoolOnGMemPoolManage(GMemPoolManage* gmpm, Size size)
{
    IF_NULL_RETURN_NULL(gmpm);
    if(NULL != gmpm->KMemPoolCache)
    {
        if((gmpm->KMemPoolCache->Size > size) && (gmpm->KMemPoolCache->Size < (size + PER_POE_INCSIZE)))
        {
            return gmpm->KMemPoolCache;
        }
    }
    for(UInt i = 0; i < KMPOOL_MAX; i++)
    {
        if(NULL != gmpm->KMemPoolArr[i])
        {
            if((gmpm->KMemPoolArr[i]->Size > size) && (gmpm->KMemPoolCache->Size < (size + PER_POE_INCSIZE)))
            {
                gmpm->KMemPoolCache = &gmpm->KMemPoolArr[i];
                return &gmpm->KMemPoolArr[i];
            }
        }
    }
    return NULL;
}

private POEntities* PickPOEntitiesOnKMemPool(KMemPool* pool)
{
    POEntities* entities = NULL;
    IF_NULL_RETURN_NULL(pool);
    IF_LTNONE_RETRUN_NULL(pool->FreeObjNR);
    IF_LTNONE_RETRUN_NULL(pool->ObjNR);
    IF_EQT_RETURN(TRUE, ListIsEmptyCareful(&pool->ObjLists), NULL);
    entities = ListFirstOne(&pool->ObjLists, POEntities, Lists);
    ListDel(&entities->Lists);
    pool->FreeObjNR--;
    return entities;
}

private UInt POEntitiesArrInitOnMemSPace(KMemPool* pool, Addr start, Addr end)
{
    POEntities* entstart = NULL;
    POEntities* entend = NULL;
    UInt i = 0;

    entstart = (POEntities*)start;
    entend = (POEntities*)end;
    for(; entstart < entend; i++)
    {
        POEntitiesInit(entstart);
        ListAdd(&entstart->Lists, &pool->ObjLists);
        pool->ObjNR++;
        pool->FreeObjNR++;
        entstart = (POEntities*)(((UInt)entstart) + ((UInt)pool->Size));
    }
    IF_EQT_RETURN(0, i, 0);
    if(start < pool->VAddrStart)
    {
        pool->VAddrStart = start;
    }
    if(end > pool->VAddrEnd)
    {
        pool->VAddrEnd = end;
    }
    return i;
}

private Bool ExtendKMemPoolCapacity(GMemPoolManage* gmpm, KMemPool* pool)
{
    PMSAD* msad = NULL;
    Addr start = NULL; 
    Addr end = NULL;
    UInt nr = 0; 
    IF_NULL_RETURN_FALSE(gmpm);
    IF_NULL_RETURN_FALSE(pool);
    msad = KrlMmAllocKernPMSADs(pool->AllocPMSADNR);
    IF_NULL_RETURN_FALSE(msad);

    start = PMSADRetVAddr(msad);
    end = start + (Addr)(KrlMmGetPMSADsSize(msad) - 1);

    nr = POEntitiesArrInitOnMemSPace(pool, start, end);
    IF_LTNONE_RETRUN_FALSE(nr);
    return TRUE;
}

private void* NewPOEntitiesOnKMemPool(GMemPoolManage* gmpm, KMemPool* pool, Size size)
{
    POEntities* entities = NULL;
    Bool rets = FALSE;
    IF_NULL_RETURN_NULL(gmpm);
    IF_NULL_RETURN_NULL(pool);
    IF_LTN_RETURN(pool->Size, size, NULL);
    IF_GTN_RETURN(pool->Size, (size + PER_POE_INCSIZE), NULL);
    entities = PickPOEntitiesOnKMemPool(pool);
    if(NULL != entities)
    {
        INIT_OBJOFPTR_ZERO(entities);
        return (void*)entities;
    }
    rets = ExtendKMemPoolCapacity(gmpm, pool);
    IF_NEQ_RETURN(TRUE, rets, NULL);
    return (void*)PickPOEntitiesOnKMemPool(pool);
}

private void* KrlMmNewPOEntitiesRealizeCore(GMemPoolManage* gmpm, Size size)
{
    void* addr = NULL;
    KMemPool* pool = NULL;
    IF_NULL_RETURN_NULL(gmpm);

    pool = ForSizeRetKMemPoolOnGMemPoolManage(gmpm, size);
    IF_NULL_RETURN_NULL(pool);
    
    KrlMmLocked(&pool->Lock);
    addr = NewPOEntitiesOnKMemPool(gmpm, pool, size);
    KrlMmUnLock(&pool->Lock);
    return addr;
}

private void* KrlMmNewPOEntitiesRealize(Size size)
{
    void* addr = NULL;
    GMemPoolManage* gmpm = NULL;
    
    gmpm = KrlMmGetGMemPoolAddr();
    IF_NULL_RETURN_NULL(gmpm);

    KrlMmLocked(&gmpm->Lock);
    addr = KrlMmNewPOEntitiesRealizeCore(gmpm, size);
    KrlMmUnLock(&gmpm->Lock);
    return addr;
}

public void* KrlMmNewPOEntities(Size size)
{
    IF_LTNONE_RETRUN_NULL(size);
    return KrlMmNewPOEntitiesRealize(size);
}

public Bool KrlMmPoolInit()
{
    GMemPoolManageInit(&GMemPoolData);
    return TRUE;
}