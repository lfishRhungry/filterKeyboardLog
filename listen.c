#include "listen.h"

// 需要处理的读键盘irp
ULONG gC2pKeyCount = 0;

// 绑定所有键盘设备
NTSTATUS
c2pAttachDevices(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
) {
    NTSTATUS status = 0;
    UNICODE_STRING uniNtNameString;
    PC2P_DEV_EXT devExt;
    PDEVICE_OBJECT pFilterDeviceObject = NULL; // 我们生成的过滤
    PDEVICE_OBJECT pTargetDeviceObject = NULL; // 要绑定的目标设备
    PDEVICE_OBJECT pLowerDeviceObject = NULL; // 绑定前的栈顶设备
    PDRIVER_OBJECT KbdDriverObject = NULL; // 键盘类驱动

    KdPrint(("MyAttach\n"));
    RtlInitUnicodeString(&uniNtNameString, KBD_DRIVER_NAME);
    //获取键盘类驱动
    status = ObReferenceObjectByName(
        &uniNtNameString,
        OBJ_CASE_INSENSITIVE,
        NULL,
        0,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        &KbdDriverObject
    );
    if (!NT_SUCCESS(status)) {
        KdPrint(("Couldn't get the kybdclass Device Object\n"));
        return(status);
    }
    else {
        // 及时解引用
        ObDereferenceObject(KbdDriverObject);
    }
    // 遍历驱动下设备链表 绑定所有设备
    pTargetDeviceObject = KbdDriverObject->DeviceObject;
    while (pTargetDeviceObject) {
        // 生成一个过滤设备
        status = IoCreateDevice(
            IN DriverObject,
            IN sizeof(C2P_DEV_EXT),
            IN NULL,
            IN pTargetDeviceObject->DeviceType,
            IN pTargetDeviceObject->Characteristics,
            IN FALSE,
            OUT & pFilterDeviceObject
        );
        if (!NT_SUCCESS(status)) {
            KdPrint(("Couldn't create the Filter Device Object\n"));
            return (status);
        }
        pLowerDeviceObject = IoAttachDeviceToDeviceStack(pFilterDeviceObject, pTargetDeviceObject);
        if (!pLowerDeviceObject) {
            KdPrint(("Couldn't attach to Device Object\n"));
            IoDeleteDevice(pFilterDeviceObject);
            pFilterDeviceObject = NULL;
            return(status);
        }
        // 初始化设备扩展
        devExt = (PC2P_DEV_EXT)(pFilterDeviceObject->DeviceExtension);
        devExt->TargetDeviceObject = pTargetDeviceObject;
        devExt->LowerDeviceObject = pLowerDeviceObject;
        // 统一设备属性
        pFilterDeviceObject->DeviceType = pLowerDeviceObject->DeviceType;
        pFilterDeviceObject->Characteristics = pLowerDeviceObject->Characteristics;
        pFilterDeviceObject->StackSize = pLowerDeviceObject->StackSize + 1;
        pFilterDeviceObject->Flags |= pLowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
        //next device 
        pTargetDeviceObject = pTargetDeviceObject->NextDevice;
    }
    return status;
}

VOID
c2pDetach(IN PDEVICE_OBJECT pDeviceObject) {
    PC2P_DEV_EXT devExt = (PC2P_DEV_EXT)pDeviceObject->DeviceExtension;
    __try {
        __try {
            IoDetachDevice(devExt->TargetDeviceObject);
            devExt->TargetDeviceObject = NULL;
            IoDeleteDevice(pDeviceObject);
            KdPrint(("Detach Finished\n"));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    __finally {}
    return;
}

VOID
c2pUnload(IN PDRIVER_OBJECT DriverObject) {
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_OBJECT OldDeviceObject;
    PC2P_DEV_EXT devExt;
    LARGE_INTEGER	lDelay;
    PRKTHREAD CurrentThread;
    //delay some time 
    lDelay = RtlConvertLongToLargeInteger(100 * DELAY_ONE_MILLISECOND);
    CurrentThread = KeGetCurrentThread();
    // 把当前线程设置为低实时模式，以便让它的运行尽量少影响其他程序。
    KeSetPriorityThread(CurrentThread, LOW_REALTIME_PRIORITY);
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("Driver unLoading...\n");
    // 遍历所有设备并一律解除绑定
    DeviceObject = DriverObject->DeviceObject;
    while (DeviceObject) {
        c2pDetach(DeviceObject);
        DeviceObject = DeviceObject->NextDevice;
    }
    // 如果还存在未处理irp
    while (gC2pKeyCount) {
        KeDelayExecutionThread(KernelMode, FALSE, &lDelay);
    }
    DbgPrint("Driver unLoad OK!\n");
    return;
}

NTSTATUS c2pDispatchGeneral(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
) {
    KdPrint(("Other Diapatch!"));
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(((PC2P_DEV_EXT)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp);
}

NTSTATUS c2pPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
) {
    PC2P_DEV_EXT devExt = (PC2P_DEV_EXT)DeviceObject->DeviceExtension;
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(devExt->LowerDeviceObject, Irp);
}

//主要是处理有设备临时拔出
NTSTATUS c2pPnP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
) {
    PC2P_DEV_EXT devExt;
    PIO_STACK_LOCATION irpStack;
    NTSTATUS status = STATUS_SUCCESS;
    // 获得真实设备。
    devExt = (PC2P_DEV_EXT)(DeviceObject->DeviceExtension);
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->MinorFunction) {
        case IRP_MN_REMOVE_DEVICE:
            KdPrint(("IRP_MN_REMOVE_DEVICE\n"));
            // 首先把请求发下去
            IoSkipCurrentIrpStackLocation(Irp);
            IoCallDriver(devExt->LowerDeviceObject, Irp);
            // 然后解除绑定。
            IoDetachDevice(devExt->LowerDeviceObject);
            // 删除我们生成的虚拟设备。
            IoDeleteDevice(DeviceObject);
            status = STATUS_SUCCESS;
            break;

        default:
            // 对于其他类型的IRP，全部都直接下发即可。 
            IoSkipCurrentIrpStackLocation(Irp);
            status = IoCallDriver(devExt->LowerDeviceObject, Irp);
    }
    return status;
}

//read完成函数
NTSTATUS c2pReadComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
) {
    PIO_STACK_LOCATION IrpSp;
    PKEYBOARD_INPUT_DATA KeyData;
    size_t i, numKeys;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    if (NT_SUCCESS(Irp->IoStatus.Status)) {
        // 缓冲区里可能有多个按键信息
        KeyData = (PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
        numKeys = Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);
        for (i = 0; i < numKeys; ++i) {
            CHAR guess[20] = { 0 };
            DbgPrint("\n");
            DbgPrint("ScanCode: 0x%x ", KeyData->MakeCode);
            MakeCodeToASCII(KeyData[i].MakeCode, KeyData[i].Flags, guess);
            DbgPrint("GuessCode: %s\n", guess);
        }
    }
    gC2pKeyCount--;
    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }
    return Irp->IoStatus.Status;
}

NTSTATUS c2pDispatchRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp) {
    NTSTATUS status = STATUS_SUCCESS;
    PC2P_DEV_EXT devExt;
    PIO_STACK_LOCATION currentIrpStack;
    KEVENT waitEvent;
    KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
    // 我们的设备变成了最底层的设备。。。。
    if (Irp->CurrentLocation == 1) {
        ULONG ReturnedInformation = 0;
        KdPrint(("Dispatch encountered bogus current location\n"));
        status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = ReturnedInformation;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return(status);
    }
    // 全局变量键计数器加1
    gC2pKeyCount++;
    // 设置回调函数并把IRP传递下去
    // 剩下的任务是要等待读请求完成。
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, c2pReadComplete, DeviceObject, TRUE, TRUE, TRUE);
    devExt = (PC2P_DEV_EXT)DeviceObject->DeviceExtension;
    return  IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
) {
    ULONG i;
    NTSTATUS status;
    KdPrint(("entering DriverEntry\n"));
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = c2pDispatchGeneral;
    }
    DriverObject->MajorFunction[IRP_MJ_READ] = c2pDispatchRead;
    // 单独的填写一个IRP_MJ_POWER函数。这是因为这类请求中间要调用
    // 一个PoCallDriver和一个PoStartNextPowerIrp，比较特殊。
    DriverObject->MajorFunction[IRP_MJ_POWER] = c2pPower;
    // 我们想知道什么时候一个我们绑定过的设备被卸载了（比如从机器上
    // 被拔掉了？）所以专门写一个PNP（即插即用）分发函数
    DriverObject->MajorFunction[IRP_MJ_PNP] = c2pPnP;
    DriverObject->DriverUnload = c2pUnload;
    // 绑定所有键盘设备
    status = c2pAttachDevices(DriverObject, RegistryPath);
    DbgPrint("Driver loaded\n");
    return status;
}

