/*-----------------------------------------------------------------------
第13章  Hook技术
《加密与解密（第四版）》
(c)  看雪学院 www.kanxue.com 2000-2018
-----------------------------------------------------------------------*/


/*

  IDTHook.C

  Author: achillis
  Last Updated: 2006-03-23

  This framework is generated by EasySYS 0.3.0
  This template file is copying from QuickSYS 0.3.0 written by Chunhua Liu

*/

#include "dbghelp.h"
#include "IDTHook.h"
#include "ntifs.h"
#include "myKernel.h"

#define MAKELONG(addr1,addr2) ((addr1) | ((addr2)<<16))

typedef struct _IDTENTRY{
	unsigned short LowOffset;
	unsigned short Selector;
	unsigned char UnUsed_Io;
	unsigned char Segment_Type:4;
	unsigned char System_SegMent_Flag:1;
	unsigned char DPL:2;
	unsigned char P:1;
	unsigned short HiOffset;
}IDTENTRY,*PIDTENTRY;

typedef struct _IDTR{
	unsigned short IDTLimit;
	unsigned short LowIDTbase;
	unsigned short HiDIDTbase;
}IDTR;

// Device driver routine declarations.
//

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT		DriverObject,
	IN PUNICODE_STRING		RegistryPath
	);

NTSTATUS
IdthookDispatchCreate(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	);

NTSTATUS
IdthookDispatchClose(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	);

NTSTATUS
IdthookDispatchDeviceControl(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	);

VOID
IdthookUnload(
	IN PDRIVER_OBJECT		DriverObject
	);

ULONG GetIdtAddress(IN const ULONG Index);
VOID SetIdtAddress(ULONG Index, ULONG Address);
VOID InitGlobalVar();
VOID IdtFun();
VOID InstallIDTHook();
VOID UnInstallIDTHook();
void MySleep(LONG msec);
VOID FilterFun();

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, IdthookDispatchCreate)
#pragma alloc_text(PAGE, IdthookDispatchClose)
#pragma alloc_text(PAGE, IdthookDispatchDeviceControl)
#pragma alloc_text(PAGE, IdthookUnload)
#endif // ALLOC_PRAGMA


ULONG g_OriginalIDTAddress = 0;
PIDTENTRY pstTempEntry = NULL;
PIDTENTRY g_pIDTEntry = NULL ;
ULONG g_CallCnt = 0 ;

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT		DriverObject,
	IN PUNICODE_STRING		RegistryPath
	)
{
	//
    // Create dispatch points for device control, create, close.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = IdthookDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = IdthookDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IdthookDispatchDeviceControl;
    DriverObject->DriverUnload                         = IdthookUnload;
	
	InitGlobalVar();
	InstallIDTHook();

	DbgPrint("[IDTHook] Loaded!\n");
    return STATUS_SUCCESS;
}

NTSTATUS
IdthookDispatchCreate(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	)
{
	NTSTATUS status = STATUS_SUCCESS;

    Irp->IoStatus.Information = 0;

	dprintf("[IDTHook] IRP_MJ_CREATE\n");

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS
IdthookDispatchClose(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	)
{
	NTSTATUS status = STATUS_SUCCESS;

    Irp->IoStatus.Information = 0;

	dprintf("[IDTHook] IRP_MJ_CLOSE\n");

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS
IdthookDispatchDeviceControl(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	dprintf("[IDTHook] IRP_MJ_DEVICE_CONTROL\n");

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

VOID
IdthookUnload(
	IN PDRIVER_OBJECT		DriverObject
	)
{
	UnInstallIDTHook();
    DbgPrint("[IDTHook] Unloaded\n");
}

VOID InitGlobalVar()
{
	IDTR idtr;
	__asm {sidt idtr}
	g_pIDTEntry = (PIDTENTRY)MAKELONG(idtr.LowIDTbase,idtr.HiDIDTbase);
}

/*
_declspec(naked) VOID IdtFun()
{
	ULONG ulAddress;
	_asm{
		pushfd
		pushad
		mov ebp,esp
		sub esp,0x40
		mov ulAddress,eax
		
	}
	DbgPrint("[%d]Idt HOOK\n",g_CallCnt);
	_asm
	{
		mov esp,ebp
		popad
		popfd
		jmp g_OriginalIDTAddress
	}
}
*/

_declspec(naked) VOID IdtFun()
{
	_asm{
		pushfd
		pushad
		call FilterFun
		popad
		popfd
		jmp g_OriginalIDTAddress
	}
}

VOID  FilterFun()
{
	InterlockedIncrement(&g_CallCnt);
	DbgPrint("[Cnt = %d] CurrentProcess = %s \n",g_CallCnt,PsGetProcessImageFileName(PsGetCurrentProcess()));
}

VOID InstallIDTHook()
{
	ULONG newAddr = (ULONG)IdtFun;
	g_OriginalIDTAddress = GetIdtAddress(0x2A);
	DbgPrint("Set 0x2A ISR to 0x%X\n",newAddr);
	__asm cli
	SetIdtAddress(0x2A,newAddr); //nt!KiCallbackReturn
	__asm sti
}

VOID UnInstallIDTHook()
{
	if (g_OriginalIDTAddress != 0)
	{
		DbgPrint("Restore 0x2A ISR to 0x%X\n",g_OriginalIDTAddress);
		__asm cli
		SetIdtAddress(0x2A,g_OriginalIDTAddress);
		__asm sti
	}
}

//设置新的地址，并返回旧地址
VOID SetIdtAddress(ULONG Index, ULONG Address)
{
	ULONG OriginalAddr = 0 ;
	PIDTENTRY pTempEntry = g_pIDTEntry + Index;
	pTempEntry->LowOffset = (USHORT)Address;
	pTempEntry->HiOffset = (USHORT)((ULONG)Address >> 16);
}

//返回指定IDT项的地址
ULONG GetIdtAddress(IN const ULONG Index)
{
	ULONG ulAddress = 0;
	PIDTENTRY pTempEntry = g_pIDTEntry + Index;
	ulAddress = MAKELONG(pTempEntry->LowOffset,pTempEntry->HiOffset);
	return ulAddress;
}

void MySleep(LONG msec)
{
	LARGE_INTEGER my_interval;
	my_interval.QuadPart=-10000; 
	my_interval.QuadPart*=msec;
	KeDelayExecutionThread(KernelMode,0,&my_interval); 
	
}