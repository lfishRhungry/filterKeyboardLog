#include "listen.h"

// ��Ҫ����Ķ�����irp
ULONG gC2pKeyCount = 0;

// �����м����豸
NTSTATUS
c2pAttachDevices(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
) {
    NTSTATUS status = 0;
    UNICODE_STRING uniNtNameString;
    PC2P_DEV_EXT devExt;
    PDEVICE_OBJECT pFilterDeviceObject = NULL; // �������ɵĹ���
    PDEVICE_OBJECT pTargetDeviceObject = NULL; // Ҫ�󶨵�Ŀ���豸
    PDEVICE_OBJECT pLowerDeviceObject = NULL; // ��ǰ��ջ���豸
    PDRIVER_OBJECT KbdDriverObject = NULL; // ����������

    KdPrint(("MyAttach\n"));
    RtlInitUnicodeString(&uniNtNameString, KBD_DRIVER_NAME);
    //��ȡ����������
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
        // ��ʱ������
        ObDereferenceObject(KbdDriverObject);
    }
    // �����������豸���� �������豸
    pTargetDeviceObject = KbdDriverObject->DeviceObject;
    while (pTargetDeviceObject) {
        // ����һ�������豸
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
        // ��ʼ���豸��չ
        devExt = (PC2P_DEV_EXT)(pFilterDeviceObject->DeviceExtension);
        devExt->TargetDeviceObject = pTargetDeviceObject;
        devExt->LowerDeviceObject = pLowerDeviceObject;
        // ͳһ�豸����
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
    // �ѵ�ǰ�߳�����Ϊ��ʵʱģʽ���Ա����������о�����Ӱ����������
    KeSetPriorityThread(CurrentThread, LOW_REALTIME_PRIORITY);
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("Driver unLoading...\n");
    // ���������豸��һ�ɽ����
    DeviceObject = DriverObject->DeviceObject;
    while (DeviceObject) {
        c2pDetach(DeviceObject);
        DeviceObject = DeviceObject->NextDevice;
    }
    // ���������δ����irp
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

//��Ҫ�Ǵ������豸��ʱ�γ�
NTSTATUS c2pPnP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
) {
    PC2P_DEV_EXT devExt;
    PIO_STACK_LOCATION irpStack;
    NTSTATUS status = STATUS_SUCCESS;
    // �����ʵ�豸��
    devExt = (PC2P_DEV_EXT)(DeviceObject->DeviceExtension);
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->MinorFunction) {
        case IRP_MN_REMOVE_DEVICE:
            KdPrint(("IRP_MN_REMOVE_DEVICE\n"));
            // ���Ȱ�������ȥ
            IoSkipCurrentIrpStackLocation(Irp);
            IoCallDriver(devExt->LowerDeviceObject, Irp);
            // Ȼ�����󶨡�
            IoDetachDevice(devExt->LowerDeviceObject);
            // ɾ���������ɵ������豸��
            IoDeleteDevice(DeviceObject);
            status = STATUS_SUCCESS;
            break;

        default:
            // �����������͵�IRP��ȫ����ֱ���·����ɡ� 
            IoSkipCurrentIrpStackLocation(Irp);
            status = IoCallDriver(devExt->LowerDeviceObject, Irp);
    }
    return status;
}

//read��ɺ���
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
        // ������������ж��������Ϣ
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
    // ���ǵ��豸�������ײ���豸��������
    if (Irp->CurrentLocation == 1) {
        ULONG ReturnedInformation = 0;
        KdPrint(("Dispatch encountered bogus current location\n"));
        status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = ReturnedInformation;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return(status);
    }
    // ȫ�ֱ�������������1
    gC2pKeyCount++;
    // ���ûص���������IRP������ȥ
    // ʣ�µ�������Ҫ�ȴ���������ɡ�
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
    // ��������дһ��IRP_MJ_POWER������������Ϊ���������м�Ҫ����
    // һ��PoCallDriver��һ��PoStartNextPowerIrp���Ƚ����⡣
    DriverObject->MajorFunction[IRP_MJ_POWER] = c2pPower;
    // ������֪��ʲôʱ��һ�����ǰ󶨹����豸��ж���ˣ�����ӻ�����
    // ���ε��ˣ�������ר��дһ��PNP�����弴�ã��ַ�����
    DriverObject->MajorFunction[IRP_MJ_PNP] = c2pPnP;
    DriverObject->DriverUnload = c2pUnload;
    // �����м����豸
    status = c2pAttachDevices(DriverObject, RegistryPath);
    DbgPrint("Driver loaded\n");
    return status;
}

