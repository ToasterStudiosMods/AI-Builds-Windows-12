#ifndef AURELIAN_EFI_SYSTEM_TABLE_H
#define AURELIAN_EFI_SYSTEM_TABLE_H

#include "efi/efitypes.h"

/* Table header common to all EFI tables */
typedef struct {
    UINT64  Signature;
    UINT32  Revision;
    UINT32  HeaderSize;
    UINT32  CRC32;
    UINT32  Reserved;
} EFI_TABLE_HEADER;

/* Runtime services */
typedef struct {
    EFI_TABLE_HEADER Hdr;
    EFI_STATUS (EFIAPI *GetTime)(EFI_TIME *Time, void *TimeZone);
    EFI_STATUS (EFIAPI *SetTime)(EFI_TIME *Time);
    EFI_STATUS (EFIAPI *GetWakeupTime)(BOOLEAN *Enabled, BOOLEAN *Pending, EFI_TIME *Time);
    EFI_STATUS (EFIAPI *SetWakeupTime)(BOOLEAN Enable, EFI_TIME *Time);
    void *SetVirtualAddressMap;
    void *ConvertPointer;
    UINT64  Reserved1;
    void *GetNextHighMonotonicCount;
    void *ResetSystem;
    void *UpdateCapsule;
    void *QueryCapsuleCapabilities;
    UINT64  Reserved2[2];
    void *QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

/* EFI Input Key */
typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

/* Boot services */
typedef struct {
    EFI_TABLE_HEADER Hdr;
    EFI_STATUS (EFIAPI *RaiseTpl)(UINTN NewTpl);
    void        (EFIAPI *RestoreTpl)(UINTN OldTpl);
    EFI_STATUS (EFIAPI *AllocatePages)(UINT32 Type, EFI_MEMORY_TYPE MemoryType,
                                       UINTN NoPages, EFI_PHYSICAL_ADDRESS *Memory);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS Memory, UINTN NoPages);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *MemoryMapSize, void *MemoryMap,
                                       UINTN *MapKey, UINTN *DescriptorSize,
                                       UINT32 *DescriptorVersion);
    EFI_STATUS (EFIAPI *AllocatePool)(EFI_MEMORY_TYPE PoolType, UINTN Size, void **Buffer);
    EFI_STATUS (EFIAPI *FreePool)(void *Buffer);
    EFI_STATUS (EFIAPI *CreateEvent)(UINT32 Type, EFI_TPL NotifyTpl,
                                     void (*NotifyFunction)(void *Event, void *Context),
                                     void *Context, void **Event);
    EFI_STATUS (EFIAPI *SetTimer)(void *Event, EFI_TIMER_DELAY Type,
                                   UINT64 TriggerTime);
    EFI_STATUS (EFIAPI *WaitForEvent)(UINTN NumberOfEvents, void **Event, UINTN *Index);
    EFI_STATUS (EFIAPI *SignalEvent)(void *Event);
    EFI_STATUS (EFIAPI *CloseEvent)(void *Event);
    EFI_STATUS (EFIAPI *CheckEvent)(void *Event);
    EFI_STATUS (EFIAPI *InstallProtocolInterface)(EFI_HANDLE *Handle, EFI_GUID *Protocol,
                                                   UINT32 InterfaceType, void *Interface);
    EFI_STATUS (EFIAPI *ReinstallProtocolInterface)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                                      void *OldInterface,
                                                      void *NewInterface);
    EFI_STATUS (EFIAPI *UninstallProtocolInterface)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                                      void *Interface);
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, void **Interface);
    void *RegisterProtocolNotify;
    EFI_STATUS (EFIAPI *LocateHandle)(EFI_LOCATE_SEARCH_TYPE SearchType,
                                        EFI_GUID *Protocol, void *SearchKey,
                                        UINTN *BufferSize, EFI_HANDLE *Buffer);
    EFI_STATUS (EFIAPI *LocateDevicePath)(EFI_GUID *Protocol,
                                           EFI_DEVICE_PATH_PROTOCOL **DevicePath,
                                           EFI_HANDLE *Device);
    EFI_STATUS (EFIAPI *InstallConfigurationTable)(EFI_GUID *Guid, void *Table);
    EFI_STATUS (EFIAPI *LoadImage)(BOOLEAN BootPolicy, EFI_HANDLE ParentImageHandle,
                                   EFI_DEVICE_PATH_PROTOCOL *FilePath, void *SourceBuffer,
                                   UINTN SourceSize, EFI_HANDLE *ImageHandle);
    EFI_STATUS (EFIAPI *StartImage)(EFI_HANDLE ImageHandle, UINTN *ExitDataSize,
                                     CHAR16 **ExitData);
    EFI_STATUS (EFIAPI *Exit)(EFI_HANDLE ImageHandle, EFI_STATUS ExitStatus,
                                UINTN ExitDataSize, CHAR16 *ExitData);
    EFI_STATUS (EFIAPI *UnloadImage)(EFI_HANDLE ImageHandle);
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    EFI_STATUS (EFIAPI *GetNextMonotonicCount)(UINT64 *Count);
    EFI_STATUS (EFIAPI *Stall)(UINTN Microseconds);
    EFI_STATUS (EFIAPI *SetWatchdogTimer)(UINTN Timeout, UINT64 WatchdogCode,
                                           UINT64 DataSize, CHAR16 *WatchdogData);
    void *ConnectController;
    void *DisconnectController;
    EFI_STATUS (EFIAPI *OpenProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                        void **Interface, EFI_HANDLE AgentHandle,
                                        EFI_HANDLE ControllerHandle, UINT32 Attributes);
    EFI_STATUS (EFIAPI *CloseProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                        EFI_HANDLE AgentHandle, EFI_HANDLE ControllerHandle);
    EFI_STATUS (EFIAPI *OpenProtocolInformation)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                                   void **EntryBuffer, UINTN *EntryCount);
    EFI_STATUS (EFIAPI *ProtocolsPerHandle)(EFI_HANDLE Handle, EFI_GUID ***ProtocolBuffer,
                                             UINTN *ProtocolBufferCount);
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(EFI_LOCATE_SEARCH_TYPE SearchType,
                                            EFI_GUID *Protocol, void *SearchKey,
                                            UINTN *NoHandles, EFI_HANDLE **Buffer);
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
    EFI_STATUS (EFIAPI *InstallMultipleProtocolInterfaces)(EFI_HANDLE *Handle, ...);
    EFI_STATUS (EFIAPI *UninstallMultipleProtocolInterfaces)(EFI_HANDLE Handle, ...);
    EFI_STATUS (EFIAPI *CalculateCrc32)(void *Data, UINTN DataSize, UINT32 *Crc32);
    void *CopyMem;
    void *SetMem;
    void *CreateEventEx;
} EFI_BOOT_SERVICES;

/* EFI System Table (tag matches the forward declaration in efitypes.h) */
struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER          Hdr;
    CHAR16                    *FirmwareVendor;
    UINT32                    FirmwareRevision;
    EFI_HANDLE                ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE                ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE                StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES     *RuntimeServices;
    EFI_BOOT_SERVICES         *BootServices;
    UINTN                     NumberOfTableEntries;
    void                      *ConfigurationTable;
};

#endif
